// 

#include "AutoSupportGameInstanceModule.h"

#include "GameInstanceModuleManager.h"
#include "ModConstants.h"

UAutoSupportGameInstanceModule* UAutoSupportGameInstanceModule::Get(const UWorld* World)
{
	auto* GameInstance = World->GetGameInstance();
	auto* GameInstanceModuleManager = GameInstance->GetSubsystem<UGameInstanceModuleManager>();

	return Cast<UAutoSupportGameInstanceModule>(GameInstanceModuleManager->FindModule(AutoSupportConstants::ModReference));
}
