// monke_island

#include "Creatures/SharkActor.h"
#include "Core/DeathCause.h"
#include "Creatures/SharkRules.h"
#include "Player/IslanderHealthComponent.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ASharkActor::ASharkActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	SetReplicateMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(45.0f);
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Collision->SetCanEverAffectNavigation(false);
	// moved kinematically: blocking the boat would shove its hull out of the water (T-063)
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	RootComponent = Collision;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Body->SetCanEverAffectNavigation(false);

	Tail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tail"));
	Tail->SetupAttachment(Body);
	Tail->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Tail->SetCanEverAffectNavigation(false);

	Health = CreateDefaultSubobject<UIslanderHealthComponent>(TEXT("Health"));
	Health->SetIsReplicated(true);
}

void ASharkActor::BeginPlay()
{
	Super::BeginPlay();

	Water = UIslandWaterSubsystem::Get(this);
	Home = GetActorLocation();
	PatrolGoal = Home;
	Health->OnDeath.AddDynamic(this, &ASharkActor::HandleDeath);

	if (HasAuthority())
	{
		if (!Rules)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_SharkRules: using the default rules"), *GetName());
			Rules = NewObject<USharkRules>(this);
		}
		Health->MaxHealth = Rules->Health;
		Health->ResetHealth();
		StateStartTime = GetWorld()->GetTimeSeconds();
		PickPatrolGoal();
	}
}

void ASharkActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ASharkActor, State);
}

void ASharkActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && Rules && State != ESharkState::Dead)
	{
		ServerTick(DeltaSeconds);
	}
	UpdateVisuals(DeltaSeconds);
}

void ASharkActor::SetState(ESharkState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	StateStartTime = GetWorld()->GetTimeSeconds();
	if (NewState == ESharkState::Patrol || NewState == ESharkState::Return)
	{
		Prey.Reset();
	}
	if (NewState == ESharkState::Patrol)
	{
		PickPatrolGoal();
	}
}

void ASharkActor::ServerTick(float DeltaSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float Elapsed = static_cast<float>(Now - StateStartTime);
	APawn* Target = Prey.Get();

	// the hunt ends cleanly when the swimmer leaves the water, dies or gets away
	const bool bPreyValid = Target && IsHuntable(Target, Rules->LoseRadius);
	const float ChaseSeconds = static_cast<float>(Now - FMath::Max(StateStartTime, LastBiteTime));
	const ESharkState Next = SharkMath::HuntStep(State, bPreyValid, State == ESharkState::Detect ? 0.0f : ChaseSeconds, Rules->ChaseGiveUpTime);
	if (Next != State)
	{
		SetState(Next);
		return;
	}

	switch (State)
	{
	case ESharkState::Patrol:
	{
		if (APawn* Found = FindPrey(Rules->DetectionRadius))
		{
			Prey = Found;
			SetState(ESharkState::Detect);
			break;
		}
		const bool bMoved = SwimToward(PatrolGoal, Rules->PatrolSpeed, DeltaSeconds, Rules->CruiseDepth);
		if (!bMoved || FVector::Dist2D(PatrolGoal, GetActorLocation()) < 150.0f)
		{
			PickPatrolGoal();
		}
		break;
	}

	case ESharkState::Detect:
		// slows down and turns toward the swimmer: the fin swings round before the rush
		SwimToward(Target->GetActorLocation(), Rules->PatrolSpeed * 0.3f, DeltaSeconds, Rules->CruiseDepth);
		if (Elapsed >= Rules->DetectionDelay)
		{
			SetState(ESharkState::Chase);
		}
		break;

	case ESharkState::Chase:
	case ESharkState::Attack:
	{
		const FVector PreyLocation = Target->GetActorLocation();
		const FVector Snout = GetActorLocation() + GetActorForwardVector() * Collision->GetScaledSphereRadius();
		const float Distance = static_cast<float>(FVector::Dist(Snout, PreyLocation));
		if (SharkMath::CanBite(Distance, Rules->AttackRange, Now, LastBiteTime, Rules->AttackInterval))
		{
			Bite(Target);
			SetState(ESharkState::Attack);
			break;
		}
		if (State == ESharkState::Attack && Now - LastBiteTime > 0.6)
		{
			SetState(ESharkState::Chase);
		}
		// dives to the swimmer's depth, never above the cruise depth nor into the bottom
		float SurfaceZ = 0.0f;
		float BottomZ = 0.0f;
		const float PreyDepth = SampleWater(PreyLocation, SurfaceZ, BottomZ) ? FMath::Max(Rules->CruiseDepth, SurfaceZ - static_cast<float>(PreyLocation.Z)) : Rules->CruiseDepth;
		if (!SwimToward(PreyLocation, Rules->ChaseSpeed, DeltaSeconds, PreyDepth) && Elapsed > 3.0f && Distance > Rules->AttackRange * 2.0f)
		{
			// the swimmer stands in water too shallow for the shark
			SetState(ESharkState::Return);
		}
		break;
	}

	case ESharkState::Return:
		SwimToward(Home, Rules->ReturnSpeed, DeltaSeconds, Rules->CruiseDepth);
		if (FVector::Dist2D(Home, GetActorLocation()) < Rules->PatrolRadius * 0.5f || Elapsed > 30.0f)
		{
			SetState(ESharkState::Patrol);
		}
		break;

	default:
		break;
	}
}

bool ASharkActor::SampleWater(const FVector& Location, float& OutSurfaceZ, float& OutBottomZ) const
{
	const UIslandWaterSubsystem* WaterSystem = Water.Get();
	OutSurfaceZ = WaterSystem ? WaterSystem->GetWaterSurfaceAt(Location) : 0.0f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SharkBottom), false, this);
	const FVector Start(Location.X, Location.Y, OutSurfaceZ + 10.0f);
	const FVector End(Location.X, Location.Y, OutSurfaceZ - 20000.0f);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		// open ocean beyond the modelled bottom: deep enough
		OutBottomZ = OutSurfaceZ - 20000.0f;
		return true;
	}
	OutBottomZ = static_cast<float>(Hit.ImpactPoint.Z);
	return true;
}

bool ASharkActor::IsNavigableAt(const FVector& Location) const
{
	float SurfaceZ = 0.0f;
	float BottomZ = 0.0f;
	return SampleWater(Location, SurfaceZ, BottomZ) && SharkMath::IsNavigable(SurfaceZ, BottomZ, Rules->MinWaterDepth);
}

bool ASharkActor::SwimToward(const FVector& Goal, float TargetSpeed, float DeltaSeconds, float DesiredDepthUnderSurface)
{
	const FVector Here = GetActorLocation();
	FVector ToGoal = Goal - Here;
	ToGoal.Z = 0.0f;

	FRotator Rotation = GetActorRotation();
	if (!ToGoal.IsNearlyZero())
	{
		Rotation.Yaw = SharkMath::TurnToward(Rotation.Yaw, ToGoal.Rotation().Yaw, Rules->TurnRateDeg, DeltaSeconds);
	}
	Speed = FMath::FInterpTo(Speed, TargetSpeed, DeltaSeconds, 2.5f);

	const FVector Forward = FRotator(0.0f, Rotation.Yaw, 0.0f).Vector();
	FVector Next = Here + Forward * Speed * DeltaSeconds;

	// a step toward the shallows is refused: the shark stops at the edge and turns
	float SurfaceZ = 0.0f;
	float BottomZ = 0.0f;
	const FVector Probe = Here + Forward * FMath::Max(Speed * DeltaSeconds, 120.0f);
	if (!SampleWater(Probe, SurfaceZ, BottomZ) || !SharkMath::IsNavigable(SurfaceZ, BottomZ, Rules->MinWaterDepth))
	{
		Speed = 0.0f;
		SetActorRotation(Rotation);
		return false;
	}

	const float MaxDepth = FMath::Max(Rules->CruiseDepth, SurfaceZ - BottomZ - 60.0f);
	const float TargetZ = SurfaceZ - FMath::Clamp(DesiredDepthUnderSurface, Rules->CruiseDepth, MaxDepth);
	Next.Z = FMath::FInterpTo(static_cast<float>(Here.Z), TargetZ, DeltaSeconds, 2.0f);
	SetActorLocationAndRotation(Next, Rotation, false);
	return true;
}

void ASharkActor::PickPatrolGoal()
{
	for (int32 Try = 0; Try < 10; ++Try)
	{
		const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Radius = Rules->PatrolRadius * FMath::Sqrt(FMath::FRand());
		const FVector Candidate = Home + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
		if (IsNavigableAt(Candidate))
		{
			PatrolGoal = Candidate;
			return;
		}
	}
	PatrolGoal = Home;
}

bool ASharkActor::IsHuntable(const APawn* Pawn, float Radius) const
{
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return false;
	}
	const UPawnMovementComponent* Move = Pawn->GetMovementComponent();
	const UIslanderHealthComponent* PawnHealth = Pawn->FindComponentByClass<UIslanderHealthComponent>();
	const float Distance = static_cast<float>(FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()));
	return SharkMath::IsValidPrey(Move && Move->IsSwimming(), PawnHealth && PawnHealth->IsDead(), Distance, Radius);
}

APawn* ASharkActor::FindPrey(float Radius) const
{
	APawn* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		if (!IsHuntable(Pawn, Radius))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Pawn;
		}
	}
	return Best;
}

void ASharkActor::Bite(APawn* Victim)
{
	LastBiteTime = GetWorld()->GetTimeSeconds();
	UGameplayStatics::ApplyDamage(Victim, Rules->AttackDamage, GetController(), this, USharkDamageType::StaticClass());
	if (ACharacter* Character = Cast<ACharacter>(Victim))
	{
		const FVector Away = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		Character->LaunchCharacter(Away * Rules->BiteKnockback, true, false);
	}
	MulticastBite();
	UE_LOG(Logmonke_island, Log, TEXT("%s bites %s: %.0f damage"), *GetName(), *Victim->GetName(), Rules->AttackDamage);
}

void ASharkActor::MulticastBite_Implementation()
{
	LungeTime = 0.35f;
}

void ASharkActor::HandleDeath(AActor* Killer)
{
	State = ESharkState::Dead;
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (HasAuthority())
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s killed by %s"), *GetName(), *GetNameSafe(Killer));
		SetLifeSpan(FMath::Max(Rules ? Rules->CorpseLifetime : 10.0f, 0.1f));
	}
}

void ASharkActor::OnRep_State()
{
	if (State == ESharkState::Dead)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ASharkActor::UpdateVisuals(float DeltaSeconds)
{
	if (State == ESharkState::Dead)
	{
		// belly up and slowly sinking; the server moves the actor, clients follow the replicated movement
		Body->SetRelativeRotation(FMath::RInterpTo(Body->GetRelativeRotation(), FRotator(0.0f, 0.0f, 180.0f), DeltaSeconds, 1.5f));
		Tail->SetRelativeRotation(FRotator::ZeroRotator);
		if (HasAuthority())
		{
			AddActorWorldOffset(FVector(0.0f, 0.0f, -25.0f * DeltaSeconds));
		}
		return;
	}

	// the tail beats faster with speed: the velocity of the replicated movement on clients
	const float CurrentSpeed = HasAuthority() ? Speed : static_cast<float>(GetVelocity().Size2D());
	const float CruiseSpeed = Rules ? FMath::Max(Rules->PatrolSpeed, 1.0f) : 220.0f;
	const float Rate = TailBeatHz * FMath::Clamp(0.35f + CurrentSpeed / CruiseSpeed, 0.35f, 3.0f);
	TailPhase = FMath::Fmod(TailPhase + 2.0f * PI * Rate * DeltaSeconds, 2.0f * PI);
	Tail->SetRelativeRotation(FRotator(0.0f, FMath::Sin(TailPhase) * TailSwingDeg, 0.0f));

	// bite: the body lunges forward and comes back
	LungeTime = FMath::Max(0.0f, LungeTime - DeltaSeconds);
	const float Lunge = LungeTime > 0.0f ? FMath::Sin((1.0f - LungeTime / 0.35f) * PI) * 60.0f : 0.0f;
	Body->SetRelativeLocation(FVector(Lunge, 0.0f, 0.0f));
}

ASharkActor* ASharkActor::SpawnInWaterNear(UWorld* World, const FVector& Near, TSubclassOf<ASharkActor> SharkClass)
{
	if (!World || !SharkClass)
	{
		return nullptr;
	}
	const ASharkActor* Defaults = SharkClass->GetDefaultObject<ASharkActor>();
	const USharkRules* SharkRules = Defaults && Defaults->Rules ? Defaults->Rules.Get() : GetDefault<USharkRules>();
	const UIslandWaterSubsystem* WaterSystem = UIslandWaterSubsystem::Get(World);

	for (int32 Ring = 1; Ring <= 8; ++Ring)
	{
		for (int32 Step = 0; Step < 12; ++Step)
		{
			const float Angle = Step * (2.0f * PI / 12.0f);
			const FVector Candidate = Near + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * (Ring * 500.0f);
			const float SurfaceZ = WaterSystem ? WaterSystem->GetWaterSurfaceAt(Candidate) : 0.0f;
			FHitResult Hit;
			const bool bBottom = World->LineTraceSingleByObjectType(Hit, FVector(Candidate.X, Candidate.Y, SurfaceZ + 10.0f),
				FVector(Candidate.X, Candidate.Y, SurfaceZ - 20000.0f), FCollisionObjectQueryParams(ECC_WorldStatic));
			const float BottomZ = bBottom ? static_cast<float>(Hit.ImpactPoint.Z) : SurfaceZ - 20000.0f;
			if (!SharkMath::IsNavigable(SurfaceZ, BottomZ, SharkRules->MinWaterDepth * 1.5f))
			{
				continue;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Location(Candidate.X, Candidate.Y, SurfaceZ - SharkRules->CruiseDepth);
			return World->SpawnActor<ASharkActor>(SharkClass, Location, FRotator(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f), Params);
		}
	}
	return nullptr;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GSpawnSharkCommand(
	TEXT("monke.SpawnShark"),
	TEXT("Spawns a shark in deep water near the local player (server / host)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Pawn || World->GetNetMode() == NM_Client)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("monke.SpawnShark: server only, with a local player"));
			return;
		}
		UClass* SharkClass = StaticLoadClass(ASharkActor::StaticClass(), nullptr, TEXT("/Game/MonkeIsland/Characters/Shark/Blueprints/BP_Shark.BP_Shark_C"));
		ASharkActor* Shark = ASharkActor::SpawnInWaterNear(World, Pawn->GetActorLocation(), SharkClass ? SharkClass : ASharkActor::StaticClass());
		UE_LOG(Logmonke_island, Log, TEXT("monke.SpawnShark: %s"), Shark ? *Shark->GetActorLocation().ToCompactString() : TEXT("no water deep enough within 40 m"));
	}));
#endif
