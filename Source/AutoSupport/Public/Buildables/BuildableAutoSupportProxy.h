// 

#pragma once

#include "CoreMinimal.h"
#include "FGBuildable.h"
#include "FGDismantleInterface.h"
#include "GameFramework/Actor.h"
#include "BuildableAutoSupportProxy.generated.h"

class AFGBuildable;
class UBoxComponent;

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportProxyBuildableHandle
{
	GENERATED_BODY()
	
	UPROPERTY(SaveGame)
	TWeakObjectPtr<AFGBuildable> Buildable;

	UPROPERTY(SaveGame)
	TSubclassOf<AFGBuildable> BuildableClass;

	UPROPERTY(SaveGame)
	int32 LightweightRuntimeIndex = -1;

	UPROPERTY(Transient)
	TWeakObjectPtr<AFGBuildable> Temporary;

	FORCEINLINE bool IsLightweightType() const
	{
		return LightweightRuntimeIndex >= 0;
	}

	FORCEINLINE bool HasBuildable() const
	{
		return Temporary.IsValid() || Buildable.IsValid();
	}

	FORCEINLINE bool IsValidHandle() const
	{
		return IsValid(BuildableClass) && (Buildable.IsValid() || LightweightRuntimeIndex >= 0);
	}

	FORCEINLINE AFGBuildable* GetBuildable() const
	{
		return Buildable.IsValid() ? Buildable.Get() : Temporary.IsValid() ? Temporary.Get() : nullptr;
	}
};

/**
 * Like AFGBlueprintProxy but for auto supports.
 */
UCLASS(Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupportProxy : public AActor, public IFGDismantleInterface, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	ABuildableAutoSupportProxy();

	void RegisterBuildable(AFGBuildable* Buildable);
	void UnregisterBuildable(AFGBuildable* Buildable);

	void CalculateBounds();

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto Support")
	TObjectPtr<UBoxComponent> BuildablesBoundingBox;

	/**
	 * The registered buildable handles. The first entry is considered the root buildable.
	 */
	UPROPERTY(VisibleInstanceOnly, Category = "Auto Support", SaveGame)
	TArray<FAutoSupportProxyBuildableHandle> RegisteredHandles;

	virtual void BeginPlay() override;

	const FAutoSupportProxyBuildableHandle* GetRootHandle() const;
	FAutoSupportProxyBuildableHandle* EnsureBuildablesAvailable();
	void RemoveTemporaries();
};
