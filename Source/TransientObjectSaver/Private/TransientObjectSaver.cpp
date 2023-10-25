// Copyright 2023, Roberto De Ioris.

#include "TransientObjectSaver.h"

#define LOCTEXT_NAMESPACE "FTransientObjectSaverModule"

void FTransientObjectSaverModule::StartupModule()
{
}

void FTransientObjectSaverModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FTransientObjectSaverModule, TransientObjectSaver)