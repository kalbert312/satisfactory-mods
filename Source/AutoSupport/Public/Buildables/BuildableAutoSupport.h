// 

#pragma once

#include "CoreMinimal.h"
#include "FGBuildable.h"
#include "BuildableAutoSupport.generated.h"

class UFGBuildingDescriptor;

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupport : public AFGBuildable
{
	GENERATED_BODY()

public:
	ABuildableAutoSupport();

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Auto Supports")
	TSubclassOf<UFGBuildingDescriptor> TopPartDescriptor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Auto Supports")
	TSubclassOf<UFGBuildingDescriptor> MiddlePartDescriptor;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Auto Supports")
	TSubclassOf<UFGBuildingDescriptor> BottomPartDescriptor;

	UFUNCTION(BlueprintCallable)
	void BuildSupports();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buildable Components")
	TObjectPtr<UFGColoredInstanceMeshProxy> InstancedMeshProxy;

	void TraceDown();

	void BuildPart(AFGBuildableSubsystem* Buildables, TSubclassOf<UFGBuildingDescriptor> PartDescriptor, const FTransform& Transform);
};
