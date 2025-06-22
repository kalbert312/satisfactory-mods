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

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool CanAffordItemBill(AFGCharacterPlayer* Player, const TArray<FItemAmount>& BillOfParts, bool bTakeFromDepot);
	
	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static void PayItemBill(AFGCharacterPlayer* Player, const TArray<FItemAmount>& BillOfParts, bool bTakeFromDepot, bool bTakeFromInventoryFirst);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool PayItemBillIfAffordable(AFGCharacterPlayer* Player, const TArray<FItemAmount>& BillOfParts, bool bTakeFromDepot, bool bTakeFromInventoryFirst);
};