// 

#pragma once

#include "CoreMinimal.h"
#include "SML/Public/Subsystem/ModSubsystem.h"
#include "AutoSupportModSubsystem.generated.h"

UCLASS(Blueprintable)
class AUTOSUPPORT_API AAutoSupportModSubsystem : public AModSubsystem
{
	GENERATED_BODY()

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
};
