// 

#include "BuildableAutoSupport.h"

#include "FGBuildingDescriptor.h"
#include "FGColoredInstanceMeshProxy.h"
#include "LandscapeProxy.h"
#include "ModLogging.h"

ABuildableAutoSupport::ABuildableAutoSupport()
{
	InstancedMeshProxy = CreateDefaultSubobject<UFGColoredInstanceMeshProxy>(TEXT("BuildableInstancedMeshProxy"));
	InstancedMeshProxy->SetupAttachment(RootComponent);
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

		// TODO(k.a): other orientations.
		switch (PartOrientation)
		{
			case EAutoSupportBuildDirection::Down:
				break;
			case EAutoSupportBuildDirection::Up:
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
			case EAutoSupportBuildDirection::Count:
				break;
		}

		// TODO(k.a): This needs to rotate in-place.
		SpawnTransform.SetRotation(SpawnTransform.Rotator().Add(DeltaPitch, DeltaYaw, DeltaRoll).Quaternion());
		
		// Spawn the part
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
		BuildParts(Buildables, AutoSupportData.EndPartDescriptor, AutoSupportData.EndPartOrientation, PartCounts.Z, Plan.EndBox.GetSize(), TraceResult.Direction, WorkingTransform);
	}

	// Dismantle self
	Execute_Dismantle(this);
}

void ABuildableAutoSupport::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects)
{
	
}

bool ABuildableAutoSupport::NeedTransform_Implementation()
{
	return false;
}

void ABuildableAutoSupport::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	AutoSupportData.ClearInvalidReferences();
}

void ABuildableAutoSupport::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	
}

void ABuildableAutoSupport::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	
}

void ABuildableAutoSupport::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	AutoSupportData.ClearInvalidReferences();
}

bool ABuildableAutoSupport::ShouldSave_Implementation() const
{
	return true;
}

void ABuildableAutoSupport::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoConfigure)
	{
		AutoConfigure();
		bAutoConfigure = false;
	}
}

void ABuildableAutoSupport::AutoConfigure()
{
	K2_AutoConfigure();
}

FAutoSupportTraceResult ABuildableAutoSupport::Trace() const
{
	FAutoSupportTraceResult Result;
	Result.BuildDistance = MaxBuildDistance;
	
	FCollisionQueryParams QueryParams;
	QueryParams.TraceTag = FName("BuildableAutoSupport_Trace");
	QueryParams.AddIgnoredActor(this);
	
	const auto StartLocation = GetActorLocation();
	
	auto DirectionVector = FVector(0, 0, -1); // TODO other directions
	if (AutoSupportData.BuildDirection == EAutoSupportBuildDirection::Up)
	{
		DirectionVector = FVector(0, 0, 1);
	}

	Result.Direction = DirectionVector;
	
	const auto EndLocation = StartLocation + DirectionVector * MaxBuildDistance;

	const FCollisionShape CollisionShape = FCollisionShape::MakeBox(FVector(.5, .5, .5));

	FCollisionResponseParams ResponseParams(ECR_Overlap);
	
	TArray<FHitResult> HitResults;
	GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
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

		if (IsLandscapeHit || (!AutoSupportData.OnlyIntersectTerrain && (IsBuildableHit)))
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

		if (RemainingBuildDistance < 0)
		{
			// Not enough room for more.
			return;
		}
	}
	
	if (AutoSupportData.MiddlePartDescriptor.IsValid() && MidSize.Z > 0 && MidSize.Z <= MaxPartHeight)
	{
		auto NumMiddleParts = static_cast<int32>(RemainingBuildDistance / MidSize.Z);
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Mid Size: %s, Num Mid: %d"), *MidSize.ToString(), NumMiddleParts);
		
		OutPlan.PartCounts.Y = NumMiddleParts;
	}
}

void ABuildableAutoSupport::GetBuildableClearance(const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor, OUT FBox& OutBox)
{
	const auto Buildable = GetDefault<AFGBuildable>(UFGBuildingDescriptor::GetBuildableClass(PartDescriptor.Get()));
	OutBox = Buildable->GetCombinedClearanceBox();
}

