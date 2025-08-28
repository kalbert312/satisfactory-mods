// 

#include "AutoSupportModClientSubsystem.h"

AAutoSupportModClientSubsystem::AAutoSupportModClientSubsystem()
{
	ReplicationPolicy = ESubsystemReplicationPolicy::SpawnOnClient;
}

FBuildableAutoSupportData AAutoSupportModClientSubsystem::GetLastAutoSupportData() const
{
	return LastAutoSupportData;
}

FBuildableAutoSupportData AAutoSupportModClientSubsystem::GetLastAutoSupport1mData() const
{
	return LastAutoSupport1mData;
}

FBuildableAutoSupportData AAutoSupportModClientSubsystem::GetLastAutoSupport2mData() const
{
	return LastAutoSupport2mData;
}

FBuildableAutoSupportData AAutoSupportModClientSubsystem::GetLastAutoSupport4mData() const
{
	return LastAutoSupport4mData;
}

void AAutoSupportModClientSubsystem::SetLastAutoSupport1mData(const FBuildableAutoSupportData& Data)
{
	LastAutoSupport1mData = Data;
	LastAutoSupportData = Data;
}

void AAutoSupportModClientSubsystem::SetLastAutoSupport2mData(const FBuildableAutoSupportData& Data)
{
	LastAutoSupport2mData = Data;
	LastAutoSupportData = Data;
}

void AAutoSupportModClientSubsystem::SetLastAutoSupport4mData(const FBuildableAutoSupportData& Data)
{
	LastAutoSupport4mData = Data;
	LastAutoSupportData = Data;
}

void AAutoSupportModClientSubsystem::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	LastAutoSupportData.ClearInvalidReferences();
	LastAutoSupport1mData.ClearInvalidReferences();
	LastAutoSupport2mData.ClearInvalidReferences();
	LastAutoSupport4mData.ClearInvalidReferences();
}

bool AAutoSupportModClientSubsystem::ShouldSave_Implementation() const
{
	return true;
}
