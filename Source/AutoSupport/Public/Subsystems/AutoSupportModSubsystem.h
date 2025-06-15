// 

#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Buildables/BuildableAutoSupport_Types.h"
#include "SML/Public/Subsystem/ModSubsystem.h"
#include "AutoSupportModSubsystem.generated.h"

UCLASS(Blueprintable)
class AUTOSUPPORT_API AAutoSupportModSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static bool IsValidAutoSupportPresetName(FString PresetName, FString& OutName, FText& OutError);
	
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual bool ShouldSave_Implementation() const override;

	UFUNCTION(BlueprintCallable)
	void UpdateLastAutoSupportData(FBuildableAutoSupportData Data);

	UFUNCTION(BlueprintCallable)
	void GetAutoSupportPresetNames(TArray<FString>& OutNames) const;

	UFUNCTION(BlueprintCallable)
	bool GetAutoSupportPreset(FString PresetName, FBuildableAutoSupportData& OutData);

	UFUNCTION(BlueprintCallable)
	void SaveAutoSupportPreset(FString PresetName, FBuildableAutoSupportData Data);

	UFUNCTION(BlueprintCallable)
	void DeleteAutoSupportPreset(FString PresetName);

	UFUNCTION(BlueprintCallable)
	void RenameAutoSupportPreset(FString CurrentName, FString NewName);

	UFUNCTION(BlueprintCallable)
	void DuplicateAutoSupportPreset(FString PresetName, FString NewName);

protected:
	/**
	 * The last configuration used. Will preconfigure new placements.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	FBuildableAutoSupportData LastAutoSupportData;

	/**
	 * The selected preset in the interactable UI. Persists between opens. Note that this is just the selection and does not affect support configuration.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, SaveGame)
	FString SelectedAutoSupportPresetName;

	/**
	 * The player saved presets.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	TMap<FString, FBuildableAutoSupportData> AutoSupportPresets;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
