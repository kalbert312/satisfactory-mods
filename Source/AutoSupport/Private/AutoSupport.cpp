// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoSupport.h"

#include "AutoSupportModSubsystem.h"
#include "Common/ModLogging.h"
#include "NativeHookManager.h"

#define LOCTEXT_NAMESPACE "FAutoSupportModule"

void FAutoSupportModule::StartupModule()
{
	if (!WITH_EDITOR)
	{
		RegisterHooks();
	}
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FAutoSupportModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FAutoSupportModule::RegisterHooks()
{
	MOD_LOG(Verbose, TEXT("Registering native hooks."))

	SUBSCRIBE_UOBJECT_METHOD(AFGBuildable, EndPlay, [](auto& Scope, AFGBuildable* Buildable, const EEndPlayReason::Type EndType)
	{
		if (Buildable->ShouldConvertToLightweight()) 
		{
			// call our subsys delegate in this case b/c it isn't working. This is to solve not catching removals on individual buildables for support proxys.
			auto* SupportSubsys = AAutoSupportModSubsystem::Get(Buildable->GetWorld());
			SupportSubsys->OnWorldBuildableRemoved(Buildable);
		}
	});
	// TODO
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAutoSupportModule, AutoSupport)