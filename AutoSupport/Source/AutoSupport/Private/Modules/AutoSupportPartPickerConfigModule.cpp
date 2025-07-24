// 

#include "AutoSupportPartPickerConfigModule.h"

#include "BP_ModConfig_AutoSupportStruct.h"
#include "ContentTagRegistry.h"
#include "FGBuildDescriptor.h"
#include "FGCategory.h"
#include "ModConstants.h"
#include "ModLogging.h"

UAutoSupportPartPickerConfigModule* UAutoSupportPartPickerConfigModule::Get(const UWorld* World)
{
	return GetChild<UAutoSupportPartPickerConfigModule>(World, AutoSupportConstants::ModuleName_PartPickerConfig);
}

void UAutoSupportPartPickerConfigModule::DispatchLifecycleEvent(const ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	if (Phase == ELifecyclePhase::CONSTRUCTION)
	{
		ExcludeTag = FGameplayTag::RequestGameplayTag(AutoSupportConstants::TagName_AutoSupport_PartPicker_Exclude);
		IncludeTag = FGameplayTag::RequestGameplayTag(AutoSupportConstants::TagName_AutoSupport_PartPicker_Include);
	}
	else if (Phase == ELifecyclePhase::INITIALIZATION)
	{
		const auto ExcludedCategoryNamesStr = FBP_ModConfig_AutoSupportStruct::GetActiveConfig(GetWorld()).ConstraintsSection.PartPickerExcludedCategoryNames;

		if (!ExcludedCategoryNamesStr.IsEmpty())
		{
			TArray<FString> ExcludedCategoryNames;
			ExcludedCategoryNamesStr.ParseIntoArray(ExcludedCategoryNames, TEXT(","), true);
		
			for (const auto& ExcludedCategoryName : ExcludedCategoryNames)
			{
				AdditionalExcludedCategoryNames.Add(ExcludedCategoryName.TrimStartAndEnd().ToLower());
			}
		}
	}
}

bool UAutoSupportPartPickerConfigModule::IsCategoryExcluded(const TSubclassOf<UFGCategory> Category) const
{
	if (AdditionalExcludedCategoryNames.Contains(UFGCategory::GetCategoryName(Category).ToString().TrimStartAndEnd().ToLower()))
	{
		return true;
	}
	
	auto* ContentTagRegistry = UContentTagRegistry::Get(GetWorld());
	const auto CategoryTags = ContentTagRegistry->GetGameplayTagContainerFor(Category);

	return CategoryTags.HasTagExact(ExcludeTag);
}

bool UAutoSupportPartPickerConfigModule::IsSubCategoryExcluded(
	TSubclassOf<UFGCategory> Category,
	TSubclassOf<UFGCategory> SubCategory) const
{
	// TODO(k.a): do we actually have a case of not blocking a subcategory in a certain category?
	
	auto* ContentTagRegistry = UContentTagRegistry::Get(GetWorld());
	const auto SubCategoryTags = ContentTagRegistry->GetGameplayTagContainerFor(SubCategory);

	return SubCategoryTags.HasTagExact(ExcludeTag);
}

bool UAutoSupportPartPickerConfigModule::IsPartExcluded(const TSubclassOf<UFGBuildDescriptor> Descriptor) const
{
	auto* ContentTagRegistry = UContentTagRegistry::Get(GetWorld());
	const auto CategoryTags = ContentTagRegistry->GetGameplayTagContainerFor(Descriptor);

	return CategoryTags.HasTagExact(ExcludeTag);
}

bool UAutoSupportPartPickerConfigModule::IsPartForceIncluded(const TSubclassOf<UFGBuildDescriptor> Descriptor) const
{
	auto* ContentTagRegistry = UContentTagRegistry::Get(GetWorld());
	const auto CategoryTags = ContentTagRegistry->GetGameplayTagContainerFor(Descriptor);

	return CategoryTags.HasTagExact(IncludeTag);
}
