// monke_island

#include "Creatures/TikiStalkerTactics.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiTacticsMath.h"
#include "Creatures/TikiWarrior.h"
#include "GameFramework/Pawn.h"

bool UTikiStalkerTactics::IsCommitted() const
{
	return Phase == EPhase::Feint || Phase == EPhase::Charge || Phase == EPhase::Heavy;
}

void UTikiStalkerTactics::Enter(EPhase InPhase, double Now, double Duration)
{
	Phase = InPhase;
	PhaseStart = Now;
	PhaseEnd = Now + Duration;
}

void UTikiStalkerTactics::OnEngaged(APawn* Foe)
{
	if (Phase == EPhase::Flee || Phase == EPhase::Cower)
	{
		return;
	}
	LastFoe = Foe;
	BeginStalk(GetNow(), false);
}

void UTikiStalkerTactics::OnNoFoe()
{
	if (Phase != EPhase::Flee && Phase != EPhase::Cower)
	{
		Phase = EPhase::Stalk;
		LastFoe.Reset();
		UnwatchedSince = -1.0;
		TorchHeldSince = -1.0;
		bTorchFear = false;
		TorchFearDistance = 0.0f;
	}
	Super::OnNoFoe();
}

void UTikiStalkerTactics::BeginStalk(double Now, bool bShort)
{
	const UTikiRules* Rules = GetRules();
	if (!Rules)
	{
		return;
	}
	const FTikiStalkerRules& S = Rules->Stalker;
	const float Seconds = FMath::FRandRange(S.StalkSecondsMin, FMath::Max(S.StalkSecondsMin, S.StalkSecondsMax));
	Enter(EPhase::Stalk, Now, bShort ? Seconds * 0.5f : Seconds);
	UnwatchedSince = -1.0;
	OrbitSide = FMath::RandBool() ? 1 : -1;
	NextOrbitSwitch = Now + FMath::FRandRange(S.OrbitSwitchSecondsMin, FMath::Max(S.OrbitSwitchSecondsMin, S.OrbitSwitchSecondsMax));
	UpdateStuck(false);
}

void UTikiStalkerTactics::BeginFeint(APawn* Foe, double Now)
{
	const FTikiStalkerRules& S = GetRules()->Stalker;
	Enter(EPhase::Feint, Now, S.FeintDashSeconds + S.FeintBackSeconds);
	SetState(ETikiTacticState::Feint, true, false);
	FaceToward(Foe->GetActorLocation());
	PlayAction(ETikiAction::Feint);
	UE_LOG(LogMonkeTactics, Log, TEXT("%s feints at %s"), *GetNameSafe(GetWarrior()), *Foe->GetName());
}

void UTikiStalkerTactics::BeginCharge(double Now)
{
	const FTikiStalkerRules& S = GetRules()->Stalker;
	Enter(EPhase::Charge, Now, S.ChargeMaxSeconds);
	SetState(ETikiTacticState::Charge, false, false);
	UpdateStuck(false);
	// its roar announces the charge
	PlayAction(ETikiAction::Roar);
	UE_LOG(LogMonkeTactics, Log, TEXT("%s charges"), *GetNameSafe(GetWarrior()));
}

void UTikiStalkerTactics::BeginHeavy(APawn* Foe, double Now)
{
	const FTikiStalkerRules& S = GetRules()->Stalker;
	// it stops and turns to its foe once: the blow lands where it was aimed, a step aside avoids it
	StopMoving();
	FaceToward(Foe->GetActorLocation());
	SetState(ETikiTacticState::Charge, false, false);
	PlayAction(ETikiAction::HeavyAttack);
	StrikeTime = Now + S.HeavyWindup;
	Enter(EPhase::Heavy, Now, S.HeavyWindup);
}

void UTikiStalkerTactics::BeginFlee(AActor* Causer, double Now)
{
	const FTikiStalkerRules& S = GetRules()->Stalker;
	ATikiWarrior* Self = GetWarrior();
	const APawn* Foe = Self->GetTarget();
	const FVector Here = Self->GetActorLocation();
	const FVector From = Foe ? Foe->GetActorLocation() : (Causer ? Causer->GetActorLocation() : Here - Self->GetActorForwardVector());
	const FVector Away = (Here - From).GetSafeNormal2D(UE_SMALL_NUMBER, -Self->GetActorForwardVector());
	// a bush ahead on the way out, else straight away
	FVector Bush;
	bFleeToBush = FindBush(Here + Away * (S.FleeBushSearchRadius * 0.5f), S.FleeBushSearchRadius * 0.5f, Bush) && FVector::DotProduct(Bush - Here, Away) > 0.0;
	FleeGoal = bFleeToBush ? Bush : Here + Away * S.FleeBushSearchRadius;
	Enter(EPhase::Flee, Now, S.FleeMaxSeconds);
	NextFleeAllowed = Now + S.FleeCooldown;
	SetState(ETikiTacticState::Flee, false, false);
	UpdateStuck(false);
	PlayAction(ETikiAction::Flee);
	UE_LOG(LogMonkeTactics, Log, TEXT("%s, wounded and alone (%.0f %%), runs away to %s"), *Self->GetName(), GetHealthFraction() * 100.0f,
		bFleeToBush ? TEXT("a bush") : TEXT("open ground"));
}

void UTikiStalkerTactics::Replan(const APawn* Foe, double Now)
{
	if (Now < NextReplan)
	{
		return;
	}
	const UTikiRules* Rules = GetRules();
	const FTikiStalkerRules& S = Rules->Stalker;
	NextReplan = Now + Rules->Pack.ReplanSeconds;
	Pack = ReadPack(Foe);
	// the flame of a lit torch keeps it away a while (T-275): the courage comes back, or with enough of the pack
	bool bRaised = false;
	const float Fear = TikiTactics::TorchFearRadius(ATikiWarrior::HoldsLitTorch(Foe, bRaised), bRaised, S.TorchFearRadius, S.TorchRaisedFearRadius);
	if (Fear <= 0.0f)
	{
		TorchHeldSince = -1.0;
		bTorchFear = false;
		TorchFearDistance = 0.0f;
		return;
	}
	if (TorchHeldSince < 0.0)
	{
		TorchHeldSince = Now;
		UE_LOG(LogMonkeTactics, Log, TEXT("%s keeps away from the torch of %s"), *GetNameSafe(GetWarrior()), *GetNameSafe(Foe));
	}
	bTorchFear = TikiTactics::IsTorchFearHolding(TorchHeldSince, Now, S.TorchCourageSeconds, Pack.SameRole, S.TorchCouragePack);
	TorchFearDistance = bTorchFear ? Fear : 0.0f;
}

bool UTikiStalkerTactics::ThinkBusy(double Now, float DeltaSeconds)
{
	const UTikiRules* Rules = GetRules();
	if (!Rules)
	{
		return false;
	}
	const FTikiStalkerRules& S = Rules->Stalker;
	switch (Phase)
	{
	case EPhase::Heavy:
		if (Now >= StrikeTime)
		{
			// without the arm of its weapon it strikes weaker (T-167)
			const float Scale = GetWarrior()->IsMaimed() ? Rules->MaimedDamageScale : 1.0f;
			const int32 Struck = StrikeWith(S.HeavyReach, S.HeavyRadius, S.HeavyDamage * Scale, S.HeavyKnockback);
			UE_LOG(LogMonkeTactics, Log, TEXT("%s: heavy blow, %d struck"), *GetNameSafe(GetWarrior()), Struck);
			// open a moment after the blow
			Enter(EPhase::Recover, Now, S.RecoverSeconds);
			SetState(ETikiTacticState::Recover, false, false);
		}
		return true;
	case EPhase::Recover:
		if (Now < PhaseEnd)
		{
			return true;
		}
		// then it slips away and stalks again
		Enter(EPhase::Fade, Now, 3.0);
		return false;
	case EPhase::Feint:
		return ThinkFeint(Now, DeltaSeconds);
	case EPhase::Flee:
		return ThinkFlee(Now);
	case EPhase::Cower:
		return ThinkCower(Now, DeltaSeconds);
	default:
		return false;
	}
}

bool UTikiStalkerTactics::ThinkFeint(double Now, float DeltaSeconds)
{
	const FTikiStalkerRules& S = GetRules()->Stalker;
	ATikiWarrior* Self = GetWarrior();
	APawn* Foe = Self->GetTarget();
	if (!Foe)
	{
		BeginStalk(Now, true);
		return false;
	}
	const FVector Here = Self->GetActorLocation();
	const FVector There = Foe->GetActorLocation();
	if (Now - PhaseStart < S.FeintDashSeconds)
	{
		// a false start at its foe...
		if (FVector::Dist2D(Here, There) > S.HeavyRange)
		{
			MoveToward(There, S.FeintDashSpeed);
		}
		TurnToward(There, DeltaSeconds);
		return true;
	}
	if (Now < PhaseEnd)
	{
		// ...then a step back, facing it
		MoveToward(Here + (Here - There).GetSafeNormal2D() * 300.0f, S.CreepSpeed);
		TurnToward(There, DeltaSeconds);
		return true;
	}
	// the real charge, while its foe is off guard (the feint held its place in the pack)
	BeginCharge(Now);
	return false;
}

bool UTikiStalkerTactics::ThinkFlee(double Now)
{
	const UTikiRules* Rules = GetRules();
	const FTikiStalkerRules& S = Rules->Stalker;
	ATikiWarrior* Self = GetWarrior();
	const APawn* Foe = Self->GetTarget();
	// caught: it turns and fights, a cornered animal
	if (Foe && FVector::Dist2D(Foe->GetActorLocation(), Self->GetActorLocation()) <= S.CorneredDistance)
	{
		UE_LOG(LogMonkeTactics, Log, TEXT("%s is cornered: it turns and fights"), *Self->GetName());
		BeginCharge(Now);
		return false;
	}
	if (Now >= PhaseEnd)
	{
		BeginStalk(Now, true);
		return false;
	}
	SetState(ETikiTacticState::Flee, false, false);
	const float Left = MoveToward(FleeGoal, Rules->WarriorRunSpeed);
	if (Left <= 150.0f)
	{
		// at its bush: a night warrior, an ambusher or a defender hides there for good; a guardian waits there
		if (bFleeToBush && Self->GetRole() != ETikiWarriorRole::Guardian)
		{
			UE_LOG(LogMonkeTactics, Log, TEXT("%s hides in its bush"), *Self->GetName());
			Self->Burrow();
			return true;
		}
		Enter(EPhase::Cower, Now, S.CowerSeconds);
		SetState(ETikiTacticState::Cower, false, false);
		StopMoving();
		return true;
	}
	if (UpdateStuck(true))
	{
		// something in the way: another way out
		const FVector Here = Self->GetActorLocation();
		const FVector Away = Foe ? (Here - Foe->GetActorLocation()).GetSafeNormal2D(UE_SMALL_NUMBER, Self->GetActorForwardVector()) : Self->GetActorForwardVector();
		FleeGoal = Here + Away.RotateAngleAxis(FMath::FRandRange(-70.0f, 70.0f), FVector::UpVector) * 800.0f;
		bFleeToBush = false;
	}
	return true;
}

bool UTikiStalkerTactics::ThinkCower(double Now, float DeltaSeconds)
{
	const FTikiStalkerRules& S = GetRules()->Stalker;
	ATikiWarrior* Self = GetWarrior();
	const APawn* Foe = Self->GetTarget();
	if (Foe && FVector::Dist2D(Foe->GetActorLocation(), Self->GetActorLocation()) <= S.CorneredDistance)
	{
		BeginCharge(Now);
		return false;
	}
	if (Now >= PhaseEnd)
	{
		UE_LOG(LogMonkeTactics, Log, TEXT("%s gets its courage back"), *Self->GetName());
		BeginStalk(Now, true);
		return false;
	}
	if (Foe)
	{
		TurnToward(Foe->GetActorLocation(), DeltaSeconds);
	}
	return true;
}

bool UTikiStalkerTactics::ThinkEngaged(APawn* Foe, float Distance, double Now, float DeltaSeconds)
{
	const UTikiRules* Rules = GetRules();
	if (!Rules || !Foe)
	{
		return false;
	}
	const FTikiStalkerRules& S = Rules->Stalker;
	if (Foe != LastFoe.Get())
	{
		LastFoe = Foe;
		BeginStalk(Now, false);
	}
	// out of sight (crouched, the night): it goes where it saw or heard its foe last, no attack (T-262)
	if (!SeesFoe())
	{
		SetState(ETikiTacticState::Stalk, false, false);
		MoveToward(KnownFoeLocation(Foe), Rules->WarriorRunSpeed * 0.7f);
		return true;
	}
	Replan(Foe, Now);
	bFoeDowned = IsFoeDowned(Foe);
	// its foe rushed within reach: the quick blow, then it slips away
	if (!bFoeDowned && Phase != EPhase::Charge && Distance <= Rules->AttackRange && CanStartStrike(Now) && TryTakeAttackToken(Foe))
	{
		SetState(ETikiTacticState::Stalk, true, false);
		BeginStrike(Foe, Now);
		Enter(EPhase::Fade, Now, 3.0);
		return true;
	}
	switch (Phase)
	{
	case EPhase::Charge:
		if (bFoeDowned || Now >= PhaseEnd)
		{
			BeginStalk(Now, true);
			return ThinkStalk(Foe, Distance, Now, DeltaSeconds);
		}
		if (Distance <= S.HeavyRange)
		{
			BeginHeavy(Foe, Now);
			return true;
		}
		SetState(ETikiTacticState::Charge, false, false);
		MoveToward(Foe->GetActorLocation(), S.ChargeSpeed);
		if (UpdateStuck(true))
		{
			BeginStalk(Now, true);
		}
		return true;
	case EPhase::Fade:
	{
		if (Distance >= FMath::Max(S.FadeDistance, TorchFearDistance) || Now >= PhaseEnd)
		{
			BeginStalk(Now, true);
			return ThinkStalk(Foe, Distance, Now, DeltaSeconds);
		}
		// it backs away facing its foe, into the dark
		const FVector Here = GetWarrior()->GetActorLocation();
		SetState(ETikiTacticState::Stalk, true, false);
		MoveToward(Here + (Here - Foe->GetActorLocation()).GetSafeNormal2D() * 300.0f, S.CreepSpeed);
		TurnToward(Foe->GetActorLocation(), DeltaSeconds);
		return true;
	}
	case EPhase::Encircle:
		return ThinkEncircle(Foe, Distance, Now, DeltaSeconds);
	default:
		return ThinkStalk(Foe, Distance, Now, DeltaSeconds);
	}
}

bool UTikiStalkerTactics::ThinkStalk(APawn* Foe, float Distance, double Now, float DeltaSeconds)
{
	const FTikiStalkerRules& S = GetRules()->Stalker;
	// with other stalkers on the same foe, it takes its place around it
	if (Pack.SameRole >= 2)
	{
		Enter(EPhase::Encircle, Now, S.EncircleMaxSeconds);
		return ThinkEncircle(Foe, Distance, Now, DeltaSeconds);
	}
	FVector ViewFrom;
	FVector ViewDirection;
	FoeView(Foe, ViewFrom, ViewDirection);
	const FVector Here = GetWarrior()->GetActorLocation();
	const bool bWatched = TikiTactics::IsInView(ViewFrom, ViewDirection, Here, S.WatchedHalfAngle);
	if (bWatched)
	{
		UnwatchedSince = -1.0;
	}
	else if (UnwatchedSince < 0.0)
	{
		UnwatchedSince = Now;
	}
	if (Now >= NextOrbitSwitch)
	{
		OrbitSide = -OrbitSide;
		NextOrbitSwitch = Now + FMath::FRandRange(S.OrbitSwitchSecondsMin, FMath::Max(S.OrbitSwitchSecondsMin, S.OrbitSwitchSecondsMax));
	}
	// time to attack: its stalk is over, or nobody has looked at it for a while close behind its foe
	const bool bBehind = !bWatched && UnwatchedSince >= 0.0 && Now - UnwatchedSince >= S.UnwatchedRushSeconds && Distance <= S.FeintDistance * 1.6f;
	const bool bReady = !bFoeDowned && !bTorchFear && (Now >= PhaseEnd || bBehind);
	if (bReady && (Distance <= S.FeintDistance || bBehind) && TryTakeAttackToken(Foe))
	{
		if (!bBehind && FMath::FRand() < S.FeintChance)
		{
			BeginFeint(Foe, Now);
		}
		else
		{
			UE_LOG(LogMonkeTactics, Log, TEXT("%s comes at %s from %s"), *GetNameSafe(GetWarrior()), *Foe->GetName(), bBehind ? TEXT("behind") : TEXT("its side"));
			BeginCharge(Now);
		}
		return true;
	}
	// it circles at its distance, facing its foe: far while looked at, near when not; ready, it closes in to feint
	float Radius = TikiTactics::StalkRadius(bWatched, S.StalkRadiusMin, S.StalkRadiusMax, TorchFearDistance);
	if (bReady)
	{
		Radius = FMath::Max(S.FeintDistance * 0.9f, TorchFearDistance);
	}
	const FVector Goal = TikiTactics::OrbitPoint(Foe->GetActorLocation(), Here, Radius, S.OrbitStepDegrees, OrbitSide);
	SetState(ETikiTacticState::Stalk, true, false);
	MoveToward(Goal, bWatched ? S.StalkSpeed : S.CreepSpeed);
	TurnToward(Foe->GetActorLocation(), DeltaSeconds);
	if (UpdateStuck(true))
	{
		OrbitSide = -OrbitSide;
	}
	return true;
}

bool UTikiStalkerTactics::ThinkEncircle(APawn* Foe, float Distance, double Now, float DeltaSeconds)
{
	const UTikiRules* Rules = GetRules();
	const FTikiStalkerRules& S = Rules->Stalker;
	if (Pack.SameRole < 2)
	{
		BeginStalk(Now, true);
		return ThinkStalk(Foe, Distance, Now, DeltaSeconds);
	}
	// its place around the foe: the first behind it, the others spread from there (the same order on every think)
	FVector ViewFrom;
	FVector ViewDirection;
	FoeView(Foe, ViewFrom, ViewDirection);
	const float SlotYaw = TikiTactics::EncircleSlotYaw(Pack.SameRoleRank, Pack.SameRole, static_cast<float>(ViewDirection.Rotation().Yaw));
	const FVector Slot = TikiTactics::RingPoint(Foe->GetActorLocation(), SlotYaw, FMath::Max(S.EncircleRadius, TorchFearDistance));
	const FVector Here = GetWarrior()->GetActorLocation();
	const float ToSlot = static_cast<float>(FVector::Dist2D(Slot, Here));
	const bool bInPlace = ToSlot <= S.SlotReachedRadius;
	// in its place, or tired of waiting: it attacks when the pack lets it
	if (!bFoeDowned && !bTorchFear && (bInPlace || Now >= PhaseEnd) && TryTakeAttackToken(Foe))
	{
		if (FMath::FRand() < S.FeintChance)
		{
			BeginFeint(Foe, Now);
		}
		else
		{
			BeginCharge(Now);
		}
		return true;
	}
	// near its place it moves facing its foe, farther it runs there
	const bool bNear = ToSlot < 400.0f;
	SetState(ETikiTacticState::Encircle, bNear, false);
	if (!bInPlace)
	{
		MoveToward(Slot, bNear ? S.CreepSpeed : Rules->WarriorRunSpeed);
	}
	if (bNear)
	{
		TurnToward(Foe->GetActorLocation(), DeltaSeconds);
	}
	return true;
}

void UTikiStalkerTactics::OnDamaged(AActor* Causer, float Damage)
{
	const UTikiRules* Rules = GetRules();
	if (!Rules)
	{
		return;
	}
	const FTikiStalkerRules& S = Rules->Stalker;
	const double Now = GetNow();
	// struck while it prepared its blow or feinted: the move is lost, it slips away
	if (Phase == EPhase::Heavy || Phase == EPhase::Feint)
	{
		Enter(EPhase::Fade, Now, 2.5);
		SetState(ETikiTacticState::Stalk, true, false);
		UE_LOG(LogMonkeTactics, Log, TEXT("%s: struck, its move is lost"), *GetNameSafe(GetWarrior()));
	}
	// wounded with no other warrior near: it runs away
	if (Phase != EPhase::Flee && Phase != EPhase::Cower && Now >= NextFleeAllowed
		&& TikiTactics::ShouldFlee(GetHealthFraction(), S.FleeHealthFraction, CountAlliesNear(S.FleeAllyRadius)))
	{
		BeginFlee(Causer, Now);
		return;
	}
	// found out while it circled: it charges at once
	APawn* Foe = GetWarrior()->GetTarget();
	if ((Phase == EPhase::Stalk || Phase == EPhase::Encircle) && Foe && !IsFoeDowned(Foe) && TryTakeAttackToken(Foe))
	{
		BeginCharge(Now);
	}
}
