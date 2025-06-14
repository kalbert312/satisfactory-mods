#pragma once

#include "CoreMinimal.h"
#include "FGBuildingDescriptor.h"
#include "Common/ModTypes.h"
#include "BuildableAutoSupport_Types.generated.h"

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

	/**
	 * Set to true to only consider the terrain for the support collision trace.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool OnlyIntersectTerrain;

	/**
	 * The direction to build the supports relative to the auto support buildable actor origin.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	EAutoSupportBuildDirection BuildDirection = EAutoSupportBuildDirection::Down;

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FBuildableAutoSupportData& Data)
	{
		Ar << Data.StartPartDescriptor;
		Ar << Data.MiddlePartDescriptor;
		Ar << Data.EndPartDescriptor;
		Ar << Data.OnlyIntersectTerrain;
		Ar << Data.BuildDirection;
		
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct FAutoSupportTraceResult
{
	GENERATED_BODY()
	
	UPROPERTY()
	float BuildDistance = 0;

	UPROPERTY()
	bool IsLandscapeHit = false;

	UPROPERTY()
	FVector Direction = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FAutoSupportBuildPlan
{
	GENERATED_BODY()

	UPROPERTY()
	FVector PartCounts;

	UPROPERTY()
	FBox StartBox;

	UPROPERTY()
	FBox MidBox;

	UPROPERTY()
	FBox EndBox;
};