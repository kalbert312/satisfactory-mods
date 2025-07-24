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
	 * The color customization to apply to the start part.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FFactoryCustomizationData StartPartCustomization;

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
	 * The customization to apply to the middle parts.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FFactoryCustomizationData MiddlePartCustomization;

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
	 * The customization to apply to the end part.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	FFactoryCustomizationData EndPartCustomization;

	/**
	 * The distance to bury the part into terrain.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	float EndPartTerrainBuryPercentage = 0.f;

	/**
	 * Set to true to only consider the terrain for the support collision trace.
	 */
	UPROPERTY(SaveGame, BlueprintReadWrite)
	bool OnlyIntersectTerrain = false;

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
		Ar << Data.EndPartTerrainBuryPercentage;
		Ar << Data.StartPartCustomization;
		Ar << Data.MiddlePartCustomization;
		Ar << Data.EndPartCustomization;
		
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
	FQuat StartRelativeRotation = FQuat::Identity;

	UPROPERTY()
	FVector Direction = FVector::ZeroVector;

	UPROPERTY()
	EAutoSupportBuildDirection BuildDirection = EAutoSupportBuildDirection::Top;

	UPROPERTY()
	FHitResult EndHitResult = FHitResult(ForceInit);

	UPROPERTY()
	TSubclassOf<UFGConstructDisqualifier> Disqualifier = nullptr;
};

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportBuildPlanPartData
{
	GENERATED_BODY()

	/**
	 * The part's descriptor.
	 */
	UPROPERTY(BlueprintReadWrite)
	TSubclassOf<UFGBuildingDescriptor> PartDescriptorClass = nullptr;

	/**
	 * The class of the part.
	 */
	UPROPERTY(BlueprintReadWrite)
	TSubclassOf<AFGBuildable> BuildableClass = nullptr;

	/**
	 * The recipe used when building.
	 */
	UPROPERTY(BlueprintReadWrite)
	TSubclassOf<UFGRecipe> BuildRecipeClass = nullptr;

	/**
	 * The customization to apply.
	 */
	UPROPERTY(BlueprintReadWrite)
	FFactoryCustomizationData CustomizationData;
	
	/**
	 * The amount of it to build.
	 */
	UPROPERTY(BlueprintReadWrite)
	int Count = 0;
	
	/**
	 * The BBox data for the part.
	 */
	UPROPERTY(BlueprintReadWrite)
	FBox BBox = FBox(ForceInit);

	/**
	 * The build location offset in part local space.
	 */
	UPROPERTY(BlueprintReadWrite)
	FVector LocalTranslation = FVector::ZeroVector;
	
	/**
	 * Local rotation to apply to part.
	 */
	UPROPERTY(BlueprintReadWrite)
	FQuat LocalRotation = FQuat::Identity;

	/**
	 * The build space that will be occupied by this part.
	 */
	UPROPERTY(BlueprintReadWrite)
	float ConsumedBuildSpace = 0;

	/**
	 * The configured part orientation.
	 */
	UPROPERTY(BlueprintReadWrite)
	EAutoSupportBuildDirection Orientation;
	
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
	 * The build direction relative to the cube.
	 */
	UPROPERTY(BlueprintReadWrite)
	EAutoSupportBuildDirection BuildDirection = EAutoSupportBuildDirection::Top;

	/**
	 * The relative translation to apply to a copy of the actor transform to building.
	 */
	UPROPERTY(BlueprintReadWrite)
	FVector RelativeLocation = FVector::ZeroVector;

	/**
	 * The relative rotation to apply to a copy of the actor transform to begin building.
	 */
	UPROPERTY(BlueprintReadWrite)
	FQuat RelativeRotation = FQuat::Identity;

	/**
	 * The world location to start building.
	 */
	UPROPERTY(BlueprintReadWrite)
	FVector StartWorldLocation = FVector::ZeroVector;
	
	/**
	 * The build world direction (world trace direction).
	 */
	UPROPERTY(BlueprintReadWrite)
	FVector BuildWorldDirection = FVector::ZeroVector;
	
	/**
	 * Plan for start parts.
	 */
	UPROPERTY(BlueprintReadWrite)
	FAutoSupportBuildPlanPartData StartPart;

	/**
	 * Plan for mid parts.
	 */
	UPROPERTY(BlueprintReadWrite)
	FAutoSupportBuildPlanPartData MidPart;

	/**
	 * Plan for end parts.
	 */
	UPROPERTY(BlueprintReadWrite)
	FAutoSupportBuildPlanPartData EndPart;
	
	/**
	 * The position offset to build the end part at to make it seated correctly.
	 */
	UPROPERTY(BlueprintReadWrite)
	float EndPartPositionOffset = 0.f;

	/**
	 * Reasons the plan cannot be built.
	 */
	UPROPERTY(BlueprintReadWrite)
	TArray<TSubclassOf<UFGConstructDisqualifier>> BuildDisqualifiers;

	/**
	 * The item bill for this plan.
	 */
	UPROPERTY(BlueprintReadWrite)
	TArray<FItemAmount> ItemBill;
	
	FORCEINLINE bool IsActionable() const
	{
		if (!BuildDisqualifiers.IsEmpty())
		{
			return false;
		}
		
		if (MidPart.IsUnspecified() && StartPart.IsUnspecified() && EndPart.IsUnspecified())
		{
			return false;
		}
		
		if ((MidPart.IsUnspecified() || MidPart.IsActionable()) && (StartPart.IsUnspecified() || StartPart.IsActionable()) && (EndPart.IsUnspecified() || EndPart.IsActionable()))
		{
			return true;
		}

		return false;
	}
};
