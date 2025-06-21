// 

#include "BuildableAutoSupport.h"

#include "AbstractInstanceManager.h"
#include "FGBuildingDescriptor.h"
#include "FGColoredInstanceMeshProxy.h"
#include "FGHologram.h"
#include "LandscapeProxy.h"
#include "ModBlueprintLibrary.h"
#include "ModLogging.h"

ABuildableAutoSupport::ABuildableAutoSupport(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	mMeshComponentProxy = CreateDefaultSubobject<UFGColoredInstanceMeshProxy>(TEXT("BuildableInstancedMeshProxy"));
	mMeshComponentProxy->SetupAttachment(RootComponent);
}

void ABuildableAutoSupport::BuildSupports(APawn* BuildInstigator)
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
	if (!Plan.IsValidBuild())
	{
		return;
	}

	const auto& PartCounts = Plan.PartCounts;
	// Build the parts
	auto* Buildables = AFGBuildableSubsystem::Get(GetWorld());
	auto WorkingTransform = GetActorTransform();

	if (PartCounts.X > 0)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildSupports Building Start Part, Orientation: %i"), static_cast<int32>(AutoSupportData.StartPartOrientation));
		BuildParts(Buildables, AutoSupportData.StartPartDescriptor, AutoSupportData.StartPartOrientation, PartCounts.X, Plan.StartBox, TraceResult.Direction, BuildInstigator, WorkingTransform);
	}

	if (PartCounts.Y > 0)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildSupports Building Mid Parts, Orientation: %i"), static_cast<int32>(AutoSupportData.MiddlePartOrientation));
		BuildParts(Buildables, AutoSupportData.MiddlePartDescriptor, AutoSupportData.MiddlePartOrientation, PartCounts.Y, Plan.MidBox, TraceResult.Direction, BuildInstigator, WorkingTransform);
	}

	if (PartCounts.Z > 0)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildSupports Building End Part, Orientation: %i"), static_cast<int32>(AutoSupportData.EndPartOrientation));
		auto EndPartTransform = GetActorTransform();
		EndPartTransform.AddToTranslation(TraceResult.Direction * Plan.EndPartBuildDistance);
		
		BuildParts(Buildables, AutoSupportData.EndPartDescriptor, AutoSupportData.EndPartOrientation, PartCounts.Z, Plan.EndBox, TraceResult.Direction, BuildInstigator, EndPartTransform);
	}
	
	// Dismantle self
	Execute_Dismantle(this);
}

void ABuildableAutoSupport::BuildParts(
	AFGBuildableSubsystem* Buildables,
	const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor,
	const EAutoSupportBuildDirection PartOrientation,
	const int32 Count,
	const FBox& PartBBox,
	const FVector& Direction,
	APawn* BuildInstigator,
	FTransform& WorkingTransform)
{
	const TSubclassOf<AFGBuildable> BuildableClass = UFGBuildingDescriptor::GetBuildableClass(PartDescriptor.Get());
	UClass* HologramClass = UFGBuildDescriptor::GetHologramClass(PartDescriptor.Get());
	auto* World = GetWorld();
	auto Extent = PartBBox.GetExtent();
	auto Size = PartBBox.GetSize();
	auto LocationOffset = Direction * Size;
	
	double DeltaRoll = 0, DeltaPitch = 0, DeltaYaw = 0;

	bool bSkipOrient = false;
	// Determine the origin of the part from the extent retrieved from clearance data.
	// Typically two cases: bottom (walls) and center (pillars, foundations)
	// BBox Min Z = 0 means bottom origin, Z < 0 should be center origin.
	bool bIsCenterOriginPart = FMath::IsNearlyEqual(PartBBox.Min.Z, -Extent.Z, KINDA_SMALL_NUMBER);
	bool bSkipOrientTransform = !bIsCenterOriginPart;
	
	switch (PartOrientation)
	{
		case EAutoSupportBuildDirection::Bottom:
		default:
			bSkipOrient = true;
			break;
		case EAutoSupportBuildDirection::Top:
			DeltaRoll = 180;
			break;
		case EAutoSupportBuildDirection::Front:
			DeltaYaw = 180;
			break;
		case EAutoSupportBuildDirection::Back:
			DeltaYaw = -180;
			break;
		case EAutoSupportBuildDirection::Left:
			DeltaPitch = 180;
			break;
		case EAutoSupportBuildDirection::Right:
			DeltaPitch = -180;
			break;
	}
	
	for (auto i = 0; i < Count; ++i)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Spawning part. Start Transform: %s"), *WorkingTransform.ToString());

		// Copy the transform to apply the orientation.
		FTransform SpawnTransform = WorkingTransform;
		
		if (bIsCenterOriginPart)
		{
			// we start building from the trace start location which is at the cube face opposite our build direction, so if it's a center origin part,
			// initially translate by the direction vector multiplied by the extent Z. This will ensure that the part is placed at the trace start location.
			SpawnTransform.AddToTranslation(Direction * Extent.Z);
		}

		// Spawn the part
		/** Spawns a hologram from recipe */
		// static AFGHologram* SpawnHologramFromRecipe( TSubclassOf< class UFGRecipe > inRecipe, AActor* hologramOwner, const FVector& spawnLocation, APawn* hologramInstigator = nullptr, const TFunction< void( AFGHologram* ) >& preSpawnFunction = nullptr );
		
		// auto* Hologram = World->SpawnActorDeferred<AFGBuildableHologram>(
		// 	HologramClass,
		// 	SpawnTransform,
		// 	nullptr,
		// 	Instigator);
		//
		// Hologram->SetBuildableClass(BuildableClass);
		//
		// Hologram->FinishSpawning(SpawnTransform);
		
		if (!bSkipOrient)
		{
			OrientPart(Extent, Direction, DeltaRoll, DeltaPitch, DeltaYaw, bSkipOrientTransform, SpawnTransform);
		}
		else
		{
			MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Skipping Part Orient"));
		}

		auto* Buildable = Buildables->BeginSpawnBuildable(BuildableClass, SpawnTransform);
		Buildable->FinishSpawning(SpawnTransform);
		
		// TODO(k.a): play build effects and sounds
		// Buildable->PlayBuildEffects(UGameplayStatics::GetPlayerController(GetWorld(), 0));

		// TODO(k.a): See if we can hook this into blueprint dismantle mode

		// Update the transform
		WorkingTransform.AddToTranslation(LocationOffset);
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Next Transform: %s"), *WorkingTransform.ToString());
	}

	// TODO: subtract resources from inventory.
}

void ABuildableAutoSupport::OrientPart(const FVector& Extent, const FVector& Direction, float DeltaRoll, float DeltaPitch, float DeltaYaw, bool bSkipTranslate, FTransform& SpawnTransform) const
{
	// Roll = X, Pitch = Y, Yaw = Z
	// Translate the pivot point (bottom of building) to bbox center before rotating.

	if (!bSkipTranslate)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPart Translated by %s"), *(-1 * Direction * Extent).ToString());
		SpawnTransform.AddToTranslation(-1 * Direction * Extent);
	}
	
	SpawnTransform.SetRotation(SpawnTransform.Rotator().Add(DeltaPitch, DeltaYaw, DeltaRoll).Quaternion());
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPart Rotated by %s"), *SpawnTransform.Rotator().ToString());

	if (!bSkipTranslate)
	{
		SpawnTransform.AddToTranslation(Direction * Extent);
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPart Translated by %s"), *(Direction * Extent).ToString());
	}
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

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PreSaveGame Start, Mid, End Orientation: %i,%i,%i"), AutoSupportData.StartPartOrientation, AutoSupportData.MiddlePartOrientation, AutoSupportData.EndPartOrientation);
}

void ABuildableAutoSupport::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	Super::PostLoadGame_Implementation(saveVersion, gameVersion);
	
	AutoSupportData.ClearInvalidReferences();

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PostLoadGame Start, Mid, End Orientation: %i,%i,%i"), AutoSupportData.StartPartOrientation, AutoSupportData.MiddlePartOrientation, AutoSupportData.EndPartOrientation);
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
	// Origin is at the bottom face.
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

