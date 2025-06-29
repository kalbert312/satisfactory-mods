
#include "ModBlueprintLibrary.h"

#include "BuildableAutoSupportProxy.h"
#include "BuildableAutoSupport_Types.h"
#include "FGBuildable.h"
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
		default:
			break;
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

FRotator UAutoSupportBlueprintLibrary::GetForwardVectorRotator(const EAutoSupportBuildDirection Direction)
{
	FRotator DeltaRot(0, 0, 0);
	
	switch (Direction)
	{
		default:
		case EAutoSupportBuildDirection::Bottom:
		case EAutoSupportBuildDirection::Top:
		case EAutoSupportBuildDirection::Front:
			// No-op
			break;
		case EAutoSupportBuildDirection::Back:
			DeltaRot.Pitch = 180;
			break;
		case EAutoSupportBuildDirection::Left:
			DeltaRot.Roll = -90;
			break;
		case EAutoSupportBuildDirection::Right:
			DeltaRot.Roll = -90;
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
	const FAutoSupportBuildPlan& Plan,
	TSubclassOf<ABuildableAutoSupportProxy> ProxyClass,
	APawn* BuildInstigator,
	AActor* Parent,
	AActor* Owner,
	ABuildableAutoSupportProxy*& OutProxy)
{
	check(BuildInstigator)

	// Spawn a proxy container for the parts.
	auto* SupportProxy = BuildInstigator->GetWorld()->SpawnActorDeferred<ABuildableAutoSupportProxy>(
		ProxyClass,
		FTransform(Plan.StartWorldLocation), // place it at the origin of the trace
		nullptr,
		BuildInstigator);

	OutProxy = SupportProxy;
	
	// Build the parts
	auto WorkingTransform = FTransform::Identity; // Build the holograms in relative space
	AFGHologram* RootHologram = nullptr; // we'll assign this while building
	
	if (Plan.StartPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building Start Part, Orientation: %i"), static_cast<int32>(Plan.StartPart.Orientation));
		SpawnPartPlanHolograms(RootHologram, Plan.StartPart, BuildInstigator, SupportProxy, Owner, WorkingTransform);
	}

	if (Plan.MidPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building Mid Parts, Orientation: %i"), static_cast<int32>(Plan.EndPart.Orientation));
		SpawnPartPlanHolograms(RootHologram, Plan.MidPart, BuildInstigator, SupportProxy, Owner, WorkingTransform);
	}

	if (Plan.EndPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building End Part, Orientation: %i"), static_cast<int32>(Plan.EndPart.Orientation));
		WorkingTransform.AddToTranslation(FVector::UpVector * Plan.EndPartPositionOffset);
		
		SpawnPartPlanHolograms(RootHologram, Plan.EndPart, BuildInstigator, SupportProxy, Owner, WorkingTransform);
	}

	check(RootHologram)

	// TODO(k.a): understand the rotation calculation better. I trailed and errored for a bit. Need a visualization.
	
	// Rotation: Apply the rotation of the parent actor.
	const auto WorldRot = Parent ? Parent->GetActorRotation().Quaternion() : FQuat::Identity;
	SupportProxy->SetActorRotation(WorldRot);

	// ...then relative rotation of the build direction.
	// ...then forward vector rotation of the build direction, but relative to the build dir rot.
	const auto LocalRot = GetForwardVectorRotator(Plan.BuildDirection).Quaternion() * Plan.RelativeRotation;
	SupportProxy->AddActorLocalRotation(LocalRot);

	return RootHologram;
}

void UAutoSupportBlueprintLibrary::PlanBuild(UWorld* World, const FAutoSupportTraceResult& TraceResult, const FBuildableAutoSupportData& AutoSupportData, OUT FAutoSupportBuildPlan& OutPlan)
{
	OutPlan = FAutoSupportBuildPlan();

	// Copy trace result's relative location & rotation.
	OutPlan.StartWorldLocation = TraceResult.StartLocation;
	OutPlan.RelativeLocation = TraceResult.StartRelativeLocation;
	OutPlan.RelativeRotation = TraceResult.StartRelativeRotation;
	
	OutPlan.BuildDirection = TraceResult.BuildDirection;
	OutPlan.BuildWorldDirection = TraceResult.Direction;
	
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
				OutPlan.EndPartPositionOffset = -1 * (SinglePartConsumedBuildSpace - RemainingBuildDistance);

				MOD_LOG(Verbose, TEXT("Not a perfect fit. Added extra mid part and offsetting end part position by [%f]"), OutPlan.EndPartPositionOffset);
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
	APawn* BuildInstigator,
	AActor* Parent,
	AActor* Owner,
	FTransform& WorkingTransform)
{
	auto PreSpawnFn = [&](AFGHologram* PreSpawnHolo)
	{
		PreSpawnHolo->AddActorLocalOffset(PartPlan.LocalTranslation);
		PreSpawnHolo->AddActorLocalRotation(PartPlan.LocalRotation);
		PreSpawnHolo->DoMultiStepPlacement(false);
		// auto* AsBuildableHolo = CastChecked<AFGBuildableHologram>(PreSpawnHolo);
		// AsBuildableHolo->SetCustomizationData(PartPlan.CustomizationData);
	};
	
	for (auto i = 0; i < PartPlan.Count; ++i)
	{
		// Copy the transform, then apply the orientation below
		MOD_LOG(Verbose, TEXT("World Part Spawn Transform: [%s]"), *WorkingTransform.ToHumanReadableString());
		
		if (ParentHologram)
		{
			auto* Child = AFGHologram::SpawnChildHologramFromRecipe(
				ParentHologram,
				FName(FGuid::NewGuid().ToString()),
				PartPlan.BuildRecipeClass,
				Owner,
				WorkingTransform.GetLocation(),
				PreSpawnFn);

			Child->AttachToActor(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		}
		else
		{
			ParentHologram = AFGHologram::SpawnHologramFromRecipe(
				PartPlan.BuildRecipeClass,
				Owner,
				WorkingTransform.GetLocation(),
				BuildInstigator,
				PreSpawnFn);

			ParentHologram->SetShouldSpawnChildHolograms(true);
			ParentHologram->AttachToActor(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		}
		
		// Update the transform
		WorkingTransform.AddToTranslation(FVector::UpVector * PartPlan.ConsumedBuildSpace); // local space dir is UP.
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

	// First, rotate the part.
	const auto DeltaRot = GetDirectionRotator(PartOrientation).Quaternion();
	Plan.LocalRotation = DeltaRot;

	// Next, apply translations. We need to fit correctly in the space we're supposed to occupy in the build, while respecting the differing
	// sizes of the part along X,Y,Z as well as the mesh's pivot relative to the buildable actor that will be placed.
	const auto RotatedSize = DeltaRot.RotateVector(PartSize).GetAbs();
	const auto DeltaSize = RotatedSize - PartSize;
	
	Plan.ConsumedBuildSpace = RotatedSize.Z; // Z is rel trace direction

	auto LocalParallelOffset = PartOrientation == EAutoSupportBuildDirection::Bottom || PartOrientation == EAutoSupportBuildDirection::Top
		? -PartBBox.Min.Z // align the bottom of the part with transform we'll be working off of. 
		: 0; 
	LocalParallelOffset += FMath::IsNearlyZero(DeltaSize.Z) || DeltaSize.Z >= 0 ? DeltaSize.Z : (PartSize.Z + DeltaSize.Z) / 2.f;
	FVector LocalTranslation = FVector(0, 0, LocalParallelOffset);

	if (!ActorOriginCenterOffset.IsNearlyZero())
	{
		switch (PartOrientation)
		{
			case EAutoSupportBuildDirection::Front:
				LocalTranslation.Y = -1 * PartSize.Y / 2.f;
				break;
			case EAutoSupportBuildDirection::Back:
				LocalTranslation.Y = PartSize.Y / 2.f;
				break;
			case EAutoSupportBuildDirection::Left:
				LocalTranslation.X = PartSize.Z / 2.f;
				break;
			case EAutoSupportBuildDirection::Right:
				LocalTranslation.X = -1 * PartSize.Z / 2.f;
				break;
			default:
				break;
		}
	}
	
	Plan.LocalTranslation = LocalTranslation;
	
	MOD_LOG(Verbose, TEXT("Origin Offset: [%s], BBox Min [%s], BBox Max: [%s]"), *ActorOriginCenterOffset.ToCompactString(), *PartBBox.Min.ToCompactString(), *PartBBox.Max.ToCompactString());
	MOD_LOG(Verbose, TEXT("Extent: [%s], DeltaRotation: [%s]"), *PartBBox.GetExtent().ToCompactString(), *DeltaRot.ToString());
	MOD_LOG(Verbose, TEXT("DeltaSize: [%s], ConsumedBuildSpace: [%f]"), *DeltaSize.ToCompactString(), Plan.ConsumedBuildSpace);
	MOD_LOG(Verbose, TEXT("LocalTranslation: [%s], LocalTranslation: [%s]"), TEXT_STR(LocalTranslation.ToCompactString()), TEXT_STR(Plan.LocalTranslation.ToCompactString()))
}

#pragma endregion