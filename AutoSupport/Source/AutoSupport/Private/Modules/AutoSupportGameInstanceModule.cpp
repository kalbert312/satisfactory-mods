// 

#include "AutoSupportGameInstanceModule.h"

#include "GameInstanceModuleManager.h"
#include "ModConstants.h"

UAutoSupportGameInstanceModule* UAutoSupportGameInstanceModule::Get(const UWorld* World)
{
	const auto* GameInstance = World->GetGameInstance();
	const auto* GameInstanceModuleManager = GameInstance->GetSubsystem<UGameInstanceModuleManager>();

	return Cast<UAutoSupportGameInstanceModule>(GameInstanceModuleManager->FindModule(AutoSupportConstants::ModReference));
}
