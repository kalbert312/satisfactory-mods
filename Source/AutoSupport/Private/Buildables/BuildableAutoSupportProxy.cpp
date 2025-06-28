// 

#include "BuildableAutoSupportProxy.h"

#include "FGBuildable.h"
#include "FGLightweightBuildableSubsystem.h"
#include "ModLogging.h"
#include "Components/BoxComponent.h"

ABuildableAutoSupportProxy::ABuildableAutoSupportProxy()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent->SetMobility(EComponentMobility::Type::Static);
	
	BuildablesBoundingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BuildablesBoundingBox"));
	BuildablesBoundingBox->SetMobility(EComponentMobility::Type::Static);
	BuildablesBoundingBox->SetupAttachment(RootComponent);
}

void ABuildableAutoSupportProxy::RegisterBuildable(AFGBuildable* Buildable)
{
	FAutoSupportProxyBuildableHandle Handle;
	Handle.Buildable = Buildable;
	Handle.BuildableClass = Buildable->GetClass();
	
	if (Buildable->ManagedByLightweightBuildableSubsystem())
	{
		Handle.LightweightRuntimeIndex = Buildable->GetRuntimeDataIndex();
	}

	if (!Handle.Buildable.IsValid() && Handle.LightweightRuntimeIndex < 0)
	{
		check(!"Buildable is invalid")
	}

	RegisteredHandles.Add(Handle);
}

void ABuildableAutoSupportProxy::UnregisterBuildable(AFGBuildable* Buildable)
{
	// RegisteredBuildables.Remove(Buildable);
}

void ABuildableAutoSupportProxy::CalculateBounds()
{
	// BuildablesBoundingBox->SetBoxExtent(FVector(400, 400, 400)); // TESTING
}

void ABuildableAutoSupportProxy::BeginPlay()
{
	Super::BeginPlay();

	MOD_LOG(Verbose, TEXT("%i buildables registered."), RegisteredHandles.Num())
	
	CalculateBounds();
}

const FAutoSupportProxyBuildableHandle* ABuildableAutoSupportProxy::GetRootHandle() const
{
	return RegisteredHandles.Num() > 0 ? &RegisteredHandles[0] : nullptr;
}

FAutoSupportProxyBuildableHandle* ABuildableAutoSupportProxy::EnsureBuildablesAvailable()
{
	if (RegisteredHandles.Num() == 0)
	{
		return nullptr;
	}
	
	auto* LightBuildables = AFGLightweightBuildableSubsystem::Get(GetWorld());

	FAutoSupportProxyBuildableHandle* RootHandle = &RegisteredHandles[0];
	TArray<int32> IndexesToRemove;
	
	for (auto i = RegisteredHandles.Num() - 1; i >= 0; --i)
	{
		auto& BuildableHandle = RegisteredHandles[i];
		
		if (!BuildableHandle.IsValidHandle())
		{
			MOD_LOG(Verbose, TEXT("Buildable is invalid. Class: [%s], Index: [%i]"), BuildableHandle.BuildableClass ? *BuildableHandle.BuildableClass->GetName() : TEXT_NULL, BuildableHandle.LightweightRuntimeIndex)
			RegisteredHandles.RemoveAt(i);
			continue;
		}

		auto* Buildable = BuildableHandle.GetBuildable();
		
		if (BuildableHandle.IsLightweightType() && !BuildableHandle.Temporary.IsValid())
		{
			MOD_LOG(Verbose, TEXT("Spawning buildable from lightweight. Class: [%s], Index: [%i]"), *(BuildableHandle.BuildableClass->GetName()), BuildableHandle.LightweightRuntimeIndex)
			auto* InstanceData = LightBuildables->GetRuntimeDataForBuildableClassAndIndex(BuildableHandle.BuildableClass, BuildableHandle.LightweightRuntimeIndex);
			check(InstanceData);
			bool bWasSpawned = false;
			const auto* NewTemporary = LightBuildables->FindOrSpawnBuildableForRuntimeData(BuildableHandle.BuildableClass, InstanceData, BuildableHandle.LightweightRuntimeIndex, bWasSpawned);

			if (bWasSpawned)
			{
				check(NewTemporary);

				Buildable = NewTemporary->Buildable;
				Buildable->SetBlockCleanupOfTemporary(true);
			
				BuildableHandle.Temporary = Buildable;
			}
			else
			{
				MOD_LOG(Verbose, TEXT("The handle at [%i] is not valid, removing it."), i)
				RegisteredHandles.RemoveAt(i);
				continue;
			}
		}
		
		if (i > 0)
		{
			// TODO(k.a): should this be chained or a single root?
			Buildable->SetParentBuildableActor(RootHandle->GetBuildable());
		}
	}

	return RootHandle;
}

void ABuildableAutoSupportProxy::RemoveTemporaries()
{
	MOD_LOG(Verbose, TEXT("Begin"))
	
	for (auto i = 0; i < RegisteredHandles.Num(); ++i)
	{
		auto& BuildableHandle = RegisteredHandles[i];

		if (!BuildableHandle.IsLightweightType() || !BuildableHandle.Temporary.IsValid())
		{
			continue;
		}

		auto* Temporary = BuildableHandle.Temporary.Get();
		Temporary->SetBlockCleanupOfTemporary(false);
		MOD_LOG(Verbose, TEXT("Temporary Class: [%s], ManagedByLightweightSys: [%s], IsLightweightTemporary: [%s]"), *(BuildableHandle.BuildableClass->GetName()), TEXT_CONDITION(Temporary->ManagedByLightweightBuildableSubsystem()), TEXT_CONDITION(Temporary->GetIsLightweightTemporary()))
		
		BuildableHandle.Temporary = nullptr;
	}
}

#pragma region IFGDismantleInterface

bool ABuildableAutoSupportProxy::CanDismantle_Implementation() const
{
	return true;
}

void ABuildableAutoSupportProxy::Dismantle_Implementation()
{
	MOD_LOG(Verbose, TEXT("Dismantling %i buildables..."), RegisteredHandles.Num())

	if (const auto* RootHandle = EnsureBuildablesAvailable(); RootHandle->HasBuildable())
	{
		Execute_Dismantle(RootHandle->GetBuildable());
		Destroy();
	}
}

void ABuildableAutoSupportProxy::GetChildDismantleActors_Implementation(TArray<AActor*>& out_ChildDismantleActors) const
{
}

void ABuildableAutoSupportProxy::GetDismantleDependencies_Implementation(TArray<AActor*>& out_dismantleDependencies) const
{
	// for (auto Buildable : RegisteredHandles)
	// {
	// 	if (IsValid(Buildable))
	// 	{
	// 		Execute_GetDismantleDependencies(Buildable, out_dismantleDependencies);
	// 	}
	// }
}

void ABuildableAutoSupportProxy::GetDismantleDisqualifiers_Implementation(
	TArray<TSubclassOf<UFGConstructDisqualifier>>& out_dismantleDisqualifiers,
	const TArray<AActor*>& allSelectedActors) const
{
	// for (auto Buildable : RegisteredHandles)
	// {
	// 	if (IsValid(Buildable))
	// 	{
	// 		Execute_GetDismantleDisqualifiers(Buildable, out_dismantleDisqualifiers, allSelectedActors);
	// 	}
	// }
}

void ABuildableAutoSupportProxy::GetDismantleRefund_Implementation(TArray<FInventoryStack>& out_refund, bool noBuildCostEnabled) const
{
	if (auto* RootHandle = GetRootHandle(); RootHandle->HasBuildable())
	{
		Execute_GetDismantleRefund(RootHandle->GetBuildable(), out_refund, noBuildCostEnabled);
	}
}

FVector ABuildableAutoSupportProxy::GetRefundSpawnLocationAndArea_Implementation(const FVector& aimHitLocation, float& out_radius) const
{
	return FVector::ZeroVector;
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

FText ABuildableAutoSupportProxy::GetDismantleDisplayName_Implementation(AFGCharacterPlayer* byCharacter) const
{
	return FText::FromString(FString("TODO Dismantle Name"));
}

void ABuildableAutoSupportProxy::StartIsLookedAtForDismantle_Implementation(AFGCharacterPlayer* byCharacter)
{
	EnsureBuildablesAvailable();
}

void ABuildableAutoSupportProxy::StopIsLookedAtForDismantle_Implementation(AFGCharacterPlayer* byCharacter)
{
	RemoveTemporaries();
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
