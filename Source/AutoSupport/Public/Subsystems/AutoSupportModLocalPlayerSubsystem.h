// 

#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupportProxy.h"
#include "EnhancedInputComponent.h"
#include "FGBuildGun.h"
#include "FGBuildGunModeDescriptor.h"
#include "GameFramework/Actor.h"
#include "AutoSupportModLocalPlayerSubsystem.generated.h"

class AFGCharacterPlayer;
class AFGBuildGun;

UCLASS()
class AUTOSUPPORT_API UAutoSupportModLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UAutoSupportModLocalPlayerSubsystem* Get(const UWorld* World);
	static UAutoSupportModLocalPlayerSubsystem* Get(const APawn* Pawn);
	static UAutoSupportModLocalPlayerSubsystem* Get(const APlayerController* Controller);
	static UAutoSupportModLocalPlayerSubsystem* Get(const ULocalPlayer* Player);
	
	UAutoSupportModLocalPlayerSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	void SyncProxyWithBuildGunState(ABuildableAutoSupportProxy* Proxy) const;

	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	
	void OnBuildGunBeginPlay(AFGBuildGun* BuildGun);
	void OnBuildGunEndPlay(AFGBuildGun* BuildGun, EEndPlayReason::Type EndType);

	bool IsHoldingAutoBuildKey() const;

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	TWeakObjectPtr<AFGPlayerController> CurrentPlayerController;
	
	TArray<uint32> InputActionBindingHandles;

	UPROPERTY(Transient)
	bool IsAutoBuildKeyHeld = false;
	
	AFGCharacterPlayer* GetPlayerCharacter() const;
	AFGBuildGun* GetBuildGun() const;

	UFUNCTION()
	void OnBuildGunModeChanged(TSubclassOf<UFGBuildGunModeDescriptor> Descriptor);
	UFUNCTION()
	void OnBuildGunStateChanged(EBuildGunState NewState);
	
	void AddBuildModeInputBindings();
	void RemoveBuildModeInputBindings();
	
	void OnAutoBuildSupportsKeyStarted();
	void OnAutoBuildSupportsKeyCompleted();
};
