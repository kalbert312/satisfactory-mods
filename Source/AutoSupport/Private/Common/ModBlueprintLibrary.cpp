
#include "ModBlueprintLibrary.h"

#include "FGBuildable.h"
#include "FGBuildingDescriptor.h"

EAutoSupportBuildDirection UAutoSupportBlueprintLibrary::GetOppositeDirection(const EAutoSupportBuildDirection Direction)
{
	switch (Direction)
	{
		case EAutoSupportBuildDirection::Top:
			return EAutoSupportBuildDirection::Bottom;
		case EAutoSupportBuildDirection::Bottom:
			return EAutoSupportBuildDirection::Top;
		case EAutoSupportBuildDirection::Front:
			return EAutoSupportBuildDirection::Back;
		case EAutoSupportBuildDirection::Back:
			return EAutoSupportBuildDirection::Front;
		case EAutoSupportBuildDirection::Left:
			return EAutoSupportBuildDirection::Right;
		case EAutoSupportBuildDirection::Right:
			return EAutoSupportBuildDirection::Left;
	}

	checkf(Direction != Direction, TEXT("Direction is %i"), static_cast<int32>(Direction));
	
	return Direction;
}

FVector UAutoSupportBlueprintLibrary::GetDirectionVector(const EAutoSupportBuildDirection Direction)
{
	switch (Direction)
	{
		case EAutoSupportBuildDirection::Top:
			return FVector(0, 0, 1);
		case EAutoSupportBuildDirection::Bottom:
			return FVector(0, 0, -1);
		case EAutoSupportBuildDirection::Front:
			return FVector(0, 1, 0);
		case EAutoSupportBuildDirection::Back:
			return FVector(0, -1, 0);
		case EAutoSupportBuildDirection::Left:
			return FVector(-1, 0, 0);
		case EAutoSupportBuildDirection::Right:
			return FVector(1, 0, 0);
		default:
			return FVector::ZeroVector;
	}
}

void UAutoSupportBlueprintLibrary::GetBuildableClearance(TSoftClassPtr<UFGBuildingDescriptor> PartDescriptor, FBox& OutBox)
{
	const auto Buildable = GetDefault<AFGBuildable>(UFGBuildingDescriptor::GetBuildableClass(PartDescriptor.Get()));
	OutBox = Buildable->GetCombinedClearanceBox();
}
