// 

#pragma once

#include "CoreMinimal.h"
#include "FGInputMappingContext.h"
#include "AutoSupportBuildGunInputMappingContext.generated.h"

UCLASS(Blueprintable, BlueprintType)
class AUTOSUPPORT_API UAutoSupportBuildGunInputMappingContext : public UFGInputMappingContext
{
	GENERATED_BODY()

public:
	/**
	 * Action to automatically build an auto support when placed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UInputAction> IA_AutoBuildSupports;
};
