// monke_island

#include "Creatures/TikiVillager.h"
#include "Ceremony/FlagCeremonyMath.h"
#include "Ceremony/FlagCeremonyRules.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "Items/NetRestComponent.h"
#include "Stealth/StealthRules.h"
#include "Stealth/StealthSubsystem.h"
#include "Village/VillageHouseKit.h"
#include "Village/VillageHouseNav.h"
#include "Village/VillageSpawner.h"
#include "monke_island.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ATikiVillager::ATikiVillager()
{
}

float ATikiVillager::GetRulesHealth() const
{
	return Rules ? Rules->VillagerHealth : 60.0f;
}

const FTikiNetRules* ATikiVillager::GetNetRules() const
{
	return Rules ? &Rules->VillagerNet : nullptr;
}

void ATikiVillager::SetState(ETikiVillagerState NewState)
{
	// a relic only travels with its villager during the ceremony: anything else lets go of it
	if (CarriedRelic && NewState != ETikiVillagerState::Ceremony)
	{
		DropRelic();
	}
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
	// a villager of a house panics when the threat is near it (its house, the square), otherwise it stays in
	if (ResidenceIndex != INDEX_NONE)
	{
		if (State != ETikiVillagerState::Panic && (!InThreat || FVector::Dist(InThreat->GetActorLocation(), GetActorLocation()) <= 2500.0f))
		{
			Panic(InThreat);
		}
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
	// a villager of a house is home already: it backs off into the room farthest from the threat and cowers there
	if (ResidenceIndex != INDEX_NONE)
	{
		const FVillageHouseInfo* Info = GetResidenceInfo();
		LeaveActivity();
		Threat = InThreat;
		FleeTime = Duration;
		Watched.Reset();
		bCornered = true;
		SetState(ETikiVillagerState::Panic);
		BeStartled();
		NextScreamTime = GetWorld()->GetTimeSeconds() + (Rules ? Rules->ScreamInterval : 2.0f);
		if (Info)
		{
			const FVector From = InThreat ? InThreat->GetActorLocation() : GetActorLocation();
			const int32 Start = IsInResidence(GetActorLocation(), 0.0f) ? NodeNear(GetActorLocation()) : Info->Kit->DoorNode;
			RouteToNode(VillageHouseNav::FarthestNode(Info->Kit->Nodes, Start, Info->Transform.InverseTransformPosition(From)));
		}
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
	DOREPLIFETIME(ATikiVillager, CarriedRelic);
}

void ATikiVillager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		DropRelic();
	}
	Super::EndPlay(EndPlayReason);
}

void ATikiVillager::ChooseNextGoal()
{
	// a villager of a house lives inside
	if (ResidenceIndex != INDEX_NONE)
	{
		ChooseHomeGoal();
		return;
	}
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

	// in its routine it listens for the players: a noise may call it away (T-262)
	if (IsListeningState() && ListenInRoutine())
	{
		return;
	}

	switch (State)
	{
	case ETikiVillagerState::HomeWalk:
	case ETikiVillagerState::HomeActivity:
	case ETikiVillagerState::Watch:
	case ETikiVillagerState::Panic:
	case ETikiVillagerState::ReturnHome:
		ThinkHome(DeltaSeconds, Elapsed);
		return;
	case ETikiVillagerState::Ceremony:
		ThinkCeremony(DeltaSeconds, Elapsed);
		return;
	case ETikiVillagerState::Investigate:
		ThinkInvestigate(Elapsed);
		return;
	default:
		break;
	}

	switch (State)
	{
	case ETikiVillagerState::Pause:
		if (Elapsed >= PauseDuration)
		{
			ChooseNextGoal();
			break;
		}
		// a player close by it sees (not one crouched, T-262): it waves, now and then; otherwise it may look around
		if (!IsActionPlaying() && Anims.Greet && GetWorld()->GetTimeSeconds() - LastGreetTime >= Rules->GreetCooldown)
		{
			if (const APawn* Near = FindNearestPlayer(Rules->GreetRadius, true))
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

	default:
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
	// a villager of a house hit by a player always panics, wherever the alarm leaves the others
	if (ResidenceIndex != INDEX_NONE && State != ETikiVillagerState::Panic)
	{
		Panic(Attacker);
	}
}

void ATikiVillager::OnKilled(AActor* Killer)
{
	LeaveActivity();
	DropRelic();
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

// ---------------------------------------------------------------- villager of a house

void ATikiVillager::SetResidence(AVillageSpawner* InSpawner, int32 InHouse)
{
	Spawner = InSpawner;
	ResidenceIndex = InHouse;
	if (const FVillageHouseInfo* Info = GetResidenceInfo())
	{
		SetHouse(Info->Door, Info->Gate);
	}
}

const FVillageHouseInfo* ATikiVillager::GetResidenceInfo() const
{
	const AVillageSpawner* HouseSpawner = Spawner.Get();
	const FVillageHouseInfo* Info = HouseSpawner ? HouseSpawner->GetHouse(ResidenceIndex) : nullptr;
	return Info && Info->Kit && !Info->Kit->Nodes.IsEmpty() ? Info : nullptr;
}

FVector ATikiVillager::NodeWorld(int32 Node) const
{
	const FVillageHouseInfo* Info = GetResidenceInfo();
	return Info && Info->Kit->Nodes.IsValidIndex(Node) ? Info->Transform.TransformPosition(Info->Kit->Nodes[Node].Location) : GetActorLocation();
}

bool ATikiVillager::IsInResidence(const FVector& Location, float Margin) const
{
	const FVillageHouseInfo* Info = GetResidenceInfo();
	return Info && VillageHouseNav::IsInside(Info->Kit->Interior, Info->Transform.InverseTransformPosition(Location), Margin);
}

int32 ATikiVillager::NodeNear(const FVector& Location) const
{
	const FVillageHouseInfo* Info = GetResidenceInfo();
	return Info ? VillageHouseNav::NearestNode(Info->Kit->Nodes, Info->Transform.InverseTransformPosition(Location)) : INDEX_NONE;
}

bool ATikiVillager::RouteToNode(int32 Goal)
{
	Route.Reset();
	RouteStep = 0;
	UpdateStuck(false);
	const FVillageHouseInfo* Info = GetResidenceInfo();
	if (!Info || !Info->Kit->Nodes.IsValidIndex(Goal))
	{
		return false;
	}
	const UVillageHouseKit& Kit = *Info->Kit;
	int32 Start = INDEX_NONE;
	if (IsInResidence(GetActorLocation(), 0.0f))
	{
		Start = NodeNear(GetActorLocation());
	}
	else
	{
		// from outside: by the gate of its plot when it comes from beyond it, its door, then the doorway
		const double ToDoor = FVector::DistSquared2D(GetActorLocation(), Info->Door);
		if (FVector::DistSquared2D(Info->Gate, Info->Door) > 1.0 && ToDoor > FVector::DistSquared2D(Info->Gate, Info->Door))
		{
			Route.Add(Info->Gate);
		}
		Route.Add(Info->Door);
		Route.Add(Info->Transform.TransformPosition(Kit.OutsideLocation));
		Start = Kit.DoorNode;
	}
	for (const int32 Node : VillageHouseNav::FindPath(Kit.Nodes, Start, Goal))
	{
		Route.Add(NodeWorld(Node));
	}
	TargetNode = Goal;
	return !Route.IsEmpty();
}

void ATikiVillager::RouteOut()
{
	Route.Reset();
	RouteStep = 0;
	UpdateStuck(false);
	const FVillageHouseInfo* Info = GetResidenceInfo();
	if (!Info || !IsInResidence(GetActorLocation(), 0.0f))
	{
		return;
	}
	for (const int32 Node : VillageHouseNav::FindPath(Info->Kit->Nodes, NodeNear(GetActorLocation()), Info->Kit->DoorNode))
	{
		Route.Add(NodeWorld(Node));
	}
	Route.Add(Info->Transform.TransformPosition(Info->Kit->OutsideLocation));
	Route.Add(Info->Door);
	if (FVector::DistSquared2D(Info->Gate, Info->Door) > 1.0)
	{
		Route.Add(Info->Gate);
	}
}

bool ATikiVillager::FollowRoute(float Speed)
{
	if (RouteStep >= Route.Num())
	{
		return true;
	}
	const float Left = MoveToward(Route[RouteStep], Speed);
	if (Left < Rules->WaypointRadius)
	{
		++RouteStep;
		StuckHops = 0;
		UpdateStuck(false);
	}
	else if (UpdateStuck(true))
	{
		// at the flag ceremony a fence or a barrier in the way must not keep it from the ring (owner report 2026-10-07):
		// it hops over toward the waypoint, then is set on it; elsewhere a waypoint it cannot reach (a closed door,
		// someone in the way) is passed after the stuck delay, as before
		const bool bCeremony = State == ETikiVillagerState::Ceremony;
		if (bCeremony && StuckHops < Rules->CeremonyStuckHops)
		{
			const FVector Direction = (Route[RouteStep] - GetActorLocation()).GetSafeNormal2D();
			LaunchCharacter(Direction * Rules->StuckHopForward + FVector(0.0f, 0.0f, Rules->StuckHopUp), true, true);
			++StuckHops;
		}
		else
		{
			if (bCeremony)
			{
				FVector Ground;
				const ATikiDirector* Dir = Director.Get();
				const FVector Waypoint = Route[RouteStep];
				if (Dir && Dir->ProjectToGround(Waypoint, Ground))
				{
					TeleportTo(Ground + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f), GetActorRotation(), false, false);
				}
				UE_LOG(Logmonke_island, Log, TEXT("%s: stuck on its way to the flag, set on its waypoint"), *GetName());
			}
			++RouteStep;
			StuckHops = 0;
		}
		UpdateStuck(false);
	}
	return RouteStep >= Route.Num();
}

void ATikiVillager::ChooseHomeGoal()
{
	const FVillageHouseInfo* Info = GetResidenceInfo();
	ATikiDirector* Dir = Director.Get();
	LeaveActivity();
	if (!Info)
	{
		ResidenceIndex = INDEX_NONE;
		PauseDuration = Rules->WanderPauseMax;
		SetState(ETikiVillagerState::Pause);
		return;
	}
	TArray<int32> Points = VillageHouseNav::ActivityNodes(Info->Kit->Nodes);
	Points.Remove(TargetNode);
	// nobody near the village: it stays where it is, idle
	if (Points.IsEmpty() || (Dir && !Dir->IsVillageActive(VillageIndex)))
	{
		ActivityDuration = Rules->WanderPauseMax;
		SetState(ETikiVillagerState::HomeActivity);
		return;
	}
	if (RouteToNode(Points[FMath::RandRange(0, Points.Num() - 1)]))
	{
		SetState(ETikiVillagerState::HomeWalk);
	}
	else
	{
		ActivityDuration = Rules->WanderPauseMax;
		SetState(ETikiVillagerState::HomeActivity);
	}
}

APawn* ATikiVillager::FindPlayerInResidence() const
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		// a player crouched in its house is not seen, unless at contact (T-262)
		if (IsLivingPlayer(Pawn) && IsInResidence(Pawn->GetActorLocation(), 0.0f) && CanSpot(Pawn, 0.0f))
		{
			return Pawn;
		}
	}
	return nullptr;
}

void ATikiVillager::StartWatching(APawn* Player)
{
	LeaveActivity();
	Watched = Player;
	WatchDoorUntil = -1.0;
	WatchGoalNode = INDEX_NONE;
	LastWatchPlan = -1.0e6;
	WatchSeenAt = Player->GetActorLocation();
	WatchSeenTime = GetWorld()->GetTimeSeconds();
	Lead.Clear();
	Route.Reset();
	RouteStep = 0;
	SetState(ETikiVillagerState::Watch);
	FaceToward(Player->GetActorLocation());
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s came into house %d, it watches him"), *GetName(), *Player->GetName(), ResidenceIndex);
}

bool ATikiVillager::IsWatchingHouse() const
{
	// looking for a noise in its house, it is home too (T-262)
	return ResidenceIndex != INDEX_NONE && !IsDead()
		&& (State == ETikiVillagerState::HomeWalk || State == ETikiVillagerState::HomeActivity || State == ETikiVillagerState::Watch
			|| State == ETikiVillagerState::Investigate)
		&& IsInResidence(GetActorLocation(), 60.0f);
}

bool ATikiVillager::IsListeningState() const
{
	switch (State)
	{
	case ETikiVillagerState::Pause:
	case ETikiVillagerState::Wander:
	case ETikiVillagerState::GoToCoconut:
	case ETikiVillagerState::Collect:
	case ETikiVillagerState::GoToActivity:
	case ETikiVillagerState::Activity:
	case ETikiVillagerState::HomeWalk:
	case ETikiVillagerState::HomeActivity:
	case ETikiVillagerState::Investigate:
		return true;
	default:
		// running away, hiding, watching a player, at the flag ceremony: it has other things in mind
		return false;
	}
}

bool ATikiVillager::ListenInRoutine()
{
	const UStealthRules& StealthRules = UStealthSubsystem::RulesOf(this);
	FStealthNoise Noise;
	if (!Rules || !Listen(StealthRules.VillagerHearingScale, Noise))
	{
		return false;
	}
	AActor* Source = Noise.Instigator.Get();
	// a shot or a blast: it runs away from the shooter (a villager of a house panics when it is near); no tiki comes
	if (Noise.Kind == EStealthNoise::Gunshot || Noise.Kind == EStealthNoise::Impact)
	{
		if (!Source || !Lead.TakeNoise(Noise))
		{
			return false;
		}
		Lead.Clear();
		UE_LOG(LogMonkeStealth, Log, TEXT("%s hears a shot of %s, runs away"), *GetName(), *Source->GetName());
		Alarm(Source, Rules->FleeDuration);
		return true;
	}
	// running steps or a voice: it goes to look; a villager of a house only minds its house
	if (ResidenceIndex != INDEX_NONE && !IsInResidence(Noise.Location, StealthRules.HouseNoiseMarginCm))
	{
		return false;
	}
	const bool bWasLooking = State == ETikiVillagerState::Investigate;
	if (!Lead.TakeNoise(Noise))
	{
		return false;
	}
	if (!bWasLooking)
	{
		UE_LOG(LogMonkeStealth, Log, TEXT("%s hears %s at %.0f m, goes to look"), *GetName(), *GetNameSafe(Source),
			FVector::Dist(Noise.Location, GetActorLocation()) / 100.0);
	}
	StartInvestigating();
	return true;
}

void ATikiVillager::StartInvestigating()
{
	LeaveActivity();
	Route.Reset();
	RouteStep = 0;
	// a villager of a house goes there along the waypoints of its house
	if (ResidenceIndex != INDEX_NONE && GetResidenceInfo())
	{
		RouteToNode(NodeNear(Lead.Location));
	}
	if (State != ETikiVillagerState::Investigate)
	{
		SetState(ETikiVillagerState::Investigate);
	}
	UpdateStuck(false);
	FaceToward(Lead.Location);
}

void ATikiVillager::ThinkInvestigate(double Elapsed)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const UStealthRules& StealthRules = UStealthSubsystem::RulesOf(this);
	// a player in sight: a villager of a house watches him in its house; a villager of the square turns to him, waves
	// when he is close, and goes back to its routine
	if (ResidenceIndex != INDEX_NONE)
	{
		if (APawn* Player = FindPlayerInResidence())
		{
			StartWatching(Player);
			return;
		}
	}
	else if (const APawn* Player = FindNearestPlayer(StealthRules.VillagerSightCm, true))
	{
		Lead.Clear();
		FaceToward(Player->GetActorLocation());
		if (Anims.Greet && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) <= Rules->GreetRadius && Now - LastGreetTime >= Rules->GreetCooldown)
		{
			LastGreetTime = Now;
			PlayAction(ETikiAction::Greet);
		}
		GoBackToRoutine();
		return;
	}
	// nobody found: back to its routine once it forgot (or after a minute whatever happens)
	if (!Lead.IsActive(Now, StealthRules.NoiseMemorySeconds) || Elapsed > 60.0)
	{
		UE_LOG(LogMonkeStealth, Log, TEXT("%s found nobody, back to its routine"), *GetName());
		Lead.Clear();
		GoBackToRoutine();
		return;
	}
	if (!Lead.bArrived)
	{
		bool bThere = false;
		if (!Route.IsEmpty())
		{
			bThere = FollowRoute(Rules->VillagerWalkSpeed);
		}
		else
		{
			bThere = MoveToward(Lead.Location, Rules->VillagerWalkSpeed) <= StealthRules.ArriveRadiusCm || UpdateStuck(true);
		}
		if (!bThere)
		{
			return;
		}
		Lead.bArrived = true;
		NextSearchTurnTime = Now;
		GetCharacterMovement()->StopMovementImmediately();
	}
	// there: it looks around now and then
	if (Now >= NextSearchTurnTime && !IsActionPlaying())
	{
		NextSearchTurnTime = Now + StealthRules.SearchTurnSeconds;
		if (Anims.LookAround)
		{
			PlayAction(ETikiAction::LookAround);
		}
		else
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
			FaceToward(GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 200.0f);
		}
	}
}

bool ATikiVillager::CanSee(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiVillagerSight), false, this);
	Params.AddIgnoredActor(Pawn);
	if (CarriedRelic)
	{
		Params.AddIgnoredActor(CarriedRelic);
	}
	const FVector Eyes = GetActorLocation() + FVector(0.0f, 0.0f, BaseEyeHeight);
	return !GetWorld()->LineTraceTestByChannel(Eyes, Pawn->GetActorLocation(), ECC_Visibility, Params);
}

void ATikiVillager::TurnToward(const FVector& Target, float DeltaSeconds)
{
	FVector Delta = Target - GetActorLocation();
	Delta.Z = 0.0f;
	if (Delta.IsNearlyZero())
	{
		return;
	}
	const FRotator Goal(0.0f, Delta.Rotation().Yaw, 0.0f);
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Goal, DeltaSeconds, 6.0f));
}

void ATikiVillager::GoBackToRoutine()
{
	Threat.Reset();
	Watched.Reset();
	Officiant.Reset();
	CeremonyStep = ETikiCeremonyStep::None;
	bCornered = false;
	if (ResidenceIndex == INDEX_NONE)
	{
		PauseDuration = Rules->WanderPauseMin;
		SetState(ETikiVillagerState::Pause);
		return;
	}
	const FVillageHouseInfo* Info = GetResidenceInfo();
	const TArray<int32> Points = Info ? VillageHouseNav::ActivityNodes(Info->Kit->Nodes) : TArray<int32>();
	if (!Points.IsEmpty() && RouteToNode(Points[FMath::RandRange(0, Points.Num() - 1)]))
	{
		SetState(ETikiVillagerState::ReturnHome);
		return;
	}
	ChooseHomeGoal();
}

void ATikiVillager::Panic(AActor* InThreat)
{
	if (IsDead())
	{
		return;
	}
	LeaveActivity();
	Watched.Reset();
	Officiant.Reset();
	CeremonyStep = ETikiCeremonyStep::None;
	Threat = InThreat;
	FleeTime = Rules->PanicSeconds;
	bCornered = false;
	SetState(ETikiVillagerState::Panic);
	BeStartled();
	NextScreamTime = GetWorld()->GetTimeSeconds() + Rules->ScreamInterval;
	Route.Reset();
	RouteStep = 0;
	if (const FVillageHouseInfo* Info = GetResidenceInfo(); Info && IsInResidence(GetActorLocation(), 0.0f))
	{
		// out by its door, unless the threat stands between it and the door: then into the farthest room
		const FVector From = InThreat ? InThreat->GetActorLocation() : GetActorLocation();
		const FVector Door = NodeWorld(Info->Kit->DoorNode);
		if (InThreat && IsInResidence(From, 50.0f) && FVector::DistSquared2D(From, Door) < FVector::DistSquared2D(GetActorLocation(), Door))
		{
			bCornered = true;
			RouteToNode(VillageHouseNav::FarthestNode(Info->Kit->Nodes, NodeNear(GetActorLocation()), Info->Transform.InverseTransformPosition(From)));
		}
		else
		{
			RouteOut();
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s panics (%s), %s"), *GetName(), *GetNameSafe(InThreat),
		bCornered ? TEXT("backing off into its house") : TEXT("running away"));
}

void ATikiVillager::ThinkHome(float DeltaSeconds, double Elapsed)
{
	const double Now = GetWorld()->GetTimeSeconds();
	switch (State)
	{
	case ETikiVillagerState::HomeWalk:
	case ETikiVillagerState::ReturnHome:
	{
		if (State == ETikiVillagerState::HomeWalk || IsInResidence(GetActorLocation(), 0.0f))
		{
			if (APawn* Player = FindPlayerInResidence())
			{
				StartWatching(Player);
				break;
			}
		}
		const float Speed = Rules->VillagerWalkSpeed;
		if (!FollowRoute(Speed))
		{
			// lost outside for too long: it slips back in
			if (State == ETikiVillagerState::ReturnHome && Elapsed > 90.0 && GetResidenceInfo())
			{
				SetActorLocation(NodeWorld(TargetNode) + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), false, nullptr, ETeleportType::TeleportPhysics);
				Route.Reset();
			}
			break;
		}
		// at the point of its routine: its activity, facing what it does there
		const FVillageHouseInfo* Info = GetResidenceInfo();
		if (!Info || !Info->Kit->Nodes.IsValidIndex(TargetNode))
		{
			ChooseHomeGoal();
			break;
		}
		const FVillageHouseNode& Node = Info->Kit->Nodes[TargetNode];
		FaceToward(GetActorLocation() + Info->Transform.TransformVectorNoScale(FRotator(0.0f, Node.Yaw, 0.0f).Vector()) * 100.0f);
		CurrentActivity = TikiMath::ActivityFromName(Node.Activity);
		ActivityDuration = FMath::FRandRange(Rules->ResidentActivityMin, FMath::Max(Rules->ResidentActivityMin, Rules->ResidentActivityMax));
		SetState(ETikiVillagerState::HomeActivity);
		PlayActivityClip();
		break;
	}

	case ETikiVillagerState::HomeActivity:
		if (APawn* Player = FindPlayerInResidence())
		{
			StartWatching(Player);
			break;
		}
		if (Elapsed >= ActivityDuration)
		{
			ChooseHomeGoal();
		}
		else if (!IsActionPlaying())
		{
			PlayActivityClip();
		}
		break;

	case ETikiVillagerState::Watch:
	{
		APawn* Player = Watched.Get();
		const bool bInside = IsLivingPlayer(Player) && IsInResidence(Player->GetActorLocation(), 0.0f);
		// a player it no longer sees in its house (crouched, the night: T-262) is looked for where it saw him last
		if (bInside && WatchDoorUntil < 0.0)
		{
			if (CanSpot(Player, 0.0f))
			{
				WatchSeenAt = Player->GetActorLocation();
				WatchSeenTime = Now;
			}
			else if (Now - WatchSeenTime > UStealthSubsystem::RulesOf(this).LoseSightGraceSeconds)
			{
				UE_LOG(LogMonkeStealth, Log, TEXT("%s lost sight of %s in house %d, looks for him"), *GetName(), *Player->GetName(), ResidenceIndex);
				Lead.Hunt(Player, WatchSeenAt, Now);
				Watched.Reset();
				StartInvestigating();
				break;
			}
		}
		if (WatchDoorUntil >= 0.0)
		{
			// the player went out: it watches him go from its door, or follows him again if he comes back in sight
			if (bInside && CanSpot(Player, 0.0f))
			{
				WatchDoorUntil = -1.0;
				WatchSeenAt = Player->GetActorLocation();
				WatchSeenTime = Now;
				Route.Reset();
				break;
			}
			if (FollowRoute(Rules->VillagerWalkSpeed))
			{
				if (const FVillageHouseInfo* Info = GetResidenceInfo())
				{
					TurnToward(Info->Door, DeltaSeconds);
				}
				if (Now >= WatchDoorUntil)
				{
					Watched.Reset();
					ChooseHomeGoal();
				}
			}
			break;
		}
		if (!bInside)
		{
			const FVillageHouseInfo* Info = GetResidenceInfo();
			if (!Info || !RouteToNode(Info->Kit->DoorNode))
			{
				Watched.Reset();
				ChooseHomeGoal();
				break;
			}
			WatchDoorUntil = Now + Rules->WatchLingerSeconds + 8.0;
			break;
		}
		const FVector Target = Player->GetActorLocation();
		FVector Away = GetActorLocation() - Target;
		Away.Z = 0.0f;
		const float Distance = static_cast<float>(Away.Size());
		if (CanSee(Player))
		{
			// in sight: it keeps its distance, never bumping into him, and stares at him
			Route.Reset();
			RouteStep = 0;
			const FVector Direction = Distance > KINDA_SMALL_NUMBER ? Away / Distance : -GetActorForwardVector();
			if (Distance > Rules->WatchDistance + 30.0f)
			{
				MoveToward(Target + Direction * Rules->WatchDistance, Rules->VillagerWalkSpeed * 1.3f);
			}
			else if (Distance < Rules->WatchMinDistance)
			{
				MoveToward(GetActorLocation() + Direction * 80.0f, Rules->VillagerWalkSpeed);
			}
			else
			{
				TurnToward(Target, DeltaSeconds);
			}
			break;
		}
		// out of sight (another room): along the waypoints toward him
		const int32 Goal = NodeNear(Target);
		if (Goal != WatchGoalNode || (RouteStep >= Route.Num() && Now - LastWatchPlan >= Rules->WatchReplanInterval))
		{
			RouteToNode(Goal);
			WatchGoalNode = Goal;
			LastWatchPlan = Now;
		}
		FollowRoute(Rules->VillagerWalkSpeed * 1.3f);
		break;
	}

	case ETikiVillagerState::Panic:
	{
		if (Now >= NextScreamTime && !bCornered)
		{
			PlayAction(ETikiAction::Scream);
			NextScreamTime = Now + Rules->ScreamInterval;
		}
		if (Now < StartleEndTime)
		{
			break;
		}
		if (RouteStep < Route.Num())
		{
			FollowRoute(Rules->VillagerRunSpeed);
			break;
		}
		if (Elapsed >= FleeTime)
		{
			GoBackToRoutine();
			break;
		}
		if (bCornered)
		{
			const AActor* From = Threat.Get();
			if (From)
			{
				TurnToward(From->GetActorLocation(), DeltaSeconds);
			}
			if (!IsActionPlaying())
			{
				PlayAction(ETikiAction::Cower);
			}
			break;
		}
		// out of the house: away from the threat
		const AActor* From = Threat.Get();
		FVector Away = From ? GetActorLocation() - From->GetActorLocation() : GetActorForwardVector();
		Away.Z = 0.0f;
		if (Away.IsNearlyZero())
		{
			Away = GetActorForwardVector();
		}
		MoveToward(GetActorLocation() + Away.GetSafeNormal() * 500.0f, Rules->VillagerRunSpeed);
		break;
	}

	default:
		break;
	}
}

// ---------------------------------------------------------------- flag ceremony

void ATikiVillager::SetCeremonyStep(ETikiCeremonyStep Step)
{
	CeremonyStep = Step;
	CeremonyStepTime = GetWorld()->GetTimeSeconds();
	UpdateStuck(false);
}

void ATikiVillager::RouteToRing()
{
	const UFlagCeremonyRules* CeremonyRulesNow = CeremonyRules.Get();
	if (!CeremonyRulesNow)
	{
		return;
	}
	if (RouteStep >= Route.Num())
	{
		Route.Reset();
		RouteStep = 0;
	}
	const FVector From = Route.IsEmpty() ? GetActorLocation() : Route.Last();
	// round the outside of the ring: never across it, where the officiant stands still
	for (const FVector& Point : FlagCeremonyMath::RingApproach(CeremonyCenter, From, CeremonySlot, CeremonyRulesNow->GatherRadius,
		CeremonyRulesNow->GatherRadius + 110.0f, 35.0f))
	{
		Route.Add(FlagCeremonyMath::KeepOffCairn(Point, CeremonyCairn, CeremonyRulesNow->CairnClearance));
	}
}

void ATikiVillager::JoinCeremony(APawn* InOfficiant, const FVector& InCenter, const FVector& InCairn, float SlotDegrees, const UFlagCeremonyRules* InRules)
{
	if (IsDead() || bHiding || !InRules)
	{
		return;
	}
	LeaveActivity();
	Watched.Reset();
	WatchDoorUntil = -1.0;
	Threat.Reset();
	Officiant = InOfficiant;
	CeremonyCenter = InCenter;
	CeremonyCairn = InCairn;
	CeremonySlot = SlotDegrees;
	CeremonyRules = InRules;
	bOfferRelic = false;
	SetState(ETikiVillagerState::Ceremony);
	Route.Reset();
	RouteStep = 0;
	// a villager of a house goes for the relic of its house first, while it is still there
	if (const FVillageHouseInfo* Info = GetResidenceInfo())
	{
		const AActor* Relic = Info->Relic.Get();
		if (IsValid(Relic) && VillageHouseNav::IsInside(Info->Kit->Interior, Info->Transform.InverseTransformPosition(Relic->GetActorLocation()), 0.0f))
		{
			const int32 Node = Info->Kit->Nodes.IsValidIndex(Info->Kit->RelicNode) ? Info->Kit->RelicNode : NodeNear(Relic->GetActorLocation());
			if (RouteToNode(Node))
			{
				SetCeremonyStep(ETikiCeremonyStep::FetchRelic);
				return;
			}
		}
		RouteOut();
	}
	RouteToRing();
	SetCeremonyStep(ETikiCeremonyStep::ToRing);
}

void ATikiVillager::EndCeremony(bool bRaised)
{
	if (State != ETikiVillagerState::Ceremony)
	{
		return;
	}
	if (bRaised)
	{
		if (CarriedRelic)
		{
			// it lays the relic as soon as it stands in the ring
			bOfferRelic = true;
		}
		else if (CeremonyStep == ETikiCeremonyStep::FetchRelic || CeremonyStep == ETikiCeremonyStep::TakeRelic)
		{
			GoBackToRoutine();
		}
		else
		{
			Route.Reset();
			RouteStep = 0;
			PlayAction(ETikiAction::Gesture, static_cast<uint8>(FMath::Clamp(Anims.Gestures.Num() - 1, 0, 4)));
			SetCeremonyStep(ETikiCeremonyStep::Celebrate);
		}
		return;
	}
	// interrupted: the relic goes back where it stood
	if (CarriedRelic)
	{
		const FVillageHouseInfo* Info = GetResidenceInfo();
		const int32 Node = Info ? (Info->Kit->Nodes.IsValidIndex(Info->Kit->RelicNode) ? Info->Kit->RelicNode : NodeNear(Info->RelicHome.GetLocation())) : INDEX_NONE;
		if (RouteToNode(Node))
		{
			SetCeremonyStep(ETikiCeremonyStep::BringBack);
			return;
		}
		PutBackRelic();
	}
	GoBackToRoutine();
}

void ATikiVillager::ThinkCeremony(float DeltaSeconds, double Elapsed)
{
	const UFlagCeremonyRules* CeremonyRulesNow = CeremonyRules.Get();
	if (!CeremonyRulesNow)
	{
		if (CarriedRelic)
		{
			PutBackRelic();
		}
		GoBackToRoutine();
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const double InStep = Now - CeremonyStepTime;
	const APawn* Leader = Officiant.Get();
	const FVector Facing = Leader ? Leader->GetActorLocation() : CeremonyCenter;
	switch (CeremonyStep)
	{
	case ETikiCeremonyStep::FetchRelic:
		if (FollowRoute(CeremonyRulesNow->GatherSpeed))
		{
			if (const FVillageHouseInfo* Info = GetResidenceInfo(); Info && Info->Relic.IsValid())
			{
				FaceToward(Info->Relic->GetActorLocation());
			}
			PlayAction(ETikiAction::Collect);
			SetCeremonyStep(ETikiCeremonyStep::TakeRelic);
		}
		else if (InStep > 40.0)
		{
			// the relic is out of reach: it comes without it
			RouteOut();
			RouteToRing();
			SetCeremonyStep(ETikiCeremonyStep::ToRing);
		}
		break;

	case ETikiCeremonyStep::TakeRelic:
		if (InStep >= Rules->CollectDuration * 0.5f)
		{
			TakeRelic();
			RouteOut();
			RouteToRing();
			SetCeremonyStep(ETikiCeremonyStep::ToRing);
		}
		break;

	case ETikiCeremonyStep::ToRing:
		if (FollowRoute(CeremonyRulesNow->GatherSpeed))
		{
			SetCeremonyStep(ETikiCeremonyStep::InRing);
		}
		break;

	case ETikiCeremonyStep::InRing:
		if (bOfferRelic && CarriedRelic)
		{
			SetCeremonyStep(ETikiCeremonyStep::ToOffer);
			break;
		}
		TurnToward(Facing, DeltaSeconds);
		break;

	case ETikiCeremonyStep::ToOffer:
	{
		// a step in from its place, then it bends down to lay the relic before it
		const FVector Stand = FlagCeremonyMath::KeepOffCairn(FlagCeremonyMath::RingPoint(CeremonyCenter, CeremonySlot,
			CeremonyRulesNow->OfferRadius + 70.0f), CeremonyCairn, CeremonyRulesNow->CairnClearance + 70.0f);
		if (MoveToward(Stand, Rules->VillagerWalkSpeed) < Rules->WaypointRadius || UpdateStuck(true) || InStep > 15.0)
		{
			FaceToward(CeremonyCenter);
			PlayAction(ETikiAction::Collect);
			SetCeremonyStep(ETikiCeremonyStep::Offer);
		}
		break;
	}

	case ETikiCeremonyStep::Offer:
		if (InStep >= Rules->CollectDuration * 0.5f)
		{
			FVector Spot = FlagCeremonyMath::KeepOffCairn(FlagCeremonyMath::RingPoint(CeremonyCenter, CeremonySlot, CeremonyRulesNow->OfferRadius),
				CeremonyCairn, CeremonyRulesNow->CairnClearance);
			FVector Ground;
			if (const ATikiDirector* Dir = Director.Get(); Dir && Dir->ProjectToGround(Spot, Ground))
			{
				Spot = Ground;
			}
			else
			{
				Spot.Z = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			}
			LayRelic(Spot, FlagCeremonyMath::AngleAround(CeremonyCenter, Spot) + 180.0f);
			PlayAction(ETikiAction::Gesture, static_cast<uint8>(FMath::Clamp(Anims.Gestures.Num() - 1, 0, 4)));
			SetCeremonyStep(ETikiCeremonyStep::Celebrate);
		}
		break;

	case ETikiCeremonyStep::Celebrate:
		TurnToward(Facing, DeltaSeconds);
		if (!IsActionPlaying() && FMath::FRand() < DeltaSeconds * 0.4f && !Anims.Gestures.IsEmpty())
		{
			PlayAction(ETikiAction::Gesture, static_cast<uint8>(FMath::RandRange(0, Anims.Gestures.Num() - 1)));
		}
		if (InStep >= CeremonyRulesNow->CelebrateSeconds)
		{
			GoBackToRoutine();
		}
		break;

	case ETikiCeremonyStep::BringBack:
		if (FollowRoute(Rules->VillagerWalkSpeed) || InStep > 60.0)
		{
			PutBackRelic();
			GoBackToRoutine();
		}
		break;

	default:
		GoBackToRoutine();
		break;
	}
}

bool ATikiVillager::TakeRelic()
{
	const FVillageHouseInfo* Info = GetResidenceInfo();
	AActor* Relic = Info ? Info->Relic.Get() : nullptr;
	if (!IsValid(Relic) || Relic->GetAttachParentActor())
	{
		return false;
	}
	// carried by its own villager: it is no theft
	if (AVillageSpawner* HouseSpawner = Spawner.Get())
	{
		HouseSpawner->ForgetHouseContent(ResidenceIndex, Relic);
	}
	CarriedRelic = Relic;
	AttachRelic(Relic, true);
	ForceNetUpdate();
	UE_LOG(Logmonke_island, Log, TEXT("%s takes %s for the flag ceremony"), *GetName(), *Relic->GetName());
	return true;
}

void ATikiVillager::LayRelic(const FVector& Spot, float Yaw)
{
	AActor* Relic = CarriedRelic;
	if (!IsValid(Relic))
	{
		CarriedRelic = nullptr;
		return;
	}
	CarriedRelic = nullptr;
	ForceNetUpdate();
	AttachRelic(Relic, false);
	Relic->SetActorLocationAndRotation(Spot + FVector(0.0f, 0.0f, 2.0f), FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Relic->GetRootComponent()))
	{
		Body->SetSimulatePhysics(true);
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s lays %s at the flag"), *GetName(), *Relic->GetName());
}

void ATikiVillager::PutBackRelic()
{
	AActor* Relic = CarriedRelic;
	const FVillageHouseInfo* Info = GetResidenceInfo();
	if (!IsValid(Relic))
	{
		CarriedRelic = nullptr;
		return;
	}
	CarriedRelic = nullptr;
	ForceNetUpdate();
	AttachRelic(Relic, false);
	if (Info)
	{
		Relic->SetActorTransform(Info->RelicHome, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Relic->GetRootComponent()))
	{
		Body->SetSimulatePhysics(true);
		Body->PutAllRigidBodiesToSleep();
	}
	if (AVillageSpawner* HouseSpawner = Spawner.Get())
	{
		HouseSpawner->RestoreHouseRelic(ResidenceIndex, Relic);
	}
}

void ATikiVillager::DropRelic()
{
	AActor* Relic = CarriedRelic;
	if (!Relic)
	{
		return;
	}
	CarriedRelic = nullptr;
	if (!IsValid(Relic))
	{
		return;
	}
	ForceNetUpdate();
	AttachRelic(Relic, false);
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Relic->GetRootComponent()))
	{
		Body->SetSimulatePhysics(true);
	}
}

void ATikiVillager::OnRep_CarriedRelic(AActor* Previous)
{
	if (IsValid(Previous) && Previous != CarriedRelic && Previous->GetAttachParentActor() == this)
	{
		AttachRelic(Previous, false);
	}
	if (IsValid(CarriedRelic))
	{
		AttachRelic(CarriedRelic, true);
	}
}

void ATikiVillager::AttachRelic(AActor* Relic, bool bAttach) const
{
	if (!IsValid(Relic))
	{
		return;
	}
	// a relic asleep in its house does not replicate any more (T-312): taken or laid down, it does again at once
	UNetRestComponent::WakeActorNet(Relic);
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Relic->GetRootComponent());
	if (bAttach)
	{
		// held in front of it: no physics, no collision against its carrier or the players around
		if (Body)
		{
			Body->SetSimulatePhysics(false);
			Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Relic->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Relic->SetActorRelativeLocation(Rules ? Rules->RelicCarryOffset : FVector(38.0f, 0.0f, -18.0f));
		Relic->SetActorRelativeRotation(FRotator::ZeroRotator);
		return;
	}
	Relic->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (Body)
	{
		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}
