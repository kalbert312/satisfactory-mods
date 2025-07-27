// 

#pragma once

#include "CoreMinimal.h"
#include "FGGenericBuildableHologram.h"
#include "ModTypes.h"

#include "BuildableAutoSupport_Hologram.generated.h"

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportSnapConfig
{
	GENERATED_BODY()

	/**
	 * Key = EAutoSupportBuildDirection
	 */
	UPROPERTY(EditDefaultsOnly, meta=(GetKeyOptions="AutoSupportBlueprintLibrary.GetBuildDirectionNames"))
	TMap<FName, FTransform> SnapTransforms;

	/**
	 * True if this is to be handled by the TrySnapActor, which will lock rotation and placement. False for SetHologramLocationAndRotation to handle this.
	 */
	UPROPERTY(EditDefaultsOnly)
	bool IsLocked = false;
};

UCLASS(BlueprintType)
class AUTOSUPPORT_API ABuildableAutoSupport_Hologram : public AFGGenericBuildableHologram
{
	GENERATED_BODY()

public:
	virtual bool TrySnapToActor(const FHitResult& hitResult) override;
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;

protected:
	virtual bool IsValidHitActor(AActor* hitActor) const override;
	bool TryCustomSnap(const FHitResult& HitResult, const AFGBuildable* HitBuildable);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TMap<TSubclassOf<AFGBuildable>, FAutoSupportSnapConfig> CustomSnapConfigurations;
};
