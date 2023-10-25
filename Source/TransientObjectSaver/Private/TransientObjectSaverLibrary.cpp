// Copyright 2023, Roberto De Ioris.


#include "TransientObjectSaverLibrary.h"
#include "IMeshBuilderModule.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Materials/MaterialInstanceConstant.h"
#include "LODUtilities.h"


namespace TransientObjectSaver
{
	bool IsTransient(UObject* Object)
	{
		while (Object)
		{
			if (Object->HasAnyFlags(RF_Transient) || Object->IsA<UMaterialInstanceDynamic>())
			{
				return true;
			}

			Object = Object->GetOuter();
		}

		return false;
	}

	bool SaveUObject(UObject* Object, const FString& Path)
	{
		if (!FPackageName::IsValidObjectPath(Path))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid UPackage path %s"), *Path);
			return false;
		}

		if (FindPackage(nullptr, *Path) || LoadPackage(nullptr, *Path, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone))
		{
			UE_LOG(LogTemp, Error, TEXT("UPackage %s already exists"), *Path);
			return false;
		}

		UPackage* NewPackage = CreatePackage(*Path);
		if (!NewPackage)
		{
			UE_LOG(LogTemp, Error, TEXT("Unable to create UPackage %s"), *Path);
			return false;
		}

		UPackage* Package = Object->GetPackage();

		if (!Object->Rename(nullptr, NewPackage, REN_DontCreateRedirectors))
		{
			UE_LOG(LogTemp, Error, TEXT("Unable to move UObject %s into UPackage %s"), *Object->GetFullName(), *Path);
			return false;
		}

		if (Package)
		{
			Package->ClearDirtyFlag();
		}

		Object->SetFlags(Object->GetFlags() | RF_Public | RF_Standalone);

		FString NewName = FPackageName::GetShortName(Path);

		if (!Object->Rename(*NewName, nullptr, REN_DontCreateRedirectors))
		{
			UE_LOG(LogTemp, Error, TEXT("Unable to rename UObject %s to %s"), *Object->GetFullName(), *NewName);
			return false;
		}

		if (!UPackage::SavePackage(NewPackage, Object, EObjectFlags::RF_Standalone | EObjectFlags::RF_Public, *FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension())))
		{
			UE_LOG(LogTemp, Error, TEXT("Unable to save UPackage %s"), *NewPackage->GetPathName());
			return false;
		}

		return true;
	}
}

bool UTransientObjectSaverLibrary::SaveTransientSkeleton(USkeleton* Skeleton, const FString& Path)
{
	if (!Skeleton)
	{
		return false;
	}

	return TransientObjectSaver::SaveUObject(Skeleton, Path);
}

bool UTransientObjectSaverLibrary::SaveTransientMaterial(UMaterialInterface* Material, const FString& MaterialPath, UMaterialInterface*& OutMaterial, const FTransientObjectSaverTextureNameGenerator& TextureNameGenerator)
{
	if (!Material)
	{
		return false;
	}

	UE_LOG(LogTemp, Error, TEXT("Material %s %s %d"), *(Material->GetFullName()), *(Material->GetOutermost()->GetFullName()), Material->GetOutermost()->HasAnyFlags(EObjectFlags::RF_Transient));
	UMaterialInstanceDynamic* MaterialInstanceDynamic = Cast<UMaterialInstanceDynamic>(Material);
	if (MaterialInstanceDynamic)
	{
		UMaterialInstanceConstant* MaterialInstance = NewObject<UMaterialInstanceConstant>();
		MaterialInstance->SetParentEditorOnly(MaterialInstanceDynamic->Parent);
		TArray<FMaterialParameterInfo> MaterialsParameterInfos;
		TArray<FGuid> ParameterGuids;

		MaterialInstanceDynamic->GetAllScalarParameterInfo(MaterialsParameterInfos, ParameterGuids);
		for (int32 ParameterIndex = 0; ParameterIndex < MaterialsParameterInfos.Num(); ParameterIndex++)
		{
			float Value = 0;
			if (MaterialInstanceDynamic->GetScalarParameterValue(MaterialsParameterInfos[ParameterIndex].Name, Value, true))
			{
				UE_LOG(LogTemp, Warning, TEXT("Param: %s [%s] = %f"), *MaterialsParameterInfos[ParameterIndex].Name.ToString(), *ParameterGuids[ParameterIndex].ToString(), Value);
				MaterialInstance->SetScalarParameterValueEditorOnly(MaterialsParameterInfos[ParameterIndex], Value);
			}
		}

		MaterialInstanceDynamic->GetAllVectorParameterInfo(MaterialsParameterInfos, ParameterGuids);
		for (int32 ParameterIndex = 0; ParameterIndex < MaterialsParameterInfos.Num(); ParameterIndex++)
		{
			FLinearColor Value = FLinearColor::Black;
			if (MaterialInstanceDynamic->GetVectorParameterValue(MaterialsParameterInfos[ParameterIndex].Name, Value, true))
			{
				UE_LOG(LogTemp, Warning, TEXT("Param: %s [%s] = %s"), *MaterialsParameterInfos[ParameterIndex].Name.ToString(), *ParameterGuids[ParameterIndex].ToString(), *Value.ToString());
				MaterialInstance->SetVectorParameterValueEditorOnly(MaterialsParameterInfos[ParameterIndex], Value);
			}
		}

		MaterialInstanceDynamic->GetAllTextureParameterInfo(MaterialsParameterInfos, ParameterGuids);
		for (int32 ParameterIndex = 0; ParameterIndex < MaterialsParameterInfos.Num(); ParameterIndex++)
		{
			UTexture* Value = nullptr;
			if (MaterialInstanceDynamic->GetTextureParameterValue(MaterialsParameterInfos[ParameterIndex].Name, Value, true))
			{
				if (TransientObjectSaver::IsTransient(Value))
				{
					if (!Value->Source.IsValid())
					{
						UTexture2D* Texture2D = Cast<UTexture2D>(Value);
						if (Texture2D)
						{
							FTexturePlatformData* PlatformData = Texture2D->GetPlatformData();
							if (PlatformData->Mips.IsValidIndex(0))
							{
								const void* Data = PlatformData->Mips[0].BulkData.LockReadOnly();

								FImageView ImageView(const_cast<void*>(Data), PlatformData->Mips[0].SizeX, PlatformData->Mips[0].SizeY, ERawImageFormat::BGRA8);
								PlatformData->Mips[0].BulkData.Unlock();
								Value->Source.Init(ImageView);
							}
						}
					}
					const FString TextureName = TextureNameGenerator.Execute(Value, Material, MaterialPath, MaterialsParameterInfos[ParameterIndex].Name.ToString());
					if (!TextureName.IsEmpty())
					{
						TransientObjectSaver::SaveUObject(Value, TextureName);
					}
				}
				UE_LOG(LogTemp, Warning, TEXT("Param: %s [%s] = %s"), *MaterialsParameterInfos[ParameterIndex].Name.ToString(), *ParameterGuids[ParameterIndex].ToString(), *Value->GetFullName());
				MaterialInstance->SetTextureParameterValueEditorOnly(MaterialsParameterInfos[ParameterIndex], Value);
			}
		}
		OutMaterial = MaterialInstance;
		return TransientObjectSaver::SaveUObject(MaterialInstance, MaterialPath);
	}
	else if (TransientObjectSaver::IsTransient(Material))
	{
		OutMaterial = Material;
		return TransientObjectSaver::SaveUObject(Material, MaterialPath);
	}

	return false;
}

bool UTransientObjectSaverLibrary::SaveTransientStaticMesh(UStaticMesh* StaticMesh, const FString& StaticMeshPath, const FTransientObjectSaverMaterialNameGenerator& MaterialNameGenerator, const FTransientObjectSaverTextureNameGenerator& TextureNameGenerator)
{
	if (!StaticMesh)
	{
		return false;
	}

	if (StaticMeshPath.IsEmpty())
	{
		return false;
	}

	if (!StaticMesh->GetMeshDescription(0))
	{
		UE_LOG(LogTemp, Error, TEXT("The StaticMesh has no MeshDescription"));
		return false;
	}

	// now fix materials
	int32 MaterialIndex = 0;
	TArray<FStaticMaterial> NewMaterials = StaticMesh->GetStaticMaterials();
	for (const FStaticMaterial& StaticMaterial : StaticMesh->GetStaticMaterials())
	{
		UMaterialInterface* Material = StaticMaterial.MaterialInterface;
		UMaterialInterface* OutMaterial = nullptr;
		const FString NewMaterialName = MaterialNameGenerator.Execute(Material, MaterialIndex, StaticMaterial.MaterialSlotName.ToString());
		if (!NewMaterialName.IsEmpty())
		{
			if (SaveTransientMaterial(Material, NewMaterialName, OutMaterial, TextureNameGenerator))
			{
				NewMaterials[MaterialIndex].MaterialInterface = OutMaterial;
			}
		}
		MaterialIndex++;
	}

	StaticMesh->SetStaticMaterials(NewMaterials);

	return TransientObjectSaver::SaveUObject(StaticMesh, StaticMeshPath);
}

bool UTransientObjectSaverLibrary::SaveTransientSkeletalMesh(USkeletalMesh* SkeletalMesh, const FString& SkeletalMeshPath, const FString& SkeletonPath, const FString& PhysicsAssetPath, const FTransientObjectSaverMaterialNameGenerator& MaterialNameGenerator, const FTransientObjectSaverTextureNameGenerator& TextureNameGenerator)
{
	if (!SkeletalMesh)
	{
		return false;
	}

	if (!SkeletonPath.IsEmpty())
	{
		if (!TransientObjectSaver::SaveUObject(SkeletalMesh->GetSkeleton(), SkeletonPath))
		{
			return false;
		}
	}

	FSkeletalMeshModel* ImportedResource = SkeletalMesh->GetImportedModel();
	if (!ImportedResource)
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to GetImportedModel()"));
		return false;
	}

	UE_LOG(LogTemp, Error, TEXT("ImportedResource: %d"), ImportedResource->LODModels.Num());

	FSkeletalMeshRenderData* RenderData = SkeletalMesh->GetResourceForRendering();
	if (!RenderData)
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to access RenderData"));
		return false;
	}

	//IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForRunningPlatform();

	const int32 NumLODs = RenderData->LODRenderData.Num();
	for (int32 LODIndex = 0; LODIndex < NumLODs; LODIndex++)
	{
		ImportedResource->LODModels[LODIndex].ActiveBoneIndices = RenderData->LODRenderData[LODIndex].ActiveBoneIndices;
		ImportedResource->LODModels[LODIndex].RequiredBones = RenderData->LODRenderData[LODIndex].RequiredBones;
		ImportedResource->LODModels[LODIndex].NumVertices = RenderData->LODRenderData[LODIndex].GetNumVertices();
		FMultiSizeIndexContainerData MultiSizeIndexContainer;
		RenderData->LODRenderData[LODIndex].MultiSizeIndexContainer.GetIndexBufferData(MultiSizeIndexContainer);
		ImportedResource->LODModels[LODIndex].IndexBuffer = MultiSizeIndexContainer.Indices;
		ImportedResource->LODModels[LODIndex].NumTexCoords = RenderData->LODRenderData[LODIndex].GetNumTexCoords();

		/*FSkeletalMeshImportData ImportData;
		ImportData.NumTexCoords = 1;
		ImportData.bHasNormals = true;
		ImportData.bHasTangents = false;
		ImportData.bHasVertexColors = false;*/

		//const uint32 NumPositions = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
		//for (uint32 VertexIndex = 0; VertexIndex < NumPositions; VertexIndex++)
		//{
			//ImportData.Points.Add(RenderData->LODRenderData[LODIndex].StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex));
			//ImportData.PointToRawMap.Add(VertexIndex);
		//}

		UE_LOG(LogTemp, Error, TEXT("LOD %d has %d vertices and %d sections"), LODIndex, ImportedResource->LODModels[LODIndex].NumVertices, ImportedResource->LODModels[LODIndex].Sections.Num());
		const int32 NumSections = RenderData->LODRenderData[LODIndex].RenderSections.Num();
		for (int32 SectionIndex = 0; SectionIndex < NumSections; SectionIndex++)
		{
			const FSkelMeshRenderSection& RenderSection = RenderData->LODRenderData[LODIndex].RenderSections[SectionIndex];

			FSkelMeshSection& Section = ImportedResource->LODModels[LODIndex].Sections[SectionIndex];
			Section.BaseIndex = RenderSection.BaseIndex;
			Section.BaseVertexIndex = RenderSection.BaseVertexIndex;
			Section.bCastShadow = RenderSection.bCastShadow;
			Section.bDisabled = RenderSection.bDisabled;
			Section.BoneMap = RenderSection.BoneMap;
			Section.bRecomputeTangent = RenderSection.bRecomputeTangent;
			Section.MaterialIndex = RenderSection.MaterialIndex;
			Section.MaxBoneInfluences = RenderSection.MaxBoneInfluences;
			Section.NumTriangles = RenderSection.NumTriangles;
			Section.NumVertices = RenderSection.NumVertices;
			Section.bUse16BitBoneIndex = RenderData->LODRenderData[LODIndex].SkinWeightVertexBuffer.Use16BitBoneIndex();

			for (uint32 Index = RenderSection.BaseIndex; Index < RenderSection.BaseIndex + (RenderSection.NumTriangles * 3); Index++)
			{
				const uint32 VertexIndex = ImportedResource->LODModels[LODIndex].IndexBuffer[Index];
				FSoftSkinVertex SkinVertex;
				SkinVertex.Position = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.PositionVertexBuffer.VertexPosition(VertexIndex);
				SkinVertex.TangentX = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentX(VertexIndex);
				SkinVertex.TangentY = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentY(VertexIndex);
				SkinVertex.TangentZ = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(VertexIndex);
				for (uint32 UVIndex = 0; UVIndex < ImportedResource->LODModels[LODIndex].NumTexCoords; UVIndex++)
				{
					SkinVertex.UVs[UVIndex] = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(VertexIndex, UVIndex);
				}
				for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; InfluenceIndex++)
				{
					SkinVertex.InfluenceBones[InfluenceIndex] = RenderData->LODRenderData[LODIndex].SkinWeightVertexBuffer.GetBoneIndex(VertexIndex, InfluenceIndex);
					SkinVertex.InfluenceWeights[InfluenceIndex] = RenderData->LODRenderData[LODIndex].SkinWeightVertexBuffer.GetBoneWeight(VertexIndex, InfluenceIndex);
				}
				Section.SoftVertices.Add(SkinVertex);

				//ImportedResource->LODModels[LODIndex].MeshToImportVertexMap.Add(VertexIndex);
			}

			/*

			for (uint32 Index = RenderSection.BaseIndex; Index < RenderSection.BaseIndex + static_cast<uint32>(RenderSection.NumVertices); Index += 3)
			{
				SkeletalMeshImportData::FTriangle Triangle;
				Triangle.MatIndex = RenderSection.MaterialIndex;
				Triangle.TangentZ[0] = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Index);
				Triangle.TangentZ[1] = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Index + 1);
				Triangle.TangentZ[2] = RenderData->LODRenderData[LODIndex].StaticVertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Index + 2);
				Triangle.WedgeIndex[0] = Index;
				Triangle.WedgeIndex[1] = Index + 1;
				Triangle.WedgeIndex[2] = Index + 2;
				ImportData.Faces.Add(Triangle);

				for (int32 TriangleIndex = 0; TriangleIndex < 3; TriangleIndex++)
				{
					SkeletalMeshImportData::FVertex Vertex;
					Vertex.VertexIndex = MultiSizeIndexContainer.Indices[Index + TriangleIndex];
					Vertex.MatIndex = Triangle.MatIndex;
					Vertex.UVs[0] = FVector2D::ZeroVector;
					ImportData.Wedges.Add(Vertex);

					for (uint32 InfluenceIndex = 0; InfluenceIndex < MaxBoneInfluences; InfluenceIndex++)
					{
						SkeletalMeshImportData::FRawBoneInfluence Influence;
						Influence.VertexIndex = Vertex.VertexIndex;
						Influence.BoneIndex = RenderData->LODRenderData[LODIndex].SkinWeightVertexBuffer.GetBoneIndex(Influence.VertexIndex, InfluenceIndex);
						Influence.Weight = FMath::Clamp(RenderData->LODRenderData[LODIndex].SkinWeightVertexBuffer.GetBoneWeight(Influence.VertexIndex, InfluenceIndex) / 255.0f, 0.0f, 1.0f);
						ImportData.Influences.Add(Influence);
					}
				}
			}
			*/
		}
	}

	// now fix materials
	int32 MaterialIndex = 0;
	TArray<FSkeletalMaterial> NewMaterials = SkeletalMesh->GetMaterials();
	for (const FSkeletalMaterial& SkeletalMaterial : SkeletalMesh->GetMaterials())
	{
		UMaterialInterface* Material = SkeletalMaterial.MaterialInterface;
		UMaterialInterface* OutMaterial = nullptr;
		const FString NewMaterialName = MaterialNameGenerator.Execute(Material, MaterialIndex, SkeletalMaterial.MaterialSlotName.ToString());
		if (!NewMaterialName.IsEmpty())
		{
			if (SaveTransientMaterial(Material, NewMaterialName, OutMaterial, TextureNameGenerator))
			{
				NewMaterials[MaterialIndex].MaterialInterface = OutMaterial;
			}
		}
		MaterialIndex++;
	}

	SkeletalMesh->SetMaterials(NewMaterials);

	SkeletalMesh->Build();

	return TransientObjectSaver::SaveUObject(SkeletalMesh, SkeletalMeshPath);
}