// 

#pragma once

#include "CoreMinimal.h"
#include "SML/Public/Module/GameWorldModule.h"
#include "AutoSupportGameWorldModule.generated.h"

UCLASS(Blueprintable)
class AUTOSUPPORT_API UAutoSupportGameWorldModule : public UGameWorldModule
{
	GENERATED_BODY()

public:
	static UAutoSupportGameWorldModule* Get(const UWorld* World);
	
};
