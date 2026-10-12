// monke_island

#include "Galleon/GalleonDepartureComponent.h"
#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonCrewComponent.h"
#include "Galleon/GalleonRules.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** The ticks of a notice are sampled this often on every machine (s): presentation only, the times are the server's */
	constexpr float NoticeTickSampleSeconds = 0.1f;

	/** The galleon is looked for at most this often while a map has none (s): no actor search every frame (ARC-05) */
	constexpr double GalleonSearchSeconds = 1.0;

	/** Same state: nothing to replicate now nor to present */
	bool IsSameState(const FGalleonDepartureState& A, const FGalleonDepartureState& B)
	{
		return A.Stage == B.Stage && A.bAllAboard == B.bAllAboard && A.Voters == B.Voters && A.Alive == B.Alive
			&& A.Aboard == B.Aboard && A.Needed == B.Needed && A.StageStartTime == B.StageStartTime
			&& A.StageEndTime == B.StageEndTime && A.NextVoteTime == B.NextVoteTime && A.LastEnd == B.LastEnd
			&& A.LastEndTime == B.LastEndTime;
	}

	/** A count kept in a byte of the replicated state */
	uint8 ToByte(int32 Value)
	{
		return static_cast<uint8>(FMath::Clamp(Value, 0, 255));
	}
}

UGalleonDepartureComponent::UGalleonDepartureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UGalleonDepartureComponent* UGalleonDepartureComponent::Get(const UObject* WorldContextObject)
{
	const AExpeditionGameState* Expedition = AExpeditionGameState::Get(WorldContextObject);
	return Expedition ? Expedition->GetDeparture() : nullptr;
}

bool UGalleonDepartureComponent::IsLivingPawn(const APawn* Pawn)
{
	const UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	return Health && !Health->IsDead();
}

void UGalleonDepartureComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGalleonDepartureComponent, State);
	DOREPLIFETIME(UGalleonDepartureComponent, Hold);
}

void UGalleonDepartureComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	if (AExpeditionGameState* Expedition = GetExpedition())
	{
		Expedition->OnDaySurvived.AddDynamic(this, &UGalleonDepartureComponent::HandleDayOver);
		Expedition->OnTeamWiped.AddDynamic(this, &UGalleonDepartureComponent::HandleDayOver);
	}
	// every player controller gets its keys and requests of the departure: those already in, those to come
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UGalleonDepartureComponent::HandlePostLogin);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		UGalleonCrewComponent::Ensure(It->Get());
	}
}

void UGalleonDepartureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& Timers = World->GetTimerManager();
		Timers.ClearTimer(HoldTimer);
		Timers.ClearTimer(NoticeTimer);
		Timers.ClearTimer(CheckTimer);
		Timers.ClearTimer(TickTimer);
	}
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	PostLoginHandle.Reset();
	if (AExpeditionGameState* Expedition = GetExpedition())
	{
		Expedition->OnDaySurvived.RemoveDynamic(this, &UGalleonDepartureComponent::HandleDayOver);
		Expedition->OnTeamWiped.RemoveDynamic(this, &UGalleonDepartureComponent::HandleDayOver);
	}
	Super::EndPlay(EndPlayReason);
}

void UGalleonDepartureComponent::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (GameMode && GameMode->GetWorld() == GetWorld())
	{
		UGalleonCrewComponent::Ensure(NewPlayer);
	}
}

AExpeditionGameState* UGalleonDepartureComponent::GetExpedition() const
{
	return Cast<AExpeditionGameState>(GetOwner());
}

double UGalleonDepartureComponent::GetServerTime() const
{
	const AGameStateBase* GameState = Cast<AGameStateBase>(GetOwner());
	return GameState ? GameState->GetServerWorldTimeSeconds() : 0.0;
}

AGalleonActor* UGalleonDepartureComponent::GetGalleon() const
{
	if (!CachedGalleon.IsValid())
	{
		const UWorld* World = GetWorld();
		const double Now = World ? World->GetRealTimeSeconds() : 0.0;
		if (World && Now >= NextGalleonSearch)
		{
			NextGalleonSearch = Now + GalleonSearchSeconds;
			CachedGalleon = AGalleonActor::Get(this);
		}
	}
	return CachedGalleon.Get();
}

const UGalleonRules* UGalleonDepartureComponent::GetRules() const
{
	const AGalleonActor* Galleon = GetGalleon();
	return Galleon ? Galleon->Rules.Get() : nullptr;
}

const UGalleonRules& UGalleonDepartureComponent::RulesOrDefault() const
{
	const UGalleonRules* Rules = GetRules();
	return Rules ? *Rules : *GetDefault<UGalleonRules>();
}

bool UGalleonDepartureComponent::HasVoted(const APlayerState* Player) const
{
	return Player && State.Voters.ContainsByPredicate([Player](const TObjectPtr<APlayerState>& Voter) { return Voter.Get() == Player; });
}

bool UGalleonDepartureComponent::IsHelmUsable() const
{
	if (State.Stage != EGalleonDepartureStage::None)
	{
		// the key cancels it
		return true;
	}
	const AExpeditionGameState* Expedition = GetExpedition();
	return Expedition && Expedition->CanDepartFromHelm();
}

// ---------------------------------------------------------------- server: requests

void UGalleonDepartureComponent::HandleHelmKey(APlayerController* Player, bool bPressed)
{
	if (!GetOwner()->HasAuthority() || !Player)
	{
		return;
	}
	APlayerState* PlayerState = Player->PlayerState;
	if (!bPressed)
	{
		if (PlayerState && Hold.Holder == PlayerState)
		{
			ClearHold(TEXT("the key was released"));
			UpdateCheckTimer();
		}
		return;
	}

	// checked again whatever the client believes (cahier §9): alive, at the wheel of this galleon, the day under way
	const AGalleonActor* Galleon = GetGalleon();
	const UGalleonRules* Rules = GetRules();
	AExpeditionGameState* Expedition = GetExpedition();
	APawn* Pawn = Player->GetPawn();
	if (!PlayerState || !Galleon || !Rules || !Expedition || Expedition->IsExpeditionOver() || !IsLivingPawn(Pawn) || !Galleon->IsAtHelm(Pawn))
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Departure: %s pressed the wheel's key dead, away from the wheel or after the day: refused"), *GetNameSafe(Pawn));
		return;
	}

	const FGalleonDepartureState Previous = State;
	if (State.Stage != EGalleonDepartureStage::None)
	{
		// at the wheel during a vote or a notice: the key cancels it (cahier §24.2 step 7)
		LeaveStage(EGalleonDepartureEnd::Cancelled, PlayerState);
		CommitState(Previous);
		UpdateCheckTimer();
		return;
	}
	if (!Expedition->CanDepartFromHelm())
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Departure: %s at the wheel before the clock runs: refused"), *GetNameSafe(Pawn));
		return;
	}
	if (Hold.Holder)
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Departure: %s at the wheel while %s holds it: refused"), *GetNameSafe(Pawn), *GetNameSafe(Hold.Holder));
		return;
	}

	// a vote would be needed while the last one failed a moment ago: no hold for nothing
	const double Now = GetServerTime();
	const FGalleonCrewCount Count = CountCrew();
	CommitState(Previous);
	if (GalleonDeparture::OnHoldCompleted(Count, Now, State.NextVoteTime) == GalleonDeparture::EHoldOutcome::CoolingDown)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Departure: %s raises the anchor %.1f s before a new vote may open: refused"), *GetNameSafe(Pawn), State.NextVoteTime - Now);
		return;
	}

	Hold.Holder = PlayerState;
	Hold.StartTime = Now;
	Hold.EndTime = Now + Rules->HelmHoldSeconds;
	GetWorld()->GetTimerManager().SetTimer(HoldTimer, this, &UGalleonDepartureComponent::CompleteHold, FMath::Max(Rules->HelmHoldSeconds, UE_KINDA_SMALL_NUMBER), false);
	CommitHold();
	UpdateCheckTimer();
	UE_LOG(Logmonke_island, Log, TEXT("Departure: %s holds the wheel (%.1f s to raise the anchor; %d of %d living player(s) aboard)"),
		*GetNameSafe(Pawn), Rules->HelmHoldSeconds, Count.Aboard, Count.Alive);
}

bool UGalleonDepartureComponent::HandleVote(APlayerController* Player)
{
	if (!GetOwner()->HasAuthority() || !Player)
	{
		return false;
	}
	APlayerState* PlayerState = Player->PlayerState;
	const AExpeditionGameState* Expedition = GetExpedition();
	if (State.Stage != EGalleonDepartureStage::Vote || !PlayerState || !Expedition || Expedition->IsExpeditionOver()
		|| !IsLivingPawn(Player->GetPawn()) || HasVoted(PlayerState))
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Departure: vote of %s refused (no vote open, dead, or voted already)"), *GetNameSafe(PlayerState));
		return false;
	}
	const FGalleonDepartureState Previous = State;
	State.Voters.Add(PlayerState);
	const FGalleonCrewCount Count = CountCrew();
	UE_LOG(Logmonke_island, Log, TEXT("Departure: %s votes for the departure (%d of %d needed, %d living player(s), %d aboard)"),
		*GetNameSafe(PlayerState), Count.Yes, State.Needed, Count.Alive, Count.Aboard);
	if (GalleonDeparture::EvaluateVote(Count, RulesOrDefault().DepartureMajorityFraction, GetServerTime(), State.StageEndTime) == GalleonDeparture::EVoteVerdict::Notice)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Departure: the vote passed"));
		EnterNotice(false);
	}
	CommitState(Previous);
	UpdateCheckTimer();
	return true;
}

void UGalleonDepartureComponent::DebugStartNotice()
{
	const AExpeditionGameState* Expedition = GetExpedition();
	if (!GetOwner()->HasAuthority() || !Expedition || Expedition->IsExpeditionOver() || !GetRules())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Departure: no notice from the console (not the server, no galleon, or the day is over)"));
		return;
	}
	const FGalleonDepartureState Previous = State;
	ClearHold(TEXT("a notice from the console"));
	State.Voters.Reset();
	CountCrew();
	UE_LOG(Logmonke_island, Log, TEXT("Departure: notice started from the console (test)"));
	EnterNotice(false);
	CommitState(Previous);
	UpdateCheckTimer();
}

void UGalleonDepartureComponent::HandleDayOver()
{
	const FGalleonDepartureState Previous = State;
	ClearHold(TEXT("the day is over"));
	LeaveStage(EGalleonDepartureEnd::Overtaken);
	CommitState(Previous);
	UpdateCheckTimer();
}

// ---------------------------------------------------------------- server: stages

void UGalleonDepartureComponent::CompleteHold()
{
	APlayerState* Holder = IsValid(Hold.Holder.Get()) ? Hold.Holder.Get() : nullptr;
	const AGalleonActor* Galleon = GetGalleon();
	const AExpeditionGameState* Expedition = GetExpedition();
	const APawn* Pawn = Holder ? Holder->GetPawn() : nullptr;
	if (!Holder || !Galleon || !Expedition || !Expedition->CanDepartFromHelm() || State.Stage != EGalleonDepartureStage::None
		|| !IsLivingPawn(Pawn) || !Galleon->IsAtHelm(Pawn))
	{
		ClearHold(TEXT("the holder left the wheel or died, or the day ended"));
		UpdateCheckTimer();
		return;
	}
	Hold = FGalleonHelmHold();
	CommitHold();

	const FGalleonDepartureState Previous = State;
	const FGalleonCrewCount Count = CountCrew();
	switch (GalleonDeparture::OnHoldCompleted(Count, GetServerTime(), State.NextVoteTime))
	{
	case GalleonDeparture::EHoldOutcome::AllAboardNotice:
		UE_LOG(Logmonke_island, Log, TEXT("Departure: %s raised the anchor, every living player is aboard (%d)"), *GetNameSafe(Pawn), Count.Alive);
		State.Voters.Reset();
		State.Voters.Add(Holder);
		EnterNotice(true);
		break;
	case GalleonDeparture::EHoldOutcome::Vote:
		EnterVote(Holder);
		break;
	case GalleonDeparture::EHoldOutcome::CoolingDown:
		UE_LOG(Logmonke_island, Log, TEXT("Departure: %s raised the anchor before a new vote may open: nothing"), *GetNameSafe(Pawn));
		break;
	default:
		UE_LOG(Logmonke_island, Log, TEXT("Departure: %s raised the anchor with nobody alive aboard: nothing"), *GetNameSafe(Pawn));
		break;
	}
	CommitState(Previous);
	UpdateCheckTimer();
}

void UGalleonDepartureComponent::ClearHold(const TCHAR* Why)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
	if (!Hold.Holder)
	{
		return;
	}
	UE_LOG(Logmonke_island, Log, TEXT("Departure: %s lets go of the wheel (%s)"), *GetNameSafe(Hold.Holder), Why);
	Hold = FGalleonHelmHold();
	CommitHold();
}

void UGalleonDepartureComponent::EnterVote(APlayerState* By)
{
	const UGalleonRules& Rules = RulesOrDefault();
	const double Now = GetServerTime();
	State.Stage = EGalleonDepartureStage::Vote;
	State.bAllAboard = false;
	State.Voters.Reset();
	if (By)
	{
		State.Voters.Add(By);
	}
	State.StageStartTime = Now;
	State.StageEndTime = Now + Rules.DepartureVoteSeconds;
	const FGalleonCrewCount Count = CountCrew();
	UE_LOG(Logmonke_island, Log, TEXT("Departure: %s raised the anchor with %d of %d living player(s) aboard: vote opened, %d vote(s) needed, until %.1f s"),
		*GetNameSafe(By), Count.Aboard, Count.Alive, State.Needed, State.StageEndTime);
	// the majority may be there at once (the share of the rules, or nobody else to ask)
	if (GalleonDeparture::EvaluateVote(Count, Rules.DepartureMajorityFraction, Now, State.StageEndTime) == GalleonDeparture::EVoteVerdict::Notice)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Departure: the vote passed at once"));
		EnterNotice(false);
	}
}

void UGalleonDepartureComponent::EnterNotice(bool bAllAboard)
{
	const UGalleonRules& Rules = RulesOrDefault();
	const double Now = GetServerTime();
	const float Seconds = GalleonDeparture::NoticeSeconds(bAllAboard, Rules.DepartureAllAboardNoticeSeconds, Rules.DepartureVoteNoticeSeconds);
	State.Stage = EGalleonDepartureStage::Notice;
	State.bAllAboard = bAllAboard;
	State.StageStartTime = Now;
	State.StageEndTime = Now + Seconds;
	GetWorld()->GetTimerManager().SetTimer(NoticeTimer, this, &UGalleonDepartureComponent::Sail, FMath::Max(Seconds, UE_KINDA_SMALL_NUMBER), false);
	UE_LOG(Logmonke_island, Log, TEXT("Departure: notice of %.0f s (%s), the galleon sails at %.1f s; whoever is ashore then is left behind"),
		Seconds, bAllAboard ? TEXT("every living player aboard") : TEXT("vote of the crew"), State.StageEndTime);
}

void UGalleonDepartureComponent::LeaveStage(EGalleonDepartureEnd Why, const APlayerState* By)
{
	if (State.Stage == EGalleonDepartureStage::None)
	{
		return;
	}
	const double Now = GetServerTime();
	UE_LOG(Logmonke_island, Log, TEXT("Departure: %s ends: %s%s"), *UEnum::GetValueAsString(State.Stage), *UEnum::GetValueAsString(Why),
		By ? *FString::Printf(TEXT(" (%s at the wheel)"), *GetNameSafe(By)) : TEXT(""));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NoticeTimer);
	}
	State.Stage = EGalleonDepartureStage::None;
	State.bAllAboard = false;
	State.Voters.Reset();
	State.StageStartTime = -1.0;
	State.StageEndTime = -1.0;
	State.LastEnd = Why;
	State.LastEndTime = Now;
	// a failure delays the next vote: the bell does not ring again and again
	if (Why == EGalleonDepartureEnd::Cancelled || Why == EGalleonDepartureEnd::Lapsed || Why == EGalleonDepartureEnd::NobodyAboard)
	{
		State.NextVoteTime = Now + RulesOrDefault().DepartureVoteCooldownSeconds;
	}
}

void UGalleonDepartureComponent::Sail()
{
	AExpeditionGameState* Expedition = GetExpedition();
	if (State.Stage != EGalleonDepartureStage::Notice || !Expedition)
	{
		return;
	}
	const FGalleonDepartureState Previous = State;
	const FGalleonCrewCount Count = CountCrew();
	if (!GalleonDeparture::NoticeHolds(Count))
	{
		LeaveStage(EGalleonDepartureEnd::NobodyAboard);
		CommitState(Previous);
		UpdateCheckTimer();
		return;
	}
	UE_LOG(Logmonke_island, Log, TEXT("Departure: the notice ran out with %d of %d living player(s) aboard: the galleon sails"), Count.Aboard, Count.Alive);
	LeaveStage(EGalleonDepartureEnd::Sailed);
	CommitState(Previous);
	UpdateCheckTimer();
	// whoever is aboard sails with the cargo aboard; whoever is ashore is left behind and dies (as at the end of the deluge)
	Expedition->SailFromHelm();
}

void UGalleonDepartureComponent::Check()
{
	const AExpeditionGameState* Expedition = GetExpedition();
	const AGalleonActor* Galleon = GetGalleon();
	if (!Expedition)
	{
		return;
	}
	// the holder stays alive at the wheel, the day under way (a holder who left the game is gone with his player state)
	if (Hold.Holder)
	{
		const APawn* Pawn = IsValid(Hold.Holder.Get()) ? Hold.Holder->GetPawn() : nullptr;
		if (!Galleon || !IsLivingPawn(Pawn) || !Galleon->IsAtHelm(Pawn) || !Expedition->CanDepartFromHelm())
		{
			ClearHold(TEXT("left the wheel or died, or the day ended"));
		}
	}

	const FGalleonDepartureState Previous = State;
	if (State.Stage == EGalleonDepartureStage::Vote)
	{
		const FGalleonCrewCount Count = CountCrew();
		switch (GalleonDeparture::EvaluateVote(Count, RulesOrDefault().DepartureMajorityFraction, GetServerTime(), State.StageEndTime))
		{
		case GalleonDeparture::EVoteVerdict::Notice:
			UE_LOG(Logmonke_island, Log, TEXT("Departure: the vote passed (%d of %d living player(s))"), Count.Yes, Count.Alive);
			EnterNotice(false);
			break;
		case GalleonDeparture::EVoteVerdict::Void:
			LeaveStage(EGalleonDepartureEnd::NobodyAboard);
			break;
		case GalleonDeparture::EVoteVerdict::Lapse:
			LeaveStage(EGalleonDepartureEnd::Lapsed);
			break;
		default:
			break;
		}
	}
	else if (State.Stage == EGalleonDepartureStage::Notice && !GalleonDeparture::NoticeHolds(CountCrew()))
	{
		LeaveStage(EGalleonDepartureEnd::NobodyAboard);
	}
	CommitState(Previous);
	UpdateCheckTimer();
}

void UGalleonDepartureComponent::UpdateCheckTimer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerManager& Timers = World->GetTimerManager();
	if (!Hold.Holder && State.Stage == EGalleonDepartureStage::None)
	{
		Timers.ClearTimer(CheckTimer);
		return;
	}
	if (!Timers.IsTimerActive(CheckTimer))
	{
		Timers.SetTimer(CheckTimer, this, &UGalleonDepartureComponent::Check, FMath::Max(RulesOrDefault().DepartureCheckSeconds, 0.05f), true);
	}
}

FGalleonCrewCount UGalleonDepartureComponent::CountCrew()
{
	FGalleonCrewCount Count;
	const AGameStateBase* GameState = Cast<AGameStateBase>(GetOwner());
	const AGalleonActor* Galleon = GetGalleon();
	// the votes of the dead and of those who left no longer count (cahier §24.5: a disconnected player does not vote)
	State.Voters.RemoveAll([](const TObjectPtr<APlayerState>& Voter)
	{
		return !IsValid(Voter.Get()) || Voter->IsInactive() || !IsLivingPawn(Voter->GetPawn());
	});
	if (GameState)
	{
		for (APlayerState* Player : GameState->PlayerArray)
		{
			// the electorate: the living players; the dead, the spectators and the players still loading are out
			if (!Player || Player->IsOnlyASpectator() || Player->IsInactive())
			{
				continue;
			}
			const APawn* Pawn = Player->GetPawn();
			if (!IsLivingPawn(Pawn))
			{
				continue;
			}
			++Count.Alive;
			if (Galleon && Galleon->IsActorAboard(Pawn))
			{
				++Count.Aboard;
			}
			if (HasVoted(Player))
			{
				++Count.Yes;
			}
		}
	}
	State.Alive = ToByte(Count.Alive);
	State.Aboard = ToByte(Count.Aboard);
	State.Needed = ToByte(GalleonDeparture::VotesNeeded(Count.Alive, RulesOrDefault().DepartureMajorityFraction));
	return Count;
}

void UGalleonDepartureComponent::CommitState(const FGalleonDepartureState& PreviousState)
{
	if (IsSameState(PreviousState, State))
	{
		return;
	}
	GetOwner()->ForceNetUpdate();
	// a listen server is a player too: the same presentation as the clients' OnRep
	OnRep_State(PreviousState);
}

void UGalleonDepartureComponent::CommitHold()
{
	GetOwner()->ForceNetUpdate();
	OnRep_Hold();
}

// ---------------------------------------------------------------- every machine

void UGalleonDepartureComponent::OnRep_State(const FGalleonDepartureState& PreviousState)
{
	Present(PreviousState);
	OnDepartureChanged.Broadcast();
}

void UGalleonDepartureComponent::OnRep_Hold()
{
	OnDepartureChanged.Broadcast();
}

void UGalleonDepartureComponent::Present(const FGalleonDepartureState& PreviousState)
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	AGalleonActor* Galleon = GetGalleon();
	const UGalleonRules& Rules = RulesOrDefault();
	const bool bNewStage = State.Stage != PreviousState.Stage || State.StageStartTime != PreviousState.StageStartTime;
	// the whole crew hears it, ashore too (cahier §24.2: cloche, corne de brume); not a stage long under way (a player
	// arriving during a notice)
	if (bNewStage && Galleon && State.Stage != EGalleonDepartureStage::None && GetServerTime() - State.StageStartTime <= Rules.DepartureSoundGraceSeconds)
	{
		Galleon->PlayDepartureCue(EGalleonDepartureCue::Bell);
		if (State.Stage == EGalleonDepartureStage::Notice)
		{
			Galleon->PlayDepartureCue(EGalleonDepartureCue::Horn);
		}
	}
	FTimerManager& Timers = World->GetTimerManager();
	if (State.Stage == EGalleonDepartureStage::Notice)
	{
		if (bNewStage || !Timers.IsTimerActive(TickTimer))
		{
			// the second under way does not tick: the bell and the foghorn just played
			LastTickSecond = GalleonDeparture::CountdownSecond(State.StageEndTime - GetServerTime(), Rules.DepartureTickFromSeconds);
			Timers.SetTimer(TickTimer, this, &UGalleonDepartureComponent::TickNotice, NoticeTickSampleSeconds, true);
		}
	}
	else
	{
		Timers.ClearTimer(TickTimer);
	}
}

void UGalleonDepartureComponent::TickNotice()
{
	if (State.Stage != EGalleonDepartureStage::Notice)
	{
		GetWorld()->GetTimerManager().ClearTimer(TickTimer);
		return;
	}
	const int32 Second = GalleonDeparture::CountdownSecond(State.StageEndTime - GetServerTime(), RulesOrDefault().DepartureTickFromSeconds);
	if (Second > 0 && Second != LastTickSecond)
	{
		LastTickSecond = Second;
		if (const AGalleonActor* Galleon = GetGalleon())
		{
			Galleon->PlayDepartureCue(EGalleonDepartureCue::Tick);
		}
	}
}

FString UGalleonDepartureComponent::Describe() const
{
	const double Now = GetServerTime();
	TArray<FString> Names;
	for (const TObjectPtr<APlayerState>& Voter : State.Voters)
	{
		Names.Add(GetNameSafe(Voter.Get()));
	}
	return FString::Printf(TEXT("%s%s, votes [%s] %d of %d needed, %d alive, %d aboard, %.1f s left; hold: %s (%.1f s left); last end %s %.1f s ago; next vote in %.1f s"),
		*UEnum::GetValueAsString(State.Stage), State.bAllAboard ? TEXT(" (all aboard)") : TEXT(""), *FString::Join(Names, TEXT(", ")),
		State.Voters.Num(), State.Needed, State.Alive, State.Aboard, State.StageEndTime >= 0.0 ? State.StageEndTime - Now : 0.0,
		*GetNameSafe(Hold.Holder.Get()), Hold.Holder ? Hold.EndTime - Now : 0.0, *UEnum::GetValueAsString(State.LastEnd),
		State.LastEndTime >= 0.0 ? Now - State.LastEndTime : 0.0, FMath::Max(0.0, State.NextVoteTime - Now));
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GEndDayAtHelmCommand(
	TEXT("monke.EndDayAtHelm"),
	TEXT("Starts the notice of a departure vote now (server, tests): DepartureVoteNoticeSeconds, cancellable at the wheel; whoever is ashore at its end is left behind and dies"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UGalleonDepartureComponent* Departure = UGalleonDepartureComponent::Get(World))
		{
			Departure->DebugStartNotice();
		}
	}));

static FAutoConsoleCommandWithWorld GGalleonVoteCommand(
	TEXT("monke.Galleon.Vote"),
	TEXT("The local player votes for the departure under way, as the vote key does (any machine, tests)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UGalleonCrewComponent* Crew = UGalleonCrewComponent::GetLocal(World))
		{
			Crew->RequestVote();
		}
	}));

static FAutoConsoleCommandWithWorld GGalleonRaiseAnchorCommand(
	TEXT("monke.Galleon.RaiseAnchor"),
	TEXT("The local player holds the wheel's key a little longer than HelmHoldSeconds (any machine, tests without keyboard; standing at the wheel); pressed during a vote or a notice it cancels it"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UGalleonCrewComponent* Crew = UGalleonCrewComponent::GetLocal(World))
		{
			Crew->DebugHoldHelm();
		}
	}));

static FAutoConsoleCommandWithWorld GGalleonStatusCommand(
	TEXT("monke.Galleon.Status"),
	TEXT("Logs the departure from the wheel and the wait for the crew (stage, votes, counts, times)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		const AExpeditionGameState* Expedition = AExpeditionGameState::Get(World);
		const UGalleonDepartureComponent* Departure = Expedition ? Expedition->GetDeparture() : nullptr;
		if (!Departure)
		{
			UE_LOG(Logmonke_island, Display, TEXT("Departure: no expedition game state in this world"));
			return;
		}
		UE_LOG(Logmonke_island, Display, TEXT("Departure: %s"), *Departure->Describe());
		UE_LOG(Logmonke_island, Display, TEXT("Expedition: clock %s, crew %d / %d loaded%s"), Expedition->GetClockState().bStarted ? TEXT("running") : TEXT("stopped"),
			Expedition->GetCrewLoaded(), Expedition->GetCrewExpected(), Expedition->IsAwaitingCrew() ? TEXT(" (waiting)") : TEXT(""));
	}));
#endif
