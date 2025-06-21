#pragma once

#include "CoreMinimal.h"
#include "FGBuildable.h"
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
	static FRotator GetDirectionRotator(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static void GetBuildableClearance(TSubclassOf<AFGBuildable> BuildableClass, FBox& OutBox);
};