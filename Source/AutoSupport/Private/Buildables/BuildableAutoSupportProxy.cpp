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
	check(Buildable);

	auto* SupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());
	
	FAutoSupportBuildableHandle Handle;
	Handle.Buildable = Buildable;
	Handle.BuildableClass = Buildable->GetClass();
	
	if (Buildable->ManagedByLightweightBuildableSubsystem())
	{
		Handle.LightweightRuntimeIndex = Buildable->GetRuntimeDataIndex();
	}
	
	check(Handle.IsDataValid());

	RegisteredHandles.Add(Handle);
	if (HasActorBegunPlay())
	{
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

	const FAutoSupportBuildableHandle FindHandle(Buildable);
	
	for (auto i = RegisteredHandles.Num() - 1; i >= 0; --i)
	{
		if (const auto& Handle = RegisteredHandles[i]; Handle == FindHandle)
		{
			RegisteredHandles.RemoveAt(i);
			MOD_LOG(Verbose, TEXT("Removed buildable. Index: [%i]"), i)
			DestroyIfEmpty();
			break;
		}
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

	if (DestroyIfEmpty())
	{
		return;
	}

	auto* SupportSubsys = AAutoSupportModSubsystem::Get(GetWorld());

	OnBuildModeUpdate(nullptr, nullptr); // Init.

	SupportSubsys->RegisterProxy(this);

	for (const auto& Handle : RegisteredHandles)
	{
		// It's transient to the subsys, so reregister.
		SupportSubsys->RegisterHandleToProxyLink(Handle, this);
	}
}

FAutoSupportBuildableHandle* ABuildableAutoSupportProxy::EnsureBuildablesAvailable()
{
	MOD_LOG(Verbose, TEXT("Ensuring buildables are available for %i buildables."), RegisteredHandles.Num())
	
	if (RegisteredHandles.Num() == 0)
	{
		bBuildablesAvailable = true;
		return nullptr;
	}
	
	auto* LightBuildables = AFGLightweightBuildableSubsystem::Get(GetWorld());
	
	for (auto i = RegisteredHandles.Num() - 1; i >= 0; --i)
	{
		auto& BuildableHandle = RegisteredHandles[i];
		
		if (!BuildableHandle.IsDataValid())
		{
			MOD_LOG(Verbose, TEXT("Buildable data is invalid. Class: [%s], Index: [%i]"), TEXT_CLS_NAME(BuildableHandle.BuildableClass), BuildableHandle.LightweightRuntimeIndex)
			RegisteredHandles.RemoveAt(i);
			continue;
		}

		if (!BuildableHandle.IsLightweightType()) // Check non-lightweight validity
		{
			if (!BuildableHandle.Buildable.IsValid())
			{
				MOD_LOG(Verbose, TEXT("The buildable instance at [%i] is invalid, removing handle. Lightweight: [FALSE]"), i)
				RegisteredHandles.RemoveAt(i);
			}

			continue;
		}

		if (BuildableHandle.Buildable.IsValid())
		{
			MOD_LOG(Verbose, TEXT("The buildable instance at handle index [%i] is already available."), i)
			continue;
		}

		// Lightweight, spawn in a temporary
		MOD_LOG(Verbose, TEXT("Spawning buildable from lightweight. Class: [%s], Index: [%i]"), TEXT_CLS_NAME(BuildableHandle.BuildableClass), BuildableHandle.LightweightRuntimeIndex)
		auto* InstanceData = LightBuildables->GetRuntimeDataForBuildableClassAndIndex(BuildableHandle.BuildableClass, BuildableHandle.LightweightRuntimeIndex);
		check(InstanceData);
		MOD_LOG(Verbose, TEXT("Instance data, IsValid: [%s]"), TEXT_BOOL(InstanceData->IsValid()))

		if (!InstanceData->IsValid())
		{
			MOD_LOG(Verbose, TEXT("The lightweight buildable instance data at [%i] is invalid, removing handle."), i)
			RegisteredHandles.RemoveAt(i);
			continue;
		}
		
		auto bWasSpawned = false;
		const auto* NewTemporaryHandle = LightBuildables->FindOrSpawnBuildableForRuntimeData(BuildableHandle.BuildableClass, InstanceData, BuildableHandle.LightweightRuntimeIndex, bWasSpawned);
		
		MOD_LOG(Verbose, TEXT("The lightweight buildable handle at [%i], TemporarySpawned: [%s]."), i, TEXT_BOOL(bWasSpawned))
		
		check(NewTemporaryHandle)
		check(NewTemporaryHandle->Buildable);
		BuildableHandle.Buildable = NewTemporaryHandle->Buildable;
		BuildableHandle.Buildable->SetBlockCleanupOfTemporary(true); // Block temporaries clean up during dismantle
	}

	if (RegisteredHandles.Num() == 0)
	{
		bBuildablesAvailable = true;
		return nullptr;
	}
	
	auto* RootHandle = &RegisteredHandles[0];
	MOD_LOG(Verbose, TEXT("Root buildable handle, Class: [%s]"), TEXT_CLS_NAME(RootHandle->BuildableClass))
	RootHandle->Buildable->SetParentBuildableActor(nullptr);
	
	// Set up parent actor for dismantle
	for (auto i = 1; i < RegisteredHandles.Num(); ++i)
	{
		MOD_LOG(Verbose, TEXT("Set parent buildable for handle at [%i]."), i)
		RegisteredHandles[i].Buildable->SetParentBuildableActor(RootHandle->Buildable.Get());
	}

	bBuildablesAvailable = true;
	return RootHandle;
}

void ABuildableAutoSupportProxy::RemoveTemporaries(AFGCharacterPlayer* Player)
{
	MOD_LOG(Verbose, TEXT("Removing temporaries for up to %i buildables."), RegisteredHandles.Num())
	bBuildablesAvailable = false;
	auto* Outline = Player ? Player->GetOutline() : nullptr;
	
	for (auto i = 0; i < RegisteredHandles.Num(); ++i)
	{
		auto& BuildableHandle = RegisteredHandles[i];

		if (!BuildableHandle.Buildable.IsValid())
		{
			continue;
		}

		auto* Buildable = BuildableHandle.Buildable.Get();
	
		if (Outline)
		{
			Outline->HideOutline(Buildable);
			Outline->ShowOutline(this, EOutlineColor::OC_NONE);
		}
		
		Buildable->SetParentBuildableActor(nullptr);

		if (!BuildableHandle.IsLightweightType())
		{
			continue;
		}

		auto* Temporary = BuildableHandle.Buildable.Get();
		Temporary->SetBlockCleanupOfTemporary(false);
		
		MOD_LOG(Verbose, TEXT("Temporary marked for cleanup. Handle index: [%i], Class: [%s], ManagedByLightweightSys: [%s], IsLightweightTemporary: [%s]"), i, TEXT_CLS_NAME(BuildableHandle.BuildableClass), TEXT_BOOL(Temporary->ManagedByLightweightBuildableSubsystem()), TEXT_BOOL(Temporary->GetIsLightweightTemporary()))
		
		BuildableHandle.Buildable = nullptr;
	}
}

bool ABuildableAutoSupportProxy::DestroyIfEmpty()
{
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
	MOD_LOG(Verbose, TEXT("Invoked"))
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
	check(bBuildablesAvailable)
	
	// for (auto& Handle : RegisteredHandles)
	// {
	// 	Execute_GetDismantleDependencies(Handle.Buildable.Get(), out_dismantleDependencies);
	// }

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
	return true;
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
	// for (auto Buildable : RegisteredHandles)
	// {
	// 	if (IsValid(Buildable))
	// 	{
	// 		out_dependentObjects.Add(Buildable);
	// 	}
	// }
}

#pragma endregion
