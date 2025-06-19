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
	 * The starting part orientation.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	EAutoSupportBuildDirection StartPartOrientation = EAutoSupportBuildDirection::Down;

	/**
	 * The middle part descriptor for the auto support. This is the part that will be built in the middle of the support in a repeating pattern.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TSoftClassPtr<UFGBuildingDescriptor> MiddlePartDescriptor;

	/**
	 * The middle part orientation.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	EAutoSupportBuildDirection MiddlePartOrientation = EAutoSupportBuildDirection::Down;

	/**
	 * The starting part descriptor for the auto support. This is the part that will be built last (on the ground in downwards build), relative to this actor.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	TSoftClassPtr<UFGBuildingDescriptor> EndPartDescriptor;

	/**
	 * The end part orientation.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	EAutoSupportBuildDirection EndPartOrientation = EAutoSupportBuildDirection::Down;

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
		Ar << Data.StartPartDescriptor;
		Ar << Data.MiddlePartDescriptor;
		Ar << Data.EndPartDescriptor;
		Ar << Data.OnlyIntersectTerrain;
		Ar << Data.BuildDirection;
		Ar << Data.StartPartOrientation;
		Ar << Data.MiddlePartOrientation;
		Ar << Data.EndPartOrientation;
		
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
	FVector Direction = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportBuildPlan
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
