// 

#include "AutoSupportBuildConfigModule.h"

#include "AbstractInstanceManager.h"
#include "BP_ModConfig_AutoSupportStruct.h"
#include "ContentTagRegistry.h"
#include "FGDriveablePawn.h"
#include "FGWaterVolume.h"
#include "LandscapeProxy.h"
#include "ModConstants.h"
#include "Engine/StaticMeshActor.h"

UAutoSupportBuildConfigModule* UAutoSupportBuildConfigModule::Get(const UWorld* World)
{
	auto* RootModule = GetRoot(World);

	if (!RootModule)
	{
		return nullptr;
	}
	
	return Cast<UAutoSupportBuildConfigModule>(RootModule->GetChildModule(AutoSupportConstants::ModuleName_BuildConfig, StaticClass()));
}

void UAutoSupportBuildConfigModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	if (Phase == ELifecyclePhase::CONSTRUCTION)
	{
		TraceIgnoreTag = FGameplayTag::RequestGameplayTag(AutoSupportConstants::TagName_AutoSupport_Trace_Ignore);
		TraceLandscapeTag = FGameplayTag::RequestGameplayTag(AutoSupportConstants::TagName_AutoSupport_Trace_Landscape);
	}
}

float UAutoSupportBuildConfigModule::GetMaxBuildDistance() const
{
	return FBP_ModConfig_AutoSupportStruct::GetActiveConfig(GetWorld()).ConstraintsSection.MaxBuildDistance;
}

EAutoSupportTraceHitClassification UAutoSupportBuildConfigModule::CalculateHitClassification(
	const FHitResult& HitResult,
	const bool bOnlyLandscapeBlocks,
	TSubclassOf<UFGConstructDisqualifier>& OutDisqualifier) const
{
	OutDisqualifier = nullptr;
	const auto* HitActor = HitResult.GetActor();
	
	if (HitResult.Distance <= 1.f || !HitActor || HitActor->IsA<AFGWaterVolume>())
	{
		return EAutoSupportTraceHitClassification::Ignore;
	}
	
	if (HitActor->IsA<ALandscapeProxy>())
	{
		return EAutoSupportTraceHitClassification::Landscape;
	}

	if (HitActor->IsA<AAbstractInstanceManager>())
	{
		return bOnlyLandscapeBlocks ? EAutoSupportTraceHitClassification::Ignore : EAutoSupportTraceHitClassification::Block;
	}

	if (HitActor->IsA<APawn>())
	{
		OutDisqualifier = HitActor->IsA<AFGCharacterPlayer>()
			? UFGCDEncroachingPlayer::StaticClass()
				: HitActor->IsA<AFGDriveablePawn>()
					? UFGCDEncroachingVehicle::StaticClass()
					: UFGCDEncroachingCreature::StaticClass();

		return EAutoSupportTraceHitClassification::Pawn;
	}

	const auto ContentTags = UContentTagRegistry::Get(GetWorld())->GetGameplayTagContainerFor(HitActor->GetClass());
	if (ContentTags.HasTagExact(TraceIgnoreTag))
	{
		return EAutoSupportTraceHitClassification::Ignore;
	}
	else if (ContentTags.HasTagExact(TraceLandscapeTag))
	{
		return EAutoSupportTraceHitClassification::Landscape;
	}
	
	const auto* HitComponent = HitResult.GetComponent();
	
	if (auto* StaticMeshActor = Cast<AStaticMeshActor>(HitActor); StaticMeshActor && HitComponent)
	{
		if (const auto* HitMeshComponent = Cast<UStaticMeshComponent>(HitComponent); HitMeshComponent)
		{
			if (const auto HitMesh = HitMeshComponent->GetStaticMesh(); HitMesh)
			{
				// TODO(k.a): check mesh against set of landscape-like meshes
			}
		}
	}
	
	return bOnlyLandscapeBlocks ? EAutoSupportTraceHitClassification::Ignore : EAutoSupportTraceHitClassification::Block;
}
