//

#include "AutoSupportModLocalPlayerSubsystem.h"

#include "AutoSupportBuildGunExtensionsModule.h"
#include "AutoSupportBuildGunInputMappingContext.h"
#include "AutoSupportModSubsystem.h"
#include "BuildableAutoSupportProxy.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "FGPlayerController.h"
#include "ModLogging.h"
#include "Kismet/GameplayStatics.h"

UAutoSupportModLocalPlayerSubsystem* UAutoSupportModLocalPlayerSubsystem::Get(const UWorld* World)
{
	if (const auto* Controller = UGameplayStatics::GetPlayerController(World, 0))
	{
		return Get(Controller);
	}
	
	return nullptr;
}

UAutoSupportModLocalPlayerSubsystem* UAutoSupportModLocalPlayerSubsystem::Get(const APawn* Pawn)
{
	if (const auto* Controller = Cast<APlayerController>(Pawn->GetController()))
	{
		return Get(Controller);
	}

	return nullptr;
}

UAutoSupportModLocalPlayerSubsystem* UAutoSupportModLocalPlayerSubsystem::Get(const APlayerController* Controller)
{
	if (const auto* LocalPlayer = Controller->GetLocalPlayer())
	{
		return LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>();
	}

	return nullptr;
}

UAutoSupportModLocalPlayerSubsystem* UAutoSupportModLocalPlayerSubsystem::Get(const ULocalPlayer* Player)
{
	return Player->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>();
}

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
	
	const auto CurrentBuildModeClass = BuildGun ? BuildGun->GetCurrentBuildGunMode() : nullptr;

	AsyncUpdateAllProxiesBuildMode(CurrentBuildModeClass);
}

void UAutoSupportModLocalPlayerSubsystem::OnBuildGunModeChanged(TSubclassOf<UFGBuildGunModeDescriptor> Descriptor)
{
	// NOTE: This only fires equipping and changing modes, but not when unequipping
	MOD_LOG(Verbose, TEXT("Build gun mode changed to [%s]"), TEXT_CLS_NAME(Descriptor))

	AsyncUpdateAllProxiesBuildMode(Descriptor);
}

void UAutoSupportModLocalPlayerSubsystem::OnBuildGunStateChanged(EBuildGunState NewState)
{
	if (NewState != EBuildGunState::BGS_DISMANTLE) // clean up for non-dismantle
	{
		AsyncUpdateAllProxiesBuildMode(nullptr);
	}

	if (NewState == EBuildGunState::BGS_BUILD)
	{
		AddBuildModeInputBindings();
	}
	else
	{
		RemoveBuildModeInputBindings();
	}
}

void UAutoSupportModLocalPlayerSubsystem::OnBuildGunEndPlay(AFGBuildGun* BuildGun, EEndPlayReason::Type EndType)
{
	if (EndType == EEndPlayReason::Type::Destroyed)
	{
		MOD_LOG(Verbose, TEXT("Build gun was explicitly destroyed"))
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

	CurrentPlayerController = Cast<AFGPlayerController>(NewPlayerController);
}

AFGBuildGun* UAutoSupportModLocalPlayerSubsystem::GetBuildGun() const
{
	if (const auto* Character = GetPlayerCharacter(); Character)
	{
		return Character->GetBuildGun();
	}

	return nullptr;
}

void UAutoSupportModLocalPlayerSubsystem::AddBuildModeInputBindings()
{
	MOD_LOG(Verbose, TEXT("Invoked"))
	
	fgcheck(CurrentPlayerController.IsValid())
	const auto* BuildGunExtensions = UAutoSupportBuildGunExtensionsModule::Get(GetWorld());
	fgcheck(BuildGunExtensions)
		
	if (IsValid(BuildGunExtensions->BuildGunBuildInputMappingContext))
	{
		auto* EnhInput = Cast<UEnhancedInputComponent>(CurrentPlayerController->InputComponent);
		InputActionBindingHandles.Add(EnhInput->BindAction(BuildGunExtensions->BuildGunBuildInputMappingContext->IA_AutoBuildSupports, ETriggerEvent::Started, this, &UAutoSupportModLocalPlayerSubsystem::OnAutoBuildSupportsKeyStarted).GetHandle());
		InputActionBindingHandles.Add(EnhInput->BindAction(BuildGunExtensions->BuildGunBuildInputMappingContext->IA_AutoBuildSupports, ETriggerEvent::Completed, this, &UAutoSupportModLocalPlayerSubsystem::OnAutoBuildSupportsKeyCompleted).GetHandle());

		auto* EnhInputSubsys = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		fgcheck(EnhInputSubsys)
		EnhInputSubsys->AddMappingContext(BuildGunExtensions->BuildGunBuildInputMappingContext, 100);
		
		MOD_LOG(Verbose, TEXT("Bound build gun extension input actions"))
	}
	else
	{
		MOD_LOG(Warning, TEXT("No input mapping context"))
	}
}

void UAutoSupportModLocalPlayerSubsystem::RemoveBuildModeInputBindings()
{
	MOD_LOG(Verbose, TEXT("Invoked"))
	
	fgcheck(CurrentPlayerController.IsValid())
	const auto* BuildGunExtensions = UAutoSupportBuildGunExtensionsModule::Get(GetWorld());
	fgcheck(BuildGunExtensions)

	auto* EnhInputSubsys = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	fgcheck(EnhInputSubsys)
	EnhInputSubsys->RemoveMappingContext(BuildGunExtensions->BuildGunBuildInputMappingContext);

	auto* EnhInput = Cast<UEnhancedInputComponent>(CurrentPlayerController->InputComponent);
	for (const auto& Handle : InputActionBindingHandles)
	{
		EnhInput->RemoveBindingByHandle(Handle);
	}
}

void UAutoSupportModLocalPlayerSubsystem::OnAutoBuildSupportsKeyStarted()
{
	MOD_LOG(Verbose, TEXT("Invoked"))
	IsAutoBuildKeyHeld = true;
}

void UAutoSupportModLocalPlayerSubsystem::OnAutoBuildSupportsKeyCompleted()
{
	MOD_LOG(Verbose, TEXT("Invoked"))
	IsAutoBuildKeyHeld = false;
}

void UAutoSupportModLocalPlayerSubsystem::AsyncUpdateAllProxiesBuildMode(TSubclassOf<UFGBuildGunModeDescriptor> ModeDescriptor) const
{
	MOD_LOG(Verbose, TEXT("Invoked"))
	AsyncTask(ENamedThreads::GameThread, [this, ModeDescriptor]
	{
		auto* LocalPlayer = GetLocalPlayer();
		auto* AutoSupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());

		auto ProxyCount = AutoSupportSubsys->AllProxies.Num();
		MOD_LOG(Verbose, TEXT("Updating all (%i) proxies with build mode [%s]"), ProxyCount, TEXT_CLS_NAME(ModeDescriptor))
		
		for (const auto& Proxy : AutoSupportSubsys->AllProxies)
		{
			if (Proxy.IsValid())
			{
				Proxy->OnBuildModeUpdate(ModeDescriptor, LocalPlayer);
			}
			else
			{
				MOD_LOG(Warning, TEXT("Proxy is invalid... skipping"))
			}
		}
		
		ProxyCount = AutoSupportSubsys->AllProxies.Num();
		MOD_LOG(Verbose, TEXT("Finished updating all (%i) proxies with build mode [%s]"), ProxyCount, TEXT_CLS_NAME(ModeDescriptor))
	});
}

bool UAutoSupportModLocalPlayerSubsystem::IsHoldingAutoBuildKey() const
{
	return IsAutoBuildKeyHeld;
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
