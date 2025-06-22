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
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector StartRelativeLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator StartRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector Direction = FVector::ZeroVector;

	UPROPERTY()
	EAutoSupportBuildDirection BuildDirection = EAutoSupportBuildDirection::Top;

	UPROPERTY()
	FHitResult EndHitResult = FHitResult(ForceInit);
};

USTRUCT(BlueprintType)
struct FAutoSupportBuildPlanPartData
{
	GENERATED_BODY()

	/**
	 * The part's descriptor.
	 */
	UPROPERTY()
	TSubclassOf<UFGBuildingDescriptor> PartDescriptorClass = nullptr;

	/**
	 * The class of the part.
	 */
	UPROPERTY()
	TSubclassOf<AFGBuildable> BuildableClass = nullptr;

	/**
	 * The recipe used when building.
	 */
	UPROPERTY()
	TSubclassOf<UFGRecipe> BuildRecipeClass = nullptr;
	
	/**
	 * The amount of it to build.
	 */
	UPROPERTY()
	int Count = 0;
	
	/**
	 * The BBox data for the start part.
	 */
	UPROPERTY()
	FBox BBox = FBox(ForceInit);

	/**
	 * The build position offset.
	 */
	UPROPERTY()
	FVector BuildPositionOffset = FVector::ZeroVector;
	
	/**
	 * Positional offset applied before and after part rotation.
	 */
	UPROPERTY()
	FVector RotationalPositionOffset = FVector::ZeroVector;
	
	/**
	 * Relative rotation to apply to part.
	 */
	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;

	/**
	 * World direction aware next part position offset after part is built.
	 */
	UPROPERTY()
	FVector AfterPartPositionOffset = FVector::ZeroVector;

	FORCEINLINE bool IsUnspecified() const
	{
		return PartDescriptorClass == nullptr;
	}

	FORCEINLINE bool IsActionable() const
	{
		return Count > 0 && PartDescriptorClass && BuildableClass && BuildRecipeClass;
	}
};

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportBuildPlan
{
	GENERATED_BODY()

	/**
	 * The relative translation to apply to a copy of the actor transform to building.
	 */
	UPROPERTY()
	FVector RelativeLocation = FVector::ZeroVector;

	/**
	 * The relative rotation to apply to a copy of the actor transform to begin building.
	 */
	UPROPERTY()
	FRotator RelativeRotation = FRotator::ZeroRotator;
	
	/**
	 * Plan for start parts.
	 */
	UPROPERTY()
	FAutoSupportBuildPlanPartData StartPart;

	/**
	 * Plan for mid parts.
	 */
	UPROPERTY()
	FAutoSupportBuildPlanPartData MidPart;

	/**
	 * Plan for end parts.
	 */
	UPROPERTY()
	FAutoSupportBuildPlanPartData EndPart;
	
	/**
	 * The world direction-aware position offset to build the end part at to make it seated correctly.
	 */
	UPROPERTY()
	FVector EndPartPositionOffset = FVector::ZeroVector;
	
	FORCEINLINE bool IsActionable() const
	{
		if (MidPart.IsUnspecified() && StartPart.IsUnspecified() && EndPart.IsUnspecified())
		{
			return false;
		}
		
		return (MidPart.IsUnspecified() || MidPart.IsActionable()) && (StartPart.IsUnspecified() || StartPart.IsActionable()) && (EndPart.IsUnspecified() || EndPart.IsActionable());
	}
};

