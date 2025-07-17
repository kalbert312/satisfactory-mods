
#include "Common/ModTypes.h"

#include "FGBuildable.h"
#include "FGLightweightBuildableSubsystem.h"
#include "ModLogging.h"

FAutoSupportBuildableHandle::FAutoSupportBuildableHandle(AFGBuildable* Buildable)
{
	fgcheck(Buildable);
	this->Buildable = Buildable;
	this->BuildableClass = Buildable->GetClass();
	SetTransform(Buildable->GetTransform());
}

FAutoSupportBuildableHandle::FAutoSupportBuildableHandle(const FLightweightBuildableInstanceRef& LightweightRef)
{
	this->BuildableClass = LightweightRef.GetBuildableClass();
	SetTransform(LightweightRef.GetBuildableTransform());
}

void FAutoSupportBuildableHandle::SetTransform(const FTransform& Transform)
{
	this->Transform = Transform;

	const auto Location = Transform.GetLocation();

	const auto Int64Min = static_cast<float>(TNumericLimits<int64>::Min());
	const auto Int64Max = static_cast<float>(TNumericLimits<int64>::Max());

	const auto Xish = static_cast<int64>(FMath::Clamp(Location.X, Int64Min, Int64Max));
	const auto Yish = static_cast<int64>(FMath::Clamp(Location.Y, Int64Min, Int64Max));
	const auto Zish = static_cast<int64>(FMath::Clamp(Location.Z, Int64Min, Int64Max));

	const auto Locationish = FInt64Vector3(Xish, Yish, Zish);
	
	this->TransformHash = GetTypeHash(Locationish);
}

bool FAutoSupportBuildableHandle::Equals(const FAutoSupportBuildableHandle& Other) const
{
	if (BuildableClass != Other.BuildableClass)
	{
		return false; // different class
	}

	if (!Transform.Equals(Other.Transform, 0.01f))
	{
		return false; // different transforms
	}
	
	const auto bThisHandleIsConsideredLightweight = IsConsideredLightweight();
	const auto bOtherHandleIsConsideredLightweight = Other.IsConsideredLightweight();

	if (bThisHandleIsConsideredLightweight && bOtherHandleIsConsideredLightweight)
	{
		// For lightweights, consider equal if our class and transform are equal. Edge case is exactly overlapping lightweights but that
		// should be checked for in an outer code context.
		return true;
	}
	else if (bThisHandleIsConsideredLightweight != bOtherHandleIsConsideredLightweight)
	{
		return false; // a light weight cannot equal a non-lightweight
	}
	
	return Buildable == Other.Buildable; // non-lightweights must point to the same buildable
}

FString FAutoSupportBuildableHandle::ToString() const
{
	return FString::Printf(
		TEXT("Class: [%s], IsConsideredLightweight: [%s], BuildableInstValid: [%s], TransformHash: [%d] Transform: [%s]"),
		TEXT_CLS_NAME(BuildableClass),
		TEXT_BOOL(Buildable.IsValid()),
		TEXT_BOOL(IsConsideredLightweight()),
		TransformHash,
		TEXT_STR(Transform.ToString()));
}
