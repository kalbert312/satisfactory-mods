//

#include "AutoSupportModLocalPlayerSubsystem.h"

#include "AutoSupportModSubsystem.h"
#include "BuildableAutoSupportProxy.h"
#include "FGPlayerController.h"
#include "ModLogging.h"

UAutoSupportModLocalPlayerSubsystem::UAutoSupportModLocalPlayerSubsystem()
{
}

bool UAutoSupportModLocalPlayerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer);
}

void UAutoSupportModLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UAutoSupportModLocalPlayerSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UAutoSupportModLocalPlayerSubsystem::OnBuildGunBeginPlay(AFGBuildGun* BuildGun)
{
	BuildGun->mOnBuildGunModeChanged.AddDynamic(this, &UAutoSupportModLocalPlayerSubsystem::OnBuildGunModeChanged);
	BuildGun->mOnStateChanged.AddDynamic(this, &UAutoSupportModLocalPlayerSubsystem::OnBuildGunStateChanged);
	
	auto* AutoSupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());
	const auto CurrentBuildModeClass = BuildGun ? BuildGun->GetCurrentBuildGunMode() : nullptr;

	for (const auto& Proxy : AutoSupportSubsys->AllProxies)
	{
		Proxy->OnBuildModeUpdate(CurrentBuildModeClass, GetLocalPlayer());
	}
}

void UAutoSupportModLocalPlayerSubsystem::OnBuildGunModeChanged(TSubclassOf<UFGBuildGunModeDescriptor> Descriptor)
{
	// NOTE: This only fires equipping and changing modes, but not when unequipping
	MOD_LOG(Verbose, TEXT("Build gun mode changed to [%s]"), TEXT_CLS_NAME(Descriptor))
	
	auto* AutoSupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());

	for (const auto& Proxy : AutoSupportSubsys->AllProxies)
	{
		Proxy->OnBuildModeUpdate(Descriptor, GetLocalPlayer());
	}
}

void UAutoSupportModLocalPlayerSubsystem::OnBuildGunStateChanged(EBuildGunState NewState)
{
	if (NewState == EBuildGunState::BGS_DISMANTLE)
	{
		return;
	}
	
	auto* AutoSupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());

	for (const auto& Proxy : AutoSupportSubsys->AllProxies)
	{
		Proxy->OnBuildModeUpdate(nullptr, GetLocalPlayer());
	}
}

void UAutoSupportModLocalPlayerSubsystem::OnBuildGunEndPlay(AFGBuildGun* BuildGun, EEndPlayReason::Type EndType)
{
	if (EndType == EEndPlayReason::Type::Destroyed)
	{
		BuildGun->mOnBuildGunModeChanged.RemoveAll(this);
		// TODO(k.a): Update proxies again? Change delete
	}
}

void UAutoSupportModLocalPlayerSubsystem::SyncProxyWithBuildGunState(ABuildableAutoSupportProxy* Proxy) const
{
	const auto* BuildGun = GetBuildGun();
	const auto CurrentBuildModeClass = BuildGun ? BuildGun->GetCurrentBuildGunMode() : nullptr;

	MOD_LOG(Verbose, TEXT("Syncing proxy with build mode [%s]"), TEXT_CLS_NAME(CurrentBuildModeClass))
	
	Proxy->OnBuildModeUpdate(CurrentBuildModeClass, GetLocalPlayer());
}

void UAutoSupportModLocalPlayerSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
	MOD_LOG(Verbose, TEXT("Player controller changed to [%s]"), TEXT_CLS_NAME(NewPlayerController))

	if (auto* OldBuildGun = GetBuildGun(); OldBuildGun)
	{
		MOD_LOG(Verbose, TEXT("Removing old build gun delegates."))
		OldBuildGun->mOnBuildGunModeChanged.RemoveAll(this);
	}
	
	Super::PlayerControllerChanged(NewPlayerController);
}

AFGBuildGun* UAutoSupportModLocalPlayerSubsystem::GetBuildGun() const
{
	if (const auto* Character = GetPlayerCharacter(); Character)
	{
		return Character->GetBuildGun();
	}

	return nullptr;
}

AFGCharacterPlayer* UAutoSupportModLocalPlayerSubsystem::GetPlayerCharacter() const
{
	if (!CurrentPlayerController.IsValid())
	{
		MOD_LOG(Warning, TEXT("No player controller!"))
		return nullptr;
	}

	auto* Character = CurrentPlayerController->GetControlledCharacter();
	if (!Character)
	{
		MOD_LOG(Warning, TEXT("No controlled character!"))
		return nullptr;
	}
	
	return CastChecked<AFGCharacterPlayer>(Character);
}
