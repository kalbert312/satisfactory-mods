// 

#include "BuildableAutoSupport.h"

#include "AutoSupportGameWorldModule.h"
#include "AutoSupportModSubsystem.h"
#include "DrawDebugHelpers.h"
#include "FGBlueprintProxy.h"
#include "FGBuildingDescriptor.h"
#include "FGHologram.h"
#include "FGPlayerController.h"
#include "FGWaterVolume.h"
#include "LandscapeProxy.h"
#include "ModBlueprintLibrary.h"
#include "ModLogging.h"
#include "Kismet/GameplayStatics.h"

ABuildableAutoSupport::ABuildableAutoSupport(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

bool ABuildableAutoSupport::TraceAndCreatePlan(FAutoSupportBuildPlan& OutPlan) const
{
	// IMPORTANT: This ticks while the interact dialog is open.
	
	if (!AutoSupportData.MiddlePartDescriptor.IsValid() && !AutoSupportData.StartPartDescriptor.IsValid() && !AutoSupportData.EndPartDescriptor.IsValid())
	{
		// Nothing to build.
		return false;
	}
	
	// Trace to know how much we're going to build.
	const auto TraceResult = Trace();

	MOD_LOG(Verbose, TEXT("BuildDistance: %f, IsTerrainHit: %s, Direction: %s"), TraceResult.BuildDistance, TEXT_CONDITION(TraceResult.IsLandscapeHit), *TraceResult.Direction.ToString());

	if (FMath::IsNearlyZero(TraceResult.BuildDistance))
	{
		// No room to build.
		return true;
	}

	UAutoSupportBlueprintLibrary::PlanBuild(GetWorld(), TraceResult, AutoSupportData, OutPlan);
	
	return true;
}

void ABuildableAutoSupport::BuildSupports(APawn* BuildInstigator)
{
	FAutoSupportBuildPlan Plan;
	if (!TraceAndCreatePlan(Plan))
	{
		// Nothing planned
		return;
	}

	AFGCharacterPlayer* Player = CastChecked<AFGCharacterPlayer>(BuildInstigator);

	auto* RootHologram = UAutoSupportBlueprintLibrary::CreateCompositeHologramFromPlan(Plan, GetActorTransform(), BuildInstigator, this);
	const auto BillOfParts = RootHologram->GetCost(true);
	
	if (!UAutoSupportBlueprintLibrary::PayItemBillIfAffordable(Player, BillOfParts, true))
	{
		RootHologram->Destroy();
		return;
	}
	
	// Construct
	auto* Buildables = AFGBuildableSubsystem::Get(GetWorld());
	TArray<AActor*> ChildActors;
	RootHologram->Construct(ChildActors, Buildables->GetNewNetConstructionID());

	// TODO(k.a): BBox not finished
	FBox GroupBounds(ForceInit);
	for (auto* ChildActor : ChildActors)
	{
		auto* Buildable = CastChecked<AFGBuildable>(ChildActor);
		
		FBox PartBounds = Buildable->GetCombinedClearanceBox();

		GroupBounds += PartBounds;
		
		MOD_LOG(Verbose, TEXT("Child Buildable: %s, %s, %s, %s"), *Buildable->GetName(), TEXT_CONDITION(Buildable->ShouldConvertToLightweight()), *PartBounds.ToString(), TEXT_CONDITION(Buildable->GetBlueprintProxy() == nullptr));
	}

	// TODO(k.a): Spawn a wrapper actor, similar to AFGBlueprintProxy

	MOD_LOG(Verbose, TEXT("Completed, Bounds: %s, %"), *GroupBounds.ToString());
	
	// Dismantle self
	Execute_Dismantle(this);
}

#pragma region IFGSaveInterface

bool ABuildableAutoSupport::ShouldSave_Implementation() const
{
	return true;
}

void ABuildableAutoSupport::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	Super::PreSaveGame_Implementation(saveVersion, gameVersion);
	
	AutoSupportData.ClearInvalidReferences();
}

// This is called before BeginPlay (even when not loading a game)
void ABuildableAutoSupport::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	Super::PostLoadGame_Implementation(saveVersion, gameVersion);
	
	AutoSupportData.ClearInvalidReferences();
}

#pragma endregion

void ABuildableAutoSupport::BeginPlay()
{
	Super::BeginPlay();

	// HACK: use build effect instigator as a "is newly built" indicator
	bool bIsNew = mBuildEffectInstignator || mBuildEffectIsPlaying || mActiveBuildEffect;

	if (bIsNew)
	{
		if (GetBlueprintDesigner()) // TODO(k.a): test blueprint designer
		{
			MOD_LOG(Verbose, TEXT("Has blueprint designer"));
			bAutoConfigure = false;
		}
		else if (GetBlueprintProxy()) // Is it being built from a blueprint?
		{
			MOD_LOG(Verbose, TEXT("Built from blueprint"));
			bAutoConfigure = false;

			if (FBP_ModConfig_AutoSupportStruct::GetActiveConfig(GetWorld()).GameplayDefaultsSection.AutomaticBlueprintBuild)
			{
				BuildSupports(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
				MOD_LOG(Verbose, TEXT("Auto building!"));
			}
			else
			{
				MOD_LOG(Verbose, TEXT("Not auto building, config = false"));
			}
		}

		if (bAutoConfigure)
		{
			AutoConfigure();
		}
	}

	bAutoConfigure = false;
}

#pragma region Editor Only
#if WITH_EDITOR

EDataValidationResult ABuildableAutoSupport::IsDataValid(FDataValidationContext& Context) const
{
	const auto SuperResult = Super::IsDataValid(Context);
	
	if (!GetCombinedClearanceBox().IsValid)
	{
		Context.AddError(FText::FromString("Invalid clearance box! There must be a clearance box set on the actor properties."));
		return EDataValidationResult::Invalid;
	}
	
	return SuperResult;
}

#endif
#pragma endregion

void ABuildableAutoSupport::AutoConfigure()
{
	MOD_LOG(Verbose, TEXT("Auto Configuring"));
	K2_AutoConfigure();
}

FAutoSupportTraceResult ABuildableAutoSupport::Trace() const
{
	FAutoSupportTraceResult Result;
	Result.BuildDistance = FBP_ModConfig_AutoSupportStruct::GetActiveConfig(GetWorld()).ConstraintsSection.MaxBuildDistance;
	Result.BuildDirection = AutoSupportData.BuildDirection;

	// Start building our trace params.
	FCollisionQueryParams QueryParams;
	QueryParams.TraceTag = FName("BuildableAutoSupport_Trace");
	QueryParams.AddIgnoredActor(this);

	auto StartTransform = GetActorTransform();
	MOD_LOG(Verbose, TEXT("Start Transform: %s"), *StartTransform.ToString());
	
	// Set up a direction vector
	auto DirectionVector = UAutoSupportBlueprintLibrary::GetDirectionVector(AutoSupportData.BuildDirection);
	MOD_LOG(Verbose, TEXT("Direction Vector: %s"), *DirectionVector.ToString());
	
	DirectionVector = StartTransform.GetRotation().RotateVector(DirectionVector);
	MOD_LOG(Verbose, TEXT("Rotated Direction Vector: %s"), *DirectionVector.ToString());
	
	Result.Direction = DirectionVector;
	
	// Determine the starting trace location. This will be opposite the face of the selected auto support's configured build direction.
	// This is so the build consumes the space occupied by the auto support and is not awkwardly offset. // Example: Build direction
	// set to top means the part will build flush to the "bottom" face of the cube and then topward.
	auto FaceRelativeLocation = GetCubeFaceRelativeLocation(UAutoSupportBlueprintLibrary::GetOppositeDirection(AutoSupportData.BuildDirection));
	Result.StartRelativeLocation = DirectionVector * FaceRelativeLocation;
	Result.StartLocation = GetActorTransform().TransformPosition(FaceRelativeLocation); // TODO(k.a): verify this works for Bottom
	Result.StartRelativeRotation = UAutoSupportBlueprintLibrary::GetDirectionRotator(AutoSupportData.BuildDirection);
	
	MOD_LOG(Verbose, TEXT("Start Rel: %s, Abs: %s, Rel Rotation: %s"), *Result.StartRelativeLocation.ToString(), *Result.StartLocation.ToString(), *Result.StartRelativeRotation.ToString());
	
	// The end location is constrained by a max distance.
	const auto EndLocation = GetEndTraceWorldLocation(Result.StartLocation, DirectionVector);

	MOD_LOG(Verbose, TEXT("End Location: %s"), *EndLocation.ToString());

	const FCollisionShape CollisionShape = FCollisionShape::MakeBox(FVector(.5, .5, .5));

	// Overlap all so we can detect all collisions in our path.
	FCollisionResponseParams ResponseParams(ECR_Overlap);
	
	TArray<FHitResult> HitResults;
	GetWorld()->SweepMultiByChannel(
		HitResults,
		Result.StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Visibility,
		CollisionShape,
		QueryParams,
		ResponseParams);

	if (HitResults.Num() == 0)
	{
		MOD_LOG(Verbose, TEXT("No Hits!"));
		
		return Result;
	}

	int32 HitIndex = -1;
	for (const auto& HitResult : HitResults)
	{
		++HitIndex;
		MOD_LOG(Verbose, TEXT("HitResult[%i] with distance %f: %s"), HitIndex, HitResult.Distance, *HitResult.ToString());

		const auto* HitActor = HitResult.GetActor();
		const auto IsLandscapeHit = HitActor && HitActor->IsA<ALandscapeProxy>();
		const auto IsWaterHit = HitActor && HitActor->IsA<AFGWaterVolume>();
		const auto IsPawnHit = HitActor && HitActor->IsA<APawn>();
		const auto* HitComponent = HitResult.GetComponent();
		
		MOD_LOG(
			Verbose,
			TEXT("Actor class: %s, Component class: %s, Is Landscape Hit: %s, Is Buildable Hit: %s, Is Abstract Instance Hit: %s, Is Pawn Hit: %s"),
			HitActor ? *HitActor->GetClass()->GetName() : TEXT_NULL,
			HitComponent ? *HitComponent->GetClass()->GetName() : TEXT_NULL,
			TEXT_CONDITION(IsLandscapeHit),
			TEXT_CONDITION(IsPawnHit));

		if (IsPawnHit)
		{
			// Never build if we intersect a pawn.
			Result.BuildDistance = 0;
			return Result;
		}

		if (HitResult.Distance <= 1)
		{
			continue; // Ignore close-to-start intersects
		}

		if (IsLandscapeHit || (!AutoSupportData.OnlyIntersectTerrain && !IsWaterHit))
		{
			Result.BuildDistance = HitResult.Distance;
			Result.IsLandscapeHit = IsLandscapeHit;

			if (IsLandscapeHit && AutoSupportData.EndPartDescriptor.IsValid())
			{
				// Bury the part if and extend the build distance.
				Result.BuildDistance += UAutoSupportBlueprintLibrary::GetBuryDistance(UFGBuildingDescriptor::GetBuildableClass(AutoSupportData.EndPartDescriptor.Get()), AutoSupportData.EndPartTerrainBuryPercentage, AutoSupportData.EndPartOrientation);
			}
			
			return Result;
		}
	}

	return Result;
}

FVector ABuildableAutoSupport::GetCubeFaceRelativeLocation(const EAutoSupportBuildDirection Direction) const
{
	// Origin is at the bottom face.
	if (Direction == EAutoSupportBuildDirection::Bottom)
	{
		return FVector::ZeroVector;
	}

	const auto ClearanceData = GetCombinedClearanceBox();
	const auto Extent = ClearanceData.GetExtent();

	MOD_LOG(Verbose, TEXT("Clearance Data: %s"), *ClearanceData.ToString());
	
	switch (Direction)
	{
		case EAutoSupportBuildDirection::Top:
			return FVector(0, 0, Extent.Z * 2);
		case EAutoSupportBuildDirection::Front:
			return FVector(0, -Extent.Y, Extent.Z);
		case EAutoSupportBuildDirection::Back:
			return FVector(0, Extent.Y, Extent.Z);
		case EAutoSupportBuildDirection::Left:
			return FVector(-Extent.X, 0, Extent.Z);
		case EAutoSupportBuildDirection::Right:
			return FVector(Extent.X, 0, Extent.Z);
		default:
			return FVector::ZeroVector;
	}
}

FORCEINLINE FVector ABuildableAutoSupport::GetEndTraceWorldLocation(const FVector& StartLocation, const FVector& Direction) const
{
	return StartLocation + Direction * FBP_ModConfig_AutoSupportStruct::GetActiveConfig(GetWorld()).ConstraintsSection.MaxBuildDistance;
}

