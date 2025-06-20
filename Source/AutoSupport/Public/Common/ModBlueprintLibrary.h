#pragma once

#include "CoreMinimal.h"
#include "ModTypes.h"
#include "ModBlueprintLibrary.generated.h"

class UFGBuildingDescriptor;
class UPanelSlot;

UCLASS()
class AUTOSUPPORT_API UAutoSupportBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static EAutoSupportBuildDirection GetOppositeDirection(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static FVector GetDirectionVector(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static void GetBuildableClearance(TSoftClassPtr<UFGBuildingDescriptor> PartDescriptor, FBox& OutBox);
};