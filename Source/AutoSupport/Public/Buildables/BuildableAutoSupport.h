// 

#pragma once

#include <ostream>

#include "CoreMinimal.h"
#include "FGBuildable.h"
#include "BuildableAutoSupport.generated.h"

class UFGBuildingDescriptor;

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FBuildableAutoSupportData
{
	GENERATED_BODY()

	/**
	 * The starting part descriptor for the auto support. This is the part that will be built first, relative to this actor.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TSoftClassPtr<UFGBuildingDescriptor> StartPartDescriptor;

	/**
	 * The middle part descriptor for the auto support. This is the part that will be built in the middle of the support in a repeating pattern.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TSoftClassPtr<UFGBuildingDescriptor> MiddlePartDescriptor;

	/**
	 * The starting part descriptor for the auto support. This is the part that will be built last (on the ground in downwards build), relative to this actor.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TSoftClassPtr<UFGBuildingDescriptor> EndPartDescriptor;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FBuildableAutoSupportData& Data)
	{
		Ar << Data.StartPartDescriptor;
		Ar << Data.MiddlePartDescriptor;
		Ar << Data.EndPartDescriptor;
		
		return Ar;
	}
};

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupport : public AFGBuildable
{
	GENERATED_BODY()

public:
	static constexpr int MaxBuildDistance = 400 * 50; // 50 4m foundations
	
	ABuildableAutoSupport();

	UPROPERTY(BlueprintReadWrite, SaveGame)
	FBuildableAutoSupportData AutoSupportData;
	
	/**
	 * Builds the supports based on the provided configuration.
	 */
	UFUNCTION(BlueprintCallable)
	void BuildSupports();

	virtual void GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects) override;
	virtual bool NeedTransform_Implementation() override;
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual bool ShouldSave_Implementation() const override;

protected:
	/**
	 * The buildable mesh proxy.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buildable Components")
	TObjectPtr<UFGColoredInstanceMeshProxy> InstancedMeshProxy;
	
	/**
	 * Traces to the terrain.
	 * @return The free distance.
	 */
	float Trace() const;

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

	void BuildParts(
		AFGBuildableSubsystem* Buildables,
		const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor,
		int32 Count,
		const FVector& Size,
		FTransform& WorkingTransform);

	static void GetBuildableSize(const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor, OUT FBox& OutBox);
};
