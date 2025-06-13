// 

#include "BuildableAutoSupport.h"

#include "FGBuildingDescriptor.h"
#include "FGColoredInstanceMeshProxy.h"
#include "LandscapeProxy.h"
#include "ModLogging.h"
#include "Kismet/GameplayStatics.h"

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
	const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor,
	const int32 Count,
	const FVector& Size,
	FTransform& WorkingTransform)
{
	const TSubclassOf<AFGBuildable> BuildableClass = UFGBuildingDescriptor::GetBuildableClass(PartDescriptor.Get());
	
	for (auto i = 0; i < Count; ++i)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Spawning part. Start Transform: %s"), *WorkingTransform.ToString());
		
		// Spawn the part
		auto* Buildable = Buildables->BeginSpawnBuildable(BuildableClass, WorkingTransform);
		Buildable->FinishSpawning(WorkingTransform);
		Buildable->PlayBuildEffects(UGameplayStatics::GetPlayerController(GetWorld(), 0));

		// Update the transform
		WorkingTransform.AddToTranslation(FVector(0, 0, -1 * Size.Z));
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::BuildParts Next Transform: %s"), *WorkingTransform.ToString());
	}

	// TODO: subtract resources from inventory.
}

void ABuildableAutoSupport::BuildSupports()
{
	if (!AutoSupportData.MiddlePartDescriptor.IsValid() && !AutoSupportData.StartPartDescriptor.IsValid() && !AutoSupportData.EndPartDescriptor.IsValid())
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
	FBox StartBox, MidBox, EndBox;

	PlanBuild(BuildDistance, PartCounts, StartBox, MidBox, EndBox);
	
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
		BuildParts(Buildables, AutoSupportData.StartPartDescriptor, PartCounts.X, StartBox.GetSize(), WorkingTransform);
	}

	if (PartCounts.Y > 0)
	{
		BuildParts(Buildables, AutoSupportData.MiddlePartDescriptor, PartCounts.Y, MidBox.GetSize(), WorkingTransform);
	}

	if (PartCounts.Z > 0)
	{
		BuildParts(Buildables, AutoSupportData.EndPartDescriptor, PartCounts.Z, EndBox.GetSize(), WorkingTransform);
	}

	// Dismantle self
	Execute_Dismantle(this);
}

void ABuildableAutoSupport::GatherDependencies_Implementation(TArray<UObject*>& out_dependentObjects)
{
	
}

bool ABuildableAutoSupport::NeedTransform_Implementation()
{
	return false;
}

void ABuildableAutoSupport::PreSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	if (!AutoSupportData.StartPartDescriptor.IsValid())
	{
		AutoSupportData.StartPartDescriptor = nullptr;
	}

	if (!AutoSupportData.MiddlePartDescriptor.IsValid())
	{
		AutoSupportData.MiddlePartDescriptor = nullptr;
	}

	if (!AutoSupportData.EndPartDescriptor.IsValid())
	{
		AutoSupportData.EndPartDescriptor = nullptr;
	}
}

void ABuildableAutoSupport::PostSaveGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	
}

void ABuildableAutoSupport::PreLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	
}

void ABuildableAutoSupport::PostLoadGame_Implementation(int32 saveVersion, int32 gameVersion)
{
	if (!AutoSupportData.StartPartDescriptor.IsValid())
	{
		AutoSupportData.StartPartDescriptor = nullptr;
	}

	if (!AutoSupportData.MiddlePartDescriptor.IsValid())
	{
		AutoSupportData.MiddlePartDescriptor = nullptr;
	}

	if (!AutoSupportData.EndPartDescriptor.IsValid())
	{
		AutoSupportData.EndPartDescriptor = nullptr;
	}
}

bool ABuildableAutoSupport::ShouldSave_Implementation() const
{
	return true;
}

float ABuildableAutoSupport::Trace() const
{
	FCollisionQueryParams QueryParams;
	QueryParams.TraceTag = FName("BuildableAutoSupport_Trace");
	QueryParams.AddIgnoredActor(this);
	
	const auto StartLocation = GetActorLocation();
	const auto EndLocation = StartLocation + FVector(0, 0, -1 * MaxBuildDistance);

	const FCollisionShape CollisionShape = FCollisionShape::MakeBox(FVector(.5, .5, .5));

	FCollisionResponseParams ResponseParams(ECR_Overlap);
	
	TArray<FHitResult> HitResults;
	GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		ECC_Visibility,
		CollisionShape,
		QueryParams,
		ResponseParams);

	if (HitResults.Num() == 0)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace No Hits!"));
		
		return MaxBuildDistance;
	}

	int32 HitIndex = 0;
	for (const auto& HitResult : HitResults)
	{
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::Trace HitResult[%i]: %s"), HitIndex, *HitResult.ToString());

		const auto* HitActor = HitResult.GetActor();
		const auto IsLandscapeHit = HitActor && HitActor->IsA<ALandscapeProxy>();
		const auto* HitComponent = HitResult.GetComponent();
		
		MOD_LOG(
			Verbose,
			TEXT("BuildableAutoSupport::Trace IsLandscapeHit: %s; Actor class: %s, Component class: %s"),
			TEXT_CONDITION(IsLandscapeHit),
			HitActor ? *HitActor->GetClass()->GetName() : TEXT_NULL,
			HitComponent ? *HitComponent->GetClass()->GetName() : TEXT_NULL);

		if (IsLandscapeHit)
		{
			return HitResult.Distance;
		}

		++HitIndex;
	}

	return MaxBuildDistance;
}

void ABuildableAutoSupport::PlanBuild(float BuildDistance, OUT FVector& OutPartCounts, OUT FBox& OutStartBox, OUT FBox& OutMidBox, OUT FBox& OutEndBox) const
{
	OutPartCounts = FVector::ZeroVector;
	
	// Start with the top because that's where the auto support is planted.
	if (AutoSupportData.StartPartDescriptor.IsValid())
	{
		GetBuildableSize(AutoSupportData.StartPartDescriptor, OutStartBox);
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
	if (AutoSupportData.EndPartDescriptor.IsValid())
	{
		GetBuildableSize(AutoSupportData.EndPartDescriptor, OutEndBox);
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
	
	if (AutoSupportData.MiddlePartDescriptor.IsValid())
	{
		GetBuildableSize(AutoSupportData.MiddlePartDescriptor, OutMidBox);
		const auto Size = OutMidBox.GetSize();
		
		auto NumMiddleParts = static_cast<int32>(BuildDistance / Size.Z);
		MOD_LOG(Verbose, TEXT("BuildableAutoSupport::PlanBuild Mid Size: %s, Num Mid: %d"), *Size.ToString(), NumMiddleParts);
		
		OutPartCounts.Y = NumMiddleParts;
	}
}

void ABuildableAutoSupport::GetBuildableSize(const TSoftClassPtr<UFGBuildingDescriptor>& PartDescriptor, OUT FBox& OutBox)
{
	const auto Buildable = GetDefault<AFGBuildable>(UFGBuildingDescriptor::GetBuildableClass(PartDescriptor.Get()));
	OutBox = Buildable->GetCombinedClearanceBox();
}

