// 

#pragma once

#include "CoreMinimal.h"
#include "FGGenericBuildableHologram.h"

#include "BuildableAutoSupport_Hologram.generated.h"

UCLASS(BlueprintType)
class AUTOSUPPORT_API ABuildableAutoSupport_Hologram : public AFGGenericBuildableHologram
{
	GENERATED_BODY()

public:
	virtual bool TrySnapToActor(const FHitResult& hitResult) override;
	
};
