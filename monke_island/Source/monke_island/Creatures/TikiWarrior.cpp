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
#include "Net/UnrealNetwork.h"

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

void ATikiWarrior::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATikiWarrior, bInCombat);
}

void ATikiWarrior::InitWarrior(ETikiWarriorRole InRole, APawn* InTarget, const FVector& InLeapDirection)
{
	WarriorRole = InRole;
	Target = InTarget;
	LeapDirection = InLeapDirection.GetSafeNormal2D();
	LastTargetTime = GetWorld()->GetTimeSeconds();
}

void ATikiWarrior::Engage(APawn* InTarget)
{
	if (IsLivingPlayer(InTarget) && !IsDead())
	{
		Target = InTarget;
		LastTargetTime = GetWorld()->GetTimeSeconds();
	}
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

	// keep or find a target; a guardian also gives up a player who led it too far from its village
	const bool bHasPost = (WarriorRole == ETikiWarriorRole::Guardian || WarriorRole == ETikiWarriorRole::Ambusher) && PostRadius > 0.0f;
	APawn* Victim = Target.Get();
	// its prey down: a victory; out of reach in the water: a taunt from the shore
	if (Victim && !IsLivingPlayer(Victim))
	{
		PlayAction(ETikiAction::Victory);
	}
	else if (Victim && IsOutOfReachInWater(Victim) && Now - LastTauntTime >= Rules->TauntInterval)
	{
		LastTauntTime = Now;
		FaceToward(Victim->GetActorLocation());
		PlayAction(ETikiAction::Taunt);
	}
	if (Victim && (!IsLivingPlayer(Victim) || IsOutOfReachInWater(Victim) || FVector::Dist(Victim->GetActorLocation(), GetActorLocation()) > Rules->LoseRadius
		|| (bHasPost && FVector::Dist2D(Victim->GetActorLocation(), Post) > Rules->LoseRadius)))
	{
		Victim = nullptr;
		Target.Reset();
	}
	if (!Victim && WarriorRole != ETikiWarriorRole::Defender)
	{
		Victim = FindNearestPlayer(Rules->AggroRadius);
		if (IsOutOfReachInWater(Victim))
		{
			Victim = nullptr;
		}
		Target = Victim;
		// a growl when it spots a player, not every time it finds it again
		if (Victim && Now - LastRoarTime >= Rules->AggroVoiceCooldown)
		{
			LastRoarTime = Now;
			PlayAction(ETikiAction::Alert);
		}
	}

	bInCombat = Victim != nullptr;
	if (!Victim)
	{
		// a defender with nobody left to punish goes back into the ground, an ambusher into its bush
		if ((WarriorRole == ETikiWarriorRole::Defender && Now - LastTargetTime > 8.0)
			|| (WarriorRole == ETikiWarriorRole::Ambusher && Now - LastTargetTime > Rules->AmbushIdleTime))
		{
			Burrow();
			return;
		}
		// a guardian stays by its bush: it wanders around its post and walks back when it strayed
		if (bHasPost && TikiMath::IsOffPost(GetActorLocation(), Post, PostRadius))
		{
			IdleGoal = Post;
		}
		if (IdleGoal.IsZero() || MoveToward(IdleGoal, Rules->WarriorWalkSpeed) < 80.0f || UpdateStuck(true))
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			IdleGoal = bHasPost ? TikiMath::PointOnRing(Post, Angle, FMath::Sqrt(FMath::FRand()), 0.0f, PostRadius)
				: TikiMath::PointOnRing(GetActorLocation(), Angle, FMath::FRand(), 300.0f, 1200.0f);
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
		PlayAction(ETikiAction::Attack, static_cast<uint8>(FMath::RandRange(0, Anims.AttackVariants.Num())));
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
		// without the arm holding its weapon, it strikes with the other fist
		const float Damage = Rules->AttackDamage * (IsMaimed() ? Rules->MaimedDamageScale : 1.0f);
		UGameplayStatics::ApplyDamage(Victim, Damage, GetController(), this, nullptr);
		if (ACharacter* VictimCharacter = Cast<ACharacter>(Victim))
		{
			VictimCharacter->LaunchCharacter(Forward * Rules->AttackKnockback + FVector(0.0f, 0.0f, 150.0f), true, false);
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s strikes %s: %.0f damage"), *GetName(), *Victim->GetName(), Damage);
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
