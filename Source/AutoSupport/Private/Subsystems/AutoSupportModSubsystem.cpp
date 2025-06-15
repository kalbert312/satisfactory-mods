// 

#include "AutoSupportModSubsystem.h"

#pragma region IFGSaveInterface

void AAutoSupportModSubsystem::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	LastAutoSupportData.ClearInvalidReferences();

	if (!SelectedAutoSupportPresetName.IsEmpty() && !AutoSupportPresets.Contains(SelectedAutoSupportPresetName))
	{
		SelectedAutoSupportPresetName = TEXT("");
	}
}

void AAutoSupportModSubsystem::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	if (!SelectedAutoSupportPresetName.IsEmpty() && !AutoSupportPresets.Contains(SelectedAutoSupportPresetName))
	{
		SelectedAutoSupportPresetName = TEXT("");
	}
}

bool AAutoSupportModSubsystem::ShouldSave_Implementation() const
{
	return true;
}

#pragma endregion

#pragma region Auto Support Presets

void AAutoSupportModSubsystem::UpdateLastAutoSupportData(const FBuildableAutoSupportData Data)
{
	LastAutoSupportData = Data;
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

void AAutoSupportModSubsystem::RenameAutoSupportPreset(FString CurrentName, FString NewName)
{
	const auto PresetEntry = AutoSupportPresets.Find(CurrentName);
	if (!PresetEntry)
	{
		return;
	}
	
	DeleteAutoSupportPreset(CurrentName);
	AutoSupportPresets.Add(NewName, *PresetEntry);
}

void AAutoSupportModSubsystem::DuplicateAutoSupportPreset(FString PresetName, FString NewName)
{
	const auto PresetEntry = AutoSupportPresets.Find(PresetName);
	if (!PresetEntry)
	{
		return;
	}
	
	AutoSupportPresets.Add(NewName, *PresetEntry);
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

#pragma endregion

// Called when the game starts or when spawned
void AAutoSupportModSubsystem::BeginPlay()
{
	Super::BeginPlay();
}

