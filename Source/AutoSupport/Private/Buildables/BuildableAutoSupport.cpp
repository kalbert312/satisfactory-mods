// 

#include "BuildableAutoSupport.h"

#include "AbstractInstanceManager.h"
#include "FGBuildingDescriptor.h"
#include "FGColoredInstanceMeshProxy.h"
#include "FGHologram.h"
#include "LandscapeProxy.h"
#include "ModBlueprintLibrary.h"
#include "ModLogging.h"
#include "DrawDebugHelpers.h"

const FVector ABuildableAutoSupport::MaxPartSize = FVector(800, 800, 800);

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
	if (!Plan.IsActionable())
	{
		return;
	}
	
	// Build the parts
	auto* Buildables = AFGBuildableSubsystem::Get(GetWorld());
	auto WorkingTransform = GetActorTransform();
	WorkingTransform.AddToTranslation(Plan.RelativeLocation);
	WorkingTransform.SetRotation(WorkingTransform.Rotator().Add(Plan.RelativeRotation.Pitch, Plan.RelativeRotation.Yaw, Plan.RelativeRotation.Roll).Quaternion());

	if (Plan.StartPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildSupports Building Start Part, Orientation: %i"), static_cast<int32>(AutoSupportData.StartPartOrientation));
		BuildPartPlan(Buildables, Plan.StartPart, BuildInstigator, WorkingTransform);
	}

	if (Plan.MidPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildSupports Building Mid Parts, Orientation: %i"), static_cast<int32>(AutoSupportData.MiddlePartOrientation));
		BuildPartPlan(Buildables, Plan.MidPart, BuildInstigator, WorkingTransform);
	}

	if (Plan.EndPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildSupports Building End Part, Orientation: %i"), static_cast<int32>(AutoSupportData.EndPartOrientation));
		WorkingTransform.AddToTranslation(Plan.EndPartPositionOffset);
		
		BuildPartPlan(Buildables, Plan.EndPart, BuildInstigator, WorkingTransform);
	}
	
	// Dismantle self
	Execute_Dismantle(this);
}

void ABuildableAutoSupport::BuildPartPlan(
	AFGBuildableSubsystem* Buildables,
	const FAutoSupportBuildPlanPartData& PartPlan,
	APawn* BuildInstigator,
	FTransform& WorkingTransform) const
{
	const TSubclassOf<AFGBuildable> BuildableClass = PartPlan.BuildableClass;
	UClass* HologramClass = UFGBuildDescriptor::GetHologramClass(PartPlan.PartDescriptorClass);
	auto* World = GetWorld();
	
	for (auto i = 0; i < PartPlan.Count; ++i)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Spawning part. Start Transform: %s"), *WorkingTransform.ToString());

		// Copy the transform, then apply the orientation below
		FTransform SpawnTransform = WorkingTransform;

		SpawnTransform.AddToTranslation(PartPlan.BuildPositionOffset);

		// Apply the orientation relative to the part's origin
		SpawnTransform.AddToTranslation(-PartPlan.RotationalPositionOffset);
		SpawnTransform.SetRotation(SpawnTransform.Rotator().Add(PartPlan.Rotation.Pitch, PartPlan.Rotation.Yaw, PartPlan.Rotation.Roll).Quaternion());
		SpawnTransform.AddToTranslation(PartPlan.RotationalPositionOffset);

		auto* Buildable = Buildables->BeginSpawnBuildable(BuildableClass, SpawnTransform);
		Buildable->FinishSpawning(SpawnTransform);

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
		
		// TODO(k.a): play build effects and sounds
		// Buildable->PlayBuildEffects(UGameplayStatics::GetPlayerController(GetWorld(), 0));

		// TODO(k.a): See if we can hook this into blueprint dismantle mode

		// Update the transform
		WorkingTransform.AddToTranslation(PartPlan.AfterPartPositionOffset);
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Next Transform: %s"), *WorkingTransform.ToString());
	}

	// TODO: subtract resources from inventory ( hologram/build gun might be responsibile for this.)
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
	Result.BuildDirection = AutoSupportData.BuildDirection;

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
	auto FaceRelativeLocation = GetCubeFaceRelativeLocation(UAutoSupportBlueprintLibrary::GetOppositeDirection(AutoSupportData.BuildDirection));
	Result.StartRelativeLocation = FaceRelativeLocation;
	Result.StartLocation = GetActorTransform().TransformPosition(FaceRelativeLocation);
	Result.StartRelativeRotation = UAutoSupportBlueprintLibrary::GetDirectionRotator(UAutoSupportBlueprintLibrary::GetOppositeDirection(AutoSupportData.BuildDirection));
	
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace Start Rel: %s, Abs: %s, Rel Rotation: %s"), *Result.StartRelativeLocation.ToString(), *Result.StartLocation.ToString(), *Result.StartRelativeRotation.ToString());
	
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

	int32 HitIndex = -1;
	for (const auto& HitResult : HitResults)
	{
		++HitIndex;
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace HitResult[%i],Distance:%f: %s"), HitIndex, HitResult.Distance, *HitResult.ToString());

		const auto* HitActor = HitResult.GetActor();
		const auto IsLandscapeHit = HitActor && HitActor->IsA<ALandscapeProxy>();
		const auto IsBuildableHit = HitActor && HitActor->IsA<AFGBuildable>();
		const auto IsAbstractInstanceHit = HitActor && HitActor->IsA<AAbstractInstanceManager>();
		const auto IsPawnHit = HitActor && HitActor->IsA<APawn>();
		const auto* HitComponent = HitResult.GetComponent();
		
		MOD_LOG(
			Verbose,
			TEXT("BuildableAutoSupport::Trace Actor class: %s, Component class: %s, Is Landscape Hit: %s, Is Buildable Hit: %s, Is Abstract Instance Hit: %s, Is Pawn Hit: %s"),
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
	}

	return Result;
}

void ABuildableAutoSupport::PlanBuild(const FAutoSupportTraceResult& TraceResult, OUT FAutoSupportBuildPlan& OutPlan) const
{
	OutPlan = FAutoSupportBuildPlan();

	// Copy trace result's relative location & rotation.
	OutPlan.RelativeLocation = TraceResult.StartRelativeLocation;
	OutPlan.RelativeRotation = TraceResult.StartRelativeRotation;
	
	// Plan the parts.
	auto RemainingBuildDistance = TraceResult.BuildDistance;
	float SinglePartConsumedBuildSpace = 0;
	
	// Start with the top because that's where the auto support is planted.
	if (AutoSupportData.StartPartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Planning Start Part Positioning"));
		OutPlan.StartPart.PartDescriptorClass = AutoSupportData.StartPartDescriptor.Get();
		OutPlan.StartPart.BuildableClass = UFGBuildingDescriptor::GetBuildableClass(AutoSupportData.StartPartDescriptor.Get());
		UAutoSupportBlueprintLibrary::GetBuildableClearance(OutPlan.StartPart.BuildableClass, OutPlan.StartPart.BBox);
		
		// StartSize.Z > 0 && StartSize.Z <= MaxPartHeight
		PlanPartPositioning(
			OutPlan.StartPart.BBox,
			AutoSupportData.StartPartOrientation,
			TraceResult.Direction,
			SinglePartConsumedBuildSpace,
			OutPlan.StartPart);
		
		RemainingBuildDistance -= SinglePartConsumedBuildSpace;
		OutPlan.StartPart.Count = 1;

		if (RemainingBuildDistance < 0)
		{
			return;
		}
	}
	
	// Do the end next. There may not be enough room for mid pieces.
	if (AutoSupportData.EndPartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Planning End Part Positioning"));
		OutPlan.EndPart.PartDescriptorClass = AutoSupportData.EndPartDescriptor.Get();
		OutPlan.EndPart.BuildableClass = UFGBuildingDescriptor::GetBuildableClass(AutoSupportData.EndPartDescriptor.Get());
		UAutoSupportBlueprintLibrary::GetBuildableClearance(OutPlan.EndPart.BuildableClass, OutPlan.EndPart.BBox);
		
		FVector Discarded;
		PlanPartPositioning(
			OutPlan.EndPart.BBox,
			AutoSupportData.EndPartOrientation,
			TraceResult.Direction,
			SinglePartConsumedBuildSpace,
			OutPlan.EndPart);

		RemainingBuildDistance -= SinglePartConsumedBuildSpace;
		OutPlan.EndPart.Count = 1;

		// TODO(k.a): set end positional offset

		if (RemainingBuildDistance < 0)
		{
			// Not enough room for more.
			return;
		}
	}
	
	if (AutoSupportData.MiddlePartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Planning Mid Part Positioning"));
		OutPlan.MidPart.PartDescriptorClass = AutoSupportData.MiddlePartDescriptor.Get();
		OutPlan.MidPart.BuildableClass = UFGBuildingDescriptor::GetBuildableClass(AutoSupportData.MiddlePartDescriptor.Get());
		UAutoSupportBlueprintLibrary::GetBuildableClearance(OutPlan.MidPart.BuildableClass, OutPlan.MidPart.BBox);
		
		float SingleMidPartConsumedSpace = 0;
		PlanPartPositioning(
			OutPlan.MidPart.BBox,
			AutoSupportData.MiddlePartOrientation,
			TraceResult.Direction,
			SingleMidPartConsumedSpace,
			OutPlan.MidPart);
		
		auto NumMiddleParts = static_cast<int32>(RemainingBuildDistance / SingleMidPartConsumedSpace);
		RemainingBuildDistance -= NumMiddleParts * SingleMidPartConsumedSpace;

		auto IsNearlyPerfectFit = RemainingBuildDistance <= 1.f;

		if (!IsNearlyPerfectFit)
		{
			NumMiddleParts++; // build an extra to fill the gap.
			// Offset the end part to be flush with where the line trace hit or ended. We built an extra part so the direction is negative. We don't need to worry about the end part size because it was already subtracted from build distance.
			OutPlan.EndPartPositionOffset = -TraceResult.Direction * (SingleMidPartConsumedSpace - RemainingBuildDistance);
		}
		
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Mid Size: %s, Num Mid: %d, Perfect Fit: %s"), *OutPlan.MidPart.BBox.GetSize().ToString(), NumMiddleParts, TEXT_CONDITION(IsNearlyPerfectFit));
		
		OutPlan.MidPart.Count = NumMiddleParts;
	}
}

void ABuildableAutoSupport::PlanPartPositioning(
	const FBox& PartBBox,
	EAutoSupportBuildDirection PartOrientation,
	const FVector& Direction,
	float& OutConsumedBuildSpace,
	FAutoSupportBuildPlanPartData& Plan)
{
	// Roll = X, Pitch = Y, Yaw = Z
	const auto PartSize = PartBBox.GetSize();
	// We need an offset to translate by before and after the rotation is applied to return the part to it's occupying space in the auto support.
	const auto OriginOffset = PartBBox.GetCenter(); // Ex: (0,0,0) for mesh centered at buildable actor pivot. (0, 0, 200) for mesh bottom aligned with actor pivot.
	float AxisOriginOffset = OriginOffset.Z;
	float RelativeOffset = PartSize.Z;

	// NOTE: we're always assuming a part's UP is +Z here.
	switch (PartOrientation)
	{
		default:
			// No-op
			break;
		case EAutoSupportBuildDirection::Front:
		case EAutoSupportBuildDirection::Back:
			RelativeOffset = PartSize.Y;
			AxisOriginOffset = OriginOffset.Y;
			break;
		case EAutoSupportBuildDirection::Left:
		case EAutoSupportBuildDirection::Right:
			RelativeOffset = PartSize.X;
			AxisOriginOffset = OriginOffset.X;
			break;
	}
	
	const auto DeltaRot = UAutoSupportBlueprintLibrary::GetDirectionRotator(PartOrientation);
	
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform Origin Offset: %s, Min: %s, Max: %s"), *OriginOffset.ToString(), *PartBBox.Min.ToString(), *PartBBox.Max.ToString());
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform Extent: %s"), *PartBBox.GetExtent().ToString());
	auto DirectionalOriginOffset = Direction * OriginOffset; // We're working with a world transform, so we must include our direction
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform Directional Origin Offset: %s"), *DirectionalOriginOffset.ToString());
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform Delta Rotation: %s"), *DeltaRot.ToString());

	// This occurs before rotation, so we operate in the Z direction. Negate the result because we're building relative a transform position at the bottom of where the piece should be.
	Plan.BuildPositionOffset = -1 * Direction * (AxisOriginOffset - (RelativeOffset / 2));
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform BuildPositionOffset: %s"), *Plan.BuildPositionOffset.ToString());
	
	OutConsumedBuildSpace = RelativeOffset;
	Plan.RotationalPositionOffset = DirectionalOriginOffset;
	Plan.Rotation = DeltaRot;
	Plan.AfterPartPositionOffset = Direction * RelativeOffset;
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
			return FVector(0, -Extent.Y, 0);
		case EAutoSupportBuildDirection::Back:
			return FVector(0, Extent.Y, 0);
		case EAutoSupportBuildDirection::Left:
			return FVector(-Extent.X, 0, 0);
		case EAutoSupportBuildDirection::Right:
			return FVector(Extent.X, 0, 0);
		default:
			return FVector::ZeroVector;
	}
}

FORCEINLINE FVector ABuildableAutoSupport::GetEndTraceWorldLocation(const FVector& StartLocation, const FVector& Direction) const
{
	return StartLocation + Direction * MaxBuildDistance;
}

