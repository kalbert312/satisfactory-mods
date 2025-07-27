// 

#include "BuildableAutoSupport_Hologram.h"

#include "ModBlueprintLibrary.h"

bool ABuildableAutoSupport_Hologram::TrySnapToActor(const FHitResult& hitResult)
{
	if (Super::TrySnapToActor(hitResult))
	{
		return true;
	}

	if (const auto* HitBuildable = Cast<AFGBuildable>(hitResult.GetActor()); HitBuildable && TryCustomSnap(hitResult, HitBuildable))
	{
		return true;
	}
	
	return false;
}

void ABuildableAutoSupport_Hologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	bool bWasSet = false;

	if (const auto* HitBuildable = Cast<AFGBuildable>(hitResult.GetActor()))
	{
		if (const auto* SnapConfigEntry = CustomSnapConfigurations.Find(HitBuildable->GetClass()); SnapConfigEntry && !SnapConfigEntry->IsLocked)
		{
			FVector SnapLocation;
			FRotator SnapRotation;
			
			if (UAutoSupportBlueprintLibrary::TryGetSnapTransformFromHitResult(HitBuildable, hitResult, this, *SnapConfigEntry, SnapLocation, SnapRotation))
			{
				SetActorLocationAndRotation(SnapLocation, SnapRotation);

				// TODO(k.a): scroll rotate via GetScrollRotateValue() & AddActorLocalRotation Yaw
				
				bWasSet = true;
			}
		}
	}

	if (!bWasSet)
	{
		Super::SetHologramLocationAndRotation(hitResult);
	}
}

bool ABuildableAutoSupport_Hologram::IsValidHitActor(AActor* hitActor) const
{
	if (Super::IsValidHitActor(hitActor))
	{
		return true;
	}
	
	if (hitActor && CustomSnapConfigurations.Contains(hitActor->GetClass()))
	{
		return true;
	}

	return false;
}

bool ABuildableAutoSupport_Hologram::TryCustomSnap(const FHitResult& HitResult, const AFGBuildable* HitBuildable)
{
	const auto* SnapConfigEntry = CustomSnapConfigurations.Find(HitBuildable->GetClass());

	if (!SnapConfigEntry || !SnapConfigEntry->IsLocked)
	{
		return false;
	}

	FVector SnapLocation;
	FRotator SnapRotation;
	if (UAutoSupportBlueprintLibrary::TryGetSnapTransformFromHitResult(HitBuildable, HitResult, this, *SnapConfigEntry, SnapLocation, SnapRotation))
	{
		SetActorLocationAndRotation(SnapLocation, SnapRotation);

		return true;
	}

	return false;
}
