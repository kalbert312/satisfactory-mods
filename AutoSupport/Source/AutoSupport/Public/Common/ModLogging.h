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
#define TEXT_ENUM(enumVal) TEXT_STR(EnumToStr(enumVal))
#define TEXT_THIS_FUNC TEXT_STR(FString(__FUNCTION__))
#define TEXT_ACTOR_NAME(actor) actor ? TEXT_STR(actor->GetName()) : TEXT_NULL
#define TEXT_OBJ_CLS_NAME(uobject) uobject ? TEXT_STR(uobject->GetClass()->GetName()) : TEXT_NULL
#define TEXT_CLS_NAME(uclass) uclass ? TEXT_STR(uclass->GetName()) : TEXT_NULL

/**
 * @tparam TEnum The enum type. This must be a UENUM(BlueprintType) for the reflection to work.
 * @param EnumVal The enum value. 
 * @return The enum literal as a string.
 */
template<typename TEnum>
FString EnumToStr(const TEnum EnumVal)
{
	return StaticEnum<TEnum>()->GetNameStringByValue(static_cast<int64>(EnumVal));
}
