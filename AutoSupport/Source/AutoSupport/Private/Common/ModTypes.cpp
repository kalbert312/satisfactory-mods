
#include "Common/ModTypes.h"

#include "FGBuildable.h"
#include "FGLightweightBuildableSubsystem.h"
#include "ModDefines.h"
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

void FAutoSupportBuildableHandle::SetTransform(const FTransform& NewTransform)
{
	this->Transform = NewTransform;

	FInt64Vector3 RoundedLocation;
	GetRoundedLocation(NewTransform.GetLocation(), RoundedLocation);
	
	this->TransformHash = GetTypeHash(RoundedLocation);
}

void FAutoSupportBuildableHandle::GetRoundedLocation(const FVector& Location, FInt64Vector3& OutLocation)
{
	constexpr auto Int64Min = static_cast<float>(TNumericLimits<int64>::Min());
	constexpr auto Int64Max = static_cast<float>(TNumericLimits<int64>::Max());

	const auto Xish = static_cast<int64>(FMath::Clamp(Location.X, Int64Min, Int64Max));
	const auto Yish = static_cast<int64>(FMath::Clamp(Location.Y, Int64Min, Int64Max));
	const auto Zish = static_cast<int64>(FMath::Clamp(Location.Z, Int64Min, Int64Max));

	OutLocation = FInt64Vector3(Xish, Yish, Zish);
}

bool FAutoSupportBuildableHandle::Equals(const FAutoSupportBuildableHandle& Other) const
{
	if (BuildableClass != Other.BuildableClass)
	{
		return false; // different class
	}

	if (!Transform.Equals(Other.Transform, AUTOSUPPORT_TRANSFORM_EQUALITY_TOLERANCE))
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
		TEXT_BOOL(IsConsideredLightweight()),
		TEXT_BOOL(Buildable.IsValid()),
		TransformHash,
		TEXT_STR(Transform.ToString()));
}
