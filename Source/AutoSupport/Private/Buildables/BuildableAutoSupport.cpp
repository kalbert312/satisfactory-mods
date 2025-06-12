// 

#include "BuildableAutoSupport.h"

#include "FGBuildingDescriptor.h"
#include "FGColoredInstanceMeshProxy.h"
#include "ModLogging.h"

ABuildableAutoSupport::ABuildableAutoSupport()
{
	InstancedMeshProxy = CreateDefaultSubobject<UFGColoredInstanceMeshProxy>(TEXT("BuildableInstancedMeshProxy"));
	InstancedMeshProxy->SetupAttachment(RootComponent);
}

bool ABuildableAutoSupport::IsBuildable(const FVector& PartCounts) const
{
	if (PartCounts.IsNearlyZero())
	{
		return false;
	}
	
	// TODO: check player has enough resources to build
	return true;
}

void ABuildableAutoSupport::BuildParts(
	AFGBuildableSubsystem* Buildables,
	TSubclassOf<UFGBuildingDescriptor> PartDescriptor,
	const int32 Count,
	const FVector& Size,
	FTransform& WorkingTransform)
{
	const TSubclassOf<AFGBuildable> BuildableClass = UFGBuildingDescriptor::GetBuildableClass(PartDescriptor);
	
	for (auto i = 0; i < Count; ++i)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Spawning part. Start Transform: %s"), *WorkingTransform.ToString());
		
		// Spawn the part
		auto* Buildable = Buildables->BeginSpawnBuildable(BuildableClass, WorkingTransform);
		Buildable->FinishSpawning(WorkingTransform);

		// Update the transform
		WorkingTransform.AddToTranslation(FVector(0, 0, -1 * Size.Z));
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts End Transform: %s"), *WorkingTransform.ToString());
	}
}

void ABuildableAutoSupport::BuildSupports()
{
	if (!MiddlePartDescriptor && !StartPartDescriptor && !EndPartDescriptor)
	{
		// Nothing to build.
		return;
	}
	
	// Trace to know how much we're going to build.
	auto BuildDistance = FMath::Min(Trace(), MaxBuildDistance);

	MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildSupports BuildDistance: %f"), BuildDistance);

	if (FMath::IsNearlyZero(BuildDistance))
	{
		// No room to build.
		return;
	}

	FVector PartCounts;
	FBox TopBox, MidBox, BottomBox;

	PlanBuild(BuildDistance, PartCounts, TopBox, MidBox, BottomBox);
	
	// Check that the player building it has enough resources
	if (!IsBuildable(PartCounts))
	{
		return;
	}

	// Build the parts
	auto* Buildables = AFGBuildableSubsystem::Get(GetWorld());
	auto WorkingTransform = GetActorTransform();

	if (PartCounts.X > 0)
	{
		BuildParts(Buildables, StartPartDescriptor, PartCounts.X, TopBox.GetSize(), WorkingTransform);
	}

	if (PartCounts.Y > 0)
	{
		BuildParts(Buildables, MiddlePartDescriptor, PartCounts.Y, MidBox.GetSize(), WorkingTransform);
	}

	if (PartCounts.Z > 0)
	{
		BuildParts(Buildables, EndPartDescriptor, PartCounts.Z, BottomBox.GetSize(), WorkingTransform);
	}
}

float ABuildableAutoSupport::Trace()
{
	return 2000;
}

void ABuildableAutoSupport::PlanBuild(float BuildDistance, OUT FVector& OutPartCounts, OUT FBox& OutStartBox, OUT FBox& OutMidBox, OUT FBox& OutEndBox) const
{
	OutPartCounts = FVector::ZeroVector;

	if (!MiddlePartDescriptor && !StartPartDescriptor && !EndPartDescriptor)
	{
		return;
	}

	// Start with the top because that's where the auto support is planted.
	if (StartPartDescriptor)
	{
		GetBuildableSize(StartPartDescriptor, OutStartBox);
		const auto Size = OutStartBox.GetSize();

		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Start Size: %s"), *Size.ToString());
		BuildDistance -= Size.Z;

		if (BuildDistance < 0)
		{
			// Not enough room (TODO: might be able to in a small negative range)
			return;
		}
		
		OutPartCounts.X = 1;
	}

	// Do the end next. There may not be enough room for mid pieces.
	if (EndPartDescriptor)
	{
		GetBuildableSize(EndPartDescriptor, OutEndBox);
		const auto Size = OutEndBox.GetSize();

		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild End Size: %s"), *Size.ToString());
		BuildDistance -= Size.Z;

		if (BuildDistance < 0)
		{
			// Not enough room (TODO: might be able to in a small negative range)
			return;
		}

		OutPartCounts.Z = 1;
	}
	
	if (MiddlePartDescriptor)
	{
		GetBuildableSize(MiddlePartDescriptor, OutMidBox);
		const auto Size = OutMidBox.GetSize();
		
		auto NumMiddleParts = static_cast<int32>(BuildDistance / Size.Z);
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Mid Size: %s, Num Mid: %d"), *Size.ToString(), NumMiddleParts);
		
		OutPartCounts.Y = NumMiddleParts;
	}
}

void ABuildableAutoSupport::GetBuildableSize(const TSubclassOf<UFGBuildingDescriptor> PartDescriptor, OUT FBox& OutBox)
{
	const auto Buildable = GetDefault<AFGBuildable>(UFGBuildingDescriptor::GetBuildableClass(PartDescriptor));
	OutBox = Buildable->GetCombinedClearanceBox();
}

