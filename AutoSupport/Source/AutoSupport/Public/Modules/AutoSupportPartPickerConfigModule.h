// 

#pragma once

#include "CoreMinimal.h"
#include "AutoSupportGameWorldModule.h"
#include "GameplayTagContainer.h"
#include "AutoSupportPartPickerConfigModule.generated.h"

class UContentTagRegistry;
class UFGBuildDescriptor;
class UFGCategory;

/**
 * Child game module that supplies configuration for the Auto Support part picker. This is spawned via the Blueprint class of AutoSupportGameWorldModule.
 */
UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportPartPickerConfigModule : public UAutoSupportGameWorldModule
{
	GENERATED_BODY()
	
public:

	static UAutoSupportPartPickerConfigModule* Get(const UWorld* World);

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

	UFUNCTION(BlueprintCallable)
	bool IsCategoryExcluded(TSubclassOf<UFGCategory> Category) const;
	
	UFUNCTION(BlueprintCallable)
	bool IsSubCategoryExcluded(TSubclassOf<UFGCategory> Category, TSubclassOf<UFGCategory> SubCategory) const;
	
	UFUNCTION(BlueprintCallable)
	bool IsPartExcluded(TSubclassOf<UFGBuildDescriptor> Descriptor) const;

	UFUNCTION(BlueprintCallable)
	bool IsPartForceIncluded(TSubclassOf<UFGBuildDescriptor> Descriptor) const;

protected:

	FGameplayTag ExcludeTag;
	FGameplayTag IncludeTag;

	TSet<FString> AdditionalExcludedCategoryNames;
	
};
