// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#define MOD_LOG_CATEGORY LogAutoSupport

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"


class FAutoSupportModule : public IModuleInterface
{
	
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void RegisterHooks();
	
};
