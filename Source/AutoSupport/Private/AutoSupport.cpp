// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoSupport.h"

#include "AutoSupportModLocalPlayerSubsystem.h"
#include "AutoSupportModSubsystem.h"
#include "FGBuildGun.h"
#include "NativeHookManager.h"
#include "Common/ModLogging.h"

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
		if (EndType == EEndPlayReason::Type::Destroyed && Buildable->ShouldConvertToLightweight()) 
		{
			// call our subsys delegate in this case b/c it isn't working. This is to solve not catching removals on individual buildables for support proxys.
			auto* SupportSubsys = AAutoSupportModSubsystem::Get(Buildable->GetWorld());
			SupportSubsys->OnWorldBuildableRemoved(Buildable);
		}
	});

	SUBSCRIBE_UOBJECT_METHOD_AFTER(AFGBuildGun, BeginPlay, [](AFGBuildGun* BuildGun)
	{
		const auto* LocalPlayer = Cast<ULocalPlayer>(BuildGun->GetNetOwningPlayer());

		if (!LocalPlayer)
		{
			MOD_LOG(Verbose, TEXT("No local player found."))
			return;
		}
		
		auto* LocalSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>();
		check(LocalSubsys)

		LocalSubsys->OnBuildGunBeginPlay(BuildGun);
	});

	SUBSCRIBE_UOBJECT_METHOD(AFGEquipment, EndPlay, [](auto& Scope, AFGEquipment* Equipment, EEndPlayReason::Type EndType)
	{
		auto* BuildGun = Cast<AFGBuildGun>(Equipment);

		if (!BuildGun)
		{
			return;
		}
		
		if (EndType == EEndPlayReason::Type::Destroyed)
		{
			const auto* LocalPlayer = Cast<ULocalPlayer>(BuildGun->GetNetOwningPlayer());

			if (!LocalPlayer)
			{
				MOD_LOG(Verbose, TEXT("No local player found."))
				return;
			}
					
			auto* LocalSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>();
			check(LocalSubsys)

			LocalSubsys->OnBuildGunEndPlay(BuildGun, EndType);
		}
	});
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAutoSupportModule, AutoSupport)