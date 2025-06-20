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
	 * The direction to build the supports relative to the auto support buildable actor origin.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	EAutoSupportBuildDirection BuildDirection = EAutoSupportBuildDirection::Top;
	
	/**
	 * The starting part descriptor for the auto support. This is the part that will be built first, relative to this actor.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TSoftClassPtr<UFGBuildingDescriptor> StartPartDescriptor;

	/**
	 * The starting part orientation.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	EAutoSupportBuildDirection StartPartOrientation = EAutoSupportBuildDirection::Bottom;

	/**
	 * The middle part descriptor for the auto support. This is the part that will be built in the middle of the support in a repeating pattern.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TSoftClassPtr<UFGBuildingDescriptor> MiddlePartDescriptor;

	/**
	 * The middle part orientation.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	EAutoSupportBuildDirection MiddlePartOrientation = EAutoSupportBuildDirection::Bottom;

	/**
	 * The starting part descriptor for the auto support. This is the part that will be built last (on the ground in downwards build), relative to this actor.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TSoftClassPtr<UFGBuildingDescriptor> EndPartDescriptor;

	/**
	 * The end part orientation.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	EAutoSupportBuildDirection EndPartOrientation = EAutoSupportBuildDirection::Top;

	/**
	 * Set to true to only consider the terrain for the support collision trace.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool OnlyIntersectTerrain;

	void ClearInvalidReferences()
	{
		if (!StartPartDescriptor.IsValid())
		{
			StartPartDescriptor = nullptr;
		}

		if (!MiddlePartDescriptor.IsValid())
		{
			MiddlePartDescriptor = nullptr;
		}

		if (!EndPartDescriptor.IsValid())
		{
			EndPartDescriptor = nullptr;
		}
	}

	FORCEINLINE friend FArchive& operator<<(FArchive& Ar, FBuildableAutoSupportData& Data)
	{
		Ar << Data.BuildDirection;
		Ar << Data.StartPartDescriptor;
		Ar << Data.StartPartOrientation;
		Ar << Data.MiddlePartDescriptor;
		Ar << Data.MiddlePartOrientation;
		Ar << Data.EndPartDescriptor;
		Ar << Data.EndPartOrientation;
		Ar << Data.OnlyIntersectTerrain;
		
		return Ar;
	}
};

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportTraceResult
{
	GENERATED_BODY()
	
	UPROPERTY()
	float BuildDistance = 0;

	UPROPERTY()
	bool IsLandscapeHit = false;

	UPROPERTY()
	FVector StartLocation;

	UPROPERTY()
	FVector Direction = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportBuildPlan
{
	GENERATED_BODY()

	/**
	 * The amount of each part to build.
	 */
	UPROPERTY()
	FVector PartCounts;

	/**
	 * The BBox data for the start part.
	 */
	UPROPERTY()
	FBox StartBox;

	/**
	 * The BBox data for the mid part.
	 */
	UPROPERTY()
	FBox MidBox;

	/**
	 * The BBox data for the end part.
	 */
	UPROPERTY()
	FBox EndBox;

	/**
	 * The distance from the trace origin to build the end part so it's flush with the intersected surface or buried enough in terrain.
	 */
	UPROPERTY()
	float EndPartBuildDistance;
};
