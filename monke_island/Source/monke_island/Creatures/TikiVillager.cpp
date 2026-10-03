// monke_island

#include "Creatures/TikiVillager.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "monke_island.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

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
	bPauseActed = false;
	UpdateStuck(false);
}

void ATikiVillager::BeStartled()
{
	PlayAction(ETikiAction::Startle);
	StartleEndTime = GetWorld()->GetTimeSeconds() + (Anims.Startle && Rules ? Rules->StartleTime : 0.0f);
}

void ATikiVillager::LeaveActivity()
{
	if (ATikiDirector* Dir = Director.Get())
	{
		if (GoalCoconut.IsValid())
		{
			Dir->ReleaseCoconut(GoalCoconut.Get());
		}
		if (PoiIndex != INDEX_NONE)
		{
			Dir->ReleasePoi(this);
		}
	}
	GoalCoconut.Reset();
	PoiIndex = INDEX_NONE;
	CurrentActivity = ETikiActivity::None;
}

void ATikiVillager::PlayActivityClip()
{
	switch (CurrentActivity)
	{
	case ETikiActivity::Work:
		PlayAction(ETikiAction::Work);
		break;
	case ETikiActivity::Chat:
		// a chat is made of gestures: give, point, nod, shake the head, cheer; the basket without them
		if (Anims.Gestures.IsEmpty())
		{
			PlayAction(ETikiAction::Work);
		}
		else
		{
			PlayAction(ETikiAction::Gesture, static_cast<uint8>(FMath::RandRange(0, Anims.Gestures.Num() - 1)));
		}
		break;
	case ETikiActivity::Pray:
		PlayAction(ETikiAction::Pray);
		break;
	case ETikiActivity::Rest:
		PlayAction(ETikiAction::Rest);
		break;
	default:
		break;
	}
}

void ATikiVillager::Alarm(AActor* InThreat, float Duration)
{
	// already running home or hidden: the house is safer
	if (IsDead() || State == ETikiVillagerState::GoHome || State == ETikiVillagerState::Hidden)
	{
		return;
	}
	LeaveActivity();
	Threat = InThreat;
	FleeTime = Duration;
	SetState(ETikiVillagerState::Flee);
	// startled first (half of them scream), then it runs
	if (FMath::FRand() < 0.5f)
	{
		BeStartled();
	}
	else
	{
		StartleEndTime = 0.0;
	}
}

void ATikiVillager::HideAtHome(AActor* InThreat, float Duration)
{
	if (IsDead())
	{
		return;
	}
	if (State == ETikiVillagerState::Hidden)
	{
		// another killing while it hides: it stays in longer
		HideTime = FMath::Max(HideTime, static_cast<float>(GetWorld()->GetTimeSeconds() - StateStartTime) + Duration);
		return;
	}
	if (!bHasHouse)
	{
		Alarm(InThreat, Rules ? Rules->AlarmFleeDuration : Duration);
		return;
	}
	LeaveActivity();
	Threat = InThreat;
	HideTime = Duration;
	bPassedGate = !TikiMath::ShouldPassGate(GetActorLocation(), HouseDoor, HouseGate);
	SetState(ETikiVillagerState::GoHome);
	BeStartled();
}

void ATikiVillager::OnRep_Hiding()
{
	ApplyHiding();
}

void ATikiVillager::ApplyHiding()
{
	SetActorHiddenInGame(bHiding);
	SetActorEnableCollision(!bHiding);
	if (bHiding && HasAuthority())
	{
		GetCharacterMovement()->StopMovementImmediately();
	}
}

void ATikiVillager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATikiVillager, bHiding);
}

void ATikiVillager::ChooseNextGoal()
{
	ATikiDirector* Dir = Director.Get();
	if (Dir && !Dir->IsVillageActive(VillageIndex))
	{
		// nobody near the village: the villager rests where it is
		PauseDuration = Rules->WanderPauseMax;
		SetState(ETikiVillagerState::Pause);
		return;
	}
	if (Dir)
	{
		if (AActor* Coconut = Dir->ClaimCoconutNear(this, GetActorLocation(), Rules->CoconutSearchRadius))
		{
			GoalCoconut = Coconut;
			GoalLocation = Coconut->GetActorLocation();
			SetState(ETikiVillagerState::GoToCoconut);
			return;
		}
		if (FMath::FRand() < Rules->ActivityChance)
		{
			const int32 Index = Dir->ClaimPoi(this, VillageIndex);
			FTikiPoi Poi;
			if (Index != INDEX_NONE && Dir->GetPoi(VillageIndex, Index, Poi))
			{
				PoiIndex = Index;
				GoalLocation = Poi.Location;
				SetState(ETikiVillagerState::GoToActivity);
				return;
			}
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
			break;
		}
		// a player close by: it waves, now and then; otherwise it may look around
		if (!IsActionPlaying() && Anims.Greet && GetWorld()->GetTimeSeconds() - LastGreetTime >= Rules->GreetCooldown)
		{
			if (const APawn* Near = FindNearestPlayer(Rules->GreetRadius))
			{
				LastGreetTime = GetWorld()->GetTimeSeconds();
				FaceToward(Near->GetActorLocation());
				PlayAction(ETikiAction::Greet);
				bPauseActed = true;
				break;
			}
		}
		if (!bPauseActed)
		{
			bPauseActed = true;
			if (Anims.LookAround && Dir && Dir->IsVillageActive(VillageIndex) && FMath::FRand() < Rules->LookAroundChance)
			{
				PlayAction(ETikiAction::LookAround);
				PauseDuration = FMath::Max(PauseDuration, Anims.LookAround->GetPlayLength());
			}
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

	case ETikiVillagerState::GoToActivity:
	{
		FTikiPoi Poi;
		if (!Dir || !Dir->GetPoi(VillageIndex, PoiIndex, Poi) || Poi.Claimant.Get() != this)
		{
			PoiIndex = INDEX_NONE;
			ChooseNextGoal();
			break;
		}
		const float Left = MoveToward(Poi.Location, Rules->VillagerWalkSpeed);
		if (Left < 60.0f)
		{
			FaceToward(GetActorLocation() + Poi.Facing * 100.0f);
			CurrentActivity = Poi.Activity;
			ActivityDuration = FMath::FRandRange(Rules->ActivityDurationMin, FMath::Max(Rules->ActivityDurationMin, Rules->ActivityDurationMax));
			SetState(ETikiVillagerState::Activity);
			PlayActivityClip();
			static const TCHAR* const ActivityNames[] = { TEXT("none"), TEXT("work"), TEXT("read"), TEXT("pray"), TEXT("rest"), TEXT("watch"), TEXT("chat") };
			UE_LOG(Logmonke_island, Log, TEXT("%s: %s for %.0f s at point %d of village %d"), *GetName(),
				ActivityNames[FMath::Clamp(static_cast<int32>(CurrentActivity), 0, 6)], ActivityDuration, PoiIndex, VillageIndex);
		}
		else if (UpdateStuck(true) || Elapsed > 60.0)
		{
			LeaveActivity();
			PauseDuration = Rules->WanderPauseMin;
			SetState(ETikiVillagerState::Pause);
		}
		break;
	}

	case ETikiVillagerState::Activity:
		if (Elapsed >= ActivityDuration)
		{
			LeaveActivity();
			PauseDuration = FMath::FRandRange(Rules->WanderPauseMin, FMath::Max(Rules->WanderPauseMin, Rules->WanderPauseMax));
			SetState(ETikiVillagerState::Pause);
		}
		else if (!IsActionPlaying())
		{
			PlayActivityClip();
		}
		break;

	case ETikiVillagerState::Flee:
	{
		const AActor* From = Threat.Get();
		if (!From || Elapsed >= FleeTime)
		{
			Threat.Reset();
			PauseDuration = Rules->WanderPauseMax;
			SetState(ETikiVillagerState::Pause);
			PlayAction(ETikiAction::Cower);
			break;
		}
		// still startled: it does not run yet
		if (GetWorld()->GetTimeSeconds() < StartleEndTime)
		{
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

	case ETikiVillagerState::GoHome:
	{
		// through the gate of the plot, then to the door; blocked on the way to the gate, straight for the door;
		// blocked at the door or too long, it slips into the nearest house (it hides where it is)
		if (GetWorld()->GetTimeSeconds() < StartleEndTime)
		{
			break;
		}
		const float Left = MoveToward(bPassedGate ? HouseDoor : HouseGate, Rules->VillagerRunSpeed);
		const bool bTooLong = Elapsed > Rules->HideReachTime;
		if (!bPassedGate)
		{
			if (Left < 80.0f || UpdateStuck(true) || bTooLong)
			{
				bPassedGate = true;
				UpdateStuck(false);
			}
		}
		else if (Left < 70.0f || UpdateStuck(true) || bTooLong)
		{
			const float Hide = HideTime;
			SetState(ETikiVillagerState::Hidden);
			HideTime = Hide;
			bHiding = true;
			ApplyHiding();
		}
		break;
	}

	case ETikiVillagerState::Hidden:
		if (Elapsed >= HideTime)
		{
			// out again by its door
			bHiding = false;
			ApplyHiding();
			SetActorLocation(HouseDoor + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
			Threat.Reset();
			PauseDuration = Rules->WanderPauseMax;
			SetState(ETikiVillagerState::Pause);
		}
		break;
	}
}

void ATikiVillager::OnDamaged(AActor* Causer, float Damage)
{
	LeaveActivity();
	APawn* Attacker = Cast<APawn>(Causer);
	if (!IsLivingPlayer(Attacker))
	{
		return;
	}
	// the whole village runs away and tiki come out of the bushes around it
	if (ATikiDirector* Dir = Director.Get())
	{
		Dir->RaiseAlarm(this, Attacker);
	}
	else
	{
		Alarm(Attacker, Rules->FleeDuration);
	}
}

void ATikiVillager::OnKilled(AActor* Killer)
{
	LeaveActivity();
	APawn* Player = PawnOfCauser(Killer);
	if (!IsLivingPlayer(Player))
	{
		return;
	}
	// tiki leap out of the bushes around the killer, on top of the alarm of the village
	if (ATikiDirector* Dir = Director.Get())
	{
		Dir->AvengeVillager(this, Player);
	}
}
