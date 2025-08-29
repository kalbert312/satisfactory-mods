// 

#pragma once

#include "CoreMinimal.h"
#include "FGSaveInterface.h"
#include "Common/ModTypes.h"
#include "Buildables/BuildableAutoSupport_Types.h"
#include "SML/Public/Subsystem/ModSubsystem.h"
#include "AutoSupportModSubsystem.generated.h"

class UAutoSupportBuildConfig;
class ABuildableAutoSupportProxy;

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API AAutoSupportModSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

	friend class UAutoSupportModLocalPlayerSubsystem;
	
public:
	static AAutoSupportModSubsystem* Get(const UWorld* World);

	AAutoSupportModSubsystem();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void RegisterProxy(ABuildableAutoSupportProxy* Proxy);
	void RegisterHandleToProxyLink(const FAutoSupportBuildableHandle& Handle, ABuildableAutoSupportProxy* Proxy);
	
	void OnProxyDestroyed(const ABuildableAutoSupportProxy* Proxy);

	UFUNCTION()
	void OnWorldBuildableRemoved(AFGBuildable* Buildable);
	
	virtual bool ShouldSave_Implementation() const override;

#pragma region Auto Support Presets
	
	UFUNCTION(BlueprintCallable)
	void GetAutoSupportPresetNames(TArray<FString>& OutNames) const;

	UFUNCTION(BlueprintCallable)
	bool GetAutoSupportPreset(FString PresetName, FBuildableAutoSupportData& OutData);

	UFUNCTION(BlueprintCallable)
	void SaveAutoSupportPreset(FString PresetName, FBuildableAutoSupportData Data);

	UFUNCTION(BlueprintCallable)
	void DeleteAutoSupportPreset(FString PresetName);

	UFUNCTION(BlueprintCallable)
	bool IsExistingAutoSupportPreset(FString PresetName) const;

#pragma endregion

protected:
	/**
	 * The player saved presets.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	TMap<FString, FBuildableAutoSupportData> AutoSupportPresets;

	/**
	 * The shared player saved presets.
	 */
	UPROPERTY(ReplicatedUsing = OnRep_AutoSupportPresets)
	TArray<FAutoSupportPresetNameAndDataKvp> ReplicatedAutoSupportPresets;
	
	/**
	 * All the world proxies.
	 */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<ABuildableAutoSupportProxy>> AllProxies;

	/**
	 * The replicated proxies array (because TSets don't replicate).
	 */
	UPROPERTY(ReplicatedUsing=OnRep_AllProxies)
	TArray<TWeakObjectPtr<ABuildableAutoSupportProxy>> ReplicatedAllProxies;

	/**
	 * This is used to respond to deletion events on the buildables and notify the proxy a buildable it contains has been destroyed.
	 */
	UPROPERTY(Transient)
	TMap<FAutoSupportBuildableHandle, TWeakObjectPtr<ABuildableAutoSupportProxy>> ProxyByBuildable;

	void SyncProxiesWithBuildMode(const TArray<TWeakObjectPtr<ABuildableAutoSupportProxy>>& Proxies) const;
	
	UFUNCTION()
	void OnRep_AutoSupportPresets();

	UFUNCTION()
	void OnRep_AllProxies();

	
	virtual void Init() override;

	static TMap<TWeakObjectPtr<const UWorld>, TWeakObjectPtr<AAutoSupportModSubsystem>> CachedSubsystemLookup;
	static FCriticalSection CachedSubsystemLookupLock;
};

USTRUCT()
struct AUTOSUPPORT_API FAutoSupportBuildableHandleProxyKvp
{
	GENERATED_BODY()

	UPROPERTY()
	FAutoSupportBuildableHandle Handle;

	UPROPERTY()
	TWeakObjectPtr<ABuildableAutoSupportProxy> Proxy;
};

#pragma region Remote Call Objects

UCLASS()
class AUTOSUPPORT_API UAutoSupportSubsystemRCO : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void SaveAutoSupportPreset(AAutoSupportModSubsystem* Subsystem, const FString& PresetName, const FBuildableAutoSupportData& Data);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void DeleteAutoSupportPreset(AAutoSupportModSubsystem* Subsystem, const FString& PresetName);

private:
	UPROPERTY(Replicated, Meta = (NoAutoJson))
	bool mForceNetField_UAutoSupportSubsystemRCO = false;
};

#pragma endregion
