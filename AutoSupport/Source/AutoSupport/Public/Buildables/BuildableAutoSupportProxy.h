// 

#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupport_Types.h"
#include "FGBuildable.h"
#include "FGDismantleInterface.h"
#include "FGLightweightBuildableSubsystem.h"
#include "ModTypes.h"
#include "GameFramework/Actor.h"
#include "BuildableAutoSupportProxy.generated.h"

class UFGBuildGunModeDescriptor;
class AFGBuildable;
class UBoxComponent;

/**
 * Like AFGBlueprintProxy but for auto supports.
 * Blueprint needs to set up collider with build gun.
 */
UCLASS(Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupportProxy : public AActor, public IFGDismantleInterface, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	ABuildableAutoSupportProxy();

	/**
	 * Transient flag that must be set to true when newly spawning.
	 */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Auto Support")
	bool bIsNewlySpawned = false;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	void RegisterBuildable(AFGBuildable* Buildable);
	void UnregisterBuildable(AFGBuildable* Buildable);

	void UpdateBoundingBox(const FBox& NewBounds);
	UFUNCTION(BlueprintImplementableEvent)
	void K2_UpdateBoundingBox(const FBox& NewBounds);
	
	void OnBuildModeUpdate(TSubclassOf<UFGBuildGunModeDescriptor> BuildMode, ULocalPlayer* LocalPlayer);
	UFUNCTION(BlueprintImplementableEvent)
	void K2_OnBuildModeUpdate(TSubclassOf<UFGBuildGunModeDescriptor> BuildMode, ULocalPlayer* LocalPlayer);
	
	bool DestroyIfEmpty(bool bRemoveInvalidHandles);

#pragma region IFGSaveInterface
	
	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override;
	virtual bool NeedTransform_Implementation() override;
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual bool ShouldSave_Implementation() const override;

#pragma endregion

#pragma region IFGDismantleInterface
	
	virtual bool CanDismantle_Implementation() const override;
	virtual void Dismantle_Implementation() override;
	virtual void GetChildDismantleActors_Implementation(TArray<AActor*>& out_ChildDismantleActors) const override;
	virtual void GetDismantleDependencies_Implementation(TArray<AActor*>& out_dismantleDependencies) const override;
	virtual void GetDismantleDisqualifiers_Implementation(
		TArray<TSubclassOf<UFGConstructDisqualifier>>& out_dismantleDisqualifiers,
		const TArray<AActor*>& allSelectedActors) const override;
	virtual void GetDismantleRefund_Implementation(TArray<FInventoryStack>& out_refund, bool noBuildCostEnabled) const override;
	virtual FVector GetRefundSpawnLocationAndArea_Implementation(const FVector& aimHitLocation, float& out_radius) const override;
	virtual void PreUpgrade_Implementation() override;
	virtual bool ShouldBlockDismantleSample_Implementation() const override;
	virtual bool SupportsDismantleDisqualifiers_Implementation() const override;
	virtual void Upgrade_Implementation(AActor* newActor) override;
	virtual FText GetDismantleDisplayName_Implementation(AFGCharacterPlayer* byCharacter) const override;
	virtual void StartIsLookedAtForDismantle_Implementation(AFGCharacterPlayer* byCharacter) override;
	virtual void StopIsLookedAtForDismantle_Implementation(AFGCharacterPlayer* byCharacter) override;

#pragma endregion

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Auto Support")
	TObjectPtr<UBoxComponent> BoundingBoxComponent;
	
	/**
	 * The registered buildable handles.
	 */
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing=OnRep_RegisteredHandles, SaveGame, Category = "Auto Support")
	TArray<FAutoSupportBuildableHandle> RegisteredHandles;

	/**
	 * The bounding box of the buildable.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_BoundingBox, SaveGame, Category = "Auto Support")
	FBox BoundingBox;
	
	UPROPERTY(Transient)
	uint8 HoveredForDismantleCount = 0;

	/**
	 * Transient flag that tracks when all buildables & temporaries are available.
	 */
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Auto Support")
	bool bBuildablesAvailable = false;
	
	UPROPERTY(Transient, Replicated)
	bool bIsLoadLightweightTraceInProgress = false;

	UPROPERTY(Transient)
	bool bIsClientLightweightTraceInProgress = false;

	UPROPERTY(Transient)
	uint8 LightweightTraceRetryCount = 0; 

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "Auto Support")
	TMap<FAutoSupportBuildableHandle, FLightweightBuildableInstanceRef> LightweightRefsByHandle;
	
	FOverlapDelegate LoadTraceDelegate;
	FTraceHandle CurrentTraceHandle;
	FTimerHandle LightweightTraceRetryHandle;

	virtual void BeginPlay() override;
	void BeginPlay_Server();
	
	void EnsureBuildablesAvailable();
	void RemoveTemporaries(const AFGCharacterPlayer* Player);
	void RemoveInvalidHandles();
	void RegisterSelfAndHandlesWithSubsystem();

	void BeginLightweightTraceAndResetRetries();
	void BeginLightweightTraceRetry();
	void BeginLightweightTrace();
	void OnLightweightTraceComplete(const FTraceHandle& TraceHandle, FOverlapDatum& Datum);

	AFGBuildable* GetBuildableForHandle(const FAutoSupportBuildableHandle& Handle) const;

	UFUNCTION()
	void OnRep_BoundingBox();
	UFUNCTION()
	void OnRep_RegisteredHandles();
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
