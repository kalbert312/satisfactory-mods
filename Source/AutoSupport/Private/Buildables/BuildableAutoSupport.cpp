// 

#include "BuildableAutoSupport.h"

#include "AbstractInstanceManager.h"
#include "FGBuildingDescriptor.h"
#include "FGColoredInstanceMeshProxy.h"
#include "LandscapeProxy.h"
#include "ModBlueprintLibrary.h"
#include "ModLogging.h"

ABuildableAutoSupport::ABuildableAutoSupport(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	mMeshComponentProxy = CreateDefaultSubobject<UFGColoredInstanceMeshProxy>(TEXT("BuildableInstancedMeshProxy"));
	mMeshComponentProxy->SetupAttachment(RootComponent);
}

bool ABuildableAutoSupport::IsBuildable(const FAutoSupportBuildPlan& Plan) const
{
	if (Plan.PartCounts.IsNearlyZero())
	{
		return false;
	}
	
	// TODO(k.a): check player has enough resources to build
	return true;
}

void ABuildableAutoSupport::BuildParts(
	AFGBuildableSubsystem* Buildables,
	const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor,
	const EAutoSupportBuildDirection PartOrientation,
	const int32 Count,
	const FVector& Size,
	const FVector& Direction,
	FTransform& WorkingTransform)
{
	const TSubclassOf<AFGBuildable> BuildableClass = UFGBuildingDescriptor::GetBuildableClass(PartDescriptor.Get());
	
	for (auto i = 0; i < Count; ++i)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Spawning part. Start Transform: %s"), *WorkingTransform.ToString());

		// Copy the transform to apply the orientation.
		FTransform SpawnTransform = WorkingTransform;

		double DeltaPitch = 0, DeltaYaw = 0, DeltaRoll = 0;
		
		switch (PartOrientation)
		{
			case EAutoSupportBuildDirection::Bottom:
			default:
				break;
			case EAutoSupportBuildDirection::Top:
				DeltaPitch = 180;
				break;
			case EAutoSupportBuildDirection::Front:
				break;
			case EAutoSupportBuildDirection::Back:
				break;
			case EAutoSupportBuildDirection::Left:
				break;
			case EAutoSupportBuildDirection::Right:
				break;
		}

		// Translate the pivot point (bottom of building) to bbox center before rotating.
		SpawnTransform.AddToTranslation(-1 * Direction * Size / 2.f);
		SpawnTransform.SetRotation(SpawnTransform.Rotator().Add(DeltaPitch, DeltaYaw, DeltaRoll).Quaternion());
		
		// Spawn the part
		/** Spawns a hologram from recipe */
		// static AFGHologram* SpawnHologramFromRecipe( TSubclassOf< class UFGRecipe > inRecipe, AActor* hologramOwner, const FVector& spawnLocation, APawn* hologramInstigator = nullptr, const TFunction< void( AFGHologram* ) >& preSpawnFunction = nullptr );
		
		auto* Buildable = Buildables->BeginSpawnBuildable(BuildableClass, SpawnTransform);
		Buildable->FinishSpawning(SpawnTransform);
		// TODO(k.a): play build effects and sounds
		// Buildable->PlayBuildEffects(UGameplayStatics::GetPlayerController(GetWorld(), 0));

		// TODO(k.a): See if we can hook this into blueprint dismantle mode

		// Update the transform
		WorkingTransform.AddToTranslation(Direction * Size);
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Next Transform: %s"), *WorkingTransform.ToString());
	}

	// TODO: subtract resources from inventory.
}

void ABuildableAutoSupport::BuildSupports()
{
	if (!AutoSupportData.MiddlePartDescriptor.IsValid() && !AutoSupportData.StartPartDescriptor.IsValid() && !AutoSupportData.EndPartDescriptor.IsValid())
	{
		// Nothing to build.
		return;
	}
	
	// Trace to know how much we're going to build.
	const auto TraceResult = Trace();

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildSupports BuildDistance: %f, IsTerrainHit: %s, Direction: %s"), TraceResult.BuildDistance, TEXT_CONDITION(TraceResult.IsLandscapeHit), *TraceResult.Direction.ToString());

	if (FMath::IsNearlyZero(TraceResult.BuildDistance))
	{
		// No room to build.
		return;
	}
	
	FAutoSupportBuildPlan Plan;
	PlanBuild(TraceResult, Plan);
	
	// Check that the player building it has enough resources
	if (!IsBuildable(Plan))
	{
		return;
	}

	const auto& PartCounts = Plan.PartCounts;
	// Build the parts
	auto* Buildables = AFGBuildableSubsystem::Get(GetWorld());
	auto WorkingTransform = GetActorTransform();

	if (PartCounts.X > 0)
	{
		BuildParts(Buildables, AutoSupportData.StartPartDescriptor, AutoSupportData.StartPartOrientation, PartCounts.X, Plan.StartBox.GetSize(), TraceResult.Direction, WorkingTransform);
	}

	if (PartCounts.Y > 0)
	{
		BuildParts(Buildables, AutoSupportData.MiddlePartDescriptor, AutoSupportData.MiddlePartOrientation, PartCounts.Y, Plan.MidBox.GetSize(), TraceResult.Direction, WorkingTransform);
	}

	if (PartCounts.Z > 0)
	{
		auto EndPartTransform = GetActorTransform();
		EndPartTransform.AddToTranslation(TraceResult.Direction * Plan.EndPartBuildDistance);
		
		BuildParts(Buildables, AutoSupportData.EndPartDescriptor, AutoSupportData.EndPartOrientation, PartCounts.Z, Plan.EndBox.GetSize(), TraceResult.Direction, EndPartTransform);
	}
	
	// Dismantle self
	Execute_Dismantle(this);
}

#pragma region IFGSaveInterface

void ABuildableAutoSupport::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	AutoSupportData.ClearInvalidReferences();
}

void ABuildableAutoSupport::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	AutoSupportData.ClearInvalidReferences();
}

bool ABuildableAutoSupport::ShouldSave_Implementation() const
{
	return true;
}

#pragma endregion

void ABuildableAutoSupport::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoConfigure)
	{
		AutoConfigure();
		bAutoConfigure = false;
	}
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
	K2_AutoConfigure();
}

FAutoSupportTraceResult ABuildableAutoSupport::Trace() const
{
	FAutoSupportTraceResult Result;
	Result.BuildDistance = MaxBuildDistance;

	// Start building our trace params.
	FCollisionQueryParams QueryParams;
	QueryParams.TraceTag = FName("BuildableAutoSupport_Trace");
	QueryParams.AddIgnoredActor(this);

	auto StartTransform = GetActorTransform();
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Start Transform: %s"), *StartTransform.ToString());
	
	// Set up a direction vector
	auto DirectionVector = UAutoSupportBlueprintLibrary::GetDirectionVector(AutoSupportData.BuildDirection);
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Direction Vector: %s"), *DirectionVector.ToString());
	
	DirectionVector = StartTransform.GetRotation().RotateVector(DirectionVector);
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Rotated Direction Vector: %s"), *DirectionVector.ToString());
	
	Result.Direction = DirectionVector;
	
	// Determine the starting trace location. This will be opposite the face of the selected auto support's configured build direction.
	// This is so the build consumes the space occupied by the auto support and is not awkwardly offset. // Example: Build direction
	// set to top means the part will build flush to the "bottom" face of the cube and then topward.
	Result.StartLocation = GetCubeFaceWorldLocation(UAutoSupportBlueprintLibrary::GetOppositeDirection(AutoSupportData.BuildDirection));
	
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Start Location: %s"), *Result.StartLocation.ToString());
	
	// The end location is constrained by a max distance.
	const auto EndLocation = GetEndTraceWorldLocation(Result.StartLocation, DirectionVector);

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace End Location: %s"), *EndLocation.ToString());

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
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace No Hits!"));
		
		return Result;
	}

	int32 HitIndex = 0;
	for (const auto& HitResult : HitResults)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace HitResult[%i]: %s"), HitIndex, *HitResult.ToString());

		const auto* HitActor = HitResult.GetActor();
		const auto IsLandscapeHit = HitActor && HitActor->IsA<ALandscapeProxy>();
		const auto IsBuildableHit = HitActor && HitActor->IsA<AFGBuildable>();
		const auto IsAbstractInstanceHit = HitActor && HitActor->IsA<AAbstractInstanceManager>();
		const auto IsPawnHit = HitActor && HitActor->IsA<APawn>();
		const auto* HitComponent = HitResult.GetComponent();
		
		MOD_LOG(
			Verbose,
			TEXT("BuildableAutoSupport::Trace  Actor class: %s, Component class: %s, Is Landscape Hit: %s, Is Buildable Hit: %s, Is Abstract Instance Hit: %s, Is Pawn Hit: %s"),
			HitActor ? *HitActor->GetClass()->GetName() : TEXT_NULL,
			HitComponent ? *HitComponent->GetClass()->GetName() : TEXT_NULL,
			TEXT_CONDITION(IsLandscapeHit),
			TEXT_CONDITION(IsBuildableHit),
			TEXT_CONDITION(IsAbstractInstanceHit),
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

		if (IsLandscapeHit
			|| (!AutoSupportData.OnlyIntersectTerrain && (IsBuildableHit || IsAbstractInstanceHit)))
		{
			Result.BuildDistance = HitResult.Distance;
			Result.IsLandscapeHit = IsLandscapeHit;
			return Result;
		}

		++HitIndex;
	}

	return Result;
}

void ABuildableAutoSupport::PlanBuild(const FAutoSupportTraceResult& TraceResult, OUT FAutoSupportBuildPlan& OutPlan) const
{
	OutPlan = FAutoSupportBuildPlan();
	
	auto RemainingBuildDistance = TraceResult.BuildDistance;

	// Gather data.
	FVector StartSize, MidSize, EndSize, LastPartSize;

	if (AutoSupportData.StartPartDescriptor.IsValid())
	{
		UAutoSupportBlueprintLibrary::GetBuildableClearance(AutoSupportData.StartPartDescriptor, OutPlan.StartBox);
		StartSize = OutPlan.StartBox.GetSize();
	}

	if (AutoSupportData.MiddlePartDescriptor.IsValid())
	{
		UAutoSupportBlueprintLibrary::GetBuildableClearance(AutoSupportData.MiddlePartDescriptor, OutPlan.MidBox);
		MidSize = OutPlan.MidBox.GetSize();
		LastPartSize = MidSize;
	}

	if (AutoSupportData.EndPartDescriptor.IsValid())
	{
		UAutoSupportBlueprintLibrary::GetBuildableClearance(AutoSupportData.EndPartDescriptor, OutPlan.EndBox);
		EndSize = OutPlan.EndBox.GetSize();
		LastPartSize = EndSize;
	}

	if (TraceResult.IsLandscapeHit)
	{
		// For terrain hits, the last part should extend at most half its height from its origin into the terrain to avoid gaps.
		RemainingBuildDistance += LastPartSize.Z / 2;
	}
	
	// Start with the top because that's where the auto support is planted.
	if (AutoSupportData.StartPartDescriptor.IsValid() && StartSize.Z > 0 && StartSize.Z <= MaxPartHeight)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Start Size: %s"), *StartSize.ToString());
		RemainingBuildDistance -= StartSize.Z;

		OutPlan.PartCounts.X = 1;

		if (RemainingBuildDistance < 0)
		{
			return;
		}
	}
	
	// Do the end next. There may not be enough room for mid pieces.
	if (AutoSupportData.EndPartDescriptor.IsValid() && EndSize.Z > 0 && EndSize.Z <= MaxPartHeight)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild End Size: %s"), *EndSize.ToString());
		RemainingBuildDistance -= EndSize.Z;

		OutPlan.PartCounts.Z = 1;
		OutPlan.EndPartBuildDistance = TraceResult.BuildDistance - EndSize.Z;

		if (RemainingBuildDistance < 0)
		{
			// Not enough room for more.
			return;
		}
	}
	
	if (AutoSupportData.MiddlePartDescriptor.IsValid() && MidSize.Z > 0 && MidSize.Z <= MaxPartHeight)
	{
		auto NumMiddleParts = static_cast<int32>(RemainingBuildDistance / MidSize.Z);
		RemainingBuildDistance -= NumMiddleParts * MidSize.Z;

		auto IsNearlyPerfectFit = RemainingBuildDistance <= 1.f;

		if (!IsNearlyPerfectFit)
		{
			NumMiddleParts++; // make sure we reach the end part.
		}
		
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Mid Size: %s, Num Mid: %d, Perfect Fit: %s"), *MidSize.ToString(), NumMiddleParts, TEXT_CONDITION(IsNearlyPerfectFit));
		
		OutPlan.PartCounts.Y = NumMiddleParts;
	}
}

FVector ABuildableAutoSupport::GetCubeFaceRelativeLocation(const EAutoSupportBuildDirection Direction) const
{
	// Origin is the at bottom face.
	if (Direction == EAutoSupportBuildDirection::Bottom)
	{
		return FVector::ZeroVector;
	}

	const auto ClearanceData = GetCombinedClearanceBox();
	const auto Extent = ClearanceData.GetExtent();

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::GetCubeFaceRelativeLocation Clearance Data: %s"), *ClearanceData.ToString());
	
	switch (Direction)
	{
		case EAutoSupportBuildDirection::Top:
			return FVector(0, 0, Extent.Z * 2);
		case EAutoSupportBuildDirection::Front:
			return FVector(0, Extent.Y, 0);
		case EAutoSupportBuildDirection::Back:
			return FVector(0, -Extent.Y, 0);
		case EAutoSupportBuildDirection::Left:
			return FVector(-Extent.X, 0, 0);
		case EAutoSupportBuildDirection::Right:
			return FVector(Extent.X, 0, 0);
		default:
			return FVector::ZeroVector;
	}
}

FVector ABuildableAutoSupport::GetCubeFaceWorldLocation(const EAutoSupportBuildDirection Direction) const
{
	return GetActorTransform().TransformPosition(GetCubeFaceRelativeLocation(Direction));
}

FORCEINLINE FVector ABuildableAutoSupport::GetEndTraceWorldLocation(const FVector& StartLocation, const FVector& Direction) const
{
	return StartLocation + Direction * MaxBuildDistance;
}

