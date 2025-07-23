#pragma once

#include "CoreMinimal.h"
#include "AutoSupportBuildConfig.generated.h"

/**
 * Configuration related to building and tracing for auto supports.
 */
UCLASS(BlueprintType)
class AUTOSUPPORT_API UAutoSupportBuildConfig : public UDataAsset
{
	GENERATED_BODY()

public:

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
