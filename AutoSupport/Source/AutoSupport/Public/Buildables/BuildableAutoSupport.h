// 

#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupportProxy.h"
#include "BuildableAutoSupport_Types.h"
#include "FGBuildableFactoryBuilding.h"
#include "FGCharacterPlayer.h"
#include "BuildableAutoSupport.generated.h"

class ABuildableAutoSupportProxy;
class UFGBuildingDescriptor;

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupport : public AFGBuildableFactoryBuilding
{
	GENERATED_BODY()
	friend class UAutoSupportBuildableRCO;
	
public:
	explicit ABuildableAutoSupport(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * The current auto support configuration for this actor.
	 */
	UPROPERTY(BlueprintReadWrite, Replicated, SaveGame)
	FBuildableAutoSupportData AutoSupportData;
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * Traces and creates a build plan.
	 * @param BuildInstigator Who is initiating the build.
	 * @param OutPlan The plan.
	 * @return True if the plan is actionable.
	 */
	UFUNCTION(BlueprintCallable)
	bool TraceAndCreatePlan(AFGCharacterPlayer* BuildInstigator, FAutoSupportBuildPlan& OutPlan) const;
	
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
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Replicated, SaveGame, Category = "Auto Support")
	bool bAutoConfigureAtBeginPlay = true;
	
	/**
	 * The auto support proxy actor class to use.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto Support")
	TSubclassOf<ABuildableAutoSupportProxy> AutoSupportProxyClass;

	void BeginPlay_Client();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Auto Support")
	FBuildableAutoSupportData K2_GetAutoConfigureData() const;

	void AutoConfigure(const AFGCharacterPlayer* Player, const FBuildableAutoSupportData& Data);

	UFUNCTION(BlueprintImplementableEvent, Category = "Auto Support")
	void K2_AutoConfigure(AFGCharacterPlayer* Player);

	UFUNCTION(BlueprintCallable, Category = "Auto Support")
	void SaveLastUsedData();

	UFUNCTION(BlueprintImplementableEvent, Category = "Auto Support")
	void K2_SaveLastUsedData();
	
	/**
	 * Traces and gather data for auto support build planning.
	 * @return The trace results.
	 */
	FAutoSupportTraceResult Trace() const;

	/**
	 * Builds the supports based on the provided configuration. Requires a build instigator.
	 */
	void BuildSupports(AFGCharacterPlayer* BuildInstigator);

	FVector GetCubeFaceRelativeLocation(EAutoSupportBuildDirection Direction) const;
};

UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportClipboardSettings : public UFGFactoryClipboardSettings
{
	GENERATED_BODY()

	UPROPERTY()
	FBuildableAutoSupportData AutoSupportData;
};

UCLASS()
class AUTOSUPPORT_API UAutoSupportBuildableRCO : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void UpdateConfiguration(ABuildableAutoSupport* AutoSupport, FBuildableAutoSupportData NewData);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void UpdateConfigurationAndMaybeBuild(ABuildableAutoSupport* AutoSupport, AFGCharacterPlayer* BuilderPlayer, FBuildableAutoSupportData NewData, bool bShouldBuild);
	
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void BuildSupports(ABuildableAutoSupport* AutoSupport, AFGCharacterPlayer* BuildInstigator);

private:
	UPROPERTY(Replicated, Meta = (NoAutoJson))
	bool mForceNetField_UAutoSupportBuildableRCO = false;
};
