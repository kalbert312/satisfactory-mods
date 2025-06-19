// 

#pragma once

#include "CoreMinimal.h"
#include "FGPillarHologram.h"

#include "BuildableAutoSupport_Hologram.generated.h"

UCLASS()
class AUTOSUPPORT_API ABuildableAutoSupport_Hologram : public AFGPillarHologram
{
	GENERATED_BODY()

public:
	ABuildableAutoSupport_Hologram();
	
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;
};
