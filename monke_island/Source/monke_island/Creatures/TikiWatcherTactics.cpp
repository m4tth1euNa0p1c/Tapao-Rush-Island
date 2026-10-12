// monke_island

#include "Creatures/TikiWatcherTactics.h"
#include "Creatures/Hostility.h"
#include "Creatures/TikiAshBurst.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiTacticsMath.h"
#include "Creatures/TikiWarrior.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

bool UTikiWatcherTactics::IsCommitted() const
{
	return Phase == EPhase::Cast;
}

void UTikiWatcherTactics::OnEngaged(APawn* Foe)
{
	const double Now = GetNow();
	bHasCover = false;
	NextCoverReplan = 0.0;
	if (Foe && GetRules() && Now >= NextHowl)
	{
		BeginHowl(Foe, Now);
	}
}

void UTikiWatcherTactics::OnNoFoe()
{
	if (Phase == EPhase::Keep)
	{
		bHasCover = false;
	}
	Super::OnNoFoe();
}

void UTikiWatcherTactics::BeginHowl(APawn* Foe, double Now)
{
	const FTikiWatcherRules& W = GetRules()->Watcher;
	StopMoving();
	FaceToward(Foe->GetActorLocation());
	SetState(ETikiTacticState::Howl, false, false);
	PlayAction(ETikiAction::Howl);
	Phase = EPhase::Howl;
	PhaseEnd = Now + W.HowlSeconds;
	NextHowl = Now + W.HowlCooldown;
	CallPack(Foe);
}

void UTikiWatcherTactics::CallPack(APawn* Foe)
{
	const FTikiWatcherRules& W = GetRules()->Watcher;
	ATikiWarrior* Self = GetWarrior();
	// where it saw its foe: the others come to look there, and attack only if they see it (T-262)
	const FVector Where = KnownFoeLocation(Foe);
	int32 Heard = 0;
	if (UHostilitySubsystem* Registry = UHostilitySubsystem::Get(Self))
	{
		const double RadiusSq = FMath::Square(static_cast<double>(W.HowlRadius));
		for (const FHostileCombatant& Other : Registry->GetCombatants())
		{
			ATikiWarrior* Ally = Cast<ATikiWarrior>(Other.Character.Get());
			if (Ally && Ally != Self && !Ally->IsDead() && !Ally->GetTarget() && FVector::DistSquared2D(Ally->GetActorLocation(), Self->GetActorLocation()) <= RadiusSq)
			{
				Ally->HearHowl(Where, Foe);
				++Heard;
			}
		}
	}
	int32 Woken = 0;
	ATikiDirector* TikiDirector = GetDirector();
	if (TikiDirector && W.HowlWakeAmbushers > 0)
	{
		Woken = TikiDirector->WakeAmbushers(Where, Foe, W.HowlRadius, W.HowlWakeAmbushers);
	}
	UE_LOG(LogMonkeTactics, Log, TEXT("%s howls at %s: %d warriors within %.0f m come to look, %d out of their bushes"), *Self->GetName(),
		*GetNameSafe(Foe), Heard, W.HowlRadius / 100.0f, Woken);
}

void UTikiWatcherTactics::BeginCast(APawn* Foe, double Now)
{
	const FTikiWatcherRules& W = GetRules()->Watcher;
	StopMoving();
	FaceToward(Foe->GetActorLocation());
	SetState(ETikiTacticState::Cast, false, false);
	PlayAction(ETikiAction::Cast);
	Phase = EPhase::Cast;
	PhaseEnd = Now + W.CastWindup;
	NextCloud = Now + W.CloudCooldown;
	UE_LOG(LogMonkeTactics, Log, TEXT("%s breathes the ash in (%s)"), *GetNameSafe(GetWarrior()), *Foe->GetName());
}

void UTikiWatcherTactics::ReleaseCloud()
{
	ATikiWarrior* Self = GetWarrior();
	UTikiRules* Rules = Self ? Self->Rules.Get() : nullptr;
	if (!Rules)
	{
		return;
	}
	const FTikiWatcherRules& W = Rules->Watcher;
	APawn* Foe = Self->GetTarget();
	// where its foe will be a moment ahead, never more than MaxLead: the lump then lands there, whatever the foe does
	const FVector FoeAt = KnownFoeLocation(Foe);
	const FVector Aim = Foe && SeesFoe() ? TikiTactics::LeadAim(FoeAt, Foe->GetVelocity(), W.CloudLeadSeconds, W.CloudMaxLead) : FoeAt;
	const FVector Ground = GroundUnder(Aim, 300.0f, 800.0f);
	const FVector Feet = Self->GetActorLocation() - FVector(0.0f, 0.0f, Self->GetSimpleCollisionHalfHeight());
	const FVector Hand = Feet + Self->GetActorForwardVector() * 40.0f + FVector(0.0f, 0.0f, W.ThrowHeight);
	if (ATikiAshBurst::Launch(GetWorld(), Rules, ETikiAshBurstKind::Cloud, Self, Hand, Ground))
	{
		UE_LOG(LogMonkeTactics, Log, TEXT("%s throws the ash at %s (%.0f m)"), *Self->GetName(), *Ground.ToCompactString(), FVector::Dist(Hand, Ground) / 100.0);
	}
}

void UTikiWatcherTactics::DropEmbers(const APawn* Foe)
{
	ATikiWarrior* Self = GetWarrior();
	UTikiRules* Rules = Self ? Self->Rules.Get() : nullptr;
	if (!Rules)
	{
		return;
	}
	const FVector Here = Self->GetActorLocation();
	const FVector Toward = Foe ? (Foe->GetActorLocation() - Here).GetSafeNormal2D(UE_SMALL_NUMBER, Self->GetActorForwardVector()) : Self->GetActorForwardVector();
	// between it and its foe: the foe walks around them or burns
	const FVector Ground = GroundUnder(Here + Toward * 120.0f, 150.0f, 400.0f);
	if (ATikiAshBurst::Launch(GetWorld(), Rules, ETikiAshBurstKind::Embers, Self, Ground, Ground))
	{
		UE_LOG(LogMonkeTactics, Log, TEXT("%s leaves embers behind it"), *Self->GetName());
	}
}

FVector UTikiWatcherTactics::PickCover(const APawn* Foe) const
{
	const FTikiWatcherRules& W = GetRules()->Watcher;
	const FVector Here = GetWarrior()->GetActorLocation();
	const FVector There = Foe->GetActorLocation();
	const FVector Want = TikiTactics::KeepAwayPoint(Here, There, W.KeepDistanceMin, W.KeepDistanceMax);
	FVector Bush;
	if (FindBush(Want, W.CoverSearchRadius, Bush))
	{
		// behind the bush from its foe, still at its distance
		const FVector Cover = TikiTactics::CoverPoint(Bush, There, W.CoverOffset);
		const double Gap = FVector::Dist2D(Cover, There);
		if (Gap >= W.KeepDistanceMin * 0.8f && Gap <= W.KeepDistanceMax * 1.2f)
		{
			return Cover;
		}
	}
	return Want;
}

bool UTikiWatcherTactics::ThinkBusy(double Now, float DeltaSeconds)
{
	switch (Phase)
	{
	case EPhase::Howl:
		if (Now < PhaseEnd)
		{
			return true;
		}
		Phase = EPhase::Keep;
		return false;
	case EPhase::Cast:
		if (const APawn* Foe = GetWarrior()->GetTarget())
		{
			TurnToward(KnownFoeLocation(Foe), DeltaSeconds);
		}
		if (Now >= PhaseEnd)
		{
			ReleaseCloud();
			Phase = EPhase::Keep;
			NextCoverReplan = 0.0;
		}
		return true;
	default:
		return false;
	}
}

bool UTikiWatcherTactics::ThinkEngaged(APawn* Foe, float Distance, double Now, float DeltaSeconds)
{
	const UTikiRules* Rules = GetRules();
	if (!Rules || !Foe)
	{
		return false;
	}
	// a howl or a cast started at this think
	if (Phase != EPhase::Keep)
	{
		return true;
	}
	const FTikiWatcherRules& W = Rules->Watcher;
	const FVector Here = GetWarrior()->GetActorLocation();
	// out of sight: it walks carefully where it saw or heard its foe last (T-262)
	if (!SeesFoe())
	{
		SetState(ETikiTacticState::Keep, false, false);
		MoveToward(KnownFoeLocation(Foe), Rules->WarriorWalkSpeed);
		return true;
	}
	const FVector There = Foe->GetActorLocation();
	const bool bDowned = IsFoeDowned(Foe);
	// too close: it backs away facing its foe and leaves embers between them; closer still it turns and runs; caught,
	// it strikes with its club
	if (Distance < W.RetreatDistance)
	{
		if (!bDowned && Distance <= Rules->AttackRange && CanStartStrike(Now) && TryTakeAttackToken(Foe))
		{
			SetState(ETikiTacticState::Retreat, true, false);
			BeginStrike(Foe, Now);
			return true;
		}
		if (Now >= NextEmbers)
		{
			NextEmbers = Now + W.EmberCooldown;
			DropEmbers(Foe);
		}
		const FVector Away = Now < DetourUntil ? Detour : TikiTactics::KeepAwayPoint(Here, There, W.KeepDistanceMin, W.KeepDistanceMax);
		const bool bRun = Distance < W.TurnAndRunDistance;
		SetState(ETikiTacticState::Retreat, !bRun, false);
		MoveToward(Away, bRun ? Rules->WarriorRunSpeed : W.BackpedalSpeed);
		if (!bRun)
		{
			TurnToward(There, DeltaSeconds);
		}
		if (UpdateStuck(true))
		{
			// a wall or the sea behind it: it slips aside a moment
			const FVector Side = (Here - There).GetSafeNormal2D(UE_SMALL_NUMBER, -GetWarrior()->GetActorForwardVector())
				.RotateAngleAxis(FMath::RandBool() ? 75.0f : -75.0f, FVector::UpVector);
			Detour = Here + Side * 500.0f;
			DetourUntil = Now + 1.5;
		}
		bHasCover = false;
		return true;
	}
	// the cloud at a foe in its range
	if (!bDowned && Now >= NextCloud && Distance >= W.CloudMinRange && Distance <= W.CloudMaxRange && TryTakeAttackToken(Foe))
	{
		BeginCast(Foe, Now);
		return true;
	}
	// it keeps its distance, behind a bush when there is one
	if (!bHasCover || Now >= NextCoverReplan)
	{
		NextCoverReplan = Now + W.CoverReplanSeconds;
		CoverGoal = PickCover(Foe);
		bHasCover = true;
	}
	const float ToGoal = static_cast<float>(FVector::Dist2D(CoverGoal, Here));
	const bool bFar = ToGoal > 500.0f;
	SetState(ETikiTacticState::Keep, !bFar, false);
	if (ToGoal > 80.0f)
	{
		MoveToward(CoverGoal, bFar ? Rules->WarriorRunSpeed : Rules->WarriorWalkSpeed);
	}
	if (!bFar)
	{
		TurnToward(There, DeltaSeconds);
	}
	return true;
}

void UTikiWatcherTactics::OnDamaged(AActor* Causer, float Damage)
{
	const UTikiRules* Rules = GetRules();
	// a strong blow stops the cast; any blow makes it look for another place
	if (Rules && Phase == EPhase::Cast && Damage >= Rules->StaggerDamage)
	{
		Phase = EPhase::Keep;
		UE_LOG(LogMonkeTactics, Log, TEXT("%s: struck, its ash falls"), *GetNameSafe(GetWarrior()));
	}
	NextCoverReplan = 0.0;
}
