#pragma once
#include "CoreMinimal.h"
#include "Configuration/ConfigManager.h"
#include "Engine/Engine.h"
#include "BP_ModConfig_AutoSupportStruct.generated.h"

/* Struct generated from Mod Configuration Asset '/AutoSupport/Config/BP_ModConfig_AutoSupport' */
USTRUCT(BlueprintType)
struct FBP_ModConfig_AutoSupportStruct {
    GENERATED_BODY()
public:

    /* Retrieves active configuration value and returns object of this struct containing it */
    static FBP_ModConfig_AutoSupportStruct GetActiveConfig(UObject* WorldContext) {
        FBP_ModConfig_AutoSupportStruct ConfigStruct{};
        FConfigId ConfigId{"AutoSupport", ""};
        if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull)) {
            UConfigManager* ConfigManager = World->GetGameInstance()->GetSubsystem<UConfigManager>();
            ConfigManager->FillConfigurationStruct(ConfigId, FDynamicStructInfo{FBP_ModConfig_AutoSupportStruct::StaticStruct(), &ConfigStruct});
        }
        return ConfigStruct;
    }
};

