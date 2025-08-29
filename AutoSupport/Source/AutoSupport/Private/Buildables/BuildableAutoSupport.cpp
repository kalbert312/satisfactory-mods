// 

#include "BuildableAutoSupport.h"

#include "AutoSupportBuildConfigModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "AutoSupportModSubsystem.h"
#include "BP_ModConfig_AutoSupportStruct.h"
#include "BuildableAutoSupportProxy.h"
#include "DrawDebugHelpers.h"
#include "FGBlueprintProxy.h"
#include "FGBuildingDescriptor.h"
#include "FGConstructDisqualifier.h"
#include "FGHologram.h"
#include "FGLightweightBuildableSubsystem.h"
#include "FGPlayerController.h"
#include "ModBlueprintLibrary.h"
#include "ModConstants.h"
#include "ModLogging.h"
#include "UnrealNetwork.h"

ABuildableAutoSupport::ABuildableAutoSupport(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bReplicates = true;
}

void ABuildableAutoSupport::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABuildableAutoSupport, AutoSupportData);
	// DOREPLIFETIME(ABuildableAutoSupport, ReplicatedBuildPlan);
}

bool ABuildableAutoSupport::TraceAndCreatePlan(AFGCharacterPlayer* BuildInstigator, FAutoSupportBuildPlan& OutPlan) const
{
	OutPlan = FAutoSupportBuildPlan();

	// IMPORTANT: This ticks while the interact dialog is open.
	if (!AutoSupportData.MiddlePartDescriptor.IsValid() && !AutoSupportData.StartPartDescriptor.IsValid() && !AutoSupportData.EndPartDescriptor.IsValid())
	{
		MOD_TRACE_LOG(Verbose, TEXT("Nothing to build!"));
		return false;
	}

	if (mBlueprintDesigner)
	{
		MOD_TRACE_LOG(Verbose, TEXT("Can't build, in BP designer."));
		OutPlan.BuildDisqualifiers.Add(UFGCDIntersectingBlueprintDesigner::StaticClass());
		return false;
	}
	
	// Trace to know how much we're going to build.
	const auto TraceResult = Trace();
	
	UAutoSupportBlueprintLibrary::PlanBuild(GetWorld(), TraceResult, AutoSupportData, OutPlan);

	if (auto* Player = CastChecked<AFGCharacterPlayer>(BuildInstigator); !UAutoSupportBlueprintLibrary::CanAffordItemBill(Player, OutPlan.ItemBill, true))
	{
		MOD_TRACE_LOG(Verbose, TEXT("Cannot afford item bill."));
		OutPlan.BuildDisqualifiers.Add(UFGCDUnaffordable::StaticClass());
	}

	return OutPlan.IsActionable();
}

void ABuildableAutoSupport::BuildSupports(AFGCharacterPlayer* BuildInstigator)
{
	FAutoSupportBuildPlan Plan;

	if (!TraceAndCreatePlan(BuildInstigator, Plan))
	{
		MOD_LOG(Verbose, TEXT("The plan cannot be built."));
		return;
	}

	if (!UAutoSupportBlueprintLibrary::PayItemBillIfAffordable(CastChecked<AFGCharacterPlayer>(BuildInstigator), Plan.ItemBill, true))
	{
		MOD_LOG(Verbose, TEXT("Cannot afford."));
		return;
	}
	
	// Construct
	auto* Buildables = AFGBuildableSubsystem::Get(GetWorld());
	auto* LightBuildables = AFGLightweightBuildableSubsystem::Get(GetWorld());
	
	ABuildableAutoSupportProxy* SupportProxy = nullptr;
	auto* RootHologram = UAutoSupportBlueprintLibrary::CreateCompositeHologramFromPlan(Plan, AutoSupportProxyClass, BuildInstigator, this, this, SupportProxy);
	
	TArray<AActor*> HologramSpawnedActors;
	auto* StartBuildable = CastChecked<AFGBuildable>(RootHologram->Construct(HologramSpawnedActors, Buildables->GetNewNetConstructionID()));
	HologramSpawnedActors.Insert(StartBuildable, 0);

	// TODO(k.a): see if supports can be included in a blueprint proxy
	// auto* BlueprintProxy = GetBlueprintProxy();

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

		// TODO(k.a): crashing on dismantle
		// if (BlueprintProxy)
		// {
		// 	if (Buildable->ShouldConvertToLightweight())
		// 	{
		// 		BlueprintProxy->RegisterLightweightInstance(Buildable->GetClass(), Buildable->GetRuntimeDataIndex());
		// 	}
		// 	else
		// 	{
		// 		BlueprintProxy->RegisterBuildable(Buildable);
		// 	}
		// }
		
		MOD_LOG(
			Verbose,
			TEXT("Buildable[%i]: Name: [%s], ShouldConvertToLightweight: [%s], ManagedByLightweight: [%s], Customization Swatch: [%s]"),
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
	Destroy();
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

	const auto bIsClient = UAutoSupportBlueprintLibrary::IsSinglePlayerOrClientActor(this);
	MOD_LOG(
		Verbose,
		TEXT("Invoked. Owner: [%s], NetMode: [%i], HasNetOwner: [%s], NetOwner: [%s], HasLocalNetOwner: [%s], IsClient: [%s], AutoConfigureAtBeginPlay: [%s]"),
		TEXT_ACTOR_NAME(GetOwner()),
		GetNetMode(),
		TEXT_BOOL(HasNetOwner()),
		TEXT_ACTOR_NAME(GetNetOwner()),
		TEXT_BOOL(HasLocalNetOwner()),
		TEXT_BOOL(bIsClient),
		TEXT_BOOL(bAutoConfigureAtBeginPlay));

	if (bIsClient)
	{
		BeginPlay_Client();
	}

	bAutoConfigureAtBeginPlay = false;
}

void ABuildableAutoSupport::BeginPlay_Client()
{
	// HACK: Using build effect instigator as an "is newly built" indicator. I'm not sure if there's a way to intercept a moment between
	// SpawnActorDeferred and FinishingSpawning of a buildable as they are spawned from holograms. However, ideally, I'd want to set a
	// transient bool flag at that time rather than use something kind of unrelated.
	if (!mBuildEffectInstignator)
	{
		return;
	}
	
	auto* PlayerInstigator = Cast<AFGCharacterPlayer>(mBuildEffectInstignator);
	auto* InstigatorController = PlayerInstigator->GetFGPlayerController(); // this is null on multiplayer clients for other players.
	if (!InstigatorController)
	{
		MOD_LOG(Verbose, TEXT("No instigating player controller."));
		return;
	}
	
	auto* Rco = InstigatorController->GetRemoteCallObjectOfClass<UAutoSupportBuildableRCO>();
	fgcheck(Rco);
	auto bIsAutoBuildHeld = false;
	// NOTE: bAutoConfigureAtBeginPlay should always be true for new placements and false for loaded placements.
	
	if (GetBlueprintDesigner()) // built in a blueprint designer
	{
		// 2 cases: Fresh placement in designer and loading a blueprint into designer
		if (bAutoConfigureAtBeginPlay)
		{
			MOD_LOG(Verbose, TEXT("Fresh placement in blueprint designer. Auto configuring."));
			Rco->UpdateConfiguration(this, K2_GetAutoConfigureData());
		}
		else
		{
			MOD_LOG(Verbose, TEXT("Loaded into blueprint designer."))
		}
		
		return;
	}
	else if (GetBlueprintProxy()) // built from a blueprint (and not in a designer).
	{
		MOD_LOG(Verbose, TEXT("Built from a blueprint."));

		const auto bIsBpAutoBuildEnabled = FBP_ModConfig_AutoSupportStruct::GetActiveConfig(GetWorld()).GameplayDefaultsSection.AutomaticBlueprintBuild;
		const auto* LocalPlayerSubsys = UAutoSupportModLocalPlayerSubsystem::Get(PlayerInstigator);
		bIsAutoBuildHeld = LocalPlayerSubsys->IsHoldingAutoBuildKey();
	
		if ((bIsBpAutoBuildEnabled && !bIsAutoBuildHeld) || (!bIsBpAutoBuildEnabled && bIsAutoBuildHeld))
		{
			MOD_LOG(Verbose, TEXT("Auto building. IsAutoBuildEnabled: [%s], IsAutoBuildHeld: [%s]"), TEXT_BOOL(bIsBpAutoBuildEnabled), TEXT_BOOL(bIsAutoBuildHeld));
			Rco->BuildSupports(this, PlayerInstigator);
		}
		else
		{
			MOD_LOG(Verbose, TEXT("Not auto building. IsAutoBuildEnabled: [%s], IsAutoBuildHeld: [%s]"), TEXT_BOOL(bIsBpAutoBuildEnabled), TEXT_BOOL(bIsAutoBuildHeld));
		}

		return;
	}

	// is a standalone build
	if (bAutoConfigureAtBeginPlay)
	{
		Rco->UpdateConfigurationAndMaybeBuild(this, PlayerInstigator, K2_GetAutoConfigureData(), bIsAutoBuildHeld);
	}
	else if (bIsAutoBuildHeld)
	{
		Rco->BuildSupports(this, PlayerInstigator);
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

void ABuildableAutoSupport::AutoConfigure(const AFGCharacterPlayer* Player, const FBuildableAutoSupportData& Data)
{
	MOD_LOG(Verbose, TEXT("Auto configuring with data from player [%s]"), TEXT_ACTOR_NAME(Player));
	AutoSupportData = Data;
}

void ABuildableAutoSupport::SaveLastUsedData()
{
	MOD_LOG(Verbose, TEXT("Saving last used data"));
	K2_SaveLastUsedData();
}

FAutoSupportTraceResult ABuildableAutoSupport::Trace() const
{
	MOD_TRACE_LOG(Verbose, TEXT("BEGIN TRACE ---------------------------"));

	const auto* BuildConfig = UAutoSupportBuildConfigModule::Get(GetWorld());
	fgcheck(BuildConfig);
	
	// World +X = East, World +Y = South, +Z = Sky
	FAutoSupportTraceResult Result;
	const auto MaxBuildDistance = BuildConfig->GetMaxBuildDistance();
	Result.BuildDistance = MaxBuildDistance;
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
	const auto EndLocation = Result.StartLocation + TraceAbsDirection * MaxBuildDistance;

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

	auto* ContentTagRegistry = UContentTagRegistry::Get(GetWorld());

	int32 HitIndex = -1;
	for (const auto& HitResult : HitResults)
	{
		++HitIndex;
		
		const auto* HitActor = HitResult.GetActor();
		const auto* HitComponent = HitResult.GetComponent();
		MOD_TRACE_LOG(
			Verbose,
			TEXT("HitResult[%i] with distance %f. Actor: [%s], Component: [%s]"),
			HitIndex,
			HitResult.Distance,
			HitActor ? TEXT_STR(HitActor->GetName()) : TEXT_NULL,
			HitComponent ? TEXT_STR(HitComponent->GetName()) : TEXT_NULL);

		TSubclassOf<UFGConstructDisqualifier> Disqualifier = nullptr;

		switch (const auto HitClassification = BuildConfig->CalculateHitClassification(HitResult, AutoSupportData.OnlyIntersectTerrain, ContentTagRegistry, Disqualifier))
		{
			default:
			case EAutoSupportTraceHitClassification::Block:
			case EAutoSupportTraceHitClassification::Landscape:
			{
				Result.BuildDistance = HitResult.Distance;
				Result.IsLandscapeHit = HitClassification == EAutoSupportTraceHitClassification::Landscape;

				MOD_TRACE_LOG(Verbose, TEXT("  Is blocking hit. IsLandscape: %s"), TEXT_BOOL(Result.IsLandscapeHit))

				if (Result.IsLandscapeHit && AutoSupportData.EndPartDescriptor.IsValid())
				{
					const auto BuryDistance = UAutoSupportBlueprintLibrary::GetBuryDistance(
						UFGBuildingDescriptor::GetBuildableClass(AutoSupportData.EndPartDescriptor.Get()),
						AutoSupportData.EndPartTerrainBuryPercentage,
						AutoSupportData.EndPartOrientation);
				
					// Bury the part if and extend the build distance.
					Result.BuildDistance += BuryDistance;

					MOD_TRACE_LOG(Verbose, TEXT("  Extended build distance by %f to bury end part."), BuryDistance);
				}
			
				return Result;
			}
			case EAutoSupportTraceHitClassification::Ignore:
				MOD_TRACE_LOG(Verbose, TEXT("  Ignored hit."));
				continue;
			case EAutoSupportTraceHitClassification::Pawn:
				MOD_TRACE_LOG(Verbose, TEXT("  Pawn hit."));
				// Never build if we intersect a pawn.
				Result.BuildDistance = 0;
				Result.Disqualifier = Disqualifier;

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

void UAutoSupportBuildableRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAutoSupportBuildableRCO, mForceNetField_UAutoSupportBuildableRCO)
}

void UAutoSupportBuildableRCO::UpdateConfigurationAndMaybeBuild_Implementation(
	ABuildableAutoSupport* AutoSupport,
	AFGCharacterPlayer* BuilderPlayer,
	const FBuildableAutoSupportData NewData,
	const bool bShouldBuild)
{
	UpdateConfiguration(AutoSupport, NewData);
	
	if (bShouldBuild)
	{
		BuildSupports(AutoSupport, BuilderPlayer);
	}
}

void UAutoSupportBuildableRCO::UpdateConfiguration_Implementation(ABuildableAutoSupport* AutoSupport, const FBuildableAutoSupportData NewData)
{
	fgcheck(AutoSupport);
	AutoSupport->AutoSupportData = NewData;
}

void UAutoSupportBuildableRCO::BuildSupports_Implementation(ABuildableAutoSupport* AutoSupport, AFGCharacterPlayer* BuildInstigator)
{
	fgcheck(AutoSupport);
	AutoSupport->BuildSupports(BuildInstigator);
}
