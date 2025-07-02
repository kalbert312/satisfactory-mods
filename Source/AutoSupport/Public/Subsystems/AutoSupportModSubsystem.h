// 

#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Common/ModTypes.h"
#include "Buildables/BuildableAutoSupport_Types.h"
#include "SML/Public/Subsystem/ModSubsystem.h"
#include "AutoSupportModSubsystem.generated.h"

class ABuildableAutoSupportProxy;

UCLASS(Blueprintable)
class AUTOSUPPORT_API AAutoSupportModSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

	friend class UAutoSupportModLocalPlayerSubsystem;
	
public:
	static AAutoSupportModSubsystem* Get(const UWorld* World);
	
	UFUNCTION(BlueprintCallable, BlueprintPure)
	static bool IsValidAutoSupportPresetName(FString PresetName, FString& OutName, FText& OutError);

	void RegisterProxy(ABuildableAutoSupportProxy* Proxy);
	void RegisterHandleToProxyLink(const FAutoSupportBuildableHandle& Handle, ABuildableAutoSupportProxy* Proxy);
	
	void OnProxyDestroyed(const ABuildableAutoSupportProxy* Proxy);

	UFUNCTION()
	void OnWorldBuildableRemoved(AFGBuildable* Buildable);
	
#pragma region IFGSaveInterface
	
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual bool ShouldSave_Implementation() const override;
	
#pragma endregion

#pragma region Auto Support Presets
	
	UFUNCTION(BlueprintCallable)
	FBuildableAutoSupportData GetLastAutoSupportData() const;
	
	UFUNCTION(BlueprintCallable)
	void SetLastAutoSupportData(const FBuildableAutoSupportData& Data);

	UFUNCTION(BlueprintCallable)
	void SetSelectedAutoSupportPresetName(FString PresetName);
	
	UFUNCTION(BlueprintCallable)
	void GetAutoSupportPresetNames(TArray<FString>& OutNames) const;

	UFUNCTION(BlueprintCallable)
	bool GetAutoSupportPreset(FString PresetName, FBuildableAutoSupportData& OutData);

	UFUNCTION(BlueprintCallable)
	void SaveAutoSupportPreset(FString PresetName, FBuildableAutoSupportData Data);

	UFUNCTION(BlueprintCallable)
	void DeleteAutoSupportPreset(FString PresetName);

#pragma endregion

protected:
	/**
	 * The last configuration used. Will preconfigure new placements.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	FBuildableAutoSupportData LastAutoSupportData;

	/**
	 * The selected preset in the interactable UI. Persists between opens. Note that this is just the selection and does not affect support configuration.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, SaveGame)
	FString SelectedAutoSupportPresetName;

	/**
	 * The player saved presets.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	TMap<FString, FBuildableAutoSupportData> AutoSupportPresets;

	/**
	 * This is used to respond to deletion events on the buildables and notify the proxy a buildable it contains has been destroyed.
	 */
	UPROPERTY(Transient)
	TMap<FAutoSupportBuildableHandle, TWeakObjectPtr<ABuildableAutoSupportProxy>> ProxyByBuildable;

	/**
	 * All the world proxies.
	 */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<ABuildableAutoSupportProxy>> AllProxies;
	
	virtual void Init() override;

};
