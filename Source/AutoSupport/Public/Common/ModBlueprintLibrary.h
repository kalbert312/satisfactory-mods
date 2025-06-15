#pragma once

#include "CoreMinimal.h"
#include "ModBlueprintLibrary.generated.h"

class UPanelSlot;

UCLASS()
class AUTOSUPPORT_API UAutoSupportBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable)
	static void SetStackBoxSlotHAlign(UPanelSlot* Slot, EHorizontalAlignment Alignment);
	
	UFUNCTION(BlueprintCallable)
	static void SetStackBoxSlotVAlign(UPanelSlot* Slot, EVerticalAlignment Alignment);
};