// 

#pragma once

#include "CoreMinimal.h"
#include "FGBuildable.h"
#include "FGDismantleInterface.h"
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

	void RegisterBuildable(AFGBuildable* Buildable);
	void UnregisterBuildable(AFGBuildable* Buildable);

	void UpdateBoundingBox(const FBox& NewBounds);
	UFUNCTION(BlueprintImplementableEvent)
	void K2_UpdateBoundingBox(const FBox& NewBounds);
	
	void OnBuildModeUpdate(TSubclassOf<UFGBuildGunModeDescriptor> BuildMode, ULocalPlayer* LocalPlayer);
	UFUNCTION(BlueprintImplementableEvent)
	void K2_OnBuildModeUpdate(TSubclassOf<UFGBuildGunModeDescriptor> BuildMode, ULocalPlayer* LocalPlayer);

	bool DestroyIfEmpty();

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
	/**
	 * The registered buildable handles.
	 */
	UPROPERTY(VisibleInstanceOnly, Category = "Auto Support", SaveGame)
	TArray<FAutoSupportBuildableHandle> RegisteredHandles;

	UPROPERTY(VisibleInstanceOnly, Category = "Auto Support", SaveGame)
	bool bIsDismantled = false;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Auto Support")
	bool bIsHoveredForDismantle = false;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Auto Support")
	bool bBuildablesAvailable = false;

	virtual void BeginPlay() override;

	FORCEINLINE const FAutoSupportBuildableHandle* GetRootHandle() const
	{
		return RegisteredHandles.Num() > 0 ? &RegisteredHandles[0] : nullptr;
	}

	FORCEINLINE AFGBuildable* GetCheckedRootBuildable() const
	{
		auto* RootHandle = GetRootHandle();
		check(RootHandle);
		check(RootHandle->Buildable.IsValid());
		return RootHandle->Buildable.Get();
	}

	void EnsureBuildablesAvailable();
	void RemoveTemporaries(AFGCharacterPlayer* Player);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
