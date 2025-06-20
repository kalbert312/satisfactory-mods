#pragma once

#include "CoreMinimal.h"
#include "FGBuildDescriptor.h"
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
	
};