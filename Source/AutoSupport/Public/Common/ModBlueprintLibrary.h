#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupport_Types.h"
#include "FGBuildable.h"
#include "FGRecipeManager.h"
#include "ModTypes.h"
#include "ModBlueprintLibrary.generated.h"

class UFGBuildingDescriptor;
class UPanelSlot;

UCLASS()
class AUTOSUPPORT_API UAutoSupportBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

#pragma region Building Helpers
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static EAutoSupportBuildDirection GetOppositeDirection(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static FVector GetDirectionVector(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static FRotator GetDirectionRotator(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static void GetBuildableClearance(TSubclassOf<AFGBuildable> BuildableClass, FBox& OutBox);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static void PlanBuild(UWorld* World, const FAutoSupportTraceResult& TraceResult, const FBuildableAutoSupportData& AutoSupportData, FAutoSupportBuildPlan& OutPlan);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static AFGHologram* CreateCompositeHologramFromPlan(const FAutoSupportBuildPlan& Plan, const FTransform& Transform, APawn* BuildInstigator, AActor* Owner);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool IsPlanActionable(const FAutoSupportBuildPlan& Plan);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool IsPartPlanUnspecified(const FAutoSupportBuildPlanPartData& PartPlan);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool IsPartPlanActionable(const FAutoSupportBuildPlanPartData& PartPlan);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static void GetTotalCost(const FAutoSupportBuildPlan& Plan, TArray<FItemAmount>& OutCost);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static float GetBuryDistance(TSubclassOf<AFGBuildable> BuildableClass, float BuryPercentage, EAutoSupportBuildDirection PartOrientation);

#pragma endregion

#pragma region Inventory Helpers
	
	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool CanAffordItemBill(AFGCharacterPlayer* Player, const TArray<FItemAmount>& BillOfParts, bool bTakeFromDepot);
	
	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static void PayItemBill(AFGCharacterPlayer* Player, const TArray<FItemAmount>& BillOfParts, bool bTakeFromDepot, bool bTakeFromInventoryFirst);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool PayItemBillIfAffordable(AFGCharacterPlayer* Player, const TArray<FItemAmount>& BillOfParts, bool bTakeFromDepot);

#pragma endregion 

private:
	
	static void SpawnPartPlanHolograms(
		AFGHologram*& ParentHologram,
		const FAutoSupportBuildPlanPartData& PartPlan,
		const FVector& TraceDirection,
		APawn* BuildInstigator,
		AActor* Owner,
		FTransform& WorkingTransform);
	
	static bool PlanSinglePart(
		TSubclassOf<UFGBuildingDescriptor> PartDescriptorClass,
		EAutoSupportBuildDirection PartOrientation,
		FAutoSupportBuildPlanPartData& Plan,
		const AFGRecipeManager* RecipeManager);
	
	static void PlanPartPositioning(
		const FBox& PartBBox,
		EAutoSupportBuildDirection PartOrientation,
		FAutoSupportBuildPlanPartData& Plan);
};