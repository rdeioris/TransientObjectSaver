// Copyright 2023, Roberto De Ioris.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TransientObjectSaverLibrary.generated.h"

DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(FString, FTransientObjectSaverMaterialNameGenerator, UMaterialInterface*, Material, const int32, MaterialIndex, const FString&, SlotName);
DECLARE_DYNAMIC_DELEGATE_RetVal_FourParams(FString, FTransientObjectSaverTextureNameGenerator, UTexture*, Texture, UMaterialInterface*, Material, const FString&, MaterialPath, const FString&, ParamName);

/**
 * 
 */
UCLASS()
class TRANSIENTOBJECTSAVER_API UTransientObjectSaverLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Save Transient Skeleton"), Category="TransientObjectSaver")
	static bool SaveTransientSkeleton(USkeleton* Skeleton, const FString& Path);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Save Transient SkeletalMesh"), Category = "TransientObjectSaver")
	static bool SaveTransientSkeletalMesh(USkeletalMesh* SkeletalMesh, const FString& SkeletalMeshPath, const FString& SkeletonPath, const FString& PhysicsAssetPath, const FTransientObjectSaverMaterialNameGenerator& MaterialNameGenerator, const FTransientObjectSaverTextureNameGenerator& TextureNameGenerator);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Save Transient StaticMesh"), Category = "TransientObjectSaver")
	static bool SaveTransientStaticMesh(UStaticMesh* StaticMesh, const FString& StaticMeshPath, const FTransientObjectSaverMaterialNameGenerator& MaterialNameGenerator, const FTransientObjectSaverTextureNameGenerator& TextureNameGenerator);

	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Save Transient Material"), Category = "TransientObjectSaver")
	static bool SaveTransientMaterial(UMaterialInterface* Material, const FString& MaterialPath, UMaterialInterface*& OutMaterial, const FTransientObjectSaverTextureNameGenerator& TextureNameGenerator);
};
