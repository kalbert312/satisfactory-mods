// 

#include "AutoSupportGameWorldModule.h"

#include "ModConstants.h"
#include "WorldModuleManager.h"

UAutoSupportGameWorldModule* UAutoSupportGameWorldModule::Get(const UWorld* World)
{
	const auto* WorldModuleManager = World->GetSubsystem<UWorldModuleManager>();
	return CastChecked<UAutoSupportGameWorldModule>(WorldModuleManager->FindModule(AutoSupportConstants::ModReference));
}

