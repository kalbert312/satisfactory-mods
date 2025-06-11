// 

#include "BuildableAutoSupport.h"

#include "FGBuildingDescriptor.h"
#include "FGColoredInstanceMeshProxy.h"

ABuildableAutoSupport::ABuildableAutoSupport()
{
	InstancedMeshProxy = CreateDefaultSubobject<UFGColoredInstanceMeshProxy>(TEXT("BuildableInstancedMeshProxy"));
	InstancedMeshProxy->SetupAttachment(RootComponent);
}

void ABuildableAutoSupport::BuildSupports()
{
	if (!IsValid(MiddlePartDescriptor))
	{
		// we're building either the top or bottom
		return;
	}

	auto* Buildables = AFGBuildableSubsystem::Get(GetWorld());

	BuildPart(Buildables, MiddlePartDescriptor, GetActorTransform());
}

void ABuildableAutoSupport::TraceDown()
{
	
}

void ABuildableAutoSupport::BuildPart(AFGBuildableSubsystem* Buildables, const TSubclassOf<UFGBuildingDescriptor> PartDescriptor, const FTransform& Transform)
{
	const TSubclassOf<AFGBuildable> BuildableClass = UFGBuildingDescriptor::GetBuildableClass(PartDescriptor);
	
	auto* Buildable = Buildables->BeginSpawnBuildable(BuildableClass, Transform);

	Buildable->FinishSpawning(Transform);
}
