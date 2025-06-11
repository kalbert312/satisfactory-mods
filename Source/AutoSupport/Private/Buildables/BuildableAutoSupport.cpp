// 

#include "BuildableAutoSupport.h"

#include "FGColoredInstanceMeshProxy.h"

ABuildableAutoSupport::ABuildableAutoSupport()
{
	InstancedMeshProxy = CreateDefaultSubobject<UFGColoredInstanceMeshProxy>(TEXT("BuildableInstancedMeshProxy"));
	InstancedMeshProxy->SetupAttachment(RootComponent);
}
