
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
			return FVector(0, 1, 0);
		case EAutoSupportBuildDirection::Back:
			return FVector(0, -1, 0);
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
			DeltaRot.Roll = -90;
			break;
		case EAutoSupportBuildDirection::Back:
			DeltaRot.Roll = 90;
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
		case EAutoSupportBuildDirection::Back:
			// No-op
			break;
		case EAutoSupportBuildDirection::Front:
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
	fgcheck(BuildInstigator)

	// TODO(k.a): understand the rotation calculation better. I trialed and errored for a bit. Need a visualization.
	// Rotation: Calculate the world rot from the following rotations. Reminder: right most are applied first.
	// NOTE: Changing rotation between SpawnActorDeferred and FinishingSpawning can be a recipe for disaster.
	const auto WorldRot =
		(Parent ? Parent->GetActorRotation().Quaternion() : FQuat::Identity) // world rotation
		* GetForwardVectorRotator(Plan.BuildDirection).Quaternion() // forward vector rotation of the build direction, but relative to the build dir rot
		* Plan.RelativeRotation; // relative rotation of the build direction

	const FTransform ProxyTransform(WorldRot, Plan.StartWorldLocation);
	
	// Spawn a proxy container for the parts.
	auto* SupportProxy = BuildInstigator->GetWorld()->SpawnActorDeferred<ABuildableAutoSupportProxy>(
		ProxyClass,
		ProxyTransform, // place it at the origin of the trace
		nullptr,
		BuildInstigator);
	SupportProxy->bIsNewlySpawned = true;

	FBox LocalBoundingBox(ForceInit);

	OutProxy = SupportProxy;
	
	// Build the parts
	auto WorkingTransform = FTransform::Identity; // Build the holograms in relative space
	AFGHologram* RootHologram = nullptr; // we'll assign this while building
	
	if (Plan.StartPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building Start Part, Orientation: %i"), static_cast<int32>(Plan.StartPart.Orientation));
		SpawnPartPlanHolograms(RootHologram, Plan.StartPart, BuildInstigator, SupportProxy, Owner, WorkingTransform, LocalBoundingBox);
	}

	if (Plan.MidPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building Mid Parts, Orientation: %i"), static_cast<int32>(Plan.EndPart.Orientation));
		SpawnPartPlanHolograms(RootHologram, Plan.MidPart, BuildInstigator, SupportProxy, Owner, WorkingTransform, LocalBoundingBox);
	}

	if (Plan.EndPart.IsActionable())
	{
		MOD_LOG(Verbose, TEXT("Building End Part, Orientation: %i"), static_cast<int32>(Plan.EndPart.Orientation));
		WorkingTransform.AddToTranslation(FVector::UpVector * Plan.EndPartPositionOffset);
		
		SpawnPartPlanHolograms(RootHologram, Plan.EndPart, BuildInstigator, SupportProxy, Owner, WorkingTransform, LocalBoundingBox);
	}

	fgcheck(RootHologram)

	MOD_LOG(Verbose, TEXT("Local Bounding Box: %s"), *LocalBoundingBox.ToString())

	LocalBoundingBox.IsValid = true; // ...
	
	SupportProxy->UpdateBoundingBox(LocalBoundingBox);

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

	if (TraceResult.Disqualifier)
	{
		OutPlan.BuildDisqualifiers.Add(TraceResult.Disqualifier);
		return;
	}
	else if (FMath::IsNearlyZero(RemainingBuildDistance))
	{
		OutPlan.BuildDisqualifiers.Add(UAutoSupportConstructDisqualifier_NotEnoughRoom::StaticClass());
		return;
	}

	auto bAtLeastOneInitialized = false;
	const auto* RecipeManager = AFGRecipeManager::Get(World);
	
	// Start with the top because that's where the auto support is planted.
	if (InitializePartPlan(AutoSupportData.StartPartDescriptor, AutoSupportData.StartPartOrientation, AutoSupportData.StartPartCustomization, OutPlan.StartPart, RecipeManager))
	{
		bAtLeastOneInitialized = true;
		
		if (RemainingBuildDistance > OutPlan.StartPart.ConsumedBuildSpace || FMath::IsNearlyEqual(RemainingBuildDistance, OutPlan.StartPart.ConsumedBuildSpace))
		{
			RemainingBuildDistance -= OutPlan.StartPart.ConsumedBuildSpace;
			OutPlan.StartPart.Count = 1;
			MOD_TRACE_LOG(Verbose, TEXT("Num Start: %d"), OutPlan.StartPart.Count);
		}
		else
		{
			OutPlan.BuildDisqualifiers.Add(UAutoSupportConstructDisqualifier_NotEnoughRoom::StaticClass());
			return;
		}
	}
	else
	{
		MOD_TRACE_LOG(Verbose, TEXT("Start part is not specified or is not valid."))
	}
	
	// Do the end next. There may not be enough room for mid pieces.
	if (InitializePartPlan(AutoSupportData.EndPartDescriptor, AutoSupportData.EndPartOrientation, AutoSupportData.EndPartCustomization, OutPlan.EndPart, RecipeManager))
	{
		bAtLeastOneInitialized = true;
		
		if (RemainingBuildDistance > OutPlan.EndPart.ConsumedBuildSpace || FMath::IsNearlyEqual(RemainingBuildDistance, OutPlan.EndPart.ConsumedBuildSpace))
		{
			RemainingBuildDistance -= OutPlan.EndPart.ConsumedBuildSpace;
			OutPlan.EndPart.Count = 1;
			MOD_TRACE_LOG(Verbose, TEXT("Num End: %d"), OutPlan.EndPart.Count);
		}
		else
		{
			MOD_TRACE_LOG(Verbose, TEXT("Not enough room for end part. Not building."))
		}
	}
	else
	{
		MOD_TRACE_LOG(Verbose, TEXT("End part is not specified or is not valid."))
	}
	
	if (InitializePartPlan(AutoSupportData.MiddlePartDescriptor, AutoSupportData.MiddlePartOrientation, AutoSupportData.MiddlePartCustomization, OutPlan.MidPart, RecipeManager))
	{
		bAtLeastOneInitialized = true;
		
		const auto SinglePartConsumedBuildSpace = FMath::Max(1.f, OutPlan.MidPart.ConsumedBuildSpace);
		auto NumMiddleParts = static_cast<int32>(RemainingBuildDistance / SinglePartConsumedBuildSpace);

		if (!FMath::IsNearlyZero(RemainingBuildDistance))
		{
			RemainingBuildDistance -= NumMiddleParts * SinglePartConsumedBuildSpace;

			auto IsNearlyPerfectFit = RemainingBuildDistance <= 1.f;
			if (!IsNearlyPerfectFit)
			{
				NumMiddleParts++; // build an extra to fill the gap.
			
				// Offset the end part to be flush with where the line trace hit or ended. We built an extra part so the direction is negative because we're moving backwards. We don't need to worry about the end part size because it was already subtracted from build distance.
				OutPlan.EndPartPositionOffset = -1 * (SinglePartConsumedBuildSpace - RemainingBuildDistance);

				MOD_TRACE_LOG(Verbose, TEXT("Not a perfect fit. Added extra mid part and offsetting end part position by [%f]"), OutPlan.EndPartPositionOffset);
			}
		
			OutPlan.MidPart.Count = NumMiddleParts;
			MOD_TRACE_LOG(Verbose, TEXT("Num Mid: %d, Perfect Fit: %s"), NumMiddleParts, TEXT_CONDITION(IsNearlyPerfectFit));
		}
		else
		{
			MOD_TRACE_LOG(Verbose, TEXT("Not enough room for middle part. Not building."))
		}
	}
	else
	{
		MOD_TRACE_LOG(Verbose, TEXT("Middle part is not specified or is not valid."))
	}

	CalculateTotalCost(OutPlan);

	if (bAtLeastOneInitialized && OutPlan.StartPart.Count == 0 && OutPlan.MidPart.Count == 0 && OutPlan.EndPart.Count == 0)
	{
		OutPlan.BuildDisqualifiers.Add(UAutoSupportConstructDisqualifier_NotEnoughRoom::StaticClass());
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

void UAutoSupportBlueprintLibrary::CalculateTotalCost(FAutoSupportBuildPlan& Plan)
{
	Plan.ItemBill.Empty();

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
		Plan.ItemBill.Add(FItemAmount(ItemCountEnt.Key, ItemCountEnt.Value));
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
	const bool bTakeFromDepot)
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

		MOD_LOG(Verbose, TEXT("Item [%s] Cost: %i, Available: %i"), *BillOfPart.ItemClass->GetName(), BillOfPart.Amount, AvailableNum)

		if (AvailableNum < BillOfPart.Amount)
		{
			MOD_LOG(Verbose, TEXT("Cannot afford."))
			return false;
		}
	}

	MOD_LOG(Verbose, TEXT("Can afford."))
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

#pragma region UI Helpers

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

bool UAutoSupportBlueprintLibrary::IsValidAutoSupportPresetName(FString PresetName, FText& OutError)
{
	OutError = FText::GetEmpty();
	
	if (PresetName.IsEmpty())
	{
		OutError = FText::FromString("Preset name cannot be empty.");
		return false;
	}

	if (PresetName.Len() > 50)
	{
		OutError = FText::FromString("Preset name cannot be longer than 50 characters.");
		return false;
	}

	if (PresetName.StartsWith(TEXT("[")))
	{
		OutError = FText::FromString("Preset name cannot start with '['.");
		return false;
	}

	return true;
}

#pragma endregion

#pragma region Private

void UAutoSupportBlueprintLibrary::SpawnPartPlanHolograms(
	AFGHologram*& ParentHologram,
	const FAutoSupportBuildPlanPartData& PartPlan,
	APawn* BuildInstigator,
	AActor* Parent,
	AActor* Owner,
	FTransform& WorkingTransform,
	FBox& WorkingBBox)
{
	auto PreSpawnFn = [&](AFGHologram* PreSpawnHolo)
	{
		PreSpawnHolo->AddActorWorldRotation(PartPlan.LocalRotation);
		PreSpawnHolo->AddActorWorldOffset(PartPlan.LocalTranslation);
		PreSpawnHolo->DoMultiStepPlacement(false);
	};

	const auto PartExtent = PartPlan.BBox.GetExtent();
	
	WorkingBBox.Min.X = FMath::Min(WorkingBBox.Min.X, -PartExtent.X);
	WorkingBBox.Min.Y = FMath::Min(WorkingBBox.Min.Y, -PartExtent.Y);
		
	WorkingBBox.Max.X = FMath::Max(WorkingBBox.Max.X, PartExtent.X);
	WorkingBBox.Max.Y = FMath::Max(WorkingBBox.Max.Y, PartExtent.Y);
	
	for (auto i = 0; i < PartPlan.Count; ++i)
	{
		// Copy the transform, then apply the orientation below
		MOD_LOG(Verbose, TEXT("World Part Spawn Transform: [%s]"), *WorkingTransform.ToHumanReadableString());
		
		if (ParentHologram)
		{
			auto* Hologram = AFGHologram::SpawnChildHologramFromRecipe(
				ParentHologram,
				FName(FGuid::NewGuid().ToString()),
				PartPlan.BuildRecipeClass,
				Owner,
				WorkingTransform.GetLocation(),
				PreSpawnFn);

			Hologram->AttachToActor(Parent, FAttachmentTransformRules::KeepRelativeTransform);
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

		// Update the BBox
		WorkingBBox.Max.Z += PartPlan.ConsumedBuildSpace;
		
		// Update the transform
		WorkingTransform.AddToTranslation(FVector::UpVector * PartPlan.ConsumedBuildSpace); 
		
		MOD_LOG(Verbose, TEXT("Next Transform: [%s]"), *WorkingTransform.ToHumanReadableString());
	}
}

bool UAutoSupportBlueprintLibrary::InitializePartPlan(
	const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptorClass,
	const EAutoSupportBuildDirection PartOrientation,
	const FFactoryCustomizationData& PartCustomization,
	FAutoSupportBuildPlanPartData& PartPlan,
	const AFGRecipeManager* RecipeManager)
{
	PartPlan = FAutoSupportBuildPlanPartData();
	PartPlan.Orientation = PartOrientation;
	PartPlan.CustomizationData = PartCustomization;

	if (!PartDescriptorClass.IsValid())
	{
		return false;
	}
	
	PartPlan.PartDescriptorClass = PartDescriptorClass.Get();
	PartPlan.BuildableClass = UFGBuildingDescriptor::GetBuildableClass(PartPlan.PartDescriptorClass);

	GetBuildableClearance(PartPlan.BuildableClass, PartPlan.BBox);
	
	PlanPartPositioning(PartPlan.BBox, PartOrientation, PartPlan);

	// Should only be 1 recipe for a buildable...
	auto PartRecipeClasses = RecipeManager->FindRecipesByProduct(PartPlan.PartDescriptorClass, true, true);
	fgcheck(PartRecipeClasses.Num() <= 1);

	PartPlan.BuildRecipeClass = PartRecipeClasses.Num() > 0 ? PartRecipeClasses[0] : nullptr;

	return PartPlan.BuildRecipeClass && PartPlan.ConsumedBuildSpace > 0.f;
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
	const auto Center = PartBBox.GetCenter(); // Ex: (0,0,0) for mesh centered at buildable actor pivot. (0, 0, 200) for mesh bottom aligned with actor pivot.

	// First, we'll rotate the part.
	const auto LocalRotation = GetDirectionRotator(PartOrientation).Quaternion();
	Plan.LocalRotation = LocalRotation;

	// Next, determine the translation we need to take to plop it back in the correct location. The goal is to center the new BBox on the
	// local X and Y while aligning the min to Z = 0.
	const auto RotatedBBoxMin = LocalRotation.RotateVector(PartBBox.Min);
	const auto RotatedBBoxMax = LocalRotation.RotateVector(PartBBox.Max);
	const auto RotatedBBox = FBox(FVector::Min(RotatedBBoxMin, RotatedBBoxMax), FVector::Max(RotatedBBoxMin, RotatedBBoxMax));
	const auto RotatedCenter = RotatedBBox.GetCenter();
	const auto RotatedSize = LocalRotation.RotateVector(PartSize).GetAbs();
	const auto DeltaSize = RotatedSize - PartSize;

	// Center ourselves x and y, bottom align Z.
	auto LocalTranslation = FVector(-RotatedCenter.X, -RotatedCenter.Y, -RotatedBBox.Min.Z);
	
	Plan.LocalTranslation = LocalTranslation;
	Plan.ConsumedBuildSpace = RotatedSize.Z; // Z is rel trace direction
	Plan.BBox = RotatedBBox;
	
	MOD_TRACE_LOG(Verbose, TEXT("Center: [%s], BBoxMin [%s], BBoxMax: [%s], Extent: [%s]"), TEXT_STR(Center.ToCompactString()), TEXT_STR(PartBBox.Min.ToCompactString()), TEXT_STR(PartBBox.Max.ToCompactString()), TEXT_STR(PartBBox.GetExtent().ToCompactString()));
	MOD_TRACE_LOG(Verbose, TEXT("RotatedCenter: [%s], RotatedBBoxMin [%s], RotatedBBoxMax: [%s], RotatedExtent: [%s]"), TEXT_STR(RotatedCenter.ToCompactString()), TEXT_STR(RotatedBBox.Min.ToCompactString()), TEXT_STR(RotatedBBox.Max.ToCompactString()), TEXT_STR(RotatedBBox.GetExtent().ToCompactString()));
	MOD_TRACE_LOG(Verbose, TEXT("DeltaSize: [%s], ConsumedBuildSpace: [%f]"), TEXT_STR(DeltaSize.ToCompactString()), Plan.ConsumedBuildSpace);
	MOD_TRACE_LOG(Verbose, TEXT("LocalTranslation: [%s], LocalRotation: [%s]"), TEXT_STR(Plan.LocalTranslation.ToCompactString()), TEXT_STR(Plan.LocalRotation.Rotator().ToCompactString()))
}

#pragma endregion