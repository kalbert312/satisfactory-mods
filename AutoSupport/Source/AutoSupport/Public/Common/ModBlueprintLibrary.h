#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupport_Types.h"
#include "FGBuildable.h"
#include "FGRecipeManager.h"
#include "ModTypes.h"
#include "ModBlueprintLibrary.generated.h"

class UAutoSupportPartPickerConfigModule;
class UAutoSupportBuildConfigModule;
class UFGBuildingDescriptor;
class UPanelSlot;

UCLASS()
class AUTOSUPPORT_API UAutoSupportBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

#pragma region Modules and Subsystems

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport", meta = (DefaultToSelf = "WorldContext"))
	static UAutoSupportBuildConfigModule* GetBuildConfigModule(const UObject* WorldContext);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport", meta = (DefaultToSelf = "WorldContext"))
	static UAutoSupportPartPickerConfigModule* GetPartPickerConfigModule(const UObject* WorldContext);
	
#pragma endregion

#pragma region Building Helpers
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static EAutoSupportBuildDirection GetOppositeDirection(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static FVector GetDirectionVector(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static FRotator GetDirectionRotator(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static FRotator GetForwardVectorRotator(EAutoSupportBuildDirection Direction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AutoSupport")
	static void GetBuildableClearance(TSubclassOf<AFGBuildable> BuildableClass, FBox& OutBox);
	
	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static void PlanBuild(UWorld* World, const FAutoSupportTraceResult& TraceResult, const FBuildableAutoSupportData& AutoSupportData, FAutoSupportBuildPlan& OutPlan);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static AFGHologram* CreateCompositeHologramFromPlan(
		const FAutoSupportBuildPlan& Plan,
		TSubclassOf<ABuildableAutoSupportProxy> ProxyClass,
		APawn* BuildInstigator,
		AActor* Parent,
		AActor* Owner,
		ABuildableAutoSupportProxy*& OutProxy);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool IsPlanActionable(const FAutoSupportBuildPlan& Plan);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool IsPartPlanUnspecified(const FAutoSupportBuildPlanPartData& PartPlan);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool IsPartPlanActionable(const FAutoSupportBuildPlanPartData& PartPlan);

	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static void CalculateTotalCost(FAutoSupportBuildPlan& Plan);

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

#pragma region UI Helpers
	
	UFUNCTION(BlueprintCallable, Category = "AutoSupport")
	static bool IsValidAutoSupportPresetName(FString PresetName, FText& OutError);
	
	UFUNCTION(BlueprintCallable, Category = "AutoSupport", meta = (ArrayParam = "LeasedWidgetPool", DeterminesOutputType="WidgetClass", DynamicOutputParam="OutRemovedWidgets", ReturnDisplayName="New Start Index"))
	static int32 LeaseWidgetsExact(
		APlayerController* Controller,
		UPARAM(meta=(AllowAbstract=false)) TSubclassOf<UUserWidget> WidgetClass,
		UPARAM(Ref) TArray<UUserWidget*>& LeasedWidgetPool,
		int32 Count,
		TArray<UUserWidget*>& OutRemovedWidgets);

#pragma endregion

#pragma region Private
	
private:
	
	static void SpawnPartPlanHolograms(
		AFGHologram*& ParentHologram,
		const FAutoSupportBuildPlanPartData& PartPlan,
		APawn* BuildInstigator,
		AActor* Parent,
		AActor* Owner,
		FTransform& WorkingTransform,
		FBox& WorkingBBox);
	
	static bool InitializePartPlan(
		const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptorClass,
		EAutoSupportBuildDirection PartOrientation,
		const FFactoryCustomizationData& PartCustomization,
		FAutoSupportBuildPlanPartData& PartPlan,
		const AFGRecipeManager* RecipeManager);

	static void PlanPartPositioning(
		const FBox& PartBBox,
		EAutoSupportBuildDirection PartOrientation,
		FAutoSupportBuildPlanPartData& Plan);

#pragma endregion
};