// 

#include "BuildableAutoSupportProxy.h"

#include "AutoSupportModSubsystem.h"
#include "FGBuildable.h"
#include "FGCharacterPlayer.h"
#include "FGLightweightBuildableSubsystem.h"
#include "ModLogging.h"

// TODO(k.a): Check that non-lightweight handles save and load correctly.

ABuildableAutoSupportProxy::ABuildableAutoSupportProxy()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Type::Movable);
}

void ABuildableAutoSupportProxy::RegisterBuildable(AFGBuildable* Buildable)
{
	fgcheck(Buildable);
	
	const FAutoSupportBuildableHandle Handle(Buildable);
	RegisteredHandles.Add(Handle);

	if (Buildable->GetIsLightweightTemporary())
	{
		LightweightIndexByHandle.Add(Handle, Buildable->GetRuntimeDataIndex());
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
	
	MOD_LOG(Verbose, TEXT("Unregistering buildable. Class: [%s], Index: [%i]"), TEXT_OBJ_CLS_NAME(Buildable), Buildable->GetRuntimeDataIndex())

	if (const FAutoSupportBuildableHandle FindHandle(Buildable); RegisteredHandles.RemoveSingle(FindHandle))
	{
		MOD_LOG(Verbose, TEXT("Removed buildable."))
		DestroyIfEmpty(false);
	}
}

void ABuildableAutoSupportProxy::UpdateBoundingBox(const FBox& NewBounds)
{
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

	MOD_LOG(Verbose, TEXT("%i buildables registered."), RegisteredHandles.Num())
	
	if (DestroyIfEmpty(false))
	{
		MOD_LOG(Warning, TEXT("An empty proxy was destroyed at BeginPlay. Why was it empty?"))
		return;
	}
	
	OnBuildModeUpdate(nullptr, nullptr); // Init.
	
	auto* SupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());
	SupportSubsys->RegisterProxy(this);

	for (const auto& Handle : RegisteredHandles)
	{
		// It's transient to the subsys
		SupportSubsys->RegisterHandleToProxyLink(Handle, this);
	}
}

void ABuildableAutoSupportProxy::EnsureBuildablesAvailable()
{
	MOD_LOG(Verbose, TEXT("Ensuring buildables are available for %i buildables."), RegisteredHandles.Num())
	
	if (RegisteredHandles.Num() == 0)
	{
		bBuildablesAvailable = true;
		return;
	}

	RemoveInvalidHandles();
	
	auto* LightBuildables = AFGLightweightBuildableSubsystem::Get(GetWorld());
	
	for (auto i = 0; i < RegisteredHandles.Num(); ++i)
	{
		auto& Handle = RegisteredHandles[i];

		if (Handle.Buildable.IsValid()) // skip already available
		{
			if (Handle.Buildable->GetIsLightweightTemporary())
			{
				Handle.Buildable->SetBlockCleanupOfTemporary(true); // Block temporaries clean up during dismantle
			}
			
			continue;
		}

		const auto RuntimeIndex = GetLightweightIndex(Handle);
		fgcheck(RuntimeIndex != INDEX_NONE);
		
		MOD_LOG(Verbose, TEXT("Ensuring buildable spawned for lightweight handle. Class: [%s], Index: [%i]"), TEXT_CLS_NAME(Handle.BuildableClass), RuntimeIndex)
		auto* InstanceData = LightBuildables->GetRuntimeDataForBuildableClassAndIndex(Handle.BuildableClass, RuntimeIndex);
		fgcheck(InstanceData);
		fgcheck(InstanceData->IsValid());
		
		auto bWasSpawned = false;
		const auto* NewTemporaryHandle = LightBuildables->FindOrSpawnBuildableForRuntimeData(Handle.BuildableClass, InstanceData, RuntimeIndex, bWasSpawned);
		
		MOD_LOG(Verbose, TEXT("  TemporarySpawned: [%s]."), i, TEXT_BOOL(bWasSpawned))
		
		fgcheck(NewTemporaryHandle)
		fgcheck(NewTemporaryHandle->Buildable);
		
		Handle.Buildable = NewTemporaryHandle->Buildable;
		Handle.Buildable->SetBlockCleanupOfTemporary(true); // Block temporaries clean up during dismantle
	}

	bBuildablesAvailable = true;
}

void ABuildableAutoSupportProxy::RemoveTemporaries(AFGCharacterPlayer* Player)
{
	MOD_LOG(Verbose, TEXT("Removing temporaries for up to %i buildables."), RegisteredHandles.Num())
	bBuildablesAvailable = false;
	auto* Outline = Player ? Player->GetOutline() : nullptr;

	RemoveInvalidHandles();
	
	for (auto i = 0; i < RegisteredHandles.Num(); ++i)
	{
		auto& Handle = RegisteredHandles[i];

		if (!Handle.Buildable.IsValid())
		{
			MOD_LOG(Warning, TEXT("Buildable instance is invalid. Class: [%s], LightweightIndex: [%i]"), TEXT_CLS_NAME(Handle.BuildableClass), GetLightweightIndex(Handle))
			continue;
		}

		auto* Buildable = Handle.Buildable.Get();
	
		if (Outline)
		{
			Outline->HideOutline(Buildable);
			Outline->ShowOutline(this, EOutlineColor::OC_NONE);
		}

		if (!Buildable->GetIsLightweightTemporary())
		{
			continue;
		}

		auto* Temporary = Handle.Buildable.Get();
		Temporary->SetBlockCleanupOfTemporary(false);
		
		MOD_LOG(Verbose, TEXT("Temporary no longer blocked for cleanup. Handle index: [%i], Class: [%s], IsLightweightTemporary: [%s]"), i, TEXT_CLS_NAME(Handle.BuildableClass), TEXT_BOOL(Temporary->GetIsLightweightTemporary()))
		
		Handle.Buildable = nullptr;
	}
}

void ABuildableAutoSupportProxy::RemoveInvalidHandles()
{
	MOD_LOG(Verbose, TEXT("Checking %i handles for validity and removing any invalid handles."), RegisteredHandles.Num())
	
	if (RegisteredHandles.Num() == 0)
	{
		return;
	}
		
	auto* LightBuildables = AFGLightweightBuildableSubsystem::Get(GetWorld());
	
	for (auto i = RegisteredHandles.Num() - 1; i >= 0; --i)
	{
		auto& Handle = RegisteredHandles[i];

		if (Handle.Buildable.IsValid())
		{
			continue;
		}

		const auto* IndexEntry = LightweightIndexByHandle.Find(Handle);

		if (!IndexEntry)
		{
			MOD_LOG(Warning, TEXT("The handle at [%i] is invalid. Buildable is not valid and no registered index. Removing handle."), i)
			RegisteredHandles.RemoveAt(i);
			continue;
		}

		// if (const auto* Temporary = LightBuildables->FindTemporaryByBuildableClassAndIndex(Handle.BuildableClass, *IndexEntry); IsValid(Temporary))
		// {
		// 	continue;
		// }

		if (const auto* InstanceData = LightBuildables->GetRuntimeDataForBuildableClassAndIndex(Handle.BuildableClass, *IndexEntry); !InstanceData || !InstanceData->IsValid())
		{
			MOD_LOG(Warning, TEXT("The lightweight buildable instance data for handle at [%i] is invalid. Removing handle. IsNull: [%s], IsValidCall: [%s]"), i, TEXT_BOOL(InstanceData == nullptr), InstanceData != nullptr ? TEXT_BOOL(InstanceData->IsValid()) : TEXT_NULL)
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
}

void ABuildableAutoSupportProxy::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects)
{
}

#pragma endregion
