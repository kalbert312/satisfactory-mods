// 

#pragma once

#include "CoreMinimal.h"
#include "FGBuildable.h"
#include "BuildableAutoSupport.generated.h"

UCLASS(Abstract, Blueprintable)
class AUTOSUPPORT_API ABuildableAutoSupport : public AFGBuildable
{
	GENERATED_BODY()

public:
	ABuildableAutoSupport();
	
protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Buildable Components")
	TObjectPtr<UFGColoredInstanceMeshProxy> InstancedMeshProxy;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Auto Support")
	int WidthInGameUnits;
};
