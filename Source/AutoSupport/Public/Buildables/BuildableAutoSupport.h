// 

#pragma once

#include "CoreMinimal.h"
#include "FGBuildable.h"
#include "BuildableAutoSupport.generated.h"

class UFGBuildingDescriptor;

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupport : public AFGBuildable
{
	GENERATED_BODY()

public:
	ABuildableAutoSupport();

	/**
	 * The
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Auto Supports")
	TSubclassOf<UFGBuildingDescriptor> StartPartDescriptor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Auto Supports")
	TSubclassOf<UFGBuildingDescriptor> MiddlePartDescriptor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Auto Supports")
	TSubclassOf<UFGBuildingDescriptor> EndPartDescriptor;

	/**
	 * The maximum distance the auto support can build.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Auto Supports")
	float MaxBuildDistance = 10000.0f;

	/**
	 * Builds the supports based on the provided configuration.
	 */
	UFUNCTION(BlueprintCallable)
	void BuildSupports();
	
protected:
	/**
	 * The buildable mesh proxy.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buildable Components")
	TObjectPtr<UFGColoredInstanceMeshProxy> InstancedMeshProxy;
	
	/**
	 * Traces to the terrian.
	 * @return The free distance.
	 */
	float Trace();

	/**
	 * 
	 * @param BuildDistance The distance to build.
	 * @param OutPartCounts The output part counts. X is for the start part, Y is for the middle part, Z is for the end part.
	 * @param OutStartBox
	 * @param OutMidBox
	 * @param OutEndBox
	 */
	void PlanBuild(float BuildDistance, OUT FVector& OutPartCounts, FBox& OutStartBox, FBox& OutMidBox, FBox& OutEndBox) const;

	bool IsBuildable(const FVector& PartCounts) const;

	void BuildParts(AFGBuildableSubsystem* Buildables, TSubclassOf<UFGBuildingDescriptor> PartDescriptor, int32 Count, const FVector& Size, FTransform&
		WorkingTransform);

	static void GetBuildableSize(TSubclassOf<UFGBuildingDescriptor> PartDescriptor, OUT FBox& OutBox);
};
