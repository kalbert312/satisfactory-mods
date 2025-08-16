// 

#include "BuildableAutoSupportProxy.h"
#include "ModDefines.h"
#include "AutoSupportModSubsystem.h"
#include "FGBuildable.h"
#include "FGCharacterPlayer.h"
#include "FGLightweightBuildableSubsystem.h"
#include "ModLogging.h"
#include "Components/BoxComponent.h"

ABuildableAutoSupportProxy::ABuildableAutoSupportProxy()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Type::Movable);

	BoundingBoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundingBoxComponent"));
	BoundingBoxComponent->SetMobility(EComponentMobility::Type::Movable);
	BoundingBoxComponent->SetupAttachment(RootComponent);
}

void ABuildableAutoSupportProxy::RegisterBuildable(AFGBuildable* Buildable)
{
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
	
	MOD_LOG(Verbose, TEXT("Registered buildable. Handle: [%s]"), TEXT_STR(Handle.ToString()))
}

// This is called by the auto support subsystem
void ABuildableAutoSupportProxy::UnregisterBuildable(AFGBuildable* Buildable)
{
	if (!Buildable)
	{
		return;
	}

	const FAutoSupportBuildableHandle Handle(Buildable);
	
	MOD_LOG(Verbose, TEXT("Unregistering buildable. Handle: [%s]"), TEXT_STR(Handle.ToString()))

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

void ABuildableAutoSupportProxy::OnBuildModeUpdate(
	TSubclassOf<UFGBuildGunModeDescriptor> BuildMode,
	ULocalPlayer* LocalPlayer)
{
	K2_OnBuildModeUpdate(BuildMode, LocalPlayer);
}

void ABuildableAutoSupportProxy::BeginPlay()
{
	Super::BeginPlay();

	if (bIsNewlySpawned)
	{
		if (!DestroyIfEmpty(false))
		{
			RegisterSelfAndHandlesWithSubsystem();
		}
	}
	else
	{
		// Need to reestablish lightweight runtime indices, so trace and match by buildable class and transform.
		FCollisionQueryParams TraceParams;
		TraceParams.AddIgnoredActor(this);

		const auto TraceLocation = BoundingBoxComponent->GetComponentLocation();
		const FCollisionShape CollisionShape = BoundingBoxComponent->GetCollisionShape();
		
		// Overlap all so we can detect all collisions in our path.
		FCollisionResponseParams ResponseParams(ECR_Overlap);

		LoadTraceDelegate.BindUObject(this, &ABuildableAutoSupportProxy::OnLoadTraceComplete);
		bIsLoadTraceInProgress = true;
		
		GetWorld()->AsyncOverlapByChannel(
			TraceLocation,
			GetActorQuat(),
			ECollisionChannel::ECC_Visibility,
			CollisionShape,
			TraceParams,
			ResponseParams,
			&LoadTraceDelegate);
	}
}

void ABuildableAutoSupportProxy::EnsureBuildablesAvailable()
{
	if (bIsLoadTraceInProgress)
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

	RemoveInvalidHandles();
	
	for (auto i = 0; i < RegisteredHandles.Num(); ++i)
	{
		auto& Handle = RegisteredHandles[i];

		MOD_LOG(Verbose, TEXT("Processing handle at index %i. Handle: [%s]"), i, TEXT_STR(Handle.ToString()))

		if (Handle.Buildable.IsValid()) // skip already available
		{
			if (Handle.Buildable->GetIsLightweightTemporary())
			{
				Handle.Buildable->SetBlockCleanupOfTemporary(true); // Block temporaries clean up during dismantle
			}

			MOD_LOG(Verbose, TEXT("  Buildable already available for handle."))
			continue;
		}

		const auto* InstanceRef = LightweightRefsByHandle.Find(Handle);
		fgcheck(InstanceRef);
		fgcheck(InstanceRef->IsValid()); // we already removed invalid handles

		MOD_LOG(Verbose, TEXT("  Ensuring temporary buildable spawned for lightweight handle"))
		
		auto* Temporary = InstanceRef->SpawnTemporaryBuildable();
		fgcheck(Temporary); // this should never be null.
		Temporary->SetBlockCleanupOfTemporary(true);  // Block temporaries clean up during dismantle
		
		Handle.Buildable = Temporary;
	}

	bBuildablesAvailable = true;
}

void ABuildableAutoSupportProxy::RemoveTemporaries(AFGCharacterPlayer* Player)
{
	if (bIsLoadTraceInProgress)
	{
		MOD_LOG(Warning, TEXT("Invoked while load trace is in progress. No-op."))
		return;
	}
	
	MOD_LOG(Verbose, TEXT("Removing temporaries for up to %i buildables."), RegisteredHandles.Num())
	bBuildablesAvailable = false;
	auto* Outline = Player ? Player->GetOutline() : nullptr;

	RemoveInvalidHandles();
	
	for (auto i = 0; i < RegisteredHandles.Num(); ++i)
	{
		auto& Handle = RegisteredHandles[i];

		MOD_LOG(Verbose, TEXT("Processing handle at index %i. Handle: [%s]"), i, TEXT_STR(Handle.ToString()))

		if (!Handle.Buildable.IsValid())
		{
			MOD_LOG(Warning, TEXT("  Handle has invalid buildable. Skipping."))
			continue;
		}

		auto* Buildable = Handle.Buildable.Get();
	
		if (Outline)
		{
			Outline->HideOutline(Buildable);
			Outline->ShowOutline(this, EOutlineColor::OC_NONE);
			MOD_LOG(Verbose, TEXT("  Handle had its outline hidden."))
		}

		if (!Buildable->GetIsLightweightTemporary())
		{
			continue;
		}

		auto* Temporary = Handle.Buildable.Get();
		Temporary->SetBlockCleanupOfTemporary(false);
		
		MOD_LOG(Verbose, TEXT("  Handle had its temporary unblocked for cleanup."))
		
		Handle.Buildable = nullptr;
	}
}

void ABuildableAutoSupportProxy::RemoveInvalidHandles()
{
	if (bIsLoadTraceInProgress)
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

bool ABuildableAutoSupportProxy::DestroyIfEmpty(bool bRemoveInvalidHandles)
{
	if (bRemoveInvalidHandles)
	{
		RemoveInvalidHandles();
	}
	
	if (RegisteredHandles.Num() != 0)
	{
		return false;
	}
	
	MOD_LOG(Verbose, TEXT("Destroying empty proxy"));
	Destroy();
	return true;
}

void ABuildableAutoSupportProxy::RegisterSelfAndHandlesWithSubsystem()
{
	MOD_LOG(Verbose, TEXT("Registering self and %i handles with subsystem."), RegisteredHandles.Num())
	
	auto* SupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());
	SupportSubsys->RegisterProxy(this);

	for (const auto& Handle : RegisteredHandles)
	{
		// It's transient to the subsys
		SupportSubsys->RegisterHandleToProxyLink(Handle, this);
	}
}

void ABuildableAutoSupportProxy::OnLoadTraceComplete(const FTraceHandle& Handle, FOverlapDatum& Datum)
{
	// TODO(k.a): should this block also be done async? If so, need to register with subsystem in synchronized matter.
	MOD_TRACE_LOG(Verbose, TEXT("Invoked. Num Overlaps: [%i]"), Datum.OutOverlaps.Num())
	bIsLoadTraceInProgress = false;

	TMap<FAutoSupportBuildableHandle, FLightweightBuildableInstanceRef> OverlapRefsByHandle;
	
	if (Datum.OutOverlaps.Num() < RegisteredHandles.Num())
	{
		MOD_TRACE_LOG(Warning, TEXT("Not enough overlaps to match all handles. Expected: [%i], Actual: [%i]"), RegisteredHandles.Num(), Datum.OutOverlaps.Num())
#ifdef AUTOSUPPORT_DEV_LOGGING
		TSet<uint32> PersistedTransformHashes;
		TSet<uint32> OverlappedTransformHashes;
		
		for (const auto& PersistedHandle : RegisteredHandles)
		{
			MOD_TRACE_LOG(Warning, TEXT("  PersistedHandle: [%s]"), TEXT_STR(PersistedHandle.ToString()))
			PersistedTransformHashes.Add(PersistedHandle.GetTransformHash());
		}

		for (const auto& OverlapResult : Datum.OutOverlaps)
		{
			auto* HitActor = OverlapResult.GetActor();
			if (!HitActor || !HitActor->IsA<AAbstractInstanceManager>())
			{
				continue;
			}

			FInt64Vector3 RoundedLocation;
			FAutoSupportBuildableHandle::GetRoundedLocation(HitActor->GetActorLocation(), RoundedLocation);
			auto OverlapTransformHash = GetTypeHash(RoundedLocation);
			PersistedTransformHashes.Add(OverlapTransformHash);
			
			MOD_TRACE_LOG(Warning, TEXT("  AbstractInstanceManager OverlapResult: TransformHash: [%i], Transform: [%s], "), OverlapTransformHash, TEXT_STR(HitActor->GetTransform().ToHumanReadableString()))
		}
		
		MOD_TRACE_LOG(Error, TEXT("Persisted hashes not found in overlaps: %s"), TEXT_STR(FString::JoinBy(PersistedTransformHashes.Difference(OverlappedTransformHashes), TEXT(","), [](auto& Hash) { return FString::FromInt(Hash); })));
		MOD_TRACE_LOG(Error, TEXT("Overlap hashes not found in persisted: %s"), TEXT_STR(FString::JoinBy(OverlappedTransformHashes.Difference(PersistedTransformHashes), TEXT(","), [](auto& Hash) { return FString::FromInt(Hash); })));
#endif
	}

	// Collect refs of overlaps and assign a handle key to them.
	for (const auto& OverlapResult : Datum.OutOverlaps)
	{
		auto* HitActor = OverlapResult.GetActor();

		MOD_TRACE_LOG(Verbose, TEXT("Overlapped Actor: [%s]"), HitActor ? TEXT_STR(HitActor->GetName()) : TEXT_NULL)

		if (auto* AbstractInstManager = Cast<AAbstractInstanceManager>(HitActor))
		{
			if (FInstanceHandle InstanceHandle; AbstractInstManager->ResolveOverlap(OverlapResult, InstanceHandle))
			{
				if (FLightweightBuildableInstanceRef LightweightInstRef; AFGLightweightBuildableSubsystem::ResolveLightweightInstance(InstanceHandle, LightweightInstRef))
				{
					OverlapRefsByHandle.Add(FAutoSupportBuildableHandle(LightweightInstRef), LightweightInstRef);
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
	}

	// Store the transient ref for the handle
	for (const auto& RegisteredHandle : RegisteredHandles)
	{
		if (auto* InstanceRef = OverlapRefsByHandle.Find(RegisteredHandle); InstanceRef)
		{
			LightweightRefsByHandle.Add(RegisteredHandle, *InstanceRef);
			MOD_TRACE_LOG(Verbose, TEXT("Registered transient ref for handle: [%s]"), TEXT_STR(RegisteredHandle.ToString()))
		}
		else
		{
			MOD_TRACE_LOG(Error, TEXT("Failed to find transient ref for handle: [%s]"), TEXT_STR(RegisteredHandle.ToString()))
		}
	}

	if (!DestroyIfEmpty(true))
	{
		RegisterSelfAndHandlesWithSubsystem();
	}
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
	MOD_LOG(Verbose, TEXT("Invoked"))
	bIsHoveredForDismantle = true;
	EnsureBuildablesAvailable();
}

void ABuildableAutoSupportProxy::StopIsLookedAtForDismantle_Implementation(AFGCharacterPlayer* byCharacter)
{
	MOD_LOG(Verbose, TEXT("Invoked"))
	bIsHoveredForDismantle = false;
	RemoveTemporaries(byCharacter);
}

void ABuildableAutoSupportProxy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		auto* SupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());

		SupportSubsys->OnProxyDestroyed(this);
	}
	
	Super::EndPlay(EndPlayReason);
}

void ABuildableAutoSupportProxy::Dismantle_Implementation()
{
	MOD_LOG(Verbose, TEXT("Dismantle called. Buildables available: [%s], IsHoveredForDismantle: [%s]"), TEXT_BOOL(bBuildablesAvailable), TEXT_BOOL(bIsHoveredForDismantle))
	MOD_LOG(Verbose, TEXT("Dismantling %i buildables..."), RegisteredHandles.Num())
	EnsureBuildablesAvailable();

	Destroy();
}

void ABuildableAutoSupportProxy::GetChildDismantleActors_Implementation(TArray<AActor*>& out_ChildDismantleActors) const
{
	MOD_LOG(Verbose, TEXT("BuildablesAvailable: [%s], HoveredForDismantle: [%s]"), TEXT_BOOL(bBuildablesAvailable), TEXT_BOOL(bIsHoveredForDismantle))
	
	if (!bBuildablesAvailable)
	{
		MOD_LOG(Warning, TEXT("Invoked but BuildablesAvailable is FALSE."))
		return;
	}

	for (auto& Handle : RegisteredHandles)
	{
		out_ChildDismantleActors.Add(Handle.Buildable.Get());
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
	const auto* RootBuildable = GetCheckedRootBuildable();

	return Execute_GetRefundSpawnLocationAndArea(RootBuildable, aimHitLocation, out_radius);
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
