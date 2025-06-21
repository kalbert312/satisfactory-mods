#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogAutoSupport, Verbose, All);

#define MOD_LOG(Verbosity, Format, ...) \
	UE_LOG(LogAutoSupport, Verbosity, Format, ##__VA_ARGS__)

// use TEXT_CONDITION for booleans

#define MOD_LOG_SCREEN(Verbosity, Format, ...) \
	UE_LOG(LogAutoSupport, Verbosity, Format, ##__VA_ARGS__) \
	if (GEngine) \
	{ \
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::White, FString::Printf(Format, ##__VA_ARGS__)); \
	}
