//

#include "ModDebugBlueprintLibrary.h"

#ifdef AUTOSUPPORT_DEV_MODE

#include "Components/LineBatchComponent.h"

// Most functions are copied from UE debugging methods only available in dev builds
ULineBatchComponent* UAutoSupportDebugBlueprintLibrary::GetDebugLineBatcher( const UWorld* InWorld, bool bPersistentLines, float LifeTime, bool bDepthIsForeground )
{
	return (InWorld ? (bDepthIsForeground ? InWorld->ForegroundLineBatcher : (( bPersistentLines || (LifeTime > 0.f) ) ? InWorld->PersistentLineBatcher : InWorld->LineBatcher)) : nullptr);
}

float UAutoSupportDebugBlueprintLibrary::GetDebugLineLifeTime(ULineBatchComponent* LineBatcher, float LifeTime, bool bPersistent)
{
	return bPersistent ? -1.0f : ((LifeTime > 0.f) ? LifeTime : LineBatcher->DefaultLifeTime);
}

void UAutoSupportDebugBlueprintLibrary::DrawDebugCoordinateSystem(
	const UWorld* InWorld,
	FVector const& AxisLoc,
	FRotator const& AxisRot,
	float Scale,
	bool bPersistentLines,
	float LifeTime,
	uint8 DepthPriority,
	float Thickness)
{
	// Copied from UE impl
	if (GEngine->GetNetMode(InWorld) != NM_DedicatedServer)
	{
		FRotationMatrix R(AxisRot);
		FVector const X = R.GetScaledAxis( EAxis::X );
		FVector const Y = R.GetScaledAxis( EAxis::Y );
		FVector const Z = R.GetScaledAxis( EAxis::Z );

		// this means foreground lines can't be persistent 
		if (ULineBatchComponent* const LineBatcher = GetDebugLineBatcher(InWorld, bPersistentLines, LifeTime, (DepthPriority == SDPG_Foreground)))
		{
			const float LineLifeTime = GetDebugLineLifeTime(LineBatcher, LifeTime, bPersistentLines);
			LineBatcher->DrawLine(AxisLoc, AxisLoc + X*Scale, FColor::Red, DepthPriority, Thickness, LineLifeTime );
			LineBatcher->DrawLine(AxisLoc, AxisLoc + Y*Scale, FColor::Green, DepthPriority, Thickness, LineLifeTime );
			LineBatcher->DrawLine(AxisLoc, AxisLoc + Z*Scale, FColor::Blue, DepthPriority, Thickness, LineLifeTime );
		}
	}
}

#endif