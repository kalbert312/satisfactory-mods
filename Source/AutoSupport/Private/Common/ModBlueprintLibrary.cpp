
#include "ModBlueprintLibrary.h"

#include "FGBuildable.h"
#include "FGCentralStorageSubsystem.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryLibrary.h"
#include "ModLogging.h"

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
			return FVector(0, -1, 0);
		case EAutoSupportBuildDirection::Back:
			return FVector(0, 1, 0);
		case EAutoSupportBuildDirection::Left:
			return FVector(-1, 0, 0);
		case EAutoSupportBuildDirection::Right:
			return FVector(1, 0, 0);
		default:
			return FVector::ZeroVector;
	}
}

FRotator UAutoSupportBlueprintLibrary::GetDirectionRotator(EAutoSupportBuildDirection Direction)
{
	FRotator DeltaRot(0, 0, 0);
	
	switch (Direction)
	{
		case EAutoSupportBuildDirection::Bottom:
		default:
			// No-op
			break;
		case EAutoSupportBuildDirection::Top:
			DeltaRot.Roll = 180;
			break;
		case EAutoSupportBuildDirection::Front:
			DeltaRot.Roll = 90;
			break;
		case EAutoSupportBuildDirection::Back:
			DeltaRot.Roll = -90;
			break;
		case EAutoSupportBuildDirection::Left:
			DeltaRot.Pitch = -90;
			break;
		case EAutoSupportBuildDirection::Right:
			DeltaRot.Pitch = 90;
			break;
	}

	return DeltaRot;
}

void UAutoSupportBlueprintLibrary::GetBuildableClearance(TSubclassOf<AFGBuildable> BuildableClass, FBox& OutBox)
{
	const auto Buildable = GetDefault<AFGBuildable>(BuildableClass);
	OutBox = Buildable->GetCombinedClearanceBox();
}

bool UAutoSupportBlueprintLibrary::CanAffordItemBill(
	AFGCharacterPlayer* Player,
	const TArray<FItemAmount>& BillOfParts,
	bool bTakeFromDepot)
{
	auto* World = Player->GetWorld();
	const auto* Inventory = Player->GetInventory();
	
	if (Inventory->GetNoBuildCost())
	{
		MOD_LOG(Verbose, TEXT("CanAffordItemBill There's no build cost. Returning true"))
		return true;
	}

	auto* CentralStorageSys = AFGCentralStorageSubsystem::Get(World);

	for (const auto& BillOfPart : BillOfParts)
	{
		const auto InvAvailableNum = Inventory->GetNumItems(BillOfPart.ItemClass);
		
		const auto AvailableNum = bTakeFromDepot
			? InvAvailableNum + CentralStorageSys->GetNumItemsFromCentralStorage(BillOfPart.ItemClass)
			: InvAvailableNum;

		MOD_LOG(Verbose, TEXT("CanAffordBuildPlan Item [%s] Cost: %i, Available %i"), *BillOfPart.ItemClass->GetName(), BillOfPart.Amount, AvailableNum)

		if (AvailableNum < BillOfPart.Amount)
		{
			return false;
		}
	}

	return true;
}

void UAutoSupportBlueprintLibrary::PayItemBill(
	AFGCharacterPlayer* Player,
	const TArray<FItemAmount>& BillOfParts,
	bool bTakeFromDepot,
	bool bTakeFromInventoryFirst)
{
	auto* Inventory = Player->GetInventory();

	if (Inventory->GetNoBuildCost())
	{
		return;
	}
	
	auto* CentralStorageSys = AFGCentralStorageSubsystem::Get(Player->GetWorld());
	
	for (auto& PartBill : BillOfParts)
	{
		if (bTakeFromDepot)
		{
			UFGInventoryLibrary::GrabItemsFromInventoryAndCentralStorage(Inventory, CentralStorageSys, bTakeFromInventoryFirst, PartBill.ItemClass, PartBill.Amount);
		}
		else
		{
			Inventory->Remove(PartBill.ItemClass, PartBill.Amount);
		}
	}
}

bool UAutoSupportBlueprintLibrary::PayItemBillIfAffordable(
	AFGCharacterPlayer* Player,
	const TArray<FItemAmount>& BillOfParts,
	const bool bTakeFromDepot,
	const bool bTakeFromInventoryFirst)
{
	if (CanAffordItemBill(Player, BillOfParts, bTakeFromDepot))
	{
		PayItemBill(Player, BillOfParts, bTakeFromDepot, bTakeFromInventoryFirst);
		return true;
	}

	return false;
}
