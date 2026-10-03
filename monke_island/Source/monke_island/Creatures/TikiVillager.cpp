// monke_island

#include "Creatures/TikiVillager.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiRules.h"
#include "monke_island.h"
#include "Engine/World.h"

ATikiVillager::ATikiVillager()
{
}

float ATikiVillager::GetRulesHealth() const
{
	return Rules ? Rules->VillagerHealth : 60.0f;
}

void ATikiVillager::SetState(ETikiVillagerState NewState)
{
	State = NewState;
	StateStartTime = GetWorld()->GetTimeSeconds();
	UpdateStuck(false);
}

void ATikiVillager::ChooseNextGoal()
{
	ATikiDirector* Dir = Director.Get();
	if (Dir)
	{
		if (AActor* Coconut = Dir->ClaimCoconutNear(this, GetActorLocation(), Rules->CoconutSearchRadius))
		{
			GoalCoconut = Coconut;
			GoalLocation = Coconut->GetActorLocation();
			SetState(ETikiVillagerState::GoToCoconut);
			return;
		}
		FVector Point;
		if (Dir->FindGroundPointNear(HomeCenter, 0.0f, HomeRadius, Point))
		{
			GoalLocation = Point;
			SetState(ETikiVillagerState::Wander);
			return;
		}
	}
	PauseDuration = FMath::FRandRange(Rules->WanderPauseMin, FMath::Max(Rules->WanderPauseMin, Rules->WanderPauseMax));
	SetState(ETikiVillagerState::Pause);
}

void ATikiVillager::ServerThink(float DeltaSeconds)
{
	const double Elapsed = GetWorld()->GetTimeSeconds() - StateStartTime;
	ATikiDirector* Dir = Director.Get();

	switch (State)
	{
	case ETikiVillagerState::Pause:
		if (Elapsed >= PauseDuration)
		{
			ChooseNextGoal();
		}
		break;

	case ETikiVillagerState::Wander:
	{
		const float Left = MoveToward(GoalLocation, Rules->VillagerWalkSpeed);
		if (Left < 60.0f || UpdateStuck(true) || Elapsed > 60.0)
		{
			PauseDuration = FMath::FRandRange(Rules->WanderPauseMin, FMath::Max(Rules->WanderPauseMin, Rules->WanderPauseMax));
			SetState(ETikiVillagerState::Pause);
		}
		break;
	}

	case ETikiVillagerState::GoToCoconut:
	{
		AActor* Coconut = GoalCoconut.Get();
		if (!Coconut)
		{
			ChooseNextGoal();
			break;
		}
		const float Left = MoveToward(Coconut->GetActorLocation(), Rules->VillagerWalkSpeed);
		if (Left < 70.0f)
		{
			FaceToward(Coconut->GetActorLocation());
			PlayAction(ETikiAction::Collect);
			SetState(ETikiVillagerState::Collect);
		}
		else if (UpdateStuck(true) || Elapsed > 60.0)
		{
			if (Dir)
			{
				Dir->ReleaseCoconut(Coconut);
			}
			GoalCoconut.Reset();
			PauseDuration = Rules->WanderPauseMin;
			SetState(ETikiVillagerState::Pause);
		}
		break;
	}

	case ETikiVillagerState::Collect:
		if (Elapsed >= Rules->CollectDuration)
		{
			if (Dir && Dir->CollectCoconut(this, GoalCoconut.Get()))
			{
				++CollectedCount;
			}
			GoalCoconut.Reset();
			PauseDuration = FMath::FRandRange(Rules->WanderPauseMin, FMath::Max(Rules->WanderPauseMin, Rules->WanderPauseMax));
			SetState(ETikiVillagerState::Pause);
		}
		break;

	case ETikiVillagerState::Flee:
	{
		const AActor* From = Threat.Get();
		if (!From || Elapsed >= Rules->FleeDuration)
		{
			Threat.Reset();
			PauseDuration = Rules->WanderPauseMax;
			SetState(ETikiVillagerState::Pause);
			PlayAction(ETikiAction::Cower);
			break;
		}
		FVector Away = GetActorLocation() - From->GetActorLocation();
		Away.Z = 0.0f;
		if (Away.IsNearlyZero())
		{
			Away = GetActorForwardVector();
		}
		MoveToward(GetActorLocation() + Away.GetSafeNormal() * 500.0f, Rules->VillagerRunSpeed);
		break;
	}
	}
}

void ATikiVillager::OnDamaged(AActor* Causer, float Damage)
{
	if (GoalCoconut.IsValid() && Director.IsValid())
	{
		Director->ReleaseCoconut(GoalCoconut.Get());
	}
	GoalCoconut.Reset();

	APawn* Attacker = Cast<APawn>(Causer);
	if (!IsLivingPlayer(Attacker))
	{
		return;
	}
	Threat = Attacker;
	SetState(ETikiVillagerState::Flee);
	if (ATikiDirector* Dir = Director.Get())
	{
		Dir->CallDefender(this, Attacker);
	}
}
