// 

#pragma once

#include "CoreMinimal.h"
#include "SML/Public/Module/GameInstanceModule.h"
#include "AutoSupportGameInstanceModule.generated.h"

class UFGBuildGunModeDescriptor;

UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:

	static UAutoSupportGameInstanceModule* Get(const UWorld* World);
	
};
