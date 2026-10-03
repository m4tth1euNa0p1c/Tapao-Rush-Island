// monke_island

#include "Player/IslanderCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "TimerManager.h"
#include "monke_island.h"

UIslanderCombatComponent::UIslanderCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	PunchObjectTypes = {
		UEngineTypes::ConvertToObjectType(ECC_Pawn),
		UEngineTypes::ConvertToObjectType(ECC_PhysicsBody),
	};
}

void UIslanderCombatComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UIslanderCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(UIslanderCombatComponent, PunchCount, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(UIslanderCombatComponent, bReplicatedGuard, COND_SkipOwner);
}

ACharacter* UIslanderCombatComponent::GetCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

UIslanderMovementComponent* UIslanderCombatComponent::GetIslanderMovement() const
{
	const ACharacter* Character = GetCharacter();
	return Character ? Cast<UIslanderMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

float UIslanderCombatComponent::ComputeGuardedDamage(float Damage, bool bGuarding, float FacingDot, float FrontDot, float Multiplier, bool bHasStamina)
{
	if (!bGuarding || FacingDot < FrontDot || !bHasStamina)
	{
		return Damage;
	}
	return Damage * Multiplier;
}

bool UIslanderCombatComponent::IsGuarding() const
{
	const ACharacter* Character = GetCharacter();
	if (Character && (Character->IsLocallyControlled() || Character->HasAuthority()))
	{
		const UIslanderMovementComponent* Move = GetIslanderMovement();
		return Move && Move->IsGuarding();
	}
	return bReplicatedGuard;
}

bool UIslanderCombatComponent::CanPunch() const
{
	const ACharacter* Character = GetCharacter();
	const UIslanderMovementComponent* Move = GetIslanderMovement();
	if (!Character || !Move || AttackPhase != EIslanderAttackPhase::Idle)
	{
		return false;
	}
	if (GetWorld()->GetTimeSeconds() - LastPunchTime < PunchCooldown)
	{
		return false;
	}

	// no punch while guarding, swimming, flying or knocked out (cahier §5: carrying will block it too)
	const UIslanderHealthComponent* Health = Character->FindComponentByClass<UIslanderHealthComponent>();
	const bool bAlive = !Health || !Health->IsDead();
	return bAlive && !Move->IsGuarding() && !Move->IsSwimming() && !Move->IsFlying() && Move->MovementMode != MOVE_None
		&& Move->GetStamina() >= PunchStaminaCost;
}

void UIslanderCombatComponent::RequestPunch()
{
	ACharacter* Character = GetCharacter();
	if (!Character || !CanPunch())
	{
		return;
	}

	// punch where the player looks: turn the body to the camera yaw (replicated by the movement)
	if (Character->IsLocallyControlled() && Character->GetController())
	{
		Character->SetActorRotation(FRotator(0.0f, Character->GetController()->GetControlRotation().Yaw, 0.0f));
	}

	if (Character->HasAuthority())
	{
		ServerPunch_Implementation();
	}
	else
	{
		// predicted visuals and stamina on the owner, the server decides the hit
		GetIslanderMovement()->ConsumeStamina(PunchStaminaCost);
		LastPunchTime = GetWorld()->GetTimeSeconds();
		StartPunchTimeline(false);
		ServerPunch();
	}
}

void UIslanderCombatComponent::ServerPunch_Implementation()
{
	UIslanderMovementComponent* Move = GetIslanderMovement();
	if (!Move)
	{
		return;
	}

	// the server keeps its own cooldown and stamina: a cheating or lagging client cannot punch faster
	if (!CanPunch() || !Move->ConsumeStamina(PunchStaminaCost))
	{
		return;
	}

	LastPunchTime = GetWorld()->GetTimeSeconds();
	++PunchCount;
	StartPunchTimeline(true);
}

void UIslanderCombatComponent::OnRep_PunchCount()
{
	StartPunchTimeline(false);
}

void UIslanderCombatComponent::StartPunchTimeline(bool bApplyHit)
{
	bPendingHit = bApplyHit;
	AttackPhase = EIslanderAttackPhase::Windup;
	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &UIslanderCombatComponent::EnterStrike, FMath::Max(PunchWindup, 0.01f), false);
}

void UIslanderCombatComponent::EnterStrike()
{
	AttackPhase = EIslanderAttackPhase::Strike;
	if (bPendingHit)
	{
		bPendingHit = false;
		ApplyPunchHit();
	}
	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &UIslanderCombatComponent::EnterRecover, FMath::Max(PunchStrikeTime, 0.01f), false);
}

void UIslanderCombatComponent::EnterRecover()
{
	AttackPhase = EIslanderAttackPhase::Recover;
	const float Remaining = PunchCooldown - PunchWindup - PunchStrikeTime;
	GetWorld()->GetTimerManager().SetTimer(PhaseTimer, this, &UIslanderCombatComponent::EnterIdle, FMath::Max(Remaining, 0.05f), false);
}

void UIslanderCombatComponent::EnterIdle()
{
	AttackPhase = EIslanderAttackPhase::Idle;
}

void UIslanderCombatComponent::ApplyPunchHit()
{
	ACharacter* Character = GetCharacter();
	if (!Character || !Character->HasAuthority())
	{
		return;
	}

	const FRotator Aim = Character->GetController() ? Character->GetController()->GetControlRotation() : Character->GetActorRotation();
	const FVector Forward = FRotator(0.0f, Aim.Yaw, 0.0f).Vector();
	const float Radius = Character->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Start = Character->GetActorLocation() + Forward * Radius;
	const FVector End = Start + Forward * PunchReach;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderPunch), false, Character);
	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& Type : PunchObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(Type));
	}
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(PunchRadius), Params);

	TSet<AActor*> Done;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Target = Hit.GetActor();
		if (!Target || Target == Character || Done.Contains(Target))
		{
			continue;
		}
		Done.Add(Target);

		// damage, reduced by a frontal guard that still has the stamina to hold
		float Damage = PunchDamage;
		ACharacter* TargetCharacter = Cast<ACharacter>(Target);
		if (UIslanderCombatComponent* TargetCombat = Target->FindComponentByClass<UIslanderCombatComponent>())
		{
			UIslanderMovementComponent* TargetMove = TargetCombat->GetIslanderMovement();
			const bool bGuarding = TargetCombat->IsGuarding();
			const float FacingDot = static_cast<float>(FVector::DotProduct(Target->GetActorForwardVector(), -Forward));
			const bool bHasStamina = TargetMove && TargetMove->GetStamina() >= TargetCombat->GuardStaminaPerBlock;
			Damage = ComputeGuardedDamage(Damage, bGuarding, FacingDot, TargetCombat->GuardFrontDot, TargetCombat->GuardDamageMultiplier, bHasStamina);
			if (Damage < PunchDamage && TargetMove)
			{
				TargetMove->ConsumeStamina(TargetCombat->GuardStaminaPerBlock);
			}
		}

		if (Damage > 0.0f && Target->FindComponentByClass<UIslanderHealthComponent>())
		{
			UGameplayStatics::ApplyDamage(Target, Damage, Character->GetController(), Character, nullptr);
		}

		// knockback, smaller when blocked
		const float KnockScale = PunchDamage > 0.0f ? Damage / PunchDamage : 1.0f;
		if (TargetCharacter)
		{
			TargetCharacter->LaunchCharacter(Forward * PunchKnockback * KnockScale + FVector(0.0f, 0.0f, 120.0f * KnockScale), true, false);
		}
		else if (UPrimitiveComponent* Body = Hit.GetComponent(); Body && Body->IsSimulatingPhysics())
		{
			Body->AddImpulseAtLocation(Forward * PunchImpulse, Hit.ImpactPoint);
		}

		UE_LOG(Logmonke_island, Log, TEXT("%s punches %s: %.0f damage"), *GetNameSafe(Character), *GetNameSafe(Target), Damage);
	}
}

void UIslanderCombatComponent::SetGuard(bool bInGuard)
{
	ACharacter* Character = GetCharacter();
	UIslanderMovementComponent* Move = GetIslanderMovement();
	if (!Character || !Move)
	{
		return;
	}

	// the movement flag is predicted (slower walk); the replicated copy only drives the other players' visuals
	Move->SetWantsToGuard(bInGuard);
	if (Character->HasAuthority())
	{
		ServerSetGuard_Implementation(bInGuard);
	}
	else
	{
		ServerSetGuard(bInGuard);
	}
}

void UIslanderCombatComponent::ServerSetGuard_Implementation(bool bInGuard)
{
	bReplicatedGuard = bInGuard;
}
