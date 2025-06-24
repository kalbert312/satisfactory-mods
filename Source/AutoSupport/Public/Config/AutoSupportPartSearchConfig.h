#pragma once

#include "CoreMinimal.h"
#include "FGBuildDescriptor.h"
#include "ModTypes.h"
#include "AutoSupportPartSearchConfig.generated.h"

class UFGBuildSubCategory;
class UFGBuildCategory;

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportSubCategoryList
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	TSet<TSubclassOf<UFGCategory>> SubCategories;
};

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportPartMetadata
{
	GENERATED_BODY()

	/**
	 * True if there's a side of the part that is the ideal resting side for placement on an anchoring surface.
	 */
	UPROPERTY(EditDefaultsOnly)
	bool bHasPreferredRestingSide = false;

	/**
	 * The side of the part that is the ideal resting side for placement on an anchoring surface.
	 */
	UPROPERTY(EditDefaultsOnly, meta=(EditCondition="bHasPreferredRestingSide"))
	EAutoSupportBuildDirection PreferredRestingSide = EAutoSupportBuildDirection::Bottom;

	/**
	 * The default distance to bury the part if it is anchored to terrain. This helps avoid floating areas of the part.
	 */
	UPROPERTY(EditDefaultsOnly)
	float DefaultBuryDistance = 0.f;
};

UCLASS(BlueprintType)
class AUTOSUPPORT_API UAutoSupportPartSearchConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable)
	bool IsCategoryBlocked(TSubclassOf<UFGCategory> Category) const;
	
	UFUNCTION(BlueprintCallable)
	bool IsSubCategoryBlocked(TSubclassOf<UFGCategory> Category, TSubclassOf<UFGCategory> SubCategory) const;
	
	UFUNCTION(BlueprintCallable)
	bool IsPartBlocked(TSubclassOf<UFGBuildDescriptor> Descriptor) const;

	UFUNCTION(BlueprintCallable)
	bool TryGetPartMetadata(TSubclassOf<UFGBuildDescriptor> Descriptor, FAutoSupportPartMetadata& OutPartMetadata) const;
	
protected:
	
	/**
	 * Categories to remove from the part search.
	 */
	UPROPERTY(EditDefaultsOnly)
	TSet<TSubclassOf<UFGCategory>> BlockedCategories;

	/**
	 * Sub categories to remove from the part search.
	 */
	UPROPERTY(EditDefaultsOnly)
	TMap<TSubclassOf<UFGCategory>, FAutoSupportSubCategoryList> BlockedSubCategories;

	/**
	 * Parts to remove from the part search.
	 */
	UPROPERTY(EditDefaultsOnly)
	TSet<TSubclassOf<UFGBuildDescriptor>> BlockedParts;

	/**
	 * Well-known metadata for various buildables.
	 */
	UPROPERTY(EditDefaultsOnly)
	TMap<TSubclassOf<UFGBuildDescriptor>, FAutoSupportPartMetadata> PartMetadataMap;
	
};