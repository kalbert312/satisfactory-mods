
#include "Common\ModTypes.h"

#include "FGBuildable.h"
#include "ModLogging.h"

FAutoSupportBuildableHandle::FAutoSupportBuildableHandle(AFGBuildable* Buildable)
{
	fgcheck(Buildable);
	this->Buildable = Buildable;
	this->BuildableClass = Buildable->GetClass();
	this->LightweightRuntimeIndex = Buildable->GetRuntimeDataIndex();
	
	if (Buildable->GetIsLightweightTemporary())
	{
		fgcheck(LightweightRuntimeIndex >= 0);
	}
}

FAutoSupportBuildableHandle::FAutoSupportBuildableHandle(const TSubclassOf<AFGBuildable> BuildableClass, const int32 LightweightRuntimeIndex)
{
	this->BuildableClass = BuildableClass;
	fgcheck(LightweightRuntimeIndex >= 0);
	this->LightweightRuntimeIndex = LightweightRuntimeIndex;
}

bool FAutoSupportBuildableHandle::IsDataValid() const
{
	return IsValid(BuildableClass);
}

bool FAutoSupportBuildableHandle::Equals(const FAutoSupportBuildableHandle& Other) const
{
	if (Buildable != nullptr && Buildable == Other.Buildable)
	{
		return true;
	}
	
	if (const auto IsLightweight = IsLightweightType(); !IsLightweight || IsLightweight != Other.IsLightweightType())
	{
		return false;
	}
		
	return LightweightRuntimeIndex == Other.LightweightRuntimeIndex
		&& BuildableClass == Other.BuildableClass;
}

FString FAutoSupportBuildableHandle::ToString() const
{
	return FString::Printf(
		TEXT("Class: %s, LightweightRuntimeIndex: %d, BuildableInstValid: %s"),
		TEXT_CLS_NAME(BuildableClass),
		LightweightRuntimeIndex,
		TEXT_BOOL(Buildable.IsValid()));
}
