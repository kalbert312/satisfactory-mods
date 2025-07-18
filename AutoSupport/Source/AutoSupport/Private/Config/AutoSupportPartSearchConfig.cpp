//

#include "AutoSupportPartSearchConfig.h"

#include "FGBuildCategory.h"

bool UAutoSupportPartSearchConfig::IsCategoryBlocked(TSubclassOf<UFGCategory> Category) const
{
	return Category && BlockedCategories.Contains(Category);
}

bool UAutoSupportPartSearchConfig::IsSubCategoryBlocked(TSubclassOf<UFGCategory> Category, TSubclassOf<UFGCategory> SubCategory) const
{
	if (!Category || !SubCategory)
	{
		return false;
	}
	
	if (auto* BlockedSubCategoriesList = BlockedSubCategories.Find(Category))
	{
		return BlockedSubCategoriesList->SubCategories.Contains(SubCategory);
	}
	
	return false;
}

bool UAutoSupportPartSearchConfig::IsPartBlocked(TSubclassOf<UFGBuildDescriptor> Descriptor) const
{
	return Descriptor && BlockedParts.Contains(Descriptor);
}

bool UAutoSupportPartSearchConfig::TryGetPartMetadata(
	const TSubclassOf<UFGBuildDescriptor> Descriptor,
	FAutoSupportPartMetadata& OutPartMetadata) const
{
	if (const auto* Entry = PartMetadataMap.Find(Descriptor))
	{
		OutPartMetadata = *Entry;
		return true;
	}

	return false;
}


