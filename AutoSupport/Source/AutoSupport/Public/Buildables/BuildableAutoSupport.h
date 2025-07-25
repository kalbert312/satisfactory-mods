﻿// 

#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupportProxy.h"
#include "BuildableAutoSupport_Types.h"
#include "FGBuildableFactoryBuilding.h"
#include "BuildableAutoSupport.generated.h"

class ABuildableAutoSupportProxy;
class UFGBuildingDescriptor;

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupport : public AFGBuildableFactoryBuilding
{
	GENERATED_BODY()

public:
	ABuildableAutoSupport(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Traces and creates a build plan.
	 * @param BuildInstigator Who is initiating the build.
	 * @param OutPlan The plan.
	 * @return True if the plan is actionable.
	 */
	UFUNCTION(BlueprintCallable)
	bool TraceAndCreatePlan(APawn* BuildInstigator, FAutoSupportBuildPlan& OutPlan) const;

	/**
	 * The current auto support configuration for this actor.
	 */
	UPROPERTY(BlueprintReadWrite, SaveGame)
	FBuildableAutoSupportData AutoSupportData;
	
	/**
	 * Builds the supports based on the provided configuration. Requires a build instigator.
	 */
	UFUNCTION(BlueprintCallable)
	void BuildSupports(APawn* BuildInstigator);

	virtual void BeginPlay() override;

#pragma region IFGSaveInterface
	
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual bool ShouldSave_Implementation() const override;

#pragma endregion

#pragma region Editor Only
#if WITH_EDITOR
	
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	
#endif
#pragma endregion
	
protected:
	/**
	 * Set this to true to autoconfigure the auto support to the last configuration used. Autoconfiguration happens at BeginPlay and only occurs once.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Auto Support")
	bool bAutoConfigureAtBeginPlay = true;
	
	/**
	 * The auto support proxy actor class to use.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto Support")
	TSubclassOf<ABuildableAutoSupportProxy> AutoSupportProxyClass;
	
	void AutoConfigure();

	UFUNCTION(BlueprintImplementableEvent, Category = "Auto Support")
	void K2_AutoConfigure();

	UFUNCTION(BlueprintCallable, Category = "Auto Support")
	void SaveLastUsedData();

	UFUNCTION(BlueprintImplementableEvent, Category = "Auto Support")
	void K2_SaveLastUsedData();
	
	/**
	 * Traces and gather data for auto support build planning.
	 * @return The trace results.
	 */
	FAutoSupportTraceResult Trace() const;

	FVector GetCubeFaceRelativeLocation(EAutoSupportBuildDirection Direction) const;
	
	static FVector GetEndTraceWorldLocation(const FVector& StartLocation, const FVector& Direction, float MaxBuildDistance);
};

UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportClipboardSettings : public UFGFactoryClipboardSettings
{
	GENERATED_BODY()

	UPROPERTY()
	FBuildableAutoSupportData AutoSupportData;
};
