// 

#pragma once

#include "CoreMinimal.h"
#include "BuildableAutoSupport_Types.h"
#include "SML/Public/Subsystem/ModSubsystem.h"
#include "AutoSupportModClientSubsystem.generated.h"

UCLASS()
class AUTOSUPPORT_API AAutoSupportModClientSubsystem : public AModSubsystem, public IFGSaveInterface
{
	GENERATED_BODY()

public:
	AAutoSupportModClientSubsystem();

	UFUNCTION(BlueprintCallable)
	FBuildableAutoSupportData GetLastAutoSupportData() const;

	UFUNCTION(BlueprintCallable)
	FBuildableAutoSupportData GetLastAutoSupport1mData() const;

	UFUNCTION(BlueprintCallable)
	FBuildableAutoSupportData GetLastAutoSupport2mData() const;

	UFUNCTION(BlueprintCallable)
	FBuildableAutoSupportData GetLastAutoSupport4mData() const;
	
	UFUNCTION(BlueprintCallable)
	void SetLastAutoSupport1mData(const FBuildableAutoSupportData& Data);

	UFUNCTION(BlueprintCallable)
	void SetLastAutoSupport2mData(const FBuildableAutoSupportData& Data);

	UFUNCTION(BlueprintCallable)
	void SetLastAutoSupport4mData(const FBuildableAutoSupportData& Data);

	virtual void PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion) override;
	virtual bool ShouldSave_Implementation() const override;
	
protected:
	/**
	 * The last configuration used. Will preconfigure new placements.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	FBuildableAutoSupportData LastAutoSupportData;

	/**
	 * The last configuration used for 1m pieces. Will preconfigure new placements.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	FBuildableAutoSupportData LastAutoSupport1mData;

	/**
	 * The last configuration used for 2m pieces. Will preconfigure new placements.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	FBuildableAutoSupportData LastAutoSupport2mData;
	
	/**
	 * The last configuration used for 4m pieces. Will preconfigure new placements.
	 */
	UPROPERTY(VisibleInstanceOnly, SaveGame)
	FBuildableAutoSupportData LastAutoSupport4mData;
};
