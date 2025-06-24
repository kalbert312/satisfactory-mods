#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogAutoSupport, Verbose, All);

#define MOD_LOG(Verbosity, Format, ...) \
	UE_LOG(LogAutoSupport, Verbosity, "%s] " Format, *TEXT_THIS_CLASS_FUNC, ##__VA_ARGS__)

// use TEXT_CONDITION for booleans

#define TEXT_THIS_CLASS_FUNC (FString(__FUNCTION__))
