#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EAutoSupportBuildDirection : uint8
{
	Top,
	Bottom,
	Front,
	Back,
	Left,
	Right,
	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(EAutoSupportBuildDirection, EAutoSupportBuildDirection::Count)
