// 

#include "AutoSupportModSubsystem.h"

#include "AutoSupportGameWorldModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "BuildableAutoSupportProxy.h"
#include "ModConstants.h"
#include "ModLogging.h"
#include "UnrealNetwork.h"
#include "WorldModuleManager.h"
#include "Subsystem/SubsystemActorManager.h"

TMap<TWeakObjectPtr<const UWorld>, TWeakObjectPtr<AAutoSupportModSubsystem>> AAutoSupportModSubsystem::CachedSubsystemLookup;
FCriticalSection AAutoSupportModSubsystem::CachedSubsystemLookupLock;

AAutoSupportModSubsystem* AAutoSupportModSubsystem::Get(const UWorld* World)
{
	{
		FScopeLock Lock(&CachedSubsystemLookupLock);
		if (const auto* CachedEntry = CachedSubsystemLookup.Find(World); CachedEntry && CachedEntry->IsValid())
		{
			return CachedEntry->Get();
		}
	}
	
	const auto* WorldModuleManager = World->GetSubsystem<UWorldModuleManager>();
	if (!WorldModuleManager)
	{
		return nullptr;
	}
	
	auto* WorldModule = Cast<UGameWorldModule>(WorldModuleManager->FindModule(AutoSupportConstants::ModReference));
	if (!WorldModule)
	{
		return nullptr;
	}

	// Make sure we retrieve the blueprint version
	UClass* ImplClass = nullptr;
	for (const auto& ModSubsystemClass : WorldModule->ModSubsystems)
	{
		if (ModSubsystemClass->IsChildOf<AAutoSupportModSubsystem>())
		{
			ImplClass = ModSubsystemClass;
			break;
		}
	}

	if (!ImplClass)
	{
		return nullptr;
	}
	
	auto* SubsystemActorManager = World->GetSubsystem<USubsystemActorManager>();
	auto* Result = Cast<AAutoSupportModSubsystem>(SubsystemActorManager->K2_GetSubsystemActor(ImplClass));

	{
		FScopeLock Lock(&CachedSubsystemLookupLock);
		CachedSubsystemLookup.Add(World, Result);
	}
	
	return Result;
}

AAutoSupportModSubsystem::AAutoSupportModSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnServer_Replicate;
}

void AAutoSupportModSubsystem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AAutoSupportModSubsystem, ReplicatedAutoSupportPresets);
	DOREPLIFETIME(AAutoSupportModSubsystem, ReplicatedAllProxies);
}

void AAutoSupportModSubsystem::Init()
{
	Super::Init();

	auto* World = GetWorld();

	{
		FScopeLock Lock(&CachedSubsystemLookupLock);
		CachedSubsystemLookup.Add(World, this);
	}
	
	auto* Buildables = AFGBuildableSubsystem::Get(World);
	Buildables->mBuildableRemovedDelegate.AddDynamic(this, &AAutoSupportModSubsystem::OnWorldBuildableRemoved);
	
	MOD_LOG(Verbose, TEXT("Added AFGBuildableSubsystem delegates"))
}

void AAutoSupportModSubsystem::OnWorldBuildableRemoved(AFGBuildable* Buildable)
{
	const FAutoSupportBuildableHandle Handle(Buildable);
	const auto* ProxyEntry = ProxyByBuildable.Find(Handle);

	if (!ProxyEntry || !ProxyEntry->IsValid())
	{
		return;
	}
	
	MOD_LOG(Verbose, TEXT("Found proxy. Removing handle and unregistering buildable."))
	const auto Removed = ProxyByBuildable.Remove(Handle);
	fgcheck(Removed == 1) // Check the equality contract is working as intended.
	auto* Proxy = ProxyEntry->Get();
	Proxy->UnregisterBuildable(Buildable);
}

void AAutoSupportModSubsystem::OnProxyDestroyed(const ABuildableAutoSupportProxy* Proxy)
{
	MOD_LOG(Verbose, TEXT("Invoked"))
	
	TArray<FAutoSupportBuildableHandle> HandlesToRemove;
	
	for (const auto& Entry : ProxyByBuildable)
	{
		if (Entry.Value == Proxy)
		{
			HandlesToRemove.Add(Entry.Key);
		}
	}

	MOD_LOG(Verbose, TEXT("Found %i entries to remove"), HandlesToRemove.Num())
	
	for (const auto& Handle : HandlesToRemove)
	{
		ProxyByBuildable.Remove(Handle);
	}

	AllProxies.Remove(Proxy);
	ReplicatedAllProxies.RemoveSingle(const_cast<ABuildableAutoSupportProxy*>(Proxy));
}

bool AAutoSupportModSubsystem::ShouldSave_Implementation() const
{
	return true;
}

#pragma region Auto Support Presets

void AAutoSupportModSubsystem::GetAutoSupportPresetNames(TArray<FString>& OutNames) const
{
	AutoSupportPresets.GenerateKeyArray(OutNames);
	Algo::Sort(OutNames);
}

bool AAutoSupportModSubsystem::GetAutoSupportPreset(FString PresetName, OUT FBuildableAutoSupportData& OutData)
{
	const auto PresetEntry = AutoSupportPresets.Find(PresetName);
	if (!PresetEntry)
	{
		return false;
	}
	
	OutData = *PresetEntry;
	return true;
}

void AAutoSupportModSubsystem::SaveAutoSupportPreset(FString PresetName, const FBuildableAutoSupportData Data)
{
	AutoSupportPresets.Add(PresetName, Data);

	if (const auto& ExistingKvp = ReplicatedAutoSupportPresets.FindByPredicate(
		[&](const FAutoSupportPresetNameAndDataKvp& Kvp)
		{
			return Kvp.PresetName.Equals(PresetName);
		}))
	{
		ExistingKvp->Data = Data;
	}
	else
	{
		ReplicatedAutoSupportPresets.Add(FAutoSupportPresetNameAndDataKvp(PresetName, Data));
	}
}

void AAutoSupportModSubsystem::DeleteAutoSupportPreset(const FString PresetName)
{
	AutoSupportPresets.Remove(PresetName);
	ReplicatedAutoSupportPresets.RemoveAll(
		[&](const FAutoSupportPresetNameAndDataKvp& Kvp)
		{
			return Kvp.PresetName.Equals(PresetName);
		});
}

bool AAutoSupportModSubsystem::IsExistingAutoSupportPreset(FString PresetName) const
{
	return AutoSupportPresets.Contains(PresetName);
}

void AAutoSupportModSubsystem::SyncProxiesWithBuildMode(const TArray<TWeakObjectPtr<ABuildableAutoSupportProxy>>& Proxies) const
{
	if (Proxies.Num() == 0)
	{
		return;
	}

	const auto* GameInstance = GetWorld()->GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	for (const auto* LocalPlayer : GameInstance->GetLocalPlayers())
	{
		if (!LocalPlayer)
		{
			continue;
		}
		
		const auto* ModLocalPlayerSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>();
			
		for (auto& Proxy : Proxies)
		{
			if (Proxy.IsValid())
			{
				ModLocalPlayerSubsys->SyncProxyWithBuildGunState(Proxy.Get());
			}
		}
	}
}

void AAutoSupportModSubsystem::OnRep_AutoSupportPresets()
{
	AutoSupportPresets.Empty();

	for (const auto& [PresetName, Data] : ReplicatedAutoSupportPresets)
	{
		AutoSupportPresets.Add(PresetName, Data);
	}
}

void AAutoSupportModSubsystem::OnRep_AllProxies()
{
	TArray<TWeakObjectPtr<ABuildableAutoSupportProxy>> NewProxies;
	for (const auto& Proxy : ReplicatedAllProxies)
	{
		if (!AllProxies.Contains(Proxy))
		{
			NewProxies.Add(Proxy);
		}
	}
	
	AllProxies.Empty();

	for (const auto& Proxy : ReplicatedAllProxies)
	{
		AllProxies.Add(Proxy);
	}

	SyncProxiesWithBuildMode(NewProxies);
}

#pragma endregion

void AAutoSupportModSubsystem::RegisterProxy(ABuildableAutoSupportProxy* Proxy)
{
	AllProxies.Add(Proxy);
	ReplicatedAllProxies.Add(Proxy);

	if (GetNetMode() < NM_Client) // server side only: sync the proxy with the build gun state; other cases: do this in the OnRep callback
	{
		SyncProxiesWithBuildMode({ Proxy });
	}
}

void AAutoSupportModSubsystem::RegisterHandleToProxyLink(const FAutoSupportBuildableHandle& Handle, ABuildableAutoSupportProxy* Proxy)
{
	fgcheck(Proxy);
	MOD_LOG(Verbose, TEXT("Registering handle [%s] to proxy [%s]"), TEXT_STR(Handle.ToString()), TEXT_STR(Proxy->GetName()))
	
	if (ProxyByBuildable.Contains(Handle))
	{
		fgcheckf(false, TEXT("Handle already exists! [%s]"), TEXT_STR(Handle.ToString()))
	}
	
	ProxyByBuildable.Add(Handle, Proxy);
}

void UAutoSupportSubsystemRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAutoSupportSubsystemRCO, mForceNetField_UAutoSupportSubsystemRCO)
}

void UAutoSupportSubsystemRCO::SaveAutoSupportPreset_Implementation(
	AAutoSupportModSubsystem* Subsystem,
	const FString& PresetName,
	const FBuildableAutoSupportData& Data)
{
	fgcheck(Subsystem);
	Subsystem->SaveAutoSupportPreset(PresetName, Data);
}

void UAutoSupportSubsystemRCO::DeleteAutoSupportPreset_Implementation(AAutoSupportModSubsystem* Subsystem, const FString& PresetName)
{
	fgcheck(Subsystem);
	Subsystem->DeleteAutoSupportPreset(PresetName);
}
