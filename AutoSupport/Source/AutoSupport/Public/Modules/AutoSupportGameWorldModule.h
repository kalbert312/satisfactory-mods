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
	static UAutoSupportGameWorldModule* GetRoot(const UWorld* World);
	
	template <class TChildModule>
	static TChildModule* GetChild(const UWorld* World, FName ChildModuleName);
};

template <typename TChildModule>
TChildModule* UAutoSupportGameWorldModule::GetChild(const UWorld* World, FName ChildModuleName)
{
	UAutoSupportGameWorldModule* RootModule = GetRoot(World);
	
	if (!RootModule)
	{
		return nullptr;
	}
	
	return Cast<TChildModule>(RootModule->GetChildModule(ChildModuleName, TChildModule::StaticClass()));
}
