// 

#include "AutoSupportModSubsystem.h"

#include "AutoSupportGameWorldModule.h"
#include "ModConstants.h"
#include "WorldModuleManager.h"
#include "Subsystem/SubsystemActorManager.h"

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

void AAutoSupportModSubsystem::SetLastAutoSupportData(const FBuildableAutoSupportData& Data)
{
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

AAutoSupportModSubsystem* AAutoSupportModSubsystem::Get(const UWorld* World)
{
	auto* WorldModuleManager = World->GetSubsystem<UWorldModuleManager>();
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

bool AAutoSupportModSubsystem::IsValidAutoSupportPresetName(FString PresetName, OUT FString& OutName, OUT FText& OutError)
{
	PresetName = PresetName.TrimStartAndEnd();
	OutName = PresetName;
	
	if (PresetName.IsEmpty())
	{
		OutError = FText::FromString("Preset name cannot be empty");
		return false;
	}

	if (PresetName.Len() > 32)
	{
		OutError = FText::FromString("Preset name cannot be longer than 32 characters");
		return false;
	}

	return true;
}
