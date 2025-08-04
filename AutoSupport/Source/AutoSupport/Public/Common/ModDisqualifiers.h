#pragma once

#include "CoreMinimal.h"
#include "FGConstructDisqualifier.h"
#include "ModDisqualifiers.generated.h"

UCLASS()
class AUTOSUPPORT_API UAutoSupportConstructDisqualifier_NotEnoughRoom : public UFGConstructDisqualifier
{
	GENERATED_BODY()

	UAutoSupportConstructDisqualifier_NotEnoughRoom()
	{
		mDisqfualifyingText = NSLOCTEXT("FAutoSupportModule", "NotEnoughRoom", "Not enough room to build");
	}
};

UCLASS()
class AUTOSUPPORT_API UAutoSupportConstructDisqualifier_InvalidPart : public UFGConstructDisqualifier
{
	GENERATED_BODY()

	UAutoSupportConstructDisqualifier_InvalidPart()
	{
		mDisqfualifyingText = NSLOCTEXT("FAutoSupportModule", "InvalidPart", "One or more selected parts are invalid.");
	}
};

