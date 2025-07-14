// Copyright Epic Games, Inc. All Rights Reserved.

#include "AutoSupport.h"

#include "AutoSupportBuildGunExtensionsModule.h"
#include "AutoSupportGameInstanceModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "AutoSupportModSubsystem.h"
#include "FGBuildGun.h"
#include "FGBuildGunDismantle.h"
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
	
	SUBSCRIBE_UOBJECT_METHOD_AFTER(AFGBuildGun, BeginPlay, [&](AFGBuildGun* BuildGun)
	{
		if (IsValid(BuildGun))
		{
			auto* Module = UAutoSupportBuildGunExtensionsModule::Get(BuildGun->GetWorld());
			fgcheck(Module);
			Module->OnBuildGunBeginPlay(BuildGun);
		}
	});
	
	SUBSCRIBE_UOBJECT_METHOD(AFGEquipment, EndPlay, [&](auto& Scope, AFGEquipment* Equipment, EEndPlayReason::Type Reason)
	{
		if (auto* BuildGun = Cast<AFGBuildGun>(Equipment); BuildGun)
		{
			auto* Module = UAutoSupportBuildGunExtensionsModule::Get(BuildGun->GetWorld());
			fgcheck(IsValid(Module));
			Module->OnBuildGunEndPlay(BuildGun, Reason);
		}
	});

	SUBSCRIBE_UOBJECT_METHOD_AFTER(UFGBuildGunStateDismantle, GetSupportedBuildModes_Implementation, [&](const UFGBuildGunStateDismantle* Self, TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& out_Modes)
	{
		if (IsValid(Self))
		{
			const auto* Module = UAutoSupportBuildGunExtensionsModule::Get(Self->GetWorld());
			fgcheck(IsValid(Module));
			Module->AppendExtraDismantleModes(out_Modes);
		}
	});

	SUBSCRIBE_UOBJECT_METHOD_AFTER(UFGBuildGunStateDismantle, TickState_Implementation, [&](UFGBuildGunStateDismantle* State, float DeltaTime)
	{
		if (IsValid(State))
		{
			const auto* Module = UAutoSupportBuildGunExtensionsModule::Get(State->GetWorld());
			fgcheck(IsValid(Module));
			Module->OnBuildGunDismantleStateTick(State);
		}
	});
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAutoSupportModule, AutoSupport)