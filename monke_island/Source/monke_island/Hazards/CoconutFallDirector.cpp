// monke_island

#include "Hazards/CoconutFallDirector.h"
#include "Hazards/CoconutFallRules.h"
#include "Hazards/FallingCoconutActor.h"
#include "Storm/WindStormActor.h"
#include "monke_island.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

ACoconutFallDirector::ACoconutFallDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.25f;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ACoconutFallDirector::BeginPlay()
{
	Super::BeginPlay();

	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_CoconutFallRules: using the default rules"), *GetName());
		Rules = NewObject<UCoconutFallRules>(this);
	}
	// only the server drops nuts; they replicate to the clients
	SetActorTickEnabled(HasAuthority());
	if (HasAuthority())
	{
		FindPalms();
	}
}

void ACoconutFallDirector::FindPalms()
{
	PalmComponents.Reset();
	int32 Instances = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UInstancedStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (UInstancedStaticMeshComponent* Component : Components)
		{
			const UStaticMesh* StaticMesh = Component ? Component->GetStaticMesh() : nullptr;
			if (!StaticMesh || !StaticMesh->GetName().Contains(Rules->PalmMeshNameFilter))
			{
				continue;
			}
			// broken, dead, fallen and young palms have no fruiting crown
			const FString MeshName = StaticMesh->GetName();
			if (!Rules->CrownedPalmTokens.ContainsByPredicate([&MeshName](const FString& Token) { return MeshName.Contains(Token); }))
			{
				continue;
			}
			const FBox Box = StaticMesh->GetBoundingBox();
			if (Box.Max.Z - Box.Min.Z < Rules->MinPalmHeightCm)
			{
				continue;
			}
			PalmComponents.Add(Component);
			Instances += Component->GetInstanceCount();
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d palm components, %d palms can drop nuts"), *GetName(), PalmComponents.Num(), Instances);
}

void ACoconutFallDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Rules)
	{
		return;
	}
	float WindStrength = 0.0f;
	for (TActorIterator<AWindStormActor> It(GetWorld()); It; ++It)
	{
		WindStrength = static_cast<float>(It->GetWind().Size());
		break;
	}
	const float Interval = Rules->IntervalFor(WindStrength);

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		AController* Controller = It->Get();
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn || !Controller->IsPlayerController())
		{
			continue;
		}
		float& Timer = Timers.FindOrAdd(Controller, FMath::FRandRange(0.5f, 1.5f) * Interval);
		Timer -= DeltaSeconds;
		if (Timer <= 0.0f)
		{
			// random intervals around the mean: falls never feel scheduled
			Timer = FMath::FRandRange(0.4f, 1.6f) * Interval;
			const float Roll = FMath::FRand();
			const EPick Pick = Roll < Rules->AimedFallChance ? EPick::AimedAtTarget
				: (Roll < Rules->AimedFallChance + Rules->ClosestPalmChance ? EPick::Closest : EPick::Random);
			if (!DropNear(Pawn, Pick) && Pick == EPick::AimedAtTarget)
			{
				// no crown close enough to aim: a palm nearby drops anyway
				DropNear(Pawn, EPick::Closest);
			}
		}
	}
}

AFallingCoconutActor* ACoconutFallDirector::DropNear(const APawn* Target, EPick Pick)
{
	if (!Rules || !HasAuthority() || !Target)
	{
		return nullptr;
	}
	const FVector Location = Target->GetActorLocation();
	const bool bAim = Pick == EPick::AimedAtTarget;
	const float Radius = bAim ? Rules->MaxAimDistanceCm + 1000.0f : Rules->SearchRadiusCm;

	// candidate palms around the location
	struct FCandidate
	{
		UInstancedStaticMeshComponent* Component;
		int32 Instance;
		double DistanceSq;
	};
	TArray<FCandidate> Candidates;
	for (const TWeakObjectPtr<UInstancedStaticMeshComponent>& Weak : PalmComponents)
	{
		UInstancedStaticMeshComponent* Component = Weak.Get();
		if (!Component)
		{
			continue;
		}
		for (const int32 Instance : Component->GetInstancesOverlappingSphere(Location, Radius, true))
		{
			if (NutsLeft(Component, Instance) <= 0)
			{
				continue;
			}
			FTransform Transform;
			Component->GetInstanceTransform(Instance, Transform, true);
			Candidates.Add({Component, Instance, FVector::DistSquared2D(Transform.GetLocation(), Location)});
		}
	}
	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	// start inside the crown: the top of the mesh bounds, which follows leaning and curved trunks
	auto CrownOf = [this](const FCandidate& Candidate)
	{
		FTransform Transform;
		Candidate.Component->GetInstanceTransform(Candidate.Instance, Transform, true);
		const FBox Box = Candidate.Component->GetStaticMesh()->GetBoundingBox();
		const FVector LocalTop(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z - Rules->CrownDepthCm / FMath::Max(Transform.GetScale3D().Z, 0.1));
		return Transform.TransformPosition(LocalTop);
	};

	const FCandidate* Dropper = &Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	FVector Start = CrownOf(*Dropper);
	if (Pick != EPick::Random)
	{
		// the closest crown, not trunk base: leaning palms hang their crown away from the base
		double Best = TNumericLimits<double>::Max();
		for (const FCandidate& Candidate : Candidates)
		{
			const FVector Crown = CrownOf(Candidate);
			const double DistanceSq = FVector::DistSquared2D(Crown, Location);
			if (DistanceSq < Best && Crown.Z > Location.Z + 200.0)
			{
				Best = DistanceSq;
				Start = Crown;
				Dropper = &Candidate;
			}
		}
		if (Best == TNumericLimits<double>::Max() || (bAim && Best > FMath::Square(Rules->MaxAimDistanceCm)))
		{
			return nullptr;
		}
	}
	if (!bAim)
	{
		Start += FMath::VRand() * FVector(1.0, 1.0, 0.3) * Rules->CrownSpreadCm;
	}

	// keep the level tidy: the oldest nut goes first
	LiveCoconuts.RemoveAll([](const TWeakObjectPtr<AFallingCoconutActor>& Nut) { return !Nut.IsValid(); });
	while (LiveCoconuts.Num() >= Rules->MaxLiveCoconuts)
	{
		if (AFallingCoconutActor* Oldest = LiveCoconuts[0].Get())
		{
			Oldest->Destroy();
		}
		LiveCoconuts.RemoveAt(0);
	}

	UStaticMesh* NutMesh = nullptr;
	if (Rules->CoconutMeshes.Num() > 0)
	{
		NutMesh = Rules->CoconutMeshes[FMath::RandRange(0, Rules->CoconutMeshes.Num() - 1)].LoadSynchronous();
	}
	AFallingCoconutActor* Nut = GetWorld()->SpawnActorDeferred<AFallingCoconutActor>(AFallingCoconutActor::StaticClass(),
		FTransform(FMath::VRand().Rotation(), Start), this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Nut)
	{
		return nullptr;
	}
	Nut->Setup(NutMesh, Rules);
	Nut->FinishSpawning(FTransform(FMath::VRand().Rotation(), Start));
	LiveCoconuts.Add(Nut);
	// the nut is gone from this crown for good
	NutsDropped.FindOrAdd(TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>(Dropper->Component, Dropper->Instance))++;

	if (bAim)
	{
		// ballistic shot onto the head: fall time from the crown, where the player will be (the nut is sheltered
		// from the wind until it bounces, see AFallingCoconutActor)
		const float Gravity = FMath::Abs(GetWorld()->GetGravityZ());
		const FVector Head = Location + FVector(0.0, 0.0, 60.0);
		const float Time = FMath::Sqrt(2.0f * FMath::Max(static_cast<float>(Start.Z - Head.Z), 50.0f) / Gravity);
		const FVector Aim = Head + Target->GetVelocity() * Time;
		Nut->Launch(FVector((Aim.X - Start.X) / Time, (Aim.Y - Start.Y) / Time, 0.0));
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s nut from %.0f m above, %.1f m from %s"), *GetName(),
		bAim ? TEXT("aimed") : (Pick == EPick::Closest ? TEXT("closest") : TEXT("random")),
		(Start.Z - Location.Z) / 100.0, FVector::Dist2D(Start, Location) / 100.0, *Target->GetName());
	return Nut;
}

int32 ACoconutFallDirector::NutsLeft(UInstancedStaticMeshComponent* Component, int32 Instance) const
{
	if (!Rules || !Component)
	{
		return 0;
	}
	// stable per tree: the same instance always starts with the same number of nuts
	const uint32 Key = HashCombine(GetTypeHash(Component->GetFName()), GetTypeHash(Instance));
	const int32* Dropped = NutsDropped.Find(TPair<TWeakObjectPtr<UInstancedStaticMeshComponent>, int32>(Component, Instance));
	return Rules->NutsForPalm(Key) - (Dropped ? *Dropped : 0);
}

#if !UE_BUILD_SHIPPING

static FAutoConsoleCommandWithWorld GDropCoconutCommand(
	TEXT("monke.DropCoconut"),
	TEXT("Drops a coconut aimed at the first player from the closest palm (server)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const APawn* Pawn = nullptr;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It && !Pawn; ++It)
		{
			Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		}
		for (TActorIterator<ACoconutFallDirector> It(World); It && Pawn; ++It)
		{
			It->DropNear(Pawn, ACoconutFallDirector::EPick::AimedAtTarget);
			break;
		}
	}));

#endif
