
#include "Common\ModTypes.h"

#include "FGBuildable.h"
#include "ModLogging.h"

FAutoSupportBuildableHandle::FAutoSupportBuildableHandle(AFGBuildable* Buildable)
{
	check(Buildable);
	this->Buildable = Buildable;
	this->BuildableClass = Buildable->GetClass();

	if (Buildable->GetIsLightweightTemporary() || Buildable->ManagedByLightweightBuildableSubsystem())
	{
		this->LightweightRuntimeIndex = Buildable->GetRuntimeDataIndex();
		check(LightweightRuntimeIndex >= 0);
	}
}

FAutoSupportBuildableHandle::FAutoSupportBuildableHandle(const TSubclassOf<AFGBuildable> BuildableClass, const int32 LightweightRuntimeIndex)
{
	this->BuildableClass = BuildableClass;
	check(LightweightRuntimeIndex >= 0);
	this->LightweightRuntimeIndex = LightweightRuntimeIndex;
}

bool FAutoSupportBuildableHandle::IsDataValid() const
{
	return IsValid(BuildableClass);
}

bool FAutoSupportBuildableHandle::Equals(const FAutoSupportBuildableHandle& Other) const
{
	if (!IsDataValid() || !Other.IsDataValid())
	{
		return false;
	}
		
	if (Buildable != nullptr && Buildable == Other.Buildable)
	{
		return true;
	}

	if (const auto IsLightweight = IsLightweightType(); IsLightweight != Other.IsLightweightType() || !IsLightweight)
	{
		return false;
	}
		
	return BuildableClass == Other.BuildableClass
		&& LightweightRuntimeIndex == Other.LightweightRuntimeIndex;
}

FString FAutoSupportBuildableHandle::ToString() const
{
	return FString::Printf(
		TEXT("Class: %s, LightweightRuntimeIndex: %d, BuildableInstValid: %s"),
		TEXT_CLS_NAME(BuildableClass),
		LightweightRuntimeIndex,
		TEXT_BOOL(Buildable.IsValid()));
}
