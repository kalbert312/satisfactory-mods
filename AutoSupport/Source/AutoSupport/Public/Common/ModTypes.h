#pragma once

#include "CoreMinimal.h"
#include "FGBuildable.h"
#include "FGConstructDisqualifier.h"
#include "ModTypes.generated.h"

struct FLightweightBuildableInstanceRef;
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
	explicit FAutoSupportBuildableHandle(const FLightweightBuildableInstanceRef& LightweightRef);

	/**
	 * The buildable reference. This can be a non lightweight or lightweight temporary that will get cleaned up.
	 */
	UPROPERTY(SaveGame)
	TWeakObjectPtr<AFGBuildable> Buildable;

protected:
	/**
	 * The buildable class.
	 */
	UPROPERTY(SaveGame)
	TSubclassOf<AFGBuildable> BuildableClass;

	/**
	 * The transform of the buildable. For lightweights, this is required to link back to runtime data indexes after loading a save.
	 */
	UPROPERTY(SaveGame)
	FTransform Transform;

	/**
	 * The cached hash for the rounded transform.
	 */
	UPROPERTY(SaveGame)
	uint32 TransformHash = 0;

	void SetTransform(const FTransform& NewTransform);

public:
	FORCEINLINE bool IsConsideredLightweight() const
	{
		return Buildable == nullptr || Buildable->GetIsLightweightTemporary();
	}
	
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
			Handle.TransformHash);
	}
	
	FString ToString() const;
};

UENUM(BlueprintType)
enum class EAutoSupportTraceHitClassification : uint8
{
	Block,
	Ignore,
	Landscape,
	Pawn,
	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(EAutoSupportTraceHitClassification, EAutoSupportTraceHitClassification::Count)

UCLASS()
class AUTOSUPPORT_API UAutoSupportConstructDisqualifier_NotEnoughRoom : public UFGConstructDisqualifier
{
	GENERATED_BODY()

	UAutoSupportConstructDisqualifier_NotEnoughRoom()
	{
		mDisqfualifyingText = NSLOCTEXT("FAutoSupportModule", "NotEnoughRoom", "Not enough room to build");
	}
};

