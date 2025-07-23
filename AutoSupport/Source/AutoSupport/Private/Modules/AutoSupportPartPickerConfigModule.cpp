// 

#include "AutoSupportPartPickerConfigModule.h"

#include "ModConstants.h"

UAutoSupportPartPickerConfigModule* UAutoSupportPartPickerConfigModule::Get(const UWorld* World)
{
	return GetChild<UAutoSupportPartPickerConfigModule>(World, AutoSupportConstants::ModuleName_PartPickerConfig);
}

bool UAutoSupportPartPickerConfigModule::IsCategoryExcluded(TSubclassOf<UFGCategory> Category) const
{
	// TODO(k.a): implement
	return false;
}

bool UAutoSupportPartPickerConfigModule::IsSubCategoryExcluded(
	TSubclassOf<UFGCategory> Category,
	TSubclassOf<UFGCategory> SubCategory) const
{
	// TODO(k.a): implement
	return false;
}

bool UAutoSupportPartPickerConfigModule::IsPartExcluded(TSubclassOf<UFGBuildDescriptor> Descriptor) const
{
	// TODO(k.a): implement
	return false;
}

bool UAutoSupportPartPickerConfigModule::IsPartForceIncluded(TSubclassOf<UFGBuildDescriptor> Descriptor) const
{
	// TODO(k.a): implement
	return false;
}
