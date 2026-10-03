// monke_island

#include "Creatures/TikiCreature.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"
#include "AIController.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

ATikiCreature::ATikiCreature()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(30.0f, 55.0f);
	bUseControllerRotationYaw = false;

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	Move->MaxWalkSpeed = 120.0f;
	Move->bRunPhysicsWithNoController = true;

	// the Blender exports face +Y with the origin at the feet
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -55.0f), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	HeldItem = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeldItem"));
	HeldItem->SetupAttachment(GetMesh());
	HeldItem->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Health = CreateDefaultSubobject<UIslanderHealthComponent>(TEXT("Health"));
	Health->SetIsReplicated(true);
}

void ATikiCreature::BeginPlay()
{
	Super::BeginPlay();

	HeldItem->SetLeaderPoseComponent(GetMesh());
	Health->OnDeath.AddDynamic(this, &ATikiCreature::HandleDeath);

	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ATikiCreature::HandleTakeAnyDamage);
		Health->MaxHealth = GetRulesHealth();
		Health->ResetHealth();
	}
	StuckCheckLocation = GetActorLocation();
	StuckCheckTime = GetWorld()->GetTimeSeconds();
	UpdateAnimation();
}

void ATikiCreature::SetDirector(ATikiDirector* InDirector)
{
	Director = InDirector;
}

bool ATikiCreature::IsDead() const
{
	return Health && Health->IsDead();
}

void ATikiCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && !IsDead() && Rules)
	{
		ServerThink(DeltaSeconds);
	}
	UpdateAnimation();
}

float ATikiCreature::MoveToward(const FVector& Target, float Speed)
{
	FVector Delta = Target - GetActorLocation();
	Delta.Z = 0.0f;
	const float Distance = static_cast<float>(Delta.Size());
	if (Distance > 1.0f)
	{
		GetCharacterMovement()->MaxWalkSpeed = Speed;
		AddMovementInput(Delta / Distance, 1.0f, true);
	}
	return Distance;
}

void ATikiCreature::FaceToward(const FVector& Target)
{
	FVector Delta = Target - GetActorLocation();
	Delta.Z = 0.0f;
	if (!Delta.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, Delta.Rotation().Yaw, 0.0f));
	}
}

bool ATikiCreature::UpdateStuck(bool bWantsToMove)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (!bWantsToMove)
	{
		StuckCheckLocation = GetActorLocation();
		StuckCheckTime = Now;
		return false;
	}
	if (Now - StuckCheckTime < 2.0)
	{
		return false;
	}
	const bool bStuck = FVector::DistSquared2D(StuckCheckLocation, GetActorLocation()) < FMath::Square(30.0);
	StuckCheckLocation = GetActorLocation();
	StuckCheckTime = Now;
	return bStuck;
}

void ATikiCreature::PlayAction(ETikiAction Action)
{
	MulticastPlayAction(Action);
}

void ATikiCreature::MulticastPlayAction_Implementation(ETikiAction Action)
{
	PlayLocalAction(Action);
}

bool ATikiCreature::IsActionPlaying() const
{
	return GetWorld()->GetTimeSeconds() < ActionEndTime;
}

void ATikiCreature::PlayLocalAction(ETikiAction Action)
{
	if (bDeathPlayed)
	{
		return;
	}
	UAnimSequenceBase* Clip = nullptr;
	bool bLoop = false;
	float Duration = 0.0f;
	switch (Action)
	{
	case ETikiAction::Attack:
		Clip = Anims.Attack;
		break;
	case ETikiAction::Hit:
	case ETikiAction::Emerge:
		Clip = Anims.Hit;
		break;
	case ETikiAction::Collect:
		Clip = Anims.Collect ? Anims.Collect.Get() : Anims.Idle.Get();
		bLoop = true;
		Duration = Rules ? Rules->CollectDuration : 2.0f;
		break;
	case ETikiAction::Cower:
		Clip = Anims.Cower;
		bLoop = true;
		Duration = 1.5f;
		break;
	default:
		break;
	}
	if (!Clip)
	{
		return;
	}
	if (Duration <= 0.0f)
	{
		Duration = Clip->GetPlayLength();
	}
	ActionEndTime = GetWorld()->GetTimeSeconds() + Duration;
	CurrentClip = nullptr;
	PlayClip(Clip, bLoop);
}

void ATikiCreature::UpdateAnimation()
{
	if (bDeathPlayed || IsActionPlaying())
	{
		return;
	}
	const float WalkSpeed = Rules ? FMath::Min(Rules->VillagerWalkSpeed, Rules->WarriorWalkSpeed) : 120.0f;
	switch (TikiMath::ChooseLocomotion(static_cast<float>(GetVelocity().Size2D()), WalkSpeed * 1.2f))
	{
	case ETikiLocomotion::Walk:
		PlayClip(Anims.Walk, true);
		break;
	case ETikiLocomotion::Run:
		PlayClip(Anims.Run ? Anims.Run.Get() : Anims.Walk.Get(), true);
		break;
	default:
		PlayClip(Anims.Idle, true);
		break;
	}
}

void ATikiCreature::PlayClip(UAnimSequenceBase* Clip, bool bLoop)
{
	if (!Clip || Clip == CurrentClip)
	{
		return;
	}
	CurrentClip = Clip;
	GetMesh()->PlayAnimation(Clip, bLoop);
}

void ATikiCreature::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.0f || IsDead())
	{
		return;
	}
	AActor* Causer = InstigatedBy && InstigatedBy->GetPawn() ? InstigatedBy->GetPawn() : DamageCauser;
	PlayAction(ETikiAction::Hit);
	OnDamaged(Causer, Damage);
}

void ATikiCreature::HandleDeath(AActor* Killer)
{
	bDeathPlayed = false;
	ActionEndTime = 0.0;
	if (Anims.Death)
	{
		CurrentClip = nullptr;
		PlayClip(Anims.Death, false);
	}
	bDeathPlayed = true;

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (HasAuthority())
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s killed by %s"), *GetName(), *GetNameSafe(Killer));
		SetLifeSpan(Rules ? FMath::Max(Rules->CorpseLifetime, 0.1f) : 8.0f);
	}
}

bool ATikiCreature::IsLivingPlayer(const APawn* Pawn)
{
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return false;
	}
	const UIslanderHealthComponent* PawnHealth = Pawn->FindComponentByClass<UIslanderHealthComponent>();
	return !PawnHealth || !PawnHealth->IsDead();
}

APawn* ATikiCreature::FindNearestPlayer(float MaxDistance) const
{
	APawn* Best = nullptr;
	double BestDistSq = FMath::Square(static_cast<double>(MaxDistance));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		if (!IsLivingPlayer(Pawn))
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Pawn->GetActorLocation(), GetActorLocation());
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Pawn;
		}
	}
	return Best;
}
