// 

#include "BuildableAutoSupport_Hologram.h"

#include "ModBlueprintLibrary.h"
#include "ModLogging.h"

bool ABuildableAutoSupport_Hologram::TrySnapToActor(const FHitResult& hitResult)
{
	if (Super::TrySnapToActor(hitResult))
	{
		MOD_LOG(Verbose, TEXT("HitResult snapped: [%s],"), TEXT_ACTOR_NAME(hitResult.GetActor()))
		return true;
	}
	
	MOD_LOG(Verbose, TEXT("HitResult is: [%s]"), TEXT_ACTOR_NAME(hitResult.GetActor()))

	if (const auto* HitBuildable = Cast<AFGBuildable>(hitResult.GetActor()); HitBuildable)
	{
		const auto* SnapConfigEntry = CustomSnapConfigurations.Find(HitBuildable->GetClass());

		if (!SnapConfigEntry)
		{
			return false;
		}

		FVector SnapLocation;
		FRotator SnapRotation;
		if (UAutoSupportBlueprintLibrary::TryGetSnapTransformFromHitResult(HitBuildable, hitResult, *SnapConfigEntry, SnapLocation, SnapRotation))
		{
			SetActorLocationAndRotation(SnapLocation, SnapRotation);

			return true;
		}
	}
	
	return false;
}
