//

#include "AutoSupportBuildGunExtensionsModule.h"

#include "AutoSupportGameInstanceModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "FGBuildGun.h"
#include "FGBuildGunDismantle.h"
#include "FGCharacterPlayer.h"
#include "ModConstants.h"
#include "ModLogging.h"

UAutoSupportBuildGunExtensionsModule* UAutoSupportBuildGunExtensionsModule::Get(const UWorld* World)
{
	auto* RootModule = UAutoSupportGameWorldModule::Get(World);

	if (!RootModule)
	{
		return nullptr;
	}
	
	return Cast<UAutoSupportBuildGunExtensionsModule>(RootModule->GetChildModule(AutoSupportConstants::ModuleName_BuildGunExtensions, StaticClass()));
}

void UAutoSupportBuildGunExtensionsModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	MOD_LOG(Verbose, TEXT("Invoked. Phase: [%i]"), static_cast<int32>(Phase))
	Super::DispatchLifecycleEvent(Phase);
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

void UAutoSupportBuildGunExtensionsModule::OnBuildGunDismantleStateTick(UFGBuildGunStateDismantle* State) const
{
	if (ProxyDismantleMode && IsValid(State) && State->mCurrentlyAimedAtActor && State->IsCurrentBuildGunMode(ProxyDismantleMode) && !State->mCurrentlyAimedAtActor->IsA<ABuildableAutoSupportProxy>())
	{
		// Prevents anything other than the proxies actors from being a candidate for dismantle
		State->SetAimedAtActor(nullptr);
	}
}
