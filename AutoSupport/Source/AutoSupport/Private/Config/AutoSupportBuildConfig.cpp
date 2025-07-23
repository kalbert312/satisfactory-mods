//

#include "AutoSupportBuildConfig.h"

#include "BP_ModConfig_AutoSupportStruct.h"
#include "FGWaterVolume.h"
#include "LandscapeProxy.h"

float UAutoSupportBuildConfig::GetMaxBuildDistance() const
{
	return FBP_ModConfig_AutoSupportStruct::GetActiveConfig(GetWorld()).ConstraintsSection.MaxBuildDistance;
}

bool UAutoSupportBuildConfig::IsLandscapeTypeHit(const FHitResult& HitResult) const
{
	const auto* HitActor = HitResult.GetActor();

	if (!HitActor)
	{
		return false;
	}

	if (HitActor->IsA<ALandscapeProxy>())
	{
		return true;
	}

	// TODO(k.a): configure cliff and other landscape meshes

	// const auto* HitComponent = HitResult.GetComponent();
	//
	// if (auto* StaticMeshActor = Cast<AStaticMeshActor>(HitActor); StaticMeshActor && HitComponent)
	// {
	// 	if (const auto* HitMeshComponent = Cast<UStaticMeshComponent>(HitComponent); HitMeshComponent)
	// 	{
	// 		if (const auto HitMesh = HitMeshComponent->GetStaticMesh(); HitMesh)
	// 		{
	// 			// TODO(k.a): check mesh
	// 		}
	// 	}
	// }
	
	return false;
}

bool UAutoSupportBuildConfig::IsIgnoredHit(const FHitResult& HitResult) const
{
	return HitResult.Distance <= 1.f || !HitResult.GetActor() || HitResult.GetActor()->IsA<AFGWaterVolume>();
}
