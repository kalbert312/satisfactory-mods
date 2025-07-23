// 

#include "AutoSupportBuildConfigModule.h"

#include "AbstractInstanceManager.h"
#include "BP_ModConfig_AutoSupportStruct.h"
#include "ContentTagRegistry.h"
#include "FGDriveablePawn.h"
#include "FGWaterVolume.h"
#include "LandscapeProxy.h"
#include "ModConstants.h"
#include "ModLogging.h"
#include "Engine/StaticMeshActor.h"

UAutoSupportBuildConfigModule* UAutoSupportBuildConfigModule::Get(const UWorld* World)
{
	return GetChild<UAutoSupportBuildConfigModule>(World, AutoSupportConstants::ModuleName_BuildConfig);
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
	UContentTagRegistry* ContentTagRegistry,
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

	if (HitActor->IsA<AAbstractInstanceManager>() || HitActor->IsA<AFGBuildable>())
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

	const auto ContentTags = ContentTagRegistry->GetGameplayTagContainerFor(HitActor->GetClass());
	if (ContentTags.HasTagExact(TraceIgnoreTag))
	{
		return EAutoSupportTraceHitClassification::Ignore;
	}
	else if (ContentTags.HasTagExact(TraceLandscapeTag))
	{
		return EAutoSupportTraceHitClassification::Landscape;
	}
	
	const auto* HitComponent = HitResult.GetComponent();

	if (const auto* HitMesh = GetHitStaticMesh(HitActor, HitComponent); HitMesh)
	{
		const auto PathName = HitMesh->GetPathName();
		MOD_TRACE_LOG(Verbose, TEXT("Hit mesh path: [%s]"), TEXT_STR(PathName))
		
		for (const auto& IgnorePath : TraceIgnoreVanillaMeshContentPaths)
		{
			if (PathName.StartsWith(IgnorePath))
			{
				return EAutoSupportTraceHitClassification::Ignore;
			}
		}

		for (const auto& LandscapePath : TraceLandscapeVanillaMeshContentPaths)
		{
			if (PathName.StartsWith(LandscapePath))
			{
				return EAutoSupportTraceHitClassification::Landscape;
			}
		}
	}
	
	return bOnlyLandscapeBlocks ? EAutoSupportTraceHitClassification::Ignore : EAutoSupportTraceHitClassification::Block;
}

UStaticMesh* UAutoSupportBuildConfigModule::GetHitStaticMesh(const AActor* HitActor, const UPrimitiveComponent* HitComponent)
{
	if (const auto* StaticMeshActor = Cast<AStaticMeshActor>(HitActor); StaticMeshActor && HitComponent)
	{
		if (const auto* HitMeshComponent = Cast<UStaticMeshComponent>(HitComponent); HitMeshComponent)
		{
			return HitMeshComponent->GetStaticMesh();
		}
	}

	return nullptr;
}
