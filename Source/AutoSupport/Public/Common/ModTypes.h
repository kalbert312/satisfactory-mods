#pragma once

#include "CoreMinimal.h"
#include "ModTypes.generated.h"

class AFGBuildable;
class ABuildableAutoSupportProxy;

UENUM(BlueprintType)
enum class EAutoSupportBuildDirection : uint8
{
	Top,
	Bottom,
	Front,
	Back,
	Left,
	Right,
	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(EAutoSupportBuildDirection, EAutoSupportBuildDirection::Count)

USTRUCT(BlueprintType)
struct AUTOSUPPORT_API FAutoSupportBuildableHandle
{
	GENERATED_BODY()

	FAutoSupportBuildableHandle() = default;
	FAutoSupportBuildableHandle(const FAutoSupportBuildableHandle& Other) = default;

	explicit FAutoSupportBuildableHandle(AFGBuildable* Buildable);

	explicit FAutoSupportBuildableHandle(const TSubclassOf<AFGBuildable> BuildableClass, const int32 LightweightRuntimeIndex);
	
	UPROPERTY(SaveGame)
	TWeakObjectPtr<AFGBuildable> Buildable;

	UPROPERTY(SaveGame)
	TSubclassOf<AFGBuildable> BuildableClass;

	UPROPERTY(SaveGame)
	int32 LightweightRuntimeIndex = -1;

	FORCEINLINE bool IsLightweightType() const
	{
		return LightweightRuntimeIndex >= 0;
	}
	
	bool IsDataValid() const;

	bool Equals(const FAutoSupportBuildableHandle& Other) const;

	bool operator==(const FAutoSupportBuildableHandle& Other) const
	{
		return Equals(Other);
	}
	
	bool operator!=(const FAutoSupportBuildableHandle& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FAutoSupportBuildableHandle& Handle)
	{
		return HashCombine(
			GetTypeHash(Handle.BuildableClass),
			GetTypeHash(Handle.LightweightRuntimeIndex));
	}
	
	FString ToString() const;
};

