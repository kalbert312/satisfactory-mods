// 

#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupport_Types.h"
#include "FGBuildableFactoryBuilding.h"
#include "BuildableAutoSupport.generated.h"

class UFGBuildingDescriptor;

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupport : public AFGBuildableFactoryBuilding
{
	GENERATED_BODY()

public:
	ABuildableAutoSupport(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Traces and creates a build plan.
	 * @param OutPlan The plan
	 * @return True if a plan was initialized.
	 */
	UFUNCTION(BlueprintCallable)
	bool TraceAndCreatePlan(FAutoSupportBuildPlan& OutPlan) const;

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

#pragma region IFGSaveInterface
	
	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual void PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual bool ShouldSave_Implementation() const override;

#pragma endregion

	virtual void BeginPlay() override;

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
	bool bAutoConfigure = true;

	/**
	 * True if this actor was loaded from a save.
	 */
	UPROPERTY(Transient)
	bool bIsLoadedFromSave = false;
	
	void AutoConfigure();

	UFUNCTION(BlueprintImplementableEvent, Category = "Auto Support")
	void K2_AutoConfigure();
	
	/**
	 * Traces to the terrain.
	 * @return The free distance.
	 */
	FAutoSupportTraceResult Trace() const;

	FVector GetCubeFaceRelativeLocation(EAutoSupportBuildDirection Direction) const;
	FORCEINLINE FVector GetEndTraceWorldLocation(const FVector& StartLocation, const FVector& Direction) const;
};

UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportClipboardSettings : public UFGFactoryClipboardSettings
{
	GENERATED_BODY()

	UPROPERTY()
	FBuildableAutoSupportData AutoSupportData;
};
