//

#include "AutoSupportBuildGunExtensionsModule.h"

#include "AutoSupportGameInstanceModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "FGBuildGun.h"
#include "FGBuildGunDismantle.h"
#include "FGCharacterPlayer.h"
#include "ModConstants.h"
#include "ModLogging.h"
#include "NativeHookManager.h"

UAutoSupportBuildGunExtensionsModule* UAutoSupportBuildGunExtensionsModule::Get(const UWorld* World)
{
	auto* RootModule = UAutoSupportGameWorldModule::Get(World);
	fgcheck(RootModule)
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
		if (IsValid(BuildGun))
		{
			OnBuildGunBeginPlay(BuildGun);
		}
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
		if (IsValid(Self))
		{
			AppendExtraDismantleModes(out_Modes);
		}
	});

	SUBSCRIBE_UOBJECT_METHOD_AFTER(UFGBuildGunStateDismantle, TickState_Implementation, [&](UFGBuildGunStateDismantle* State, float DeltaTime)
	{
		if (ProxyDismantleMode && IsValid(State) && State->mCurrentlyAimedAtActor && State->IsCurrentBuildGunMode(ProxyDismantleMode) && !State->mCurrentlyAimedAtActor->IsA<ABuildableAutoSupportProxy>())
		{
			// Prevents anything other than the proxies actors from being a candidate for dismantle
			State->SetAimedAtActor(nullptr);
		}
	});
}

void UAutoSupportBuildGunExtensionsModule::OnBuildGunBeginPlay(AFGBuildGun* BuildGun)
{
	const auto* LocalPlayer = Cast<ULocalPlayer>(BuildGun->GetNetOwningPlayer());

	if (!LocalPlayer)
	{
		MOD_LOG(Verbose, TEXT("No local player found."))
		return;
	}

	MOD_LOG(Verbose, TEXT("Build gun begin play. Instance: [%s], Owning player: [%s], Has Inst Char: [%s], Has Inst Controller: [%s]"), TEXT_STR(BuildGun->GetName()), TEXT_STR(LocalPlayer->GetName()), TEXT_BOOL(!!BuildGun->GetInstigatorController()), TEXT_BOOL(!!BuildGun->GetInstigatorCharacter()))

	if (auto* LocalSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>())
	{
		LocalSubsys->OnBuildGunBeginPlay(BuildGun);
	}
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

		if (auto* LocalSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>())
		{
			LocalSubsys->OnBuildGunEndPlay(BuildGun, Reason);
		}
	}
}

void UAutoSupportBuildGunExtensionsModule::AppendExtraDismantleModes(TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& OutExtraModes) const
{
	if (ProxyDismantleMode)
	{
		OutExtraModes.Add(ProxyDismantleMode);
	}
	else
	{
		MOD_LOG(Warning, TEXT("No ProxyDismantleMode was set."))
	}
}
