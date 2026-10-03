// monke_island

#include "Creatures/TikiWarrior.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ATikiWarrior::ATikiWarrior()
{
}

float ATikiWarrior::GetRulesHealth() const
{
	return Rules ? Rules->WarriorHealth : 80.0f;
}

void ATikiWarrior::BeginPlay()
{
	Super::BeginPlay();
	LastTargetTime = GetWorld()->GetTimeSeconds();
}

void ATikiWarrior::InitWarrior(ETikiWarriorRole InRole, APawn* InTarget, const FVector& InLeapDirection)
{
	WarriorRole = InRole;
	Target = InTarget;
	LeapDirection = InLeapDirection.GetSafeNormal2D();
	LastTargetTime = GetWorld()->GetTimeSeconds();
}

void ATikiWarrior::Burrow()
{
	if (bBurrowing || IsDead())
	{
		return;
	}
	bBurrowing = true;
	PendingStrikeTime = 0.0;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayAction(ETikiAction::Hit);
	SetLifeSpan(2.0f);
}

void ATikiWarrior::ServerThink(float DeltaSeconds)
{
	if (bBurrowing)
	{
		// sinks into the sand; the replicated movement carries it to the clients
		AddActorWorldOffset(FVector(0.0f, 0.0f, -80.0f * DeltaSeconds));
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	// leap out of the bush toward the target
	if (!LeapDirection.IsNearlyZero())
	{
		PlayAction(ETikiAction::Emerge);
		LaunchCharacter(LeapDirection * Rules->EmergeLeapSpeed + FVector(0.0f, 0.0f, Rules->EmergeLeapSpeed), true, true);
		LeapDirection = FVector::ZeroVector;
		return;
	}

	// blow in progress: it lands after the visible preparation
	if (PendingStrikeTime > 0.0)
	{
		if (Now >= PendingStrikeTime)
		{
			PendingStrikeTime = 0.0;
			StrikeNow();
		}
		return;
	}
	if (IsActionPlaying())
	{
		return;
	}

	// keep or find a target
	APawn* Victim = Target.Get();
	if (Victim && (!IsLivingPlayer(Victim) || FVector::Dist(Victim->GetActorLocation(), GetActorLocation()) > Rules->LoseRadius))
	{
		Victim = nullptr;
		Target.Reset();
	}
	if (!Victim && WarriorRole == ETikiWarriorRole::Night)
	{
		Victim = FindNearestPlayer(Rules->AggroRadius);
		Target = Victim;
	}

	if (!Victim)
	{
		// a defender with nobody left to punish goes back into the ground
		if (WarriorRole == ETikiWarriorRole::Defender && Now - LastTargetTime > 8.0)
		{
			Burrow();
			return;
		}
		if (IdleGoal.IsZero() || MoveToward(IdleGoal, Rules->WarriorWalkSpeed) < 80.0f || UpdateStuck(true))
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			IdleGoal = TikiMath::PointOnRing(GetActorLocation(), Angle, FMath::FRand(), 300.0f, 1200.0f);
		}
		return;
	}

	LastTargetTime = Now;
	const float Distance = static_cast<float>(FVector::Dist2D(Victim->GetActorLocation(), GetActorLocation()));
	if (TikiMath::CanStartAttack(Distance, Rules->AttackRange, Now, LastAttackTime, Rules->AttackCooldown))
	{
		GetCharacterMovement()->StopMovementImmediately();
		FaceToward(Victim->GetActorLocation());
		LastAttackTime = Now;
		PendingStrikeTime = Now + Rules->AttackWindup;
		PlayAction(ETikiAction::Attack);
		return;
	}
	if (Distance > Rules->AttackRange * 0.7f)
	{
		MoveToward(Victim->GetActorLocation(), Rules->WarriorRunSpeed);
	}
	else
	{
		FaceToward(Victim->GetActorLocation());
	}
}

void ATikiWarrior::StrikeNow()
{
	const FVector Forward = GetActorForwardVector();
	const FVector Start = GetActorLocation() + Forward * GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector End = Start + Forward * Rules->AttackReach;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiStrike), false, this);
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Rules->AttackRadius), Params);

	TSet<AActor*> Done;
	for (const FHitResult& Hit : Hits)
	{
		APawn* Victim = Cast<APawn>(Hit.GetActor());
		if (!IsLivingPlayer(Victim) || Done.Contains(Victim))
		{
			continue;
		}
		Done.Add(Victim);
		UGameplayStatics::ApplyDamage(Victim, Rules->AttackDamage, GetController(), this, nullptr);
		if (ACharacter* VictimCharacter = Cast<ACharacter>(Victim))
		{
			VictimCharacter->LaunchCharacter(Forward * Rules->AttackKnockback + FVector(0.0f, 0.0f, 150.0f), true, false);
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s strikes %s: %.0f damage"), *GetName(), *Victim->GetName(), Rules->AttackDamage);
	}
}

void ATikiWarrior::OnDamaged(AActor* Causer, float Damage)
{
	PendingStrikeTime = 0.0;
	if (APawn* Attacker = Cast<APawn>(Causer); IsLivingPlayer(Attacker))
	{
		Target = Attacker;
		LastTargetTime = GetWorld()->GetTimeSeconds();
	}
}
