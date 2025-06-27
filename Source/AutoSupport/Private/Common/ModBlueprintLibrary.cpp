
#include "ModBlueprintLibrary.h"

#include "BuildableAutoSupport_Types.h"
#include "FGBuildable.h"
#include "FGBuildableHologram.h"
#include "FGCentralStorageSubsystem.h"
#include "FGCharacterPlayer.h"
#include "FGGameUI.h"
#include "FGHologram.h"
#include "FGInventoryLibrary.h"
#include "FGPlayerController.h"
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
	WorkingTransform.SetLocation(Plan.StartWorldLocation);
	WorkingTransform.SetRotation(WorkingTransform.Rotator().Add(Plan.RelativeRotation.Pitch, Plan.RelativeRotation.Yaw, Plan.RelativeRotation.Roll).Quaternion());
	
	AFGHologram* RootHologram = nullptr; // we'll assign this while building
	
	if (Plan.StartPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building Start Part, Orientation: %i"), static_cast<int32>(Plan.StartPart.Orientation));
		SpawnPartPlanHolograms(RootHologram, Plan.StartPart, Plan.BuildWorldDirection, BuildInstigator, Owner, WorkingTransform);
	}

	if (Plan.MidPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building Mid Parts, Orientation: %i"), static_cast<int32>(Plan.EndPart.Orientation));
		SpawnPartPlanHolograms(RootHologram, Plan.MidPart, Plan.BuildWorldDirection, BuildInstigator, Owner, WorkingTransform);
	}

	if (Plan.EndPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building End Part, Orientation: %i"), static_cast<int32>(Plan.EndPart.Orientation));
		WorkingTransform.AddToTranslation(Plan.EndPartPositionOffset);
		
		SpawnPartPlanHolograms(RootHologram, Plan.EndPart, Plan.BuildWorldDirection, BuildInstigator, Owner, WorkingTransform);
	}

	check(RootHologram)

	return RootHologram;
}

void UAutoSupportBlueprintLibrary::PlanBuild(UWorld* World, const FAutoSupportTraceResult& TraceResult, const FBuildableAutoSupportData& AutoSupportData, OUT FAutoSupportBuildPlan& OutPlan)
{
	OutPlan = FAutoSupportBuildPlan();

	// Copy trace result's relative location & rotation.
	OutPlan.StartWorldLocation = TraceResult.StartLocation;
	OutPlan.BuildWorldDirection = TraceResult.Direction;
	
	OutPlan.RelativeLocation = TraceResult.StartRelativeLocation;
	OutPlan.RelativeRotation = TraceResult.StartRelativeRotation;
	
	// Plan the parts.
	auto RemainingBuildDistance = TraceResult.BuildDistance;

	const auto* RecipeManager = AFGRecipeManager::Get(World);
	
	// Start with the top because that's where the auto support is planted.
	if (AutoSupportData.StartPartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("Planning Start Part Positioning"));
		if (PlanSinglePart(AutoSupportData.StartPartDescriptor.Get(), AutoSupportData.StartPartOrientation, AutoSupportData.StartPartCustomization, OutPlan.StartPart, RecipeManager))
		{
			RemainingBuildDistance -= OutPlan.StartPart.ConsumedBuildSpace;

			OutPlan.StartPart.Count = 1;
			MOD_LOG(Verbose, TEXT("Num Start: %d"), OutPlan.StartPart.Count);
		
			if (RemainingBuildDistance < 0)
			{
				return;
			}
		}
	}
	
	// Do the end next. There may not be enough room for mid pieces.
	if (AutoSupportData.EndPartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("Planning End Part Positioning"));
		if (PlanSinglePart(AutoSupportData.EndPartDescriptor.Get(), AutoSupportData.EndPartOrientation, AutoSupportData.EndPartCustomization, OutPlan.EndPart, RecipeManager))
		{
			RemainingBuildDistance -= OutPlan.EndPart.ConsumedBuildSpace;
			
			OutPlan.EndPart.Count = 1;
			MOD_LOG(Verbose, TEXT("Num End: %d"), OutPlan.EndPart.Count);
		
			if (RemainingBuildDistance < 0)
			{
				return;
			}
		}
	}
	
	if (AutoSupportData.MiddlePartDescriptor.IsValid())
	{
		MOD_LOG(Verbose, TEXT("Planning Mid Part Positioning"));
		if (PlanSinglePart(AutoSupportData.MiddlePartDescriptor.Get(), AutoSupportData.MiddlePartOrientation, AutoSupportData.MiddlePartCustomization, OutPlan.MidPart, RecipeManager))
		{
			const auto SinglePartConsumedBuildSpace = OutPlan.MidPart.ConsumedBuildSpace;
			auto NumMiddleParts = static_cast<int32>(RemainingBuildDistance / SinglePartConsumedBuildSpace);
			RemainingBuildDistance -= NumMiddleParts * SinglePartConsumedBuildSpace;

			auto IsNearlyPerfectFit = RemainingBuildDistance <= 1.f;
			if (!IsNearlyPerfectFit)
			{
				NumMiddleParts++; // build an extra to fill the gap.
				
				// Offset the end part to be flush with where the line trace hit or ended. We built an extra part so the direction is negative because we're moving backwards. We don't need to worry about the end part size because it was already subtracted from build distance.
				OutPlan.EndPartPositionOffset = -TraceResult.Direction * (SinglePartConsumedBuildSpace - RemainingBuildDistance);

				MOD_LOG(Verbose, TEXT("Not a perfect fit. Added extra mid part and offsetting end part position by [%s]"), *OutPlan.EndPartPositionOffset.ToString());
			}
			
			OutPlan.MidPart.Count = NumMiddleParts;
			MOD_LOG(Verbose, TEXT("Num Mid: %d, Perfect Fit: %s"), NumMiddleParts, TEXT_CONDITION(IsNearlyPerfectFit));
		}
	}
}

bool UAutoSupportBlueprintLibrary::IsPlanActionable(const FAutoSupportBuildPlan& Plan)
{
	return Plan.IsActionable();
}

bool UAutoSupportBlueprintLibrary::IsPartPlanUnspecified(const FAutoSupportBuildPlanPartData& PartPlan)
{
	return PartPlan.IsUnspecified();
}

bool UAutoSupportBlueprintLibrary::IsPartPlanActionable(const FAutoSupportBuildPlanPartData& PartPlan)
{
	return PartPlan.IsActionable();
}

void UAutoSupportBlueprintLibrary::GetTotalCost(const FAutoSupportBuildPlan& Plan, TArray<FItemAmount>& OutCost)
{
	OutCost.Empty();

	TMap<TSubclassOf<UFGItemDescriptor>, int32> ItemCounts;
	
	if (Plan.StartPart.IsActionable())
	{
		const auto StartCosts = UFGRecipe::GetIngredients(Plan.StartPart.BuildRecipeClass);
		for (const auto& StartCostEnt : StartCosts)
		{
			ItemCounts.Add(StartCostEnt.ItemClass, ItemCounts.FindRef(StartCostEnt.ItemClass) + StartCostEnt.Amount * Plan.StartPart.Count);
		}
	}

	if (Plan.MidPart.IsActionable())
	{
		const auto MidCosts = UFGRecipe::GetIngredients(Plan.MidPart.BuildRecipeClass);
		for (const auto& MidCostEnt : MidCosts)
		{
			ItemCounts.Add(MidCostEnt.ItemClass, ItemCounts.FindRef(MidCostEnt.ItemClass) + MidCostEnt.Amount * Plan.MidPart.Count);
		}
	}

	if (Plan.EndPart.IsActionable())
	{
		const auto EndCosts = UFGRecipe::GetIngredients(Plan.EndPart.BuildRecipeClass);
		for (const auto& EndCostEnt : EndCosts)
		{
			ItemCounts.Add(EndCostEnt.ItemClass, ItemCounts.FindRef(EndCostEnt.ItemClass) + EndCostEnt.Amount * Plan.EndPart.Count);
		}
	}
	
	for (const auto& ItemCountEnt : ItemCounts)
	{
		MOD_LOG(Verbose, TEXT("Item [%s] Count: %d"), *ItemCountEnt.Key->GetName(), ItemCountEnt.Value);
		OutCost.Add(FItemAmount(ItemCountEnt.Key, ItemCountEnt.Value));
	}
}

float UAutoSupportBlueprintLibrary::GetBuryDistance(
	TSubclassOf<AFGBuildable> BuildableClass,
	float BuryPercentage,
	const EAutoSupportBuildDirection PartOrientation)
{
	BuryPercentage = FMath::Clamp(BuryPercentage, 0.f, 0.5f);
	
	if (FMath::IsNearlyZero(BuryPercentage))
	{
		return 0.f;
	}

	FBox PartBBox;
	GetBuildableClearance(BuildableClass, PartBBox);
	const auto PartSize = PartBBox.GetSize();
	
	float Size;
	switch (PartOrientation)
	{
		case EAutoSupportBuildDirection::Left:
		case EAutoSupportBuildDirection::Right:
			Size = PartSize.X;
			break;
		case EAutoSupportBuildDirection::Front:
		case EAutoSupportBuildDirection::Back:
			Size = PartSize.Y;
			break;
		default:
		case EAutoSupportBuildDirection::Top:
		case EAutoSupportBuildDirection::Bottom:
			Size = PartSize.Z;
			break;
	}

	return Size * BuryPercentage;
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
		MOD_LOG(Verbose, TEXT("There's no build cost. Returning true"))
		return true;
	}

	auto* CentralStorageSys = AFGCentralStorageSubsystem::Get(World);

	for (const auto& BillOfPart : BillOfParts)
	{
		const auto InvAvailableNum = Inventory->GetNumItems(BillOfPart.ItemClass);
		
		const auto AvailableNum = bTakeFromDepot
			? InvAvailableNum + CentralStorageSys->GetNumItemsFromCentralStorage(BillOfPart.ItemClass)
			: InvAvailableNum;

		MOD_LOG(Verbose, TEXT("Item [%s] Cost: %i, Available %i"), *BillOfPart.ItemClass->GetName(), BillOfPart.Amount, AvailableNum)

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

int32 UAutoSupportBlueprintLibrary::LeaseWidgetsExact(
	APlayerController* Controller,
	TSubclassOf<UUserWidget> WidgetClass,
	TArray<UUserWidget*>& LeasedWidgetPool,
	int32 Count,
	TArray<UUserWidget*>& OutRemovedWidgets)
{
	OutRemovedWidgets.Empty();
	int32 NewStartIndex = -1;
	
	if (LeasedWidgetPool.Num() == Count)
	{
		return NewStartIndex;
	}

	const auto* FController = CastChecked<AFGPlayerController>(Controller);
	auto* GameUI = FController->GetGameUI();
	
	if (LeasedWidgetPool.Num() < Count)
	{
		NewStartIndex = LeasedWidgetPool.Num();
		for (auto i = NewStartIndex; i < Count; ++i)
		{
			auto* NewWidget = GameUI->RequestWidget(WidgetClass);
			LeasedWidgetPool.Add(NewWidget);
		}
	}
	else
	{
		for (auto i = LeasedWidgetPool.Num() - 1; i >= Count; --i)
		{
			const auto Widget = LeasedWidgetPool[i];
			Widget->RemoveFromParent();
			GameUI->ReleaseWidget(Widget);
			LeasedWidgetPool.RemoveAt(i);
			OutRemovedWidgets.Add(Widget);
		}
	}

	return NewStartIndex;
}

#pragma endregion

#pragma region Private

void UAutoSupportBlueprintLibrary::SpawnPartPlanHolograms(
	AFGHologram*& ParentHologram,
	const FAutoSupportBuildPlanPartData& PartPlan,
	const FVector& TraceDirection,
	APawn* BuildInstigator,
	AActor* Owner,
	FTransform& WorkingTransform)
{
	for (auto i = 0; i < PartPlan.Count; ++i)
	{
		// Copy the transform, then apply the orientation below
		MOD_LOG(Verbose, TEXT("World Part Spawn Transform: [%s]"), *WorkingTransform.ToHumanReadableString());

		if (ParentHologram)
		{
			AFGHologram::SpawnChildHologramFromRecipe(
				ParentHologram,
				FName(FGuid::NewGuid().ToString()),
				PartPlan.BuildRecipeClass,
				Owner,
				WorkingTransform.GetLocation(),
				[&](AFGHologram* PreSpawnHolo)
				{
					PreSpawnHolo->SetActorRotation(WorkingTransform.GetRotation());

					PreSpawnHolo->AddActorLocalRotation(PartPlan.DeltaRotation);;
					PreSpawnHolo->AddActorLocalOffset(PartPlan.PostRotationLocalTranslation);

					PreSpawnHolo->DoMultiStepPlacement(false);

					// auto* AsBuildableHolo = CastChecked<AFGBuildableHologram>(PreSpawnHolo);
					// AsBuildableHolo->SetCustomizationData(PartPlan.CustomizationData);
				});
		}
		else
		{
			ParentHologram = AFGHologram::SpawnHologramFromRecipe(
				PartPlan.BuildRecipeClass,
				Owner,
				WorkingTransform.GetLocation(),
				BuildInstigator,
				[&](AFGHologram* PreSpawnHolo)
				{
					PreSpawnHolo->SetActorRotation(WorkingTransform.GetRotation());

					PreSpawnHolo->AddActorLocalRotation(PartPlan.DeltaRotation);
					PreSpawnHolo->AddActorLocalOffset(PartPlan.PostRotationLocalTranslation);

					PreSpawnHolo->DoMultiStepPlacement(false);
					PreSpawnHolo->SetShouldSpawnChildHolograms(true);

					// auto* AsBuildableHolo = CastChecked<AFGBuildableHologram>(PreSpawnHolo);
					// AsBuildableHolo->SetCustomizationData(PartPlan.CustomizationData);
				});
		}
		
		// Update the transform
		WorkingTransform.AddToTranslation(TraceDirection * PartPlan.ConsumedBuildSpace);
		MOD_LOG(Verbose, TEXT("Next Transform: [%s]"), *WorkingTransform.ToHumanReadableString());
	}
}

bool UAutoSupportBlueprintLibrary::PlanSinglePart(
	TSubclassOf<UFGBuildingDescriptor> PartDescriptorClass,
	const EAutoSupportBuildDirection PartOrientation,
	const FFactoryCustomizationData& PartCustomization,
	FAutoSupportBuildPlanPartData& Plan,
	const AFGRecipeManager* RecipeManager)
{
	Plan.PartDescriptorClass = PartDescriptorClass;
	Plan.BuildableClass = UFGBuildingDescriptor::GetBuildableClass(PartDescriptorClass);
	Plan.Orientation = PartOrientation;
	Plan.CustomizationData = PartCustomization;
	GetBuildableClearance(Plan.BuildableClass, Plan.BBox);
	
	PlanPartPositioning(Plan.BBox, PartOrientation, Plan);

	// Should only be 1 recipe for a buildable...
	auto PartRecipeClasses = RecipeManager->FindRecipesByProduct(Plan.PartDescriptorClass, true, true);
	check(PartRecipeClasses.Num() <= 1);

	if (const auto PartRecipeClass = PartRecipeClasses.Num() > 0 ? PartRecipeClasses[0] : nullptr; PartRecipeClass && Plan.ConsumedBuildSpace > 0)
	{
		Plan.BuildRecipeClass = PartRecipeClass;
		Plan.Count = 1;
		return true;
	}

	Plan.Count = 0;
	return false;
}

void UAutoSupportBlueprintLibrary::PlanPartPositioning(
	const FBox& PartBBox,
	const EAutoSupportBuildDirection PartOrientation,
	FAutoSupportBuildPlanPartData& Plan)
{
	// Reminders:
	// - Roll = X, Pitch = Y, Yaw = Z
	// - The world transform we're building along is at the point closest to the trace origin. The space proceeding it is what we will occupy here.
	// Assertions working in local space:
	// - A part's UP is +Z. The local trace direction is fixed to +Z as well.
	// - If the part's BBox center is 0,0,0, its origin = buildable actor pivot.
	
	const auto PartSize = PartBBox.GetSize();
	const auto ActorOriginCenterOffset = PartBBox.GetCenter(); // Ex: (0,0,0) for mesh centered at buildable actor pivot. (0, 0, 200) for mesh bottom aligned with actor pivot.
	const auto LocalTraceDirection = FVector::UpVector;

	// First, rotate the part.
	const auto DeltaRot = GetDirectionRotator(PartOrientation);
	Plan.DeltaRotation = DeltaRot;

	// Next, apply translations. We need to fit correctly in the space we're supposed to occupy in the build, while respecting the differing
	// sizes of the part along X,Y,Z as well as the mesh's pivot relative to the buildable actor that will be placed.
	const auto RotatedSize = DeltaRot.RotateVector(PartSize).GetAbs();
	const auto RotatedActorOriginCenterOffset = DeltaRot.RotateVector(ActorOriginCenterOffset);
	const auto RotatedTraceDirection = DeltaRot.RotateVector(LocalTraceDirection);
	const auto DeltaSize = RotatedSize - PartSize;
	const auto DeltaActorOriginCenterOffset = RotatedActorOriginCenterOffset - ActorOriginCenterOffset;
	const auto BottomingOffset = -DeltaRot.RotateVector(PartBBox.Min); // TODO(k.a): check this again
	
	Plan.ConsumedBuildSpace = RotatedSize.Z; // Z is rel trace direction
	
	// The first translation is undoing the "translation" done because of rotation on the TRACE AXIS.
	//   Example: Pillar piece, the buildable actor pivot is 0,0,0 and mesh origin is 0,0,0, but we need to be built relative to bottom, so we need to go up +Z by -minZ
	//   Example: Wall piece, the buildable actor pivot is 0,0,0 but the mesh is bottom aligned with the pivot. The size of the part is 800,10,400 making the origin offset from actor pivot 0,0,200.
	//     If we don't rotate: Do nothing, we're already where we should be
	//     If we rotate right: The rotated size is 400,10,800. Our size DOUBLED along our travel axis, so we need to push the part up +Z by half (and thus the rest of the parts we'll build)
	//     If we rotate to top (180deg): The rotated size is still 800,800,400, but now we are offset -400 Z of the occupying space.
	Plan.PostRotationLocalTranslation = LocalTraceDirection * (DeltaSize + DeltaActorOriginCenterOffset) + LocalTraceDirection * BottomingOffset;

	if (PartOrientation != EAutoSupportBuildDirection::Top && PartOrientation != EAutoSupportBuildDirection::Bottom)
	{
		// The second translation is undoing the "translation" done because of rotation on the AXIS PERPENDICULAR TO TRACE.
		//   Example: Pillar piece, the buildable actor pivot is 0,0,0 and mesh origin is 0,0,0, so we don't need to do anything here.
		//   Example: Wall piece, the buildable actor pivot is 0,0,0 but the mesh is bottom aligned with the pivot. The size of the part is 800,10,400 making the origin offset from actor pivot 0,0,200.
		//     If we don't rotate: Do nothing, we're already where we should be
		//     If we rotate right: The rotated size is 400,10,800. Our size HALVED along our perpendicular X axis, so we need to push the part by its size towards -X (this does not affect other parts)
		//     If we rotate to top: The rotated size is still 800,10,400, but we are still where we should be horizontally. 
		Plan.PostRotationLocalTranslation += -1 * RotatedTraceDirection * (DeltaSize + DeltaActorOriginCenterOffset) + -1 * RotatedTraceDirection * BottomingOffset;
	}
	
	MOD_LOG(Verbose, TEXT("Origin Offset: [%s], BBox Min [%s], BBox Max: [%s]"), *ActorOriginCenterOffset.ToCompactString(), *PartBBox.Min.ToCompactString(), *PartBBox.Max.ToCompactString());
	MOD_LOG(Verbose, TEXT("Extent: [%s], DeltaRotation: [%s]"), *PartBBox.GetExtent().ToCompactString(), *DeltaRot.ToCompactString());
	MOD_LOG(Verbose, TEXT("DeltaSize: [%s], DeltaActorOriginCenterOffset: [%s], BottomingOffset: [%s]"), *DeltaSize.ToCompactString(), *DeltaActorOriginCenterOffset.ToCompactString(), *BottomingOffset.ToCompactString());
	MOD_LOG(Verbose, TEXT("ConsumedBuildSpace: [%f], PostRotationLocalTranslation: [%s]"), Plan.ConsumedBuildSpace, *Plan.PostRotationLocalTranslation.ToCompactString());
}

#pragma endregion