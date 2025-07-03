//

#include "AutoSupportBuildGunExtensionsModule.h"

#include "AutoSupportGameInstanceModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "FGBuildGun.h"
#include "FGBuildGunDismantle.h"
#include "ModConstants.h"
#include "ModLogging.h"
#include "NativeHookManager.h"

UAutoSupportBuildGunExtensionsModule* UAutoSupportBuildGunExtensionsModule::Get(const UWorld* World)
{
	auto* RootModule = UAutoSupportGameInstanceModule::Get(World);
	return Cast<UAutoSupportBuildGunExtensionsModule>(RootModule->GetChildModule(AutoSupportConstants::ModuleName_BuildGunExtensions, StaticClass()));
}

void UAutoSupportBuildGunExtensionsModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	if (Phase == ELifecyclePhase::CONSTRUCTION)
	{
		if (!WITH_EDITOR)
		{
			RegisterHooks();
		}
	}
}

void UAutoSupportBuildGunExtensionsModule::RegisterHooks()
{
	MOD_LOG(Verbose, TEXT("Registering hooks"))
	
	SUBSCRIBE_UOBJECT_METHOD_AFTER(AFGBuildGun, BeginPlay, [&](AFGBuildGun* BuildGun)
	{
		OnBuildGunBeginPlay(BuildGun);
	});
	
	SUBSCRIBE_UOBJECT_METHOD(AFGEquipment, EndPlay, [&](auto& Scope, AFGEquipment* Equipment, EEndPlayReason::Type Reason)
	{
		if (auto* BuildGun = Cast<AFGBuildGun>(Equipment); BuildGun)
		{
			OnBuildGunEndPlay(BuildGun, Reason);
		}
	});

	SUBSCRIBE_UOBJECT_METHOD_AFTER(UFGBuildGunStateDismantle, GetSupportedBuildModes_Implementation, [&](const UFGBuildGunStateDismantle* Self, TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& out_Modes)
	{
		AppendExtraDismantleModes(out_Modes);
	});
}

void UAutoSupportBuildGunExtensionsModule::OnBuildGunBeginPlay(AFGBuildGun* BuildGun)
{
	MOD_LOG(Verbose, TEXT("Build gun begin play. Instance: [%s]"), TEXT_STR(BuildGun->GetName()))
	
	const auto* LocalPlayer = Cast<ULocalPlayer>(BuildGun->GetNetOwningPlayer());

	if (!LocalPlayer)
	{
		MOD_LOG(Verbose, TEXT("No local player found."))
		return;
	}
			
	auto* LocalSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>();
	check(LocalSubsys)

	LocalSubsys->OnBuildGunBeginPlay(BuildGun);
}

void UAutoSupportBuildGunExtensionsModule::OnBuildGunEndPlay(AFGBuildGun* BuildGun, EEndPlayReason::Type Reason)
{
	MOD_LOG(Verbose, TEXT("Invoked. Instance: [%s], Reason: [%i]"), TEXT_STR(BuildGun->GetName()), static_cast<int32>(Reason))
	if (Reason == EEndPlayReason::Type::Destroyed)
	{
		const auto* LocalPlayer = Cast<ULocalPlayer>(BuildGun->GetNetOwningPlayer());

		if (!LocalPlayer)
		{
			MOD_LOG(Verbose, TEXT("No local player found."))
			return;
		}
					
		auto* LocalSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>();
		check(LocalSubsys)

		LocalSubsys->OnBuildGunEndPlay(BuildGun, Reason);
	}
}

void UAutoSupportBuildGunExtensionsModule::AppendExtraDismantleModes(TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& OutExtraModes) const
{
	if (ProxyDismantleMode)
	{
		OutExtraModes.Add(ProxyDismantleMode);
		MOD_LOG(Verbose, TEXT("Registered extra dismantle mode [%s]"), TEXT_CLS_NAME(ProxyDismantleMode))
	}
	else
	{
		MOD_LOG(Warning, TEXT("No ProxyDismantleMode was set."))
	}
}
