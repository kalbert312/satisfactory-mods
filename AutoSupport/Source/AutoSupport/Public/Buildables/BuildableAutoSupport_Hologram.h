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

	UPROPERTY(EditDefaultsOnly)
	TMap<EAutoSupportBuildDirection, FTransform> SnapTransforms;
};

UCLASS(BlueprintType)
class AUTOSUPPORT_API ABuildableAutoSupport_Hologram : public AFGGenericBuildableHologram
{
	GENERATED_BODY()

public:
	virtual bool TrySnapToActor(const FHitResult& hitResult) override;

protected:
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TMap<TSubclassOf<AFGBuildable>, FAutoSupportSnapConfig> CustomSnapConfigurations;
	
};
