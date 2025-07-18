#pragma once

DECLARE_LOG_CATEGORY_EXTERN(LogAutoSupport, Verbose, All);
DECLARE_LOG_CATEGORY_EXTERN(LogAutoSupportTrace, Verbose, All);

#define MOD_LOG(Verbosity, Format, ...) \
	UE_LOG(LogAutoSupport, Verbosity, "%s] " Format, TEXT_THIS_FUNC, ##__VA_ARGS__)

#define MOD_TRACE_LOG(Verbosity, Format, ...) \
	UE_LOG(LogAutoSupportTrace, Verbosity, "%s] " Format, TEXT_THIS_FUNC, ##__VA_ARGS__)

// use TEXT_CONDITION for booleans

#define TEXT_BOOL(b) TEXT_CONDITION(b)
#define TEXT_STR(fstring) *(fstring)
#define TEXT_THIS_FUNC TEXT_STR(FString(__FUNCTION__))
#define TEXT_OBJ_CLS_NAME(uobject) uobject ? TEXT_STR(uobject->GetClass()->GetName()) : TEXT_NULL
#define TEXT_CLS_NAME(uclass) uclass ? TEXT_STR(uclass->GetName()) : TEXT_NULL
