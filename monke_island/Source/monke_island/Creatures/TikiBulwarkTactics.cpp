// monke_island

#include "Creatures/TikiBulwarkTactics.h"
#include "Creatures/Hostility.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiWarrior.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Weapons/FirearmDamage.h"
#include "Weapons/FirearmWeapon.h"
#include "Weapons/MusketWeapon.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

bool UTikiBulwarkTactics::IsCommitted() const
{
	return Phase == EPhase::Grab;
}

float UTikiBulwarkTactics::GetStrikeDamageScale() const
{
	const UTikiRules* Rules = GetRules();
	return Rules ? Rules->Bulwark.StrikeDamageScale : 1.0f;
}

void UTikiBulwarkTactics::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThrowTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UTikiBulwarkTactics::OnNoFoe()
{
	if (Phase == EPhase::Guard)
	{
		Ward.Reset();
	}
	Super::OnNoFoe();
}

bool UTikiBulwarkTactics::ThinkBusy(double Now, float DeltaSeconds)
{
	switch (Phase)
	{
	case EPhase::Grab:
		if (Now >= SnapTime)
		{
			CloseClaw(Now);
		}
		return true;
	case EPhase::Recover:
	case EPhase::Broken:
		if (Now < PhaseEnd)
		{
			return true;
		}
		Phase = EPhase::Guard;
		return false;
	default:
		return false;
	}
}

bool UTikiBulwarkTactics::ThinkEngaged(APawn* Foe, float Distance, double Now, float DeltaSeconds)
{
	const UTikiRules* Rules = GetRules();
	ATikiWarrior* Self = GetWarrior();
	if (!Rules || !Self || !Foe)
	{
		return false;
	}
	const FTikiBulwarkRules& B = Rules->Bulwark;
	const FVector Here = Self->GetActorLocation();
	// out of sight: it goes where it saw or heard its foe last, behind its guard (T-262)
	if (!SeesFoe())
	{
		SetState(ETikiTacticState::Guard, false, true);
		MoveToward(KnownFoeLocation(Foe), B.GuardMoveSpeed * 1.5f);
		return true;
	}
	if (Now >= NextReplan)
	{
		NextReplan = Now + Rules->Pack.ReplanSeconds;
		Ward = PickWard(Foe);
	}
	const bool bDowned = IsFoeDowned(Foe);
	const FVector There = Foe->GetActorLocation();
	// in reach: the claw that seizes, else its blow, when the pack lets it attack
	if (!bDowned && Distance <= B.GrabRange && Now >= NextGrab && TryTakeAttackToken(Foe))
	{
		BeginGrab(Foe, Now);
		return true;
	}
	if (!bDowned && Distance <= Rules->AttackRange && CanStartStrike(Now) && TryTakeAttackToken(Foe))
	{
		// its guard drops for the blow
		SetState(ETikiTacticState::Guard, true, false);
		BeginStrike(Foe, Now);
		return true;
	}
	const ATikiWarrior* Covered = Ward.Get();
	// far: it runs there, guard down
	if (Distance > B.GuardDistance)
	{
		SetState(ETikiTacticState::Guard, false, false);
		MoveToward(Covered ? TikiTactics::InterposePoint(Covered->GetActorLocation(), There, B.InterposeDistance) : There, Rules->WarriorRunSpeed);
		return true;
	}
	// an ally its foe threatens: it stands between them, guard up
	if (Covered)
	{
		const FVector Spot = TikiTactics::InterposePoint(Covered->GetActorLocation(), There, B.InterposeDistance);
		SetState(ETikiTacticState::Protect, true, true);
		if (FVector::Dist2D(Spot, Here) > 60.0f)
		{
			MoveToward(Spot, B.GuardMoveSpeed);
		}
		TurnToward(There, DeltaSeconds, B.GuardTurnRate);
		return true;
	}
	// it comes at its foe sideways, behind its guard, turning slowly
	if (Now >= NextSideSwitch)
	{
		Side = -Side;
		NextSideSwitch = Now + B.SideSwitchSeconds;
	}
	SetState(ETikiTacticState::Guard, true, true);
	if (Distance > Rules->AttackRange * 0.8f)
	{
		MoveToward(Here + TikiTactics::SideApproach(There - Here, Side, B.SideApproachAngle) * 200.0f, B.GuardMoveSpeed);
		if (UpdateStuck(true))
		{
			Side = -Side;
		}
	}
	TurnToward(There, DeltaSeconds, B.GuardTurnRate);
	return true;
}

void UTikiBulwarkTactics::BeginGrab(APawn* Foe, double Now)
{
	const FTikiBulwarkRules& B = GetRules()->Bulwark;
	// it turns to its foe once: the claw closes where it was aimed, a step aside avoids it
	StopMoving();
	FaceToward(Foe->GetActorLocation());
	SetState(ETikiTacticState::Grab, false, false);
	PlayAction(ETikiAction::Grab);
	Phase = EPhase::Grab;
	SnapTime = Now + B.GrabWindup;
	NextGrab = Now + B.GrabCooldown;
	UE_LOG(LogMonkeTactics, Log, TEXT("%s opens its claw at %s"), *GetNameSafe(GetWarrior()), *Foe->GetName());
}

void UTikiBulwarkTactics::CloseClaw(double Now)
{
	const UTikiRules* Rules = GetRules();
	ATikiWarrior* Self = GetWarrior();
	if (!Rules || !Self)
	{
		Phase = EPhase::Guard;
		return;
	}
	const FTikiBulwarkRules& B = Rules->Bulwark;
	Phase = EPhase::Recover;
	PhaseEnd = Now + B.GrabRecoverSeconds;
	SetState(ETikiTacticState::Recover, false, false);

	APawn* Foe = Self->GetTarget();
	const FVector Here = Self->GetActorLocation();
	const FVector Forward = Self->GetActorForwardVector();
	if (!Foe || !Hostility::IsLivingFoe(Self, Foe) || FVector::Dist2D(Foe->GetActorLocation(), Here) > B.GrabReach
		|| FMath::Abs(Foe->GetActorLocation().Z - Here.Z) > 150.0f || !TikiTactics::IsInGuardArc(Here, Forward, Foe->GetActorLocation(), B.GrabHalfArc))
	{
		UE_LOG(LogMonkeTactics, Log, TEXT("%s: its claw closes on nothing"), *Self->GetName());
		return;
	}
	PlayCue(ETikiTacticCue::Seize, Foe->GetActorLocation());
	UGameplayStatics::ApplyDamage(Foe, B.GrabDamage, Self->GetController(), Self, nullptr);
	// thrown to the side, away from the ally it covers
	const ATikiWarrior* Covered = Ward.Get();
	const int32 ThrowSide = TikiTactics::ThrowSide(Forward, Here, Covered != nullptr, Covered ? Covered->GetActorLocation() : FVector::ZeroVector);
	const FVector Velocity = TikiTactics::ThrowVelocity(Forward, ThrowSide, B.ThrowSideAngle, B.ThrowSpeed, B.ThrowUpSpeed);
	UE_LOG(LogMonkeTactics, Log, TEXT("%s seizes %s and throws it to its %s"), *Self->GetName(), *Foe->GetName(), ThrowSide > 0 ? TEXT("right") : TEXT("left"));
	if (B.GrabHoldSeconds > 0.0f)
	{
		const TWeakObjectPtr<APawn> Victim(Foe);
		GetWorld()->GetTimerManager().SetTimer(ThrowTimer, FTimerDelegate::CreateWeakLambda(this, [this, Victim, Velocity]()
		{
			Throw(Victim, Velocity);
		}), B.GrabHoldSeconds, false);
	}
	else
	{
		Throw(Foe, Velocity);
	}
}

void UTikiBulwarkTactics::Throw(TWeakObjectPtr<APawn> Victim, FVector Velocity)
{
	ATikiWarrior* Self = GetWarrior();
	APawn* Pawn = Victim.Get();
	if (!Self || Self->IsDead() || !Pawn || !Hostility::IsAlive(Pawn))
	{
		return;
	}
	if (AIslanderCharacter* Player = Cast<AIslanderCharacter>(Pawn))
	{
		// a ragdoll on every machine (server call); the player stands up where it lands
		Player->Knockout(Velocity);
		if (Player->IsRagdoll())
		{
			return;
		}
	}
	// another foe (a soldier) is launched, unless it is far heavier than the crab
	ACharacter* Thrown = Cast<ACharacter>(Pawn);
	const UCharacterMovementComponent* ThrownMove = Thrown ? Thrown->GetCharacterMovement() : nullptr;
	if (ThrownMove && ThrownMove->Mass <= Self->GetCharacterMovement()->Mass * 2.0f)
	{
		Thrown->LaunchCharacter(Velocity, true, true);
	}
}

void UTikiBulwarkTactics::BreakGuard(double Now)
{
	const FTikiBulwarkRules& B = GetRules()->Bulwark;
	Phase = EPhase::Broken;
	PhaseEnd = Now + B.GuardBrokenSeconds;
	SetState(ETikiTacticState::Broken, false, false);
	StopMoving();
	PlayAction(ETikiAction::GuardBreak);
	UE_LOG(LogMonkeTactics, Log, TEXT("%s: its guard breaks, open %.1f s"), *GetNameSafe(GetWarrior()), B.GuardBrokenSeconds);
}

bool UTikiBulwarkTactics::FilterDamage(float& InOutDamage, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const UTikiRules* Rules = GetRules();
	ATikiWarrior* Self = GetWarrior();
	if (!Rules || !Self || Phase != EPhase::Guard || !Self->GetTactic().bGuardUp)
	{
		return false;
	}
	const FTikiBulwarkRules& B = Rules->Bulwark;
	const bool bPoint = DamageEvent.IsOfType(FPointDamageEvent::ClassID);
	const bool bRadial = DamageEvent.IsOfType(FRadialDamageEvent::ClassID);
	const bool bFirearm = DamageEvent.IsOfType(FFirearmDamageEvent::ClassID) || Cast<AFirearmWeapon>(DamageCauser) || Cast<AMusketWeapon>(DamageCauser);
	const TikiTactics::EGuardable Kind = TikiTactics::ClassifyBlow(bPoint, bRadial, bFirearm, Cast<APawn>(DamageCauser) != nullptr);
	if (Kind == TikiTactics::EGuardable::None)
	{
		return false;
	}
	// where the blow comes from: its attacker, else back along the shot, else its causer
	APawn* Attacker = EventInstigator ? EventInstigator->GetPawn() : nullptr;
	if (!Attacker)
	{
		Attacker = Cast<APawn>(DamageCauser);
	}
	const FVector Here = Self->GetActorLocation();
	FVector From = DamageCauser ? DamageCauser->GetActorLocation() : Here;
	FVector Point = FVector::ZeroVector;
	if (bPoint)
	{
		const FPointDamageEvent& Hit = static_cast<const FPointDamageEvent&>(DamageEvent);
		if (!Hit.ShotDirection.IsNearlyZero())
		{
			From = Here - Hit.ShotDirection.GetSafeNormal() * 500.0f;
		}
		if (Hit.HitInfo.bBlockingHit)
		{
			Point = Hit.HitInfo.ImpactPoint;
		}
	}
	if (Attacker)
	{
		From = Attacker->GetActorLocation();
	}
	if (!TikiTactics::IsInGuardArc(Here, Self->GetActorForwardVector(), From, B.GuardHalfArc))
	{
		return false;
	}
	const FVector ToBlow = (From - Here).GetSafeNormal2D(UE_SMALL_NUMBER, Self->GetActorForwardVector());
	if (Point.IsZero())
	{
		// the claw in front of the body, on the side of the blow
		Point = Here + ToBlow * 45.0f + FVector(0.0f, 0.0f, 25.0f);
	}
	const float Raw = InOutDamage;
	InOutDamage = TikiTactics::GuardedDamage(Raw, Kind, B.GuardMeleeDamageScale, B.GuardShotDamageScale);
	PlayCue(ETikiTacticCue::Block, Point);
	// a melee attacker standing close is pushed back by the claw
	ACharacter* Pushed = Kind == TikiTactics::EGuardable::Melee ? Cast<ACharacter>(Attacker) : nullptr;
	if (Pushed && FVector::Dist2D(Pushed->GetActorLocation(), Here) <= B.GuardPushRange)
	{
		Pushed->LaunchCharacter(ToBlow * B.GuardPushback + FVector(0.0f, 0.0f, 80.0f), true, false);
	}
	UE_LOG(LogMonkeTactics, Log, TEXT("%s blocks a %s of %.0f from %s (%.0f gets through)"), *Self->GetName(),
		Kind == TikiTactics::EGuardable::Shot ? TEXT("shot") : TEXT("blow"), Raw, *GetNameSafe(Attacker ? static_cast<AActor*>(Attacker) : DamageCauser), InOutDamage);
	// the heavy blows break it
	const double Now = GetNow();
	if (Poise.Add(Raw, Now, B.GuardBreakWindow, B.GuardBreakDamage, B.GuardBreakBlow))
	{
		BreakGuard(Now);
	}
	return true;
}

void UTikiBulwarkTactics::OnDamaged(AActor* Causer, float Damage)
{
	const UTikiRules* Rules = GetRules();
	// a strong blow from its side or its back stops the claw it opened; a light one does not
	if (Rules && Phase == EPhase::Grab && Damage >= Rules->StaggerDamage)
	{
		Phase = EPhase::Recover;
		PhaseEnd = GetNow() + 0.5;
		SetState(ETikiTacticState::Recover, false, false);
		UE_LOG(LogMonkeTactics, Log, TEXT("%s: its grab is cut short"), *GetNameSafe(GetWarrior()));
	}
}

ATikiWarrior* UTikiBulwarkTactics::PickWard(const APawn* Foe) const
{
	const UTikiRules* Rules = GetRules();
	ATikiWarrior* Self = GetWarrior();
	UHostilitySubsystem* Registry = Self ? UHostilitySubsystem::Get(Self) : nullptr;
	if (!Rules || !Registry || !Foe)
	{
		return nullptr;
	}
	const FTikiBulwarkRules& B = Rules->Bulwark;
	const FVector Here = Self->GetActorLocation();
	const FVector There = Foe->GetActorLocation();
	ATikiWarrior* Best = nullptr;
	float BestUrgency = 0.0f;
	for (const FHostileCombatant& Other : Registry->GetCombatants())
	{
		ATikiWarrior* Ally = Cast<ATikiWarrior>(Other.Character.Get());
		// another crab guards itself, an ally attacking needs no cover
		if (!Ally || Ally == Self || Ally->IsDead() || Ally->IsCommitted() || Ally->GetArchetype() == ETikiArchetype::Bulwark
			|| FVector::Dist2D(Ally->GetActorLocation(), Here) > B.ProtectRadius)
		{
			continue;
		}
		const UIslanderHealthComponent* Life = Ally->GetHealth();
		const float Urgency = TikiTactics::WardUrgency(Ally->GetArchetype() == ETikiArchetype::Watcher, Life ? Life->GetHealthFraction() : 1.0f,
			static_cast<float>(FVector::Dist2D(Ally->GetActorLocation(), There)), B.WardThreatRadius, B.WardWoundedFraction);
		if (Urgency > BestUrgency)
		{
			BestUrgency = Urgency;
			Best = Ally;
		}
	}
	return Best;
}
