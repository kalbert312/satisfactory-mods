// 

#pragma once

#include "CoreMinimal.h"
#include "AutoSupportGameWorldModule.h"
#include "ContentTagRegistry.h"
#include "GameplayTagContainer.h"
#include "ModTypes.h"
#include "AutoSupportBuildConfigModule.generated.h"

/**
 * Child game module that supplies configuration for Auto Support builds. This is spawned via the Blueprint class of AutoSupportGameWorldModule.
 */
UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportBuildConfigModule : public UAutoSupportGameWorldModule
{
	GENERATED_BODY()

public:

	static UAutoSupportBuildConfigModule* Get(const UWorld* World);

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

	/**
	 * @return The maximum build distance (trace distance) of an Auto Support cube. 
	 */
	float GetMaxBuildDistance() const;

	/**
	 * @param HitResult The hit result.
	 * @param bOnlyLandscapeBlocks True if landscape type hits should only be considered.
	 * @param ContentTagRegistry The content tag registry.
	 * @param OutDisqualifier Set if the hit invalidates the build.
	 * @return The hit classification.
	 */
	EAutoSupportTraceHitClassification CalculateHitClassification(
		const FHitResult& HitResult,
		bool bOnlyLandscapeBlocks,
		UContentTagRegistry* ContentTagRegistry,
		TSubclassOf<UFGConstructDisqualifier>& OutDisqualifier) const;

	

protected:
	/**
	 * Paths of meshes (ex. /Game/FactoryGame/...) to ignore hits of.
	 */
	UPROPERTY(EditDefaultsOnly)
	TSet<FString> TraceIgnoreVanillaMeshContentPaths;

	/**
	 * Paths of meshes (ex. /Game/FactoryGame/...) to consider as landscape hits (Rocks, Cliffs, Caves, etc).
	 */
	UPROPERTY(EditDefaultsOnly)
	TSet<FString> TraceLandscapeVanillaMeshContentPaths;
	
	FGameplayTag TraceLandscapeTag;

	FGameplayTag TraceIgnoreTag;

	static UStaticMesh* GetHitStaticMesh(const AActor* HitActor, const UPrimitiveComponent* HitComponent);
	
};
