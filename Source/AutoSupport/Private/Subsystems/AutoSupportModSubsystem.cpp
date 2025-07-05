// 

#include "AutoSupportModSubsystem.h"

#include "AutoSupportGameWorldModule.h"
#include "AutoSupportModLocalPlayerSubsystem.h"
#include "BuildableAutoSupportProxy.h"
#include "ModConstants.h"
#include "ModLogging.h"
#include "WorldModuleManager.h"
#include "Subsystem/SubsystemActorManager.h"

AAutoSupportModSubsystem* AAutoSupportModSubsystem::Get(const UWorld* World)
{
	const auto* WorldModuleManager = World->GetSubsystem<UWorldModuleManager>();
	auto* WorldModule = CastChecked<UGameWorldModule>(WorldModuleManager->FindModule(AutoSupportConstants::ModReference));

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
	return CastChecked<AAutoSupportModSubsystem>(SubsystemActorManager->K2_GetSubsystemActor(ImplClass));
}

void AAutoSupportModSubsystem::Init()
{
	Super::Init();

	auto* World = GetWorld();
	
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
	MOD_LOG(Verbose, TEXT("Invoked"))

	const FAutoSupportBuildableHandle Handle(Buildable);
	check(Handle.IsDataValid());
	auto ProxyPtr = ProxyByBuildable.FindRef(Handle);

	if (!ProxyPtr.IsValid())
	{
		// MOD_LOG(Verbose, TEXT("No proxy found. IsNull: [%s], IsValid: [%s]"), TEXT_BOOL(ProxyPtr.IsExplicitlyNull()), TEXT_BOOL(ProxyPtr.IsValid()))
		// MOD_LOG(Verbose, TEXT("Lookup handle: [%s]"), TEXT_STR(Handle.ToString()))
		//
		// for (const auto& Entry : ProxyByBuildable)
		// {
		// 	MOD_LOG(Verbose, TEXT("Existing registered handle: [%s]"), TEXT_STR(Entry.Key.ToString()))
		// }
		
		return;
	}
	
	MOD_LOG(Verbose, TEXT("Found proxy. Removing handle and unregistering buildable."))
	const auto Removed = ProxyByBuildable.Remove(Handle);
	check(Removed == 1) // Check the equality contract is working as intended.
	ProxyPtr->UnregisterBuildable(Buildable);
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
	check(Proxy);
	MOD_LOG(Verbose, TEXT("Registering handle [%s] to proxy [%s]"), TEXT_STR(Handle.ToString()), TEXT_STR(Proxy->GetName()))
	
	if (ProxyByBuildable.Contains(Handle))
	{
		checkf(false, TEXT("Handle already exists! [%s]"), TEXT_STR(Handle.ToString()))
	}
	
	ProxyByBuildable.Add(Handle, Proxy);
}
