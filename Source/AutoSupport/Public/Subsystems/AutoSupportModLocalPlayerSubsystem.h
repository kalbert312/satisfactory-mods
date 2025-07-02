// 

#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupportProxy.h"
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
	UAutoSupportModLocalPlayerSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION()
	void OnBuildGunModeChanged(TSubclassOf<UFGBuildGunModeDescriptor> Descriptor);
	UFUNCTION()
	void OnBuildGunStateChanged(EBuildGunState NewState);
	
	void SyncProxyWithBuildGunState(ABuildableAutoSupportProxy* Proxy) const;

	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;
	
	void OnBuildGunBeginPlay(AFGBuildGun* BuildGun);
	void OnBuildGunEndPlay(AFGBuildGun* BuildGun, EEndPlayReason::Type EndType);

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly)
	TWeakObjectPtr<AFGPlayerController> CurrentPlayerController;
	
	AFGCharacterPlayer* GetPlayerCharacter() const;
	AFGBuildGun* GetBuildGun() const;
};
