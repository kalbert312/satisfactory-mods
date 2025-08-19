#pragma once

#include "CoreMinimal.h"
#include "ModDebugBlueprintLibrary.generated.h"

UCLASS()
class AUTOSUPPORT_API UAutoSupportDebugBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static void DrawDebugCoordinateSystem(const UWorld* InWorld, FVector const& AxisLoc, FRotator const& AxisRot, float Scale, bool bPersistentLines, float LifeTime, uint8 DepthPriority, float Thickness);
	
protected:
	static ULineBatchComponent* GetDebugLineBatcher(const UWorld* InWorld, bool bPersistentLines, float LifeTime, bool bDepthIsForeground);
	static float GetDebugLineLifeTime(ULineBatchComponent* LineBatcher, float LifeTime, bool bPersistent);
	
};