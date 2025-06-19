// 

#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupport_Types.h"
#include "FGBuildable.h"
#include "BuildableAutoSupport.generated.h"

class UFGBuildingDescriptor;

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupport : public AFGBuildable
{
	GENERATED_BODY()

public:
	static constexpr int MaxBuildDistance = 400 * 50; // 50 4m foundations
	static constexpr int MaxPartHeight = 800; // Avoid anything larger.
	
	ABuildableAutoSupport();

	UPROPERTY(BlueprintReadWrite, SaveGame)
	FBuildableAutoSupportData AutoSupportData;
	
	/**
	 * Builds the supports based on the provided configuration.
	 */
	UFUNCTION(BlueprintCallable)
	void BuildSupports();

#pragma region IFGSaveInterface
	
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual bool ShouldSave_Implementation() const override;

#pragma endregion

	virtual void BeginPlay() override;

protected:
	/**
	 * The buildable mesh proxy.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buildable Components")
	TObjectPtr<UFGColoredInstanceMeshProxy> InstancedMeshProxy;

	/**
	 * Set this to true to autoconfigure the auto support to the last configuration used. Autoconfiguration happens at BeginPlay and only occurs once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Auto Support")
	bool bAutoConfigure = true;
	
	void AutoConfigure();

	UFUNCTION(BlueprintImplementableEvent, Category = "Auto Support")
	void K2_AutoConfigure();
	
	/**
	 * Traces to the terrain.
	 * @return The free distance.
	 */
	FAutoSupportTraceResult Trace() const;

	/**
	 * Plans the build. Determines cost and provides some build dimensions.
	 * @param TraceResult The trace result.
	 * @param OutPlan The output build plan.
	 */
	void PlanBuild(const FAutoSupportTraceResult& TraceResult, OUT FAutoSupportBuildPlan& OutPlan) const;

	/**
	 * Determines if we can build the given build plan.
	 * @param Plan
	 * @return Can we build this?
	 */
	bool IsBuildable(const FAutoSupportBuildPlan& Plan) const;

	/**
	 * Builds the parts using the working transform as a moving spawn point.
	 * @param Buildables The buildables subsystem.
	 * @param PartDescriptor The part descriptor for the part to build.
	 * @param PartOrientation
	 * @param PartOrientation
	 * @param Count How many to build.
	 * @param Size The size of the part. This is used to modify the working transform.
	 * @param Direction
	 * @param WorkingTransform The spawn point of the part.
	 */
	void BuildParts(
		AFGBuildableSubsystem* Buildables,
		const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor,
		EAutoSupportBuildDirection PartOrientation,
		int32 Count,
		const FVector& Size,
		const FVector& Direction,
		FTransform& WorkingTransform);

	static void GetBuildableClearance(const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor, OUT FBox& OutBox);
};

UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportClipboardSettings : public UFGFactoryClipboardSettings
{
	GENERATED_BODY()

	UPROPERTY()
	FBuildableAutoSupportData AutoSupportData;
};
