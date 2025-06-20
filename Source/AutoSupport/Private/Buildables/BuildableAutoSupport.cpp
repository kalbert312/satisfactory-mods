// 

#include "BuildableAutoSupport.h"

#include "AbstractInstanceManager.h"
#include "FGBuildingDescriptor.h"
#include "FGColoredInstanceMeshProxy.h"
#include "LandscapeProxy.h"
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
	
	FVector DirectionVector;

	// Set up a direction vector
	switch (AutoSupportData.BuildDirection)
	{
		case EAutoSupportBuildDirection::Top:
		default:
			DirectionVector = FVector(0, 0, 1);
			break;
		case EAutoSupportBuildDirection::Bottom:
			DirectionVector = FVector(0, 0, -1);
			break;
		case EAutoSupportBuildDirection::Front:
			DirectionVector = FVector(0, -1, 0);
			break;
		case EAutoSupportBuildDirection::Back:
			DirectionVector = FVector(0, 1, 0);
			break;
		case EAutoSupportBuildDirection::Left:
			DirectionVector = FVector(-1, 0, 0);
			break;
		case EAutoSupportBuildDirection::Right:
			DirectionVector = FVector(1, 0, 0);
			break;
	}

	Result.Direction = DirectionVector;

	// Determine the starting trace location. This will be opposite the face of the selected auto support's configured build direction.
	// This is so the build consumes the space occupied by the auto support and is not awkwardly offset. // Example: Build direction
	// set to down means the part will build flush to the "up" face of the cube and then downward.
		
	auto StartTransform = GetActorTransform();

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Start Transform: %s"), *StartTransform.ToString());
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Direction Vector: %s"), *DirectionVector.ToString());
	
	DirectionVector = StartTransform.GetRotation().RotateVector(DirectionVector);
	
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Rotated Direction Vector: %s"), *DirectionVector.ToString());
	
	auto ClearanceData = GetCombinedClearanceBox();

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Clearance Data: %s"), *ClearanceData.ToString());

	StartTransform.AddToTranslation(-1 * DirectionVector * ClearanceData.GetExtent());

	Result.StartLocation = StartTransform.GetLocation();

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Start Location: %s"), *Result.StartLocation.ToString());
	
	// The end location is constrained by a max distance.
	const auto EndLocation = Result.StartLocation + DirectionVector * MaxBuildDistance;

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
			TEXT("BuildableAutoSupport::Trace  Actor class: %s, Component class: %s, Is Landscape Hit: %s, Is Buildable Hit: %s, Is Pawn Hit: %s"),
			HitActor ? *HitActor->GetClass()->GetName() : TEXT_NULL,
			HitComponent ? *HitComponent->GetClass()->GetName() : TEXT_NULL,
			TEXT_CONDITION(IsLandscapeHit),
			TEXT_CONDITION(IsBuildableHit),
			TEXT_CONDITION(IsPawnHit));

		if (IsPawnHit)
		{
			// Never build if we intersect a pawn.
			Result.BuildDistance = 0;
			return Result;
		}

		if (IsLandscapeHit || (!AutoSupportData.OnlyIntersectTerrain && (IsBuildableHit || IsAbstractInstanceHit)))
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
	
	OutPlan.PartCounts = FVector::ZeroVector;
	auto RemainingBuildDistance = TraceResult.BuildDistance;

	// Gather data.
	FVector StartSize, MidSize, EndSize, LastPartSize;

	if (AutoSupportData.StartPartDescriptor.IsValid())
	{
		GetBuildableClearance(AutoSupportData.StartPartDescriptor, OutPlan.StartBox);
		StartSize = OutPlan.StartBox.GetSize();
	}

	if (AutoSupportData.MiddlePartDescriptor.IsValid())
	{
		GetBuildableClearance(AutoSupportData.MiddlePartDescriptor, OutPlan.MidBox);
		MidSize = OutPlan.MidBox.GetSize();
		LastPartSize = MidSize;
	}

	if (AutoSupportData.EndPartDescriptor.IsValid())
	{
		GetBuildableClearance(AutoSupportData.EndPartDescriptor, OutPlan.EndBox);
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

		auto IsPerfectFit = RemainingBuildDistance <= 0.1f;

		if (!IsPerfectFit)
		{
			NumMiddleParts++; // make sure we reach the end part.
		}
		
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Mid Size: %s, Num Mid: %d, Perfect Fit: %s"), *MidSize.ToString(), NumMiddleParts, TEXT_CONDITION(IsPerfectFit));
		
		OutPlan.PartCounts.Y = NumMiddleParts;
	}
}

void ABuildableAutoSupport::GetBuildableClearance(const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor, OUT FBox& OutBox)
{
	const auto Buildable = GetDefault<AFGBuildable>(UFGBuildingDescriptor::GetBuildableClass(PartDescriptor.Get()));
	OutBox = Buildable->GetCombinedClearanceBox();
}

