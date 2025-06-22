
#include "ModBlueprintLibrary.h"

#include "BuildableAutoSupport_Types.h"
#include "FGBuildable.h"
#include "FGCentralStorageSubsystem.h"
#include "FGCharacterPlayer.h"
#include "FGHologram.h"
#include "FGInventoryLibrary.h"
#include "FGPlayerState.h"
#include "FGRecipeManager.h"
#include "ModLogging.h"

#pragma region Building Helpers

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

AFGHologram* UAutoSupportBlueprintLibrary::CreateCompositeHologramFromPlan(
	const FAutoSupportBuildPlan& Plan, const FTransform& Transform, APawn* BuildInstigator, AActor* Owner)
{
	check(BuildInstigator)
	
	// Build the parts
	auto WorkingTransform = Transform;
	WorkingTransform.AddToTranslation(Plan.RelativeLocation);
	WorkingTransform.SetRotation(WorkingTransform.Rotator().Add(Plan.RelativeRotation.Pitch, Plan.RelativeRotation.Yaw, Plan.RelativeRotation.Roll).Quaternion());

	AFGHologram* RootHologram = nullptr; // we'll assign this while building
	
	if (Plan.StartPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("CreateCompositeHologramFromPlan |> Building Start Part, Orientation: %i"), static_cast<int32>(Plan.StartPart.Orientation));
		SpawnPartPlanHolograms(RootHologram, Plan.StartPart, BuildInstigator, Owner, WorkingTransform);
	}

	if (Plan.MidPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("CreateCompositeHologramFromPlan |> Building Mid Parts, Orientation: %i"), static_cast<int32>(Plan.EndPart.Orientation));
		SpawnPartPlanHolograms(RootHologram, Plan.MidPart, BuildInstigator, Owner, WorkingTransform);
	}

	if (Plan.EndPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("CreateCompositeHologramFromPlan |> Building End Part, Orientation: %i"), static_cast<int32>(Plan.EndPart.Orientation));
		WorkingTransform.AddToTranslation(Plan.EndPartPositionOffset);
		
		SpawnPartPlanHolograms(RootHologram, Plan.EndPart, BuildInstigator, Owner, WorkingTransform);
	}

	check(RootHologram)

	return RootHologram;
}

void UAutoSupportBlueprintLibrary::PlanBuild(UWorld* World, const FAutoSupportTraceResult& TraceResult, const FBuildableAutoSupportData& AutoSupportData, OUT FAutoSupportBuildPlan& OutPlan)
{
	OutPlan = FAutoSupportBuildPlan();

	// Copy trace result's relative location & rotation.
	OutPlan.RelativeLocation = TraceResult.StartRelativeLocation;
	OutPlan.RelativeRotation = TraceResult.StartRelativeRotation;
	
	// Plan the parts.
	auto RemainingBuildDistance = TraceResult.BuildDistance;
	float SinglePartConsumedBuildSpace = 0;

	const auto* RecipeManager = AFGRecipeManager::Get(World);
	
	// Start with the top because that's where the auto support is planted.
	if (AutoSupportData.StartPartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("PlanBuild |> Planning Start Part Positioning"));
		if (PlanSinglePart(TraceResult, AutoSupportData.StartPartDescriptor.Get(), AutoSupportData.StartPartOrientation, OutPlan.StartPart, SinglePartConsumedBuildSpace, RecipeManager))
		{
			RemainingBuildDistance -= SinglePartConsumedBuildSpace;

			OutPlan.StartPart.Count = 1;
		
			if (RemainingBuildDistance < 0)
			{
				return;
			}
		}
	}
	
	// Do the end next. There may not be enough room for mid pieces.
	if (AutoSupportData.EndPartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("PlanBuild |> Planning End Part Positioning"));
		if (PlanSinglePart(TraceResult, AutoSupportData.EndPartDescriptor.Get(), AutoSupportData.EndPartOrientation, OutPlan.EndPart, SinglePartConsumedBuildSpace, RecipeManager))
		{
			RemainingBuildDistance -= SinglePartConsumedBuildSpace;

			OutPlan.EndPart.Count = 1;
		
			if (RemainingBuildDistance < 0)
			{
				return;
			}
		}
	}
	
	if (AutoSupportData.MiddlePartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("PlanBuild |> Planning Mid Part Positioning"));
		if (PlanSinglePart(TraceResult, AutoSupportData.MiddlePartDescriptor.Get(), AutoSupportData.MiddlePartOrientation, OutPlan.MidPart, SinglePartConsumedBuildSpace, RecipeManager))
		{
			auto NumMiddleParts = static_cast<int32>(RemainingBuildDistance / SinglePartConsumedBuildSpace);
			RemainingBuildDistance -= NumMiddleParts * SinglePartConsumedBuildSpace;

			auto IsNearlyPerfectFit = RemainingBuildDistance <= 1.f;
			if (!IsNearlyPerfectFit)
			{
				NumMiddleParts++; // build an extra to fill the gap.
				// Offset the end part to be flush with where the line trace hit or ended. We built an extra part so the direction is negative. We don't need to worry about the end part size because it was already subtracted from build distance.
				OutPlan.EndPartPositionOffset = -TraceResult.Direction * (SinglePartConsumedBuildSpace - RemainingBuildDistance);
			}
		
			MOD_LOG(Verbose, TEXT("PlanBuild |> Mid Size: %s, Num Mid: %d, Perfect Fit: %s"), *OutPlan.MidPart.BBox.GetSize().ToString(), NumMiddleParts, TEXT_CONDITION(IsNearlyPerfectFit));
			OutPlan.MidPart.Count = NumMiddleParts;
		}
	}
}

#pragma endregion

#pragma region Inventory Helpers

bool UAutoSupportBlueprintLibrary::CanAffordItemBill(
	AFGCharacterPlayer* Player,
	const TArray<FItemAmount>& BillOfParts,
	bool bTakeFromDepot)
{
	auto* World = Player->GetWorld();
	const auto* Inventory = Player->GetInventory();
	
	if (Inventory->GetNoBuildCost())
	{
		MOD_LOG(Verbose, TEXT("CanAffordItemBill |> There's no build cost. Returning true"))
		return true;
	}

	auto* CentralStorageSys = AFGCentralStorageSubsystem::Get(World);

	for (const auto& BillOfPart : BillOfParts)
	{
		const auto InvAvailableNum = Inventory->GetNumItems(BillOfPart.ItemClass);
		
		const auto AvailableNum = bTakeFromDepot
			? InvAvailableNum + CentralStorageSys->GetNumItemsFromCentralStorage(BillOfPart.ItemClass)
			: InvAvailableNum;

		MOD_LOG(Verbose, TEXT("CanAffordItemBill |> Item [%s] Cost: %i, Available %i"), *BillOfPart.ItemClass->GetName(), BillOfPart.Amount, AvailableNum)

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
	const bool bTakeFromDepot)
{
	if (!CanAffordItemBill(Player, BillOfParts, bTakeFromDepot))
	{
		return false;
	}
	
	const auto* PlayerState = Player->GetPlayerStateChecked<AFGPlayerState>();
	PayItemBill(Player, BillOfParts, bTakeFromDepot, PlayerState->GetTakeFromInventoryBeforeCentralStorage());
		
	return true;
}

#pragma endregion

#pragma region Private

void UAutoSupportBlueprintLibrary::SpawnPartPlanHolograms(
	AFGHologram*& ParentHologram,
	const FAutoSupportBuildPlanPartData& PartPlan,
	APawn* BuildInstigator,
	AActor* Owner,
	FTransform& WorkingTransform)
{
	for (auto i = 0; i < PartPlan.Count; ++i)
	{
		MOD_LOG(Verbose, TEXT("SpawnPartPlanHolograms | Spawning part. Start Transform: %s"), *WorkingTransform.ToString());

		// Copy the transform, then apply the orientation below
		FTransform SpawnTransform = WorkingTransform;

		SpawnTransform.AddToTranslation(PartPlan.BuildPositionOffset);

		// Apply the orientation relative to the part's origin
		SpawnTransform.AddToTranslation(-PartPlan.RotationalPositionOffset);
		SpawnTransform.SetRotation(SpawnTransform.Rotator().Add(PartPlan.Rotation.Pitch, PartPlan.Rotation.Yaw, PartPlan.Rotation.Roll).Quaternion());
		SpawnTransform.AddToTranslation(PartPlan.RotationalPositionOffset);

		if (ParentHologram)
		{
			AFGHologram::SpawnChildHologramFromRecipe(
				ParentHologram,
				FName(FGuid::NewGuid().ToString()),
				PartPlan.BuildRecipeClass,
				Owner,
				SpawnTransform.GetLocation(),
				[&](AFGHologram* PreSpawnHolo)
			{
				PreSpawnHolo->SetActorRotation(SpawnTransform.GetRotation());
				PreSpawnHolo->DoMultiStepPlacement(false);
			});
		}
		else
		{
			ParentHologram = AFGHologram::SpawnHologramFromRecipe(
				PartPlan.BuildRecipeClass,
				Owner,
				SpawnTransform.GetLocation(),
				BuildInstigator,
				[&](AFGHologram* PreSpawnHolo)
			{
				PreSpawnHolo->SetActorRotation(SpawnTransform.GetRotation());
				PreSpawnHolo->DoMultiStepPlacement(false);
				PreSpawnHolo->SetShouldSpawnChildHolograms(true);
			});
		}
		
		// Update the transform
		WorkingTransform.AddToTranslation(PartPlan.AfterPartPositionOffset);
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Next Transform: %s"), *WorkingTransform.ToString());
	}
}

bool UAutoSupportBlueprintLibrary::PlanSinglePart(
	const FAutoSupportTraceResult& TraceResult,
	TSubclassOf<UFGBuildingDescriptor> PartDescriptorClass,
	const EAutoSupportBuildDirection PartOrientation,
	FAutoSupportBuildPlanPartData& Plan,
	float& OutSinglePartConsumedBuildSpace,
	const AFGRecipeManager* RecipeManager)
{
	Plan.PartDescriptorClass = PartDescriptorClass;
	Plan.BuildableClass = UFGBuildingDescriptor::GetBuildableClass(PartDescriptorClass);
	Plan.Orientation = PartOrientation;
	GetBuildableClearance(Plan.BuildableClass, Plan.BBox);
		
	// StartSize.Z > 0 && StartSize.Z <= MaxPartHeight
	PlanPartPositioning(
		Plan.BBox,
		PartOrientation,
		TraceResult.Direction,
		OutSinglePartConsumedBuildSpace,
		Plan);

	// Should only be 1 recipe for a buildable...
	auto StartPartRecipeClasses = RecipeManager->FindRecipesByProduct(Plan.PartDescriptorClass, true, true);
	check(StartPartRecipeClasses.Num() <= 1);

	if (const auto StartPartRecipeClass = StartPartRecipeClasses.Num() > 0 ? StartPartRecipeClasses[0] : nullptr; OutSinglePartConsumedBuildSpace > 0)
	{
		Plan.BuildRecipeClass = StartPartRecipeClass;
		Plan.Count = 1;
		return true;
	}

	Plan.Count = 0;
	return false;
}

void UAutoSupportBlueprintLibrary::PlanPartPositioning(
	const FBox& PartBBox,
	const EAutoSupportBuildDirection PartOrientation,
	const FVector& Direction,
	float& OutConsumedBuildSpace,
	FAutoSupportBuildPlanPartData& Plan)
{
	// Roll = X, Pitch = Y, Yaw = Z
	const auto PartSize = PartBBox.GetSize();
	// We need an offset to translate by before and after the rotation is applied to return the part to it's occupying space in the auto support.
	const auto OriginOffset = PartBBox.GetCenter(); // Ex: (0,0,0) for mesh centered at buildable actor pivot. (0, 0, 200) for mesh bottom aligned with actor pivot.
	float AxisOriginOffset = OriginOffset.Z;
	float RelativeOffset = PartSize.Z;

	// NOTE: we're always assuming a part's UP is +Z here.
	switch (PartOrientation)
	{
		default:
			// No-op
			break;
		case EAutoSupportBuildDirection::Front:
		case EAutoSupportBuildDirection::Back:
			RelativeOffset = PartSize.Y;
			AxisOriginOffset = OriginOffset.Y;
			break;
		case EAutoSupportBuildDirection::Left:
		case EAutoSupportBuildDirection::Right:
			RelativeOffset = PartSize.X;
			AxisOriginOffset = OriginOffset.X;
			break;
	}
	
	const auto DeltaRot = UAutoSupportBlueprintLibrary::GetDirectionRotator(UAutoSupportBlueprintLibrary::GetOppositeDirection(PartOrientation));
	
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform Origin Offset: %s, Min: %s, Max: %s"), *OriginOffset.ToString(), *PartBBox.Min.ToString(), *PartBBox.Max.ToString());
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform Extent: %s"), *PartBBox.GetExtent().ToString());
	auto DirectionalOriginOffset = Direction * OriginOffset; // We're working with a world transform, so we must include our direction
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform Directional Origin Offset: %s"), *DirectionalOriginOffset.ToString());
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform Delta Rotation: %s"), *DeltaRot.ToString());

	// This occurs before rotation, so we operate in the Z direction. Negate the result because we're building relative a transform position at the bottom of where the piece should be.
	Plan.BuildPositionOffset = -1 * Direction * (AxisOriginOffset - (RelativeOffset / 2));
	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::OrientPartTransform BuildPositionOffset: %s"), *Plan.BuildPositionOffset.ToString());
	
	OutConsumedBuildSpace = RelativeOffset;
	Plan.RotationalPositionOffset = DirectionalOriginOffset;
	Plan.Rotation = DeltaRot;
	Plan.AfterPartPositionOffset = Direction * RelativeOffset;
}

#pragma endregion