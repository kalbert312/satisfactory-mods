
#include "Common/ModTypes.h"

#include "FGBuildable.h"
#include "FGLightweightBuildableSubsystem.h"
#include "ModLogging.h"

FAutoSupportBuildableHandle::FAutoSupportBuildableHandle(AFGBuildable* Buildable)
{
	fgcheck(Buildable);
	this->Buildable = Buildable;
	this->BuildableClass = Buildable->GetClass();
	this->Transform = Buildable->GetTransform();
}

FAutoSupportBuildableHandle::FAutoSupportBuildableHandle(const FLightweightBuildableInstanceRef& LightweightRef)
{
	this->BuildableClass = LightweightRef.GetBuildableClass();
	this->Transform = LightweightRef.GetBuildableTransform();
}

bool FAutoSupportBuildableHandle::Equals(const FAutoSupportBuildableHandle& Other) const
{
	if (BuildableClass != Other.BuildableClass)
	{
		return false; // different class
	}
	
	if (Buildable != nullptr)
	{
		if (Buildable == Other.Buildable)
		{
			return true; // point to the same buildable
		}
		
		if (Other.Buildable != nullptr)
		{
			return false; // point to different buildables
		}

		if (!Buildable->GetIsLightweightTemporary())
		{
			return false; // This is not a lightweight, so to be equal, the other's buildable reference must be equal, and it was not.
		}
	}
	else if (Other.Buildable != nullptr && !Other.Buildable->GetIsLightweightTemporary())
	{
		return false; // This is a lightweight but the other isn't.
	}

	// Either "This" and/or "Other" is null at this point.
	// "This" is considered a lightweight, either because its buildable is null or is a lightweight temporary.
	// "Other" is considered a lightweight, either because its buildable is null or is a lightweight temporary.
	// We don't short circuit a result on differing null status of buildable because the temporary may not be spawned, or if it is, one of
	// the handles may just not have a reference to it.
	// Instead, check if the transforms are equal. The edge case is there are one or more duplicate lightweights at the exact same transform.
	// While we could disambiguate that with the lightweight runtime data index, that is not available when saved to disk.
	
	return Transform.Equals(Other.Transform);
}

FString FAutoSupportBuildableHandle::ToString() const
{
	return FString::Printf(
		TEXT("Class: %s, BuildableInstValid: %s, BuildableInstIsLightweight: %s, Transform: %s"),
		TEXT_CLS_NAME(BuildableClass),
		TEXT_BOOL(Buildable.IsValid()),
		TEXT_BOOL(Buildable.IsValid() && Buildable->GetIsLightweightTemporary()),
		TEXT_STR(Transform.ToString()));
}
