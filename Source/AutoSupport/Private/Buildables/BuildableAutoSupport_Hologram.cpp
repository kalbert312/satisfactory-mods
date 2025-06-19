// 

#include "BuildableAutoSupport_Hologram.h"

#include "ModLogging.h"

ABuildableAutoSupport_Hologram::ABuildableAutoSupport_Hologram()
{
}

void ABuildableAutoSupport_Hologram::SetHologramLocationAndRotation(const FHitResult& HitResult)
{
	Super::SetHologramLocationAndRotation(HitResult);

	MOD_LOG(Verbose, TEXT("Hologram Transform Result: %s"), *HitResult.ToString())
	
	SetActorRotation(FRotator::ZeroRotator);
}

