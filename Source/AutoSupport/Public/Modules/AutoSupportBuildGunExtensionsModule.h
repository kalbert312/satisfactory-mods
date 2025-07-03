
#pragma once

#include "CoreMinimal.h"
#include "AutoSupportGameWorldModule.h"
#include "AutoSupportBuildGunExtensionsModule.generated.h"

class UFGDismantleModeDescriptor;
class AFGBuildGun;
class UFGBuildGunModeDescriptor;

/**
 * Child game module that manages build gun extensions. This is spawned via the Blueprint class of AutoSupportGameWorldModule.
 */
UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportBuildGunExtensionsModule : public UAutoSupportGameWorldModule
{
	GENERATED_BODY()

public:
	static UAutoSupportBuildGunExtensionsModule* Get(const UWorld* World);
	
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UFGBuildGunModeDescriptor> ProxyDismantleMode;

protected:
	void RegisterHooks();
	
	void OnBuildGunBeginPlay(AFGBuildGun* BuildGun);
	void OnBuildGunEndPlay(AFGBuildGun* BuildGun, EEndPlayReason::Type Reason);
	void AppendExtraDismantleModes(TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& OutExtraModes) const;
};
