//

#include "AutoSupportBuildGunExtensionsModule.h"

#include "AutoSupportGameInstanceModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "FGBuildGun.h"
#include "FGBuildGunDismantle.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "ModConstants.h"
#include "ModLogging.h"

UAutoSupportBuildGunExtensionsModule* UAutoSupportBuildGunExtensionsModule::Get(const UWorld* World)
{
	return GetChild<UAutoSupportBuildGunExtensionsModule>(World, AutoSupportConstants::ModuleName_BuildGunExtensions);
}

void UAutoSupportBuildGunExtensionsModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	MOD_LOG(Verbose, TEXT("Invoked. Phase: [%i]"), static_cast<int32>(Phase))
	Super::DispatchLifecycleEvent(Phase);
}

void UAutoSupportBuildGunExtensionsModule::OnBuildGunEquip(AFGBuildGun* BuildGun, const AFGCharacterPlayer* Player)
{
	if (HookedBuildGuns.Contains(BuildGun))
	{
		MOD_LOG(Verbose, TEXT("Build gun [%s] equipped but it is already hooked."), TEXT_ACTOR_NAME(BuildGun))
		return;
	}
	
	const auto* Controller = Player->GetFGPlayerController(); // this will be null in multiplayer situations for other player's build guns.
	const auto* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;

	if (!LocalPlayer)
	{
		MOD_LOG(Verbose, TEXT("No local player found for [%s]"), TEXT_ACTOR_NAME(Player))
		return;
	}

	MOD_LOG(Verbose, TEXT("Build gun first equip. Instance: [%s], Owning player: [%s]"), TEXT_STR(BuildGun->GetName()), TEXT_STR(LocalPlayer->GetName()))

	if (auto* LocalSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>())
	{
		LocalSubsys->OnBuildGunFirstEquip(BuildGun);
	}

	HookedBuildGuns.Add(BuildGun);
}

void UAutoSupportBuildGunExtensionsModule::OnBuildGunEndPlay(AFGBuildGun* BuildGun, EEndPlayReason::Type Reason)
{
	MOD_LOG(Verbose, TEXT("Invoked. Instance: [%s], Reason: [%i]"), TEXT_STR(BuildGun->GetName()), static_cast<int32>(Reason))

	HookedBuildGuns.Remove(BuildGun);
	
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

void UAutoSupportBuildGunExtensionsModule::OnBuildGunDismantleStateTick(UFGBuildGunStateDismantle* State) const
{
	if (ProxyDismantleMode && IsValid(State) && State->mCurrentlyAimedAtActor && State->IsCurrentBuildGunMode(ProxyDismantleMode) && !State->mCurrentlyAimedAtActor->IsA<ABuildableAutoSupportProxy>())
	{
		// Prevents anything other than the proxies actors from being a candidate for dismantle
		State->SetAimedAtActor(nullptr);
	}
}
