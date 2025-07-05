
#pragma once

#include "CoreMinimal.h"
#include "AutoSupportGameWorldModule.h"
#include "InputActionValue.h"
#include "AutoSupportBuildGunExtensionsModule.generated.h"

class UAutoSupportBuildGunInputMappingContext;
class UInputAction;
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

	/**
	 * The extra dismantle mode for auto support proxies.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UFGBuildGunModeDescriptor> ProxyDismantleMode;

	/**
	 * The additional input mapping context and actions for the build gun in build mode.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAutoSupportBuildGunInputMappingContext> BuildGunBuildInputMappingContext;

protected:
	void RegisterHooks();
	void OnBuildGunBeginPlay(AFGBuildGun* BuildGun);
	void OnBuildGunEndPlay(AFGBuildGun* BuildGun, EEndPlayReason::Type Reason);
	void AppendExtraDismantleModes(TArray<TSubclassOf<UFGBuildGunModeDescriptor>>& OutExtraModes) const;
};
