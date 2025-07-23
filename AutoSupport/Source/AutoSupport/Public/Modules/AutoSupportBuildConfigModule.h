// 

#pragma once

#include "CoreMinimal.h"
#include "AutoSupportGameWorldModule.h"
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
	
	/**
	 * @return The maximum build distance (trace distance) of an Auto Support cube. 
	 */
	float GetMaxBuildDistance() const;
	
	/**
	 * @param HitResult The hit result.
	 * @return True if the hit result is classified as a landscape hit. 
	 */
	bool IsLandscapeTypeHit(const FHitResult& HitResult) const;

	/**
	 * @param HitResult The hit result.
	 * @return True if the hit result is to be ignored.
	 */
	bool IsIgnoredHit(const FHitResult& HitResult) const;
};
