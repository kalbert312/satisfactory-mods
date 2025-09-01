// 

#include "BuildableAutoSupportProxy.h"
#include "AutoSupportModSubsystem.h"
#include "FGBuildable.h"
#include "FGCharacterPlayer.h"
#include "FGLightweightBuildableBlueprintLibrary.h"
#include "FGLightweightBuildableSubsystem.h"
#include "ModBlueprintLibrary.h"
#include "ModDebugBlueprintLibrary.h"
#include "ModDefines.h"
#include "ModLogging.h"
#include "UnrealNetwork.h"
#include "Components/BoxComponent.h"

ABuildableAutoSupportProxy::ABuildableAutoSupportProxy()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Type::Movable);

	BoundingBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundingBoxComponent"));
	BoundingBoxComponent->SetMobility(EComponentMobility::Type::Movable);
	BoundingBoxComponent->SetupAttachment(RootComponent);

	bReplicates = true;
}

void ABuildableAutoSupportProxy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABuildableAutoSupportProxy, bIsLoadLightweightTraceInProgress);
	DOREPLIFETIME(ABuildableAutoSupportProxy, BoundingBox);
	DOREPLIFETIME(ABuildableAutoSupportProxy, RegisteredHandles);
}

void ABuildableAutoSupportProxy::RegisterBuildable(AFGBuildable* Buildable)
{
	if (!HasAuthority())
	{
		MOD_LOG(Warning, TEXT("RegisterBuildable called without authority. Skipping."))
		return;
	}
	
	fgcheck(Buildable);
	
	const FAutoSupportBuildableHandle Handle(Buildable);
	RegisteredHandles.Add(Handle);

	if (Buildable->GetIsLightweightTemporary())
	{
		FLightweightBuildableInstanceRef InstanceRef;
		InstanceRef.InitializeFromTemporary(Buildable);

		LightweightRefsByHandle.Add(Handle, InstanceRef);
	}
	
	if (HasActorBegunPlay())
	{
		auto* SupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());
		SupportSubsys->RegisterHandleToProxyLink(Handle, this);
	}
	
	MOD_LOG(VeryVerbose, TEXT("Registered buildable. Handle: [%s]"), TEXT_STR(Handle.ToString()))
}

// This is called by the auto support subsystem
void ABuildableAutoSupportProxy::UnregisterBuildable(AFGBuildable* Buildable)
{
	if (!HasAuthority())
	{
		MOD_LOG(Warning, TEXT("UnregisterBuildable called without authority. Skipping."))
		return;
	}
	
	if (!Buildable)
	{
		return;
	}

	const FAutoSupportBuildableHandle Handle(Buildable);
	
	MOD_LOG(VeryVerbose, TEXT("Unregistering buildable. Handle: [%s]"), TEXT_STR(Handle.ToString()))

	const auto NumRemoved = RegisteredHandles.Remove(Handle);
	fgcheck(NumRemoved <= 1); // if we're removing more than 1, we've got problems.
	
	if (NumRemoved > 0)
	{
		DestroyIfEmpty(false);
	}
}

void ABuildableAutoSupportProxy::UpdateBoundingBox(const FBox& NewBounds)
{
	BoundingBox = NewBounds;
	BoundingBoxComponent->SetRelativeLocation(NewBounds.GetCenter());
	BoundingBoxComponent->SetBoxExtent(NewBounds.GetExtent());

	K2_UpdateBoundingBox(NewBounds);
}

void ABuildableAutoSupportProxy::OnBuildModeUpdate(TSubclassOf<UFGBuildGunModeDescriptor> BuildMode, ULocalPlayer* LocalPlayer)
{
	K2_OnBuildModeUpdate(BuildMode, LocalPlayer);
}

void ABuildableAutoSupportProxy::BeginPlay()
{
	Super::BeginPlay();
	
	if (UAutoSupportBlueprintLibrary::IsSinglePlayerOrServerActor(this))
	{
		BeginPlay_Server();
	}

#ifdef AUTOSUPPORT_DRAW_DEBUG_SHAPES
	BoundingBoxComponent->SetHiddenInGame(false);
	UAutoSupportDebugBlueprintLibrary::DrawDebugCoordinateSystem(GetWorld(), BoundingBoxComponent->GetComponentLocation(), BoundingBoxComponent->GetComponentRotation(), 50.f, true, -1.f, 100, 1.f);
#endif
}

void ABuildableAutoSupportProxy::BeginPlay_Server()
{
	if (bIsNewlySpawned)
	{
		if (!DestroyIfEmpty(false))
		{
			RegisterSelfAndHandlesWithSubsystem();
		}
	}
	else
	{
		// This is done on the next tick because without it, there seems to be some initialization going on with the AbstractInstanceManager between now and trace complete that causes some traces not to return expected overlaps.
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ABuildableAutoSupportProxy::BeginLightweightTraceAndResetRetries));
	}
}

void ABuildableAutoSupportProxy::EnsureBuildablesAvailable()
{
	if (bIsLoadLightweightTraceInProgress)
	{
		MOD_LOG(Warning, TEXT("Invoked while load trace in progress. No-op."))
		return;
	}
	
	MOD_LOG(Verbose, TEXT("Ensuring buildables are available for %i buildables."), RegisteredHandles.Num())
	
	if (RegisteredHandles.Num() == 0)
	{
		bBuildablesAvailable = true;
		return;
	}

	if (HasAuthority())
	{
		RemoveInvalidHandles();
	}
	
	for (auto i = 0; i < RegisteredHandles.Num(); ++i)
	{
		auto& Handle = RegisteredHandles[i];

		MOD_LOG(VeryVerbose, TEXT("Processing handle at index %i. Handle: [%s]"), i, TEXT_STR(Handle.ToString()))

		auto* Buildable = GetBuildableForHandle(Handle);

		if (!Buildable)
		{
			MOD_LOG(Error, TEXT("  Buildable not found for handle. Skipping."))
			continue;
		}

		if (Buildable->GetIsLightweightTemporary())
		{
			Buildable->SetBlockCleanupOfTemporary(true); // Block temporaries clean up during dismantle
			MOD_LOG(VeryVerbose, TEXT("  Temporary cleanup blocked."))
		}
	}

	bBuildablesAvailable = true;
}

void ABuildableAutoSupportProxy::RemoveTemporaries(const AFGCharacterPlayer* Player)
{
	if (bIsLoadLightweightTraceInProgress)
	{
		MOD_LOG(Warning, TEXT("Invoked while load trace is in progress. No-op."))
		return;
	}
	
	MOD_LOG(Verbose, TEXT("Removing temporaries for up to %i buildables."), RegisteredHandles.Num())
	bBuildablesAvailable = false;
	auto* Outline = Player ? Player->GetOutline() : nullptr;

	if (HasAuthority())
	{
		RemoveInvalidHandles();
	}
	
	for (auto i = 0; i < RegisteredHandles.Num(); ++i)
	{
		auto& Handle = RegisteredHandles[i];

		MOD_LOG(VeryVerbose, TEXT("Processing handle at index %i. Handle: [%s]"), i, TEXT_STR(Handle.ToString()))

		auto* Buildable = GetBuildableForHandle(Handle);

		if (!Buildable)
		{
			MOD_LOG(Warning, TEXT("  Buildable not found for handle. Skipping."))
			continue;
		}
	
		if (Outline)
		{
			Outline->HideOutline(Buildable);
			Outline->ShowOutline(this, EOutlineColor::OC_NONE);
			MOD_LOG(VeryVerbose, TEXT("  Handle had its outline hidden."))
		}

		if (Buildable->GetIsLightweightTemporary())
		{
			Buildable->SetBlockCleanupOfTemporary(false);
			MOD_LOG(VeryVerbose, TEXT("  Temporary cleanup unblocked."))
		}
	}
}

void ABuildableAutoSupportProxy::RemoveInvalidHandles()
{
	if (!HasAuthority())
	{
		MOD_LOG(Verbose, TEXT("RemoveInvalidHandles called without authority. Skipping."))
		return;
	}
	
	if (bIsLoadLightweightTraceInProgress)
	{
		MOD_LOG(Warning, TEXT("RemoveInvalidHandles called while trace in progress. Skipping."))
		return;
	}

	MOD_LOG(Verbose, TEXT("Checking %i handles for validity and removing any invalid handles."), RegisteredHandles.Num())
	
	if (RegisteredHandles.Num() == 0)
	{
		return;
	}
	
	for (auto i = RegisteredHandles.Num() - 1; i >= 0; --i)
	{
		auto& Handle = RegisteredHandles[i];

		if (Handle.Buildable.IsValid())
		{
			continue;
		}

		const auto* LightweightRef = LightweightRefsByHandle.Find(Handle);

		if (!LightweightRef)
		{
			MOD_LOG(Warning, TEXT("The handle at index %i is invalid. Buildable is not valid and no lightweight ref. Removing handle."), i)
			RegisteredHandles.RemoveAt(i);
			continue;
		}

		if (!LightweightRef->IsValid())
		{
			MOD_LOG(Warning, TEXT("The handle at index [%i] is invalid. Lightweight ref was found but it's not valid. Removing handle."), i)
			RegisteredHandles.RemoveAt(i);
		}
	}
}

bool ABuildableAutoSupportProxy::DestroyIfEmpty(const bool bRemoveInvalidHandles)
{
	if (!HasAuthority())
	{
		MOD_LOG(Warning, TEXT("DestroyIfEmpty called without authority. Skipping."))
		return false;
	}
	
	if (bRemoveInvalidHandles)
	{
		RemoveInvalidHandles();
	}
	
	if (RegisteredHandles.Num() != 0)
	{
		return false;
	}

#ifdef AUTOSUPPORT_DEV_KEEP_EMPTY_PROXIES
	MOD_LOG(Warning, TEXT("Not destroying empty proxy because dev flag is enabled."));
    return false;
#else
	MOD_LOG(Verbose, TEXT("Destroying empty proxy"));
	Destroy();
	return true;
#endif
}

void ABuildableAutoSupportProxy::RegisterSelfAndHandlesWithSubsystem()
{
	if (!HasAuthority())
	{
		MOD_LOG(Warning, TEXT("RegisterSelfAndHandlesWithSubsystem called without authority. Skipping."))
		return;
	}
	
	MOD_LOG(Verbose, TEXT("Registering self and %i handles with subsystem."), RegisteredHandles.Num())
	
	auto* SupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());
	SupportSubsys->RegisterProxy(this);

	for (const auto& Handle : RegisteredHandles)
	{
		// It's transient to the subsys
		SupportSubsys->RegisterHandleToProxyLink(Handle, this);
	}
}

void ABuildableAutoSupportProxy::BeginLightweightTraceAndResetRetries()
{
	LightweightTraceRetryCount = 0;
	BeginLightweightTrace();
}

void ABuildableAutoSupportProxy::BeginLightweightTraceRetry()
{
	++LightweightTraceRetryCount;
	BeginLightweightTrace();
}

void ABuildableAutoSupportProxy::BeginLightweightTrace()
{
	// Need to reestablish lightweight runtime indices, so trace and match by buildable class and transform.
	const FCollisionObjectQueryParams ObjectQueryParams(FCollisionObjectQueryParams::AllObjects);
		
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);

	const auto TraceLocation = BoundingBoxComponent->GetComponentLocation();
	const auto TraceRotation = BoundingBoxComponent->GetComponentRotation();
	const FCollisionShape CollisionShape = BoundingBoxComponent->GetCollisionShape();
		
	// Overlap all so we can detect all collisions in our path.
	// FCollisionResponseParams ResponseParams(ECR_Overlap);
	
	if (HasAuthority())
	{
		bIsLoadLightweightTraceInProgress = true;
	}
	
	bIsClientLightweightTraceInProgress = true;
	
	LoadTraceDelegate = FOverlapDelegate();
	LoadTraceDelegate.BindUObject(this, &ABuildableAutoSupportProxy::OnLightweightTraceComplete); // for some reason binding this in begin play doesn't work. I guess it's locally scoped?

	MOD_TRACE_LOG(
		Verbose,
		TEXT("Beginning lightweight trace. TraceLocation: [%s], TraceRotation: [%s], TraceExtent: [%s], Retry: [%i]"),
		TEXT_STR(TraceLocation.ToCompactString()),
		TEXT_STR(TraceRotation.ToCompactString()),
		TEXT_STR(CollisionShape.GetExtent().ToCompactString()),
		LightweightTraceRetryCount)
	
	CurrentTraceHandle = GetWorld()->AsyncOverlapByObjectType(
		TraceLocation,
		BoundingBoxComponent->GetComponentRotation().Quaternion(),
		ObjectQueryParams,
		CollisionShape,
		TraceParams,
		&LoadTraceDelegate);

	fgcheck(GetWorld()->IsTraceHandleValid(CurrentTraceHandle, true))
}

void ABuildableAutoSupportProxy::OnLightweightTraceComplete(const FTraceHandle& TraceHandle, FOverlapDatum& Datum)
{
	if (!CurrentTraceHandle.IsValid() || CurrentTraceHandle != TraceHandle)
	{
		MOD_TRACE_LOG(Verbose,TEXT("Load trace completed but this run was either cancelled or superseded."))
		return;
	}
	
	CurrentTraceHandle.Invalidate();
	
	if (HasAuthority())
	{
		bIsLoadLightweightTraceInProgress = false;
	}
	
	bIsClientLightweightTraceInProgress = false;
	
	// TODO(k.a): should this block also be done async? If so, need to register with subsystem in synchronized matter.
	MOD_TRACE_LOG(
		Verbose,
		TEXT("Load trace completed. TraceLocation: [%s], TraceRotation: [%s], TraceExtent: [%s], Num Overlaps: [%i]"),
		TEXT_STR(BoundingBoxComponent->GetComponentLocation().ToCompactString()),
		TEXT_STR(BoundingBoxComponent->GetComponentRotation().ToCompactString()),
		TEXT_STR(BoundingBoxComponent->GetCollisionShape().GetExtent().ToCompactString()),
		Datum.OutOverlaps.Num())

	if (Datum.OutOverlaps.Num() < RegisteredHandles.Num())
	{
		MOD_TRACE_LOG(Error, TEXT("Load trace returned less overlaps than registered handles. Expected at least: [%i], Actual: [%i]"), RegisteredHandles.Num(), Datum.OutOverlaps.Num())
	}
	
	TMap<FAutoSupportBuildableHandle, FLightweightBuildableInstanceRef> OverlapRefsByHandle;

#ifdef AUTOSUPPORT_DEV_LOGGING
	for (const auto& PersistedHandle : RegisteredHandles)
	{
		MOD_TRACE_LOG(VeryVerbose, TEXT("  PersistedHandle: [%s]"), TEXT_STR(PersistedHandle.ToString()))
	}
#endif
	
	// Collect refs of overlaps and assign a handle key to them.
	for (const auto& OverlapResult : Datum.OutOverlaps)
	{
		auto* HitActor = OverlapResult.GetActor();
		
		MOD_TRACE_LOG(VeryVerbose, TEXT("Overlapped Actor: [%s]"), TEXT_ACTOR_NAME(HitActor))
		
		if (auto* AbstractInstManager = Cast<AAbstractInstanceManager>(HitActor))
		{
			if (FInstanceHandle InstanceHandle; AbstractInstManager->ResolveOverlap(OverlapResult, InstanceHandle))
			{
				if (FLightweightBuildableInstanceRef LightweightInstRef; AFGLightweightBuildableSubsystem::ResolveLightweightInstance(InstanceHandle, LightweightInstRef))
				{
					FAutoSupportBuildableHandle OverlapHandle(LightweightInstRef);
					
					MOD_TRACE_LOG(VeryVerbose, TEXT("  OverlapHandle: [%s]"), TEXT_STR(OverlapHandle.ToString()))
					
					OverlapRefsByHandle.Add(OverlapHandle, LightweightInstRef);
				}
				else
				{
					MOD_TRACE_LOG(Warning, TEXT("Failed to resolve overlap to lightweight instance ref."))
				}
			}
			else
			{
				MOD_TRACE_LOG(Warning, TEXT("Failed to resolve overlap to instance handle."))
			}
		}
		else if (auto* Buildable = Cast<AFGBuildable>(HitActor); Buildable && Buildable->ShouldConvertToLightweight())
		{
			MOD_TRACE_LOG(Error, TEXT("Hit a buildable instance that should be converted to lightweight. I don't think we're expecting this at load time?"))
		}
	}

	LightweightRefsByHandle.Empty();
	auto bAtLeastOneLightweightRefNotFound = false;
	
	// Store the transient ref for the handle
	for (const auto& RegisteredHandle : RegisteredHandles)
	{
		if (!RegisteredHandle.IsConsideredLightweight())
		{
			return;	
		}
		
		if (auto* InstanceRef = OverlapRefsByHandle.Find(RegisteredHandle))
		{
			LightweightRefsByHandle.Add(RegisteredHandle, *InstanceRef);
			MOD_TRACE_LOG(VeryVerbose, TEXT("Registered transient ref for handle: [%s]"), TEXT_STR(RegisteredHandle.ToString()))
		}
		else
		{
			bAtLeastOneLightweightRefNotFound = true;
			MOD_TRACE_LOG(Error, TEXT("Failed to find transient ref for handle: [%s]"), TEXT_STR(RegisteredHandle.ToString()))
		}
	}

	if (HasAuthority())
	{
		if (!DestroyIfEmpty(true))
		{
			RegisterSelfAndHandlesWithSubsystem();
		}
	}
	else if (bAtLeastOneLightweightRefNotFound) 
	{
		// maybe lightweight system is not fully replicated or initialized? Not sure how to verify, so implement a simple retry just in case.
		if (LightweightTraceRetryCount <= 2)
		{
			FTimerDelegate TimerDelegate;
			TimerDelegate.BindUObject(this, &ABuildableAutoSupportProxy::BeginLightweightTraceRetry);

			MOD_TRACE_LOG(Warning, TEXT("Failed to find at least 1 lightweight ref. Retrying trace in 2 seconds."))

			LightweightTraceRetryHandle = FTimerHandle();
			GetWorld()->GetTimerManager().SetTimer(LightweightTraceRetryHandle, TimerDelegate, 0, false, 2.f);
		}
		else
		{
			MOD_TRACE_LOG(Error, TEXT("Failed to find lightweight ref after all trace retries."))
		}
	}
}

AFGBuildable* ABuildableAutoSupportProxy::GetBuildableForHandle(const FAutoSupportBuildableHandle& Handle) const
{
	if (Handle.Buildable.IsValid())
	{
		return Handle.Buildable.Get();
	}

	if (Handle.IsConsideredLightweight())
	{
		if (const auto* LightweightRef = LightweightRefsByHandle.Find(Handle); LightweightRef && LightweightRef->IsValid())
		{
			return UFGLightweightBuildableBlueprintLibrary::SpawnTemporaryFromLightweight(*LightweightRef);
		}
	}

	return nullptr;
}

void ABuildableAutoSupportProxy::OnRep_BoundingBox()
{
	UpdateBoundingBox(BoundingBox);
}

void ABuildableAutoSupportProxy::OnRep_RegisteredHandles()
{
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ABuildableAutoSupportProxy::BeginLightweightTraceAndResetRetries);
}

#pragma region IFGDismantleInterface

bool ABuildableAutoSupportProxy::CanDismantle_Implementation() const
{
	return true;
}

FText ABuildableAutoSupportProxy::GetDismantleDisplayName_Implementation(AFGCharacterPlayer* byCharacter) const
{
	return FText::FromString(FString("Auto Support"));
}

void ABuildableAutoSupportProxy::StartIsLookedAtForDismantle_Implementation(AFGCharacterPlayer* byCharacter)
{
	if (!byCharacter->IsLocallyControlled()) // only execute for the player dismantling
	{
		return;
	}
	
	MOD_LOG(Verbose, TEXT("Invoked"))
	++HoveredForDismantleCount;
	
	EnsureBuildablesAvailable();
}

void ABuildableAutoSupportProxy::StopIsLookedAtForDismantle_Implementation(AFGCharacterPlayer* byCharacter)
{
	if (!byCharacter->IsLocallyControlled()) // only execute for the player dismantling
	{
		return;
	}
	
	MOD_LOG(Verbose, TEXT("Invoked"))
	HoveredForDismantleCount = FMath::Max(0, HoveredForDismantleCount - 1);
	
	RemoveTemporaries(byCharacter);
}

void ABuildableAutoSupportProxy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed && HasAuthority())
	{
		auto* SupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());

		SupportSubsys->OnProxyDestroyed(this);
	}
	
	Super::EndPlay(EndPlayReason);
}

void ABuildableAutoSupportProxy::Dismantle_Implementation()
{
	MOD_LOG(Verbose, TEXT("Dismantle called. Buildables available: [%s], HoveredForDismantleCount: [%i]"), TEXT_BOOL(bBuildablesAvailable), HoveredForDismantleCount)
	MOD_LOG(Verbose, TEXT("Dismantling %i buildables..."), RegisteredHandles.Num())
	EnsureBuildablesAvailable();

	Destroy();
}

void ABuildableAutoSupportProxy::GetChildDismantleActors_Implementation(TArray<AActor*>& out_ChildDismantleActors) const
{
	MOD_LOG(Verbose, TEXT("BuildablesAvailable: [%s], HoveredForDismantleCount: [%i]"), TEXT_BOOL(bBuildablesAvailable), HoveredForDismantleCount)

	for (auto& Handle : RegisteredHandles)
	{
		if (auto* Buildable = GetBuildableForHandle(Handle))
		{
			out_ChildDismantleActors.Add(Buildable);
		}
		else
		{
			MOD_LOG(Warning, TEXT("Buildable not found for handle. Skipping."))
		}
	}

	MOD_LOG(Verbose, TEXT("Dismantle child actors, Count: [%i]"), out_ChildDismantleActors.Num())
}

void ABuildableAutoSupportProxy::GetDismantleDependencies_Implementation(TArray<AActor*>& out_dismantleDependencies) const
{
	MOD_LOG(Verbose, TEXT("Dismantle dependencies, Count: [%i]"), out_dismantleDependencies.Num())
}

void ABuildableAutoSupportProxy::GetDismantleDisqualifiers_Implementation(
	TArray<TSubclassOf<UFGConstructDisqualifier>>& out_dismantleDisqualifiers,
	const TArray<AActor*>& allSelectedActors) const
{
}

void ABuildableAutoSupportProxy::GetDismantleRefund_Implementation(TArray<FInventoryStack>& out_refund, bool noBuildCostEnabled) const
{
	// NOTE: This is for this actor only. The refunds from registered buildables are propagated via get GetChildDismantleActors.
}

FVector ABuildableAutoSupportProxy::GetRefundSpawnLocationAndArea_Implementation(const FVector& aimHitLocation, float& out_radius) const
{
	return GetActorLocation();
}

void ABuildableAutoSupportProxy::PreUpgrade_Implementation()
{
}

bool ABuildableAutoSupportProxy::ShouldBlockDismantleSample_Implementation() const
{
	return false;
}

bool ABuildableAutoSupportProxy::SupportsDismantleDisqualifiers_Implementation() const
{
	return true;
}

void ABuildableAutoSupportProxy::Upgrade_Implementation(AActor* newActor)
{
}

#pragma endregion

#pragma region IFGSaveInterface

bool ABuildableAutoSupportProxy::ShouldSave_Implementation() const
{
	return true;
}

bool ABuildableAutoSupportProxy::NeedTransform_Implementation()
{
	return true;
}

void ABuildableAutoSupportProxy::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
}

void ABuildableAutoSupportProxy::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
}

void ABuildableAutoSupportProxy::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
}

void ABuildableAutoSupportProxy::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	BoundingBoxComponent->SetRelativeLocation(BoundingBox.GetCenter());
	BoundingBoxComponent->SetBoxExtent(BoundingBox.GetExtent());
}

void ABuildableAutoSupportProxy::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects)
{
}

#pragma endregion
