// Copyright Epic Games, Inc. All Rights Reserved.

#include "UEnes.h"
#include "EmuCore/Api/NstAPI.hpp"

#define LOCTEXT_NAMESPACE "UEnesModule"

DEFINE_LOG_CATEGORY(LogUEnesTiming);
DEFINE_LOG_CATEGORY(LogUEnesVideo);
DEFINE_LOG_CATEGORY(LogUEnesAudio);


void FUEnesModule::StartupModule()
{
	
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FUEnesModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUEnesModule, UEnes)