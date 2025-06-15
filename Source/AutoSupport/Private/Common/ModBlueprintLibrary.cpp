
#include "ModBlueprintLibrary.h"

#include "StackBoxSlot.h"

void UAutoSupportBlueprintLibrary::SetStackBoxSlotHAlign(UPanelSlot* Slot, EHorizontalAlignment Alignment)
{
	if (auto* StackBoxSlot = Cast<UStackBoxSlot>(Slot))
	{
		StackBoxSlot->SetHorizontalAlignment(Alignment);
	}
}

void UAutoSupportBlueprintLibrary::SetStackBoxSlotVAlign(UPanelSlot* Slot, EVerticalAlignment Alignment)
{
	if (auto* StackBoxSlot = Cast<UStackBoxSlot>(Slot))
	{
		StackBoxSlot->SetVerticalAlignment(Alignment);
	}
}
