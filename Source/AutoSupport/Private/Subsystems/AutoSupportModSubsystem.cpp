// 

#include "AutoSupportModSubsystem.h"

#include "AutoSupportGameWorldModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "BuildableAutoSupportProxy.h"
#include "ModConstants.h"
#include "ModLogging.h"
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

void AAutoSupportModSubsystem::BeginPlay()
{
	Super::BeginPlay();
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
}

#pragma region IFGSaveInterface

void AAutoSupportModSubsystem::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	LastAutoSupportData.ClearInvalidReferences();
}

void AAutoSupportModSubsystem::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
}

bool AAutoSupportModSubsystem::ShouldSave_Implementation() const
{
	return true;
}

#pragma endregion

#pragma region Auto Support Presets

FBuildableAutoSupportData AAutoSupportModSubsystem::GetLastAutoSupportData() const
{
	return LastAutoSupportData;
}

FBuildableAutoSupportData AAutoSupportModSubsystem::GetLastAutoSupport1mData() const
{
	return LastAutoSupport1mData;
}

FBuildableAutoSupportData AAutoSupportModSubsystem::GetLastAutoSupport2mData() const
{
	return LastAutoSupport2mData;
}

FBuildableAutoSupportData AAutoSupportModSubsystem::GetLastAutoSupport4mData() const
{
	return LastAutoSupport4mData;
}

void AAutoSupportModSubsystem::SetLastAutoSupport1mData(const FBuildableAutoSupportData& Data)
{
	LastAutoSupport1mData = Data;
	LastAutoSupportData = Data;
}

void AAutoSupportModSubsystem::SetLastAutoSupport2mData(const FBuildableAutoSupportData& Data)
{
	LastAutoSupport2mData = Data;
	LastAutoSupportData = Data;
}

void AAutoSupportModSubsystem::SetLastAutoSupport4mData(const FBuildableAutoSupportData& Data)
{
	LastAutoSupport4mData = Data;
	LastAutoSupportData = Data;
}

void AAutoSupportModSubsystem::SetSelectedAutoSupportPresetName(FString PresetName)
{
	SelectedAutoSupportPresetName = PresetName;
}

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

void AAutoSupportModSubsystem::SaveAutoSupportPreset(FString PresetName, FBuildableAutoSupportData Data)
{
	AutoSupportPresets.Add(PresetName, Data);
}

void AAutoSupportModSubsystem::DeleteAutoSupportPreset(FString PresetName)
{
	AutoSupportPresets.Remove(PresetName);

	if (PresetName.Equals(SelectedAutoSupportPresetName))
	{
		SelectedAutoSupportPresetName = TEXT("");
	}
}

#pragma endregion

void AAutoSupportModSubsystem::RegisterProxy(ABuildableAutoSupportProxy* Proxy)
{
	AllProxies.Add(Proxy);

	if (const auto* GameInstance = GetWorld()->GetGameInstance())
	{
		for (const auto* LocalPlayer : GameInstance->GetLocalPlayers())
		{
			if (LocalPlayer)
			{
				const auto* ModLocalPlayerSubsys = LocalPlayer->GetSubsystem<UAutoSupportModLocalPlayerSubsystem>();
				ModLocalPlayerSubsys->SyncProxyWithBuildGunState(Proxy);
			}
		}
	}
}

void AAutoSupportModSubsystem::RegisterHandleToProxyLink(const FAutoSupportBuildableHandle& Handle, ABuildableAutoSupportProxy* Proxy)
{
	fgcheck(Proxy);
	MOD_LOG(Verbose, TEXT("Registering handle [%s] to proxy [%s]"), TEXT_STR(Handle.ToString()), TEXT_STR(Proxy->GetName()))
	
	if (ProxyByBuildable.Contains(Handle))
	{
		checkf(false, TEXT("Handle already exists! [%s]"), TEXT_STR(Handle.ToString()))
	}
	
	ProxyByBuildable.Add(Handle, Proxy);
}
