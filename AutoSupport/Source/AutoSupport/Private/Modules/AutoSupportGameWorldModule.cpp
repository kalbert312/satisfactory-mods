// 

#include "Modules/AutoSupportGameWorldModule.h"

#include "Common/ModConstants.h"
#include "Module/WorldModuleManager.h"

UAutoSupportGameWorldModule* UAutoSupportGameWorldModule::GetRoot(const UWorld* World)
{
	const auto* WorldModuleManager = World->GetSubsystem<UWorldModuleManager>();
	return CastChecked<UAutoSupportGameWorldModule>(WorldModuleManager->FindModule(AutoSupportConstants::ModReference));
}

