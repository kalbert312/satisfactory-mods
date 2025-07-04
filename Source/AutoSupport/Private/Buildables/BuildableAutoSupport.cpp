// 

#include "BuildableAutoSupport.h"

#include "AutoSupportModSubsystem.h"
#include "BP_ModConfig_AutoSupportStruct.h"
#include "BuildableAutoSupportProxy.h"
#include "DrawDebugHelpers.h"
#include "FGBlueprintProxy.h"
#include "FGBuildingDescriptor.h"
#include "FGHologram.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGPlayerController.h"
#include "FGWaterVolume.h"
#include "LandscapeProxy.h"
#include "ModBlueprintLibrary.h"
#include "ModConstants.h"
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
		MOD_TRACE_LOG(Verbose, TEXT("Nothing to build!"));
		return false;
	}
	
	// Trace to know how much we're going to build.
	const auto TraceResult = Trace();

	if (FMath::IsNearlyZero(TraceResult.BuildDistance))
	{
		MOD_TRACE_LOG(Verbose, TEXT("No room to build!"));
		return false;
	}

	UAutoSupportBlueprintLibrary::PlanBuild(GetWorld(), TraceResult, AutoSupportData, OutPlan);
	
	return true;
}

void ABuildableAutoSupport::BuildSupports(APawn* BuildInstigator)
{
	FAutoSupportBuildPlan Plan;
	if (!TraceAndCreatePlan(Plan))
	{
		MOD_LOG(Verbose, TEXT("The plan cannot be built."));
		return;
	}

	AFGCharacterPlayer* Player = CastChecked<AFGCharacterPlayer>(BuildInstigator);

	ABuildableAutoSupportProxy* SupportProxy = nullptr;
	auto* RootHologram = UAutoSupportBlueprintLibrary::CreateCompositeHologramFromPlan(Plan, AutoSupportProxyClass, BuildInstigator, this, this, SupportProxy);
	const auto BillOfParts = RootHologram->GetCost(true);
	
	if (!UAutoSupportBlueprintLibrary::PayItemBillIfAffordable(Player, BillOfParts, true))
	{
		RootHologram->Destroy();
		SupportProxy->Destroy();
		return;
	}
	
	// Construct
	auto* Buildables = AFGBuildableSubsystem::Get(GetWorld());
	auto* LightBuildables = AFGLightweightBuildableSubsystem::Get(GetWorld());
	
	TArray<AActor*> HologramSpawnedActors;
	auto* StartBuildable = CastChecked<AFGBuildable>(RootHologram->Construct(HologramSpawnedActors, Buildables->GetNewNetConstructionID()));
	HologramSpawnedActors.Insert(StartBuildable, 0);

	int32 i = 0;
	for (auto* HologramSpawnedActor : HologramSpawnedActors)
	{
		auto* Buildable = CastChecked<AFGBuildable>(HologramSpawnedActor);

		auto CustomizationData = Plan.MidPart.CustomizationData;

		if (i == 0)
		{
			CustomizationData = Plan.StartPart.CustomizationData;
		}
		else if (i == HologramSpawnedActors.Num() - 1)
		{
			CustomizationData = Plan.EndPart.CustomizationData;
		}
		
		Buildable->SetCustomizationData_Native(CustomizationData);
		if (Buildable->ManagedByLightweightBuildableSubsystem()) // TODO(k.a): check colorable?
		{
			LightBuildables->CopyCustomizationDataFromTemporaryToInstance(Buildable);
		}
		
		SupportProxy->RegisterBuildable(Buildable);
		
		MOD_LOG(
			Verbose,
			TEXT("Buildable[%i]: Name: [%s], ShouldConvertToLightweight: [%s], ManagedByLightweight: [%s] Customization Swatch: [%s]"),
			i,
			*Buildable->GetName(),
			TEXT_CONDITION(Buildable->ShouldConvertToLightweight()),
			TEXT_CONDITION(Buildable->ManagedByLightweightBuildableSubsystem()),
			CustomizationData.SwatchDesc ? *(CustomizationData.SwatchDesc->GetName()) : TEXT_EMPTY);
		++i;
	}

	SupportProxy->FinishSpawning(SupportProxy->GetActorTransform());
	
	MOD_LOG(Verbose, TEXT("Completed, SupportProxy transform: [%s]"), *SupportProxy->GetActorTransform().ToHumanReadableString());
	
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
	const auto bIsNew = mBuildEffectInstignator || mBuildEffectIsPlaying || mActiveBuildEffect;

	if (bIsNew)
	{
		if (GetBlueprintDesigner()) // TODO(k.a): test blueprint designer
		{
			MOD_LOG(Verbose, TEXT("Has blueprint designer"));
			bAutoConfigureAtBeginPlay = false;
		}
		else if (GetBlueprintProxy()) // Is it being built from a blueprint?
		{
			MOD_LOG(Verbose, TEXT("Built from blueprint"));
			bAutoConfigureAtBeginPlay = false;

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

		if (bAutoConfigureAtBeginPlay)
		{
			AutoConfigure();
		}
	}

	bAutoConfigureAtBeginPlay = false;
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

void ABuildableAutoSupport::SaveLastUsedData()
{
	MOD_LOG(Verbose, TEXT("Saving last used data"));
	K2_SaveLastUsedData();
}

FAutoSupportTraceResult ABuildableAutoSupport::Trace() const
{
	MOD_TRACE_LOG(Verbose, TEXT("BEGIN TRACE ---------------------------"));
	// World +X = East, World +Y = South, +Z = Sky
	FAutoSupportTraceResult Result;
	Result.BuildDistance = FBP_ModConfig_AutoSupportStruct::GetActiveConfig(GetWorld()).ConstraintsSection.MaxBuildDistance;
	Result.BuildDirection = AutoSupportData.BuildDirection;

	// Start building our trace params.
	FCollisionQueryParams QueryParams;
	QueryParams.TraceTag = AutoSupportConstants::TraceTag_BuildableAutoSupport;
	QueryParams.AddIgnoredActor(this);

	const auto StartTransform = GetActorTransform();

	MOD_TRACE_LOG(
		Verbose,
		TEXT("Start Transform: [%s]"),
		*StartTransform.ToHumanReadableString());

	const auto TraceRelDirection = UAutoSupportBlueprintLibrary::GetDirectionVector(AutoSupportData.BuildDirection);
	const auto TraceAbsDirection = StartTransform.GetRotation().RotateVector(TraceRelDirection);
	
	MOD_TRACE_LOG(
		Verbose,
		TEXT("Build Dir: [%i], Trace Rel Direction: [%s], Trace Abs Direction: [%s]"),
		AutoSupportData.BuildDirection,
		*TraceRelDirection.ToCompactString(),
		*TraceAbsDirection.ToCompactString());
	
	Result.Direction = TraceAbsDirection;
	
	// Determine the starting trace location. This will be opposite the face of the selected auto support's configured build direction.
	// This is so the build consumes the space occupied by the auto support and is not awkwardly offset. // Example: Build direction
	// set to top means the part will build flush to the "bottom" face of the cube and then topward.
	const auto FaceRelLocation = GetCubeFaceRelativeLocation(UAutoSupportBlueprintLibrary::GetOppositeDirection(AutoSupportData.BuildDirection));
	Result.StartRelativeRotation = UAutoSupportBlueprintLibrary::GetDirectionRotator(UAutoSupportBlueprintLibrary::GetOppositeDirection(AutoSupportData.BuildDirection)).Quaternion();
	Result.StartRelativeLocation = FaceRelLocation;
	Result.StartLocation = StartTransform.TransformPosition(FaceRelLocation);
	const auto EndLocation = GetEndTraceWorldLocation(Result.StartLocation, TraceAbsDirection);

	MOD_TRACE_LOG(
		Verbose,
		TEXT("Face rel location: [%s], Trace start rel loc & rot: [%s][%s], Trace start abs loc [%s] with end delta: [%s]"),
		*FaceRelLocation.ToCompactString(),
		*Result.StartRelativeLocation.ToCompactString(),
		*Result.StartRelativeRotation.Rotator().ToCompactString(),
		*Result.StartLocation.ToCompactString(),
		*((EndLocation - Result.StartLocation).ToCompactString()));

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
		MOD_TRACE_LOG(Verbose, TEXT("No Hits!"));
		
		return Result;
	}

	int32 HitIndex = -1;
	for (const auto& HitResult : HitResults)
	{
		++HitIndex;
		MOD_TRACE_LOG(Verbose, TEXT("HitResult[%i] with distance %f"), HitIndex, HitResult.Distance);
		
		const auto* HitActor = HitResult.GetActor();
		if (!HitActor)
		{
			continue;
		}

		if (HitActor->IsA<APawn>())
		{
			MOD_TRACE_LOG(Verbose, TEXT("Hit Pawn!"));
			// Never build if we intersect a pawn.
			Result.BuildDistance = 0;
			return Result;
		}

		if (HitResult.Distance <= 1)
		{
			MOD_TRACE_LOG(Verbose, TEXT("Ignoring hit (too close)."));
			continue;
		}

		const auto IsLandscapeHit = HitActor->IsA<ALandscapeProxy>();
		const auto IsWaterHit = HitActor->IsA<AFGWaterVolume>();

		// Check for a blocking hit.
		if (IsLandscapeHit || (!AutoSupportData.OnlyIntersectTerrain && !IsWaterHit))
		{
			Result.BuildDistance = HitResult.Distance;
			Result.IsLandscapeHit = IsLandscapeHit;

			MOD_TRACE_LOG(Verbose, TEXT("HitResult is a blocking hit. IsLandscape: %s"), TEXT_CONDITION(IsLandscapeHit))

			if (IsLandscapeHit && AutoSupportData.EndPartDescriptor.IsValid())
			{
				const auto BuryDistance = UAutoSupportBlueprintLibrary::GetBuryDistance(
					UFGBuildingDescriptor::GetBuildableClass(AutoSupportData.EndPartDescriptor.Get()),
					AutoSupportData.EndPartTerrainBuryPercentage,
					AutoSupportData.EndPartOrientation);
				
				// Bury the part if and extend the build distance.
				Result.BuildDistance += BuryDistance;

				MOD_TRACE_LOG(Verbose, TEXT("Extended build distance by %f to bury end part."), BuryDistance);
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
			return FVector(0, Extent.Y, Extent.Z);
		case EAutoSupportBuildDirection::Back:
			return FVector(0, -Extent.Y, Extent.Z);
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

