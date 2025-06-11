#pragma once

#include "ModDefines.h"

DECLARE_LOG_CATEGORY_EXTERN(MOD_LOG_CATEGORY, Verbose, All);

#define MOD_LOG(Verbosity, Format, ...) \
	UE_LOG(MOD_LOG_CATEGORY, Verbosity, Format, ##__VA_ARGS__)

// use TEXT_CONDITION for booleans

#define MOD_LOG_SCREEN(Verbosity, Format, ...) \
	UE_LOG(MOD_LOG_CATEGORY, Verbosity, Format, ##__VA_ARGS__) \
	if (GEngine) \
	{ \
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::White, FString::Printf(Format, ##__VA_ARGS__)); \
	}
