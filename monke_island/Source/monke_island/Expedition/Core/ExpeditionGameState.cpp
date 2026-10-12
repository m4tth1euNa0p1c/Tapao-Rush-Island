// monke_island

#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionDayEnd.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Execution/ExecutionScene.h"
#include "Boats/BoatStartSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonDepartureComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunStateComponent.h"
#include "Run/RunSubsystem.h"
#include "TimerManager.h"
#include "monke_island.h"

namespace
{
	/** The wait for the crew is checked this often (s): a server poll of a few controllers, no gameplay value */
	constexpr float CrewLoadPollSeconds = 0.25f;

	/** The waits of the end of a day (T-173): DA_ExpeditionRules for the travel, DA_RunRules for the greed question */
	FDayEndDelays DayEndDelaysOf(const UExpeditionRules* Rules, const URunSubsystem* Run)
	{
		FDayEndDelays Delays;
		Delays.SurvivedTravelDelay = Rules ? Rules->DayEndTravelDelay : 0.0f;
		Delays.LostTravelDelay = Rules ? Rules->GameOverRestartDelay : 0.0f;
		Delays.GreedChoiceSeconds = Run ? Run->GetGreedChoiceSeconds() : 0.0f;
		return Delays;
	}
}

AExpeditionGameState::AExpeditionGameState()
{
	PrimaryActorTick.bCanEverTick = false;
	RunState = CreateDefaultSubobject<URunStateComponent>(TEXT("RunState"));
	Departure = CreateDefaultSubobject<UGalleonDepartureComponent>(TEXT("Departure"));
}

AExpeditionGameState* AExpeditionGameState::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetGameState<AExpeditionGameState>() : nullptr;
}

void AExpeditionGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AExpeditionGameState, ExpeditionId);
	DOREPLIFETIME(AExpeditionGameState, FragmentId);
	DOREPLIFETIME(AExpeditionGameState, ClockState);
	DOREPLIFETIME(AExpeditionGameState, Phase);
	DOREPLIFETIME(AExpeditionGameState, Resolution);
	DOREPLIFETIME(AExpeditionGameState, TeamWipedTime);
	DOREPLIFETIME(AExpeditionGameState, DayEndedTime);
	DOREPLIFETIME(AExpeditionGameState, DayEndTravelWait);
	DOREPLIFETIME(AExpeditionGameState, bExecution);
	DOREPLIFETIME(AExpeditionGameState, CrewWaitEndTime);
	DOREPLIFETIME(AExpeditionGameState, CrewLoaded);
	DOREPLIFETIME(AExpeditionGameState, CrewExpected);
}

void AExpeditionGameState::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// The server says which fragment this map plays; the clients receive the id with the game state
		if (const UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this))
		{
			FragmentId = Islands->ResolveFragmentIdForWorld(GetWorld());
			// T-141: the Start anchorage of the definition named by ?Island=, before the boat start resolves (next tick)
			Islands->InjectStartAnchorage(GetWorld());
		}
		if (!FragmentId.IsNone())
		{
			UE_LOG(Logmonke_island, Log, TEXT("Expedition: playing fragment %s"), *FragmentId.ToString());
		}
		// T-141: the boat start may pick another island (map with several fragments, no ?Island=): it has the last word
		if (UBoatStartSubsystem* BoatStart = UBoatStartSubsystem::Get(this))
		{
			ArrivalResolvedHandle = BoatStart->CallOrRegisterOnArrivalResolved(
				FOnBoatArrivalResolved::FDelegate::CreateUObject(this, &AExpeditionGameState::HandleArrivalResolved));
		}
	}

	if (HasAuthority())
	{
		// Players may spawn before or after this actor begins play: watch both
		ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AExpeditionGameState::WatchPlayerCharacter));
		for (TActorIterator<AIslanderCharacter> It(GetWorld()); It; ++It)
		{
			WatchPlayerCharacter(*It);
		}
		LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(this, &AExpeditionGameState::HandleLogout);
	}

	if (HasAuthority())
	{
		// ADR-007: a new island is a new day of the run kept by the game instance
		if (URunSubsystem* Run = URunSubsystem::Get(this))
		{
			Run->BeginDay(GetWorld());
		}
	}

	if (HasAuthority() && bStartOnBeginPlay)
	{
		StartExpedition();
	}
	else if (HasAuthority() && bStartWhenCrewLoaded)
	{
		// network review T-268 R-02 (T-292): the clock waits for the players of the previous map to load this one
		if (UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this))
		{
			ExpectedCrew = Islands->TakeTravelPlayerCount();
		}
		CrewWaitStartTime = GetWorld()->GetTimeSeconds();
		TryStartWhenCrewLoaded();
		if (!ClockState.bStarted)
		{
			UE_LOG(Logmonke_island, Log, TEXT("Expedition: the clock waits for the crew (%d player(s) expected, %.0f s at most)"),
				ExpectedCrew, Rules ? Rules->CrewLoadTimeoutSeconds : 0.0f);
			GetWorldTimerManager().SetTimer(CrewLoadTimer, this, &AExpeditionGameState::TryStartWhenCrewLoaded, CrewLoadPollSeconds, true);
		}
	}
}

void AExpeditionGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PhaseTimer);
		World->GetTimerManager().ClearTimer(RestartTimer);
		World->GetTimerManager().ClearTimer(DayEndTimer);
		World->GetTimerManager().ClearTimer(CrewLoadTimer);
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}

	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);
	if (UBoatStartSubsystem* BoatStart = UBoatStartSubsystem::Get(this))
	{
		BoatStart->OnArrivalResolved.Remove(ArrivalResolvedHandle);
	}
	ArrivalResolvedHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void AExpeditionGameState::HandleArrivalResolved(FName ArrivalFragmentId, const FTransform& BoatTransform)
{
	if (!HasAuthority() || ArrivalFragmentId.IsNone() || ArrivalFragmentId == FragmentId)
	{
		return;
	}
	UE_LOG(Logmonke_island, Log, TEXT("Expedition: the boat starts at %s, the fragment played is %s (was %s)"),
		*BoatTransform.GetLocation().ToCompactString(), *ArrivalFragmentId.ToString(), *FragmentId.ToString());
	FragmentId = ArrivalFragmentId;
	ForceNetUpdate();
}

FExpeditionClockParams AExpeditionGameState::GetParams() const
{
	return Rules ? FExpeditionClockParams::FromRules(*Rules) : FExpeditionClockParams();
}

bool AExpeditionGameState::StartExpedition()
{
	if (!HasAuthority())
	{
		return false;
	}
	if (ClockState.bStarted)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: already started, start ignored"), *ExpeditionId.ToString());
		return false;
	}
	if (!ensureMsgf(Rules, TEXT("%s has no ExpeditionRules (DA_ExpeditionRules): the expedition clock cannot start"), *GetNameSafe(this)))
	{
		return false;
	}

	FExpeditionClockParams Params = GetParams();
	// ADR-007 §11: the further the zone, the earlier the deluge (clients read the replicated deadline)
	if (const URunSubsystem* Run = URunSubsystem::Get(this))
	{
		Params.InitialFloodDelay *= Run->GetFloodDelayScale();
	}
	const double Now = GetServerWorldTimeSeconds();
	const double PreviousFloodStartTime = ClockState.FloodStartTime;

	ExpeditionId = FGuid::NewGuid();
	ClockState = ExpeditionClock::Start(Now, Params);
	Resolution = EExpeditionResolution::None;
	// the wait for the crew is over, whoever started the clock
	GetWorldTimerManager().ClearTimer(CrewLoadTimer);
	CrewWaitEndTime = -1.0;

	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: started at %.2f s, flood at %.2f s"),
		*ExpeditionId.ToString(), ClockState.StartTime, ClockState.FloodStartTime);

	OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousFloodStartTime);
	RefreshPhase();
	return true;
}

bool AExpeditionGameState::ShouldStartClock(bool bSolo, int32 Loaded, int32 Loading, int32 Expected, double Waited, double JoinGrace, double Timeout)
{
	if (bSolo || Waited >= Timeout)
	{
		return true;
	}
	if (Loaded <= 0 || Loading > 0)
	{
		return false;
	}
	return Expected > 0 ? Loaded >= Expected : Waited >= JoinGrace;
}

void AExpeditionGameState::CountCrewLoad(int32& OutLoaded, int32& OutLoading) const
{
	OutLoaded = 0;
	OutLoading = 0;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller || !Controller->PlayerState || Controller->PlayerState->IsOnlyASpectator())
		{
			continue;
		}
		// a client has loaded this map once its connection says so (the host and a solo player always have)
		if (Controller->GetPawn() && Controller->HasClientLoadedCurrentWorld())
		{
			++OutLoaded;
		}
		else
		{
			++OutLoading;
		}
	}
	// clients connected to this map but not logged in yet: loading it after a non seamless travel
	if (const UNetDriver* Driver = World->GetNetDriver())
	{
		for (const UNetConnection* Connection : Driver->ClientConnections)
		{
			if (Connection && Connection->GetConnectionState() == USOCK_Open && !Connection->PlayerController)
			{
				++OutLoading;
			}
		}
	}
}

void AExpeditionGameState::TryStartWhenCrewLoaded()
{
	if (!HasAuthority() || ClockState.bStarted || IsExpeditionOver())
	{
		GetWorldTimerManager().ClearTimer(CrewLoadTimer);
		CrewWaitEndTime = -1.0;
		return;
	}
	int32 Loaded = 0;
	int32 Loading = 0;
	CountCrewLoad(Loaded, Loading);
	const double Waited = GetWorld()->GetTimeSeconds() - CrewWaitStartTime;
	const double Timeout = Rules ? Rules->CrewLoadTimeoutSeconds : 0.0;
	const double JoinGrace = Rules ? Rules->CrewJoinGraceSeconds : 0.0;
	const bool bSolo = GetNetMode() == NM_Standalone;
	CrewLoaded = static_cast<uint8>(FMath::Clamp(Loaded, 0, 255));
	CrewExpected = static_cast<uint8>(FMath::Clamp(FMath::Max(ExpectedCrew, Loaded + Loading), 0, 255));
	if (!ShouldStartClock(bSolo, Loaded, Loading, ExpectedCrew, Waited, JoinGrace, Timeout))
	{
		// the HUD tells the crew who it waits for and until when
		CrewWaitEndTime = GetServerWorldTimeSeconds() + FMath::Max(0.0, Timeout - Waited);
		return;
	}
	GetWorldTimerManager().ClearTimer(CrewLoadTimer);
	CrewWaitEndTime = -1.0;
	if (!bSolo && (Loading > 0 || Loaded < ExpectedCrew))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition: %d of %d player(s) loaded after %.0f s (%d still on their way): the clock starts without them, they will be put on the deck"),
			Loaded, FMath::Max(ExpectedCrew, Loaded + Loading), Waited, Loading);
	}
	else
	{
		UE_LOG(Logmonke_island, Log, TEXT("Expedition: the crew is in (%d player(s) loaded after %.1f s): the clock starts"), Loaded, Waited);
	}
	StartExpedition();
}

EZoneBonusResult AExpeditionGameState::AddZoneBonus(FName ZoneId)
{
	if (!HasAuthority())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: bonus of zone '%s' requested without authority (%s), ignored"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), *GetNameSafe(this));
		return EZoneBonusResult::NoAuthority;
	}

	// The rule "strictly before the deadline" uses the server clock, never a time given by the caller
	const double ServerTime = GetServerWorldTimeSeconds();

	// The flood may be due before its timer fired this frame: settle the phase first, it is irreversible
	RefreshPhase();

	const double PreviousFloodStartTime = ClockState.FloodStartTime;
	EZoneBonusResult Result = EZoneBonusResult::TooLate;
	if (Phase == EExpeditionPhase::Submerging || Phase == EExpeditionPhase::Resolved)
	{
		// The submersion started: whatever time the caller passes, it never moves the deadline back
		Result = HasZoneBonus(ZoneId) ? EZoneBonusResult::AlreadyGranted : EZoneBonusResult::TooLate;
	}
	else
	{
		Result = ExpeditionClock::TryAddZoneBonus(ClockState, ZoneId, ServerTime, GetParams());
	}

	switch (Result)
	{
	case EZoneBonusResult::Granted:
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: zone %s pacified at %.2f s, flood moved from %.2f s to %.2f s"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), ServerTime, PreviousFloodStartTime, ClockState.FloodStartTime);
		OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousFloodStartTime);
		RefreshPhase();
		break;

	case EZoneBonusResult::AlreadyGranted:
		UE_LOG(Logmonke_island, Verbose, TEXT("Expedition %s: zone %s already gave its bonus"), *ExpeditionId.ToString(), *ZoneId.ToString());
		break;

	case EZoneBonusResult::TooLate:
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: zone %s pacified at %.2f s, after the flood at %.2f s: no bonus"),
			*ExpeditionId.ToString(), *ZoneId.ToString(), ServerTime, ClockState.FloodStartTime);
		break;

	default:
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition %s: bonus of zone '%s' refused (%s)"), *ExpeditionId.ToString(), *ZoneId.ToString(),
			*UEnum::GetValueAsString(Result));
		break;
	}
	return Result;
}

double AExpeditionGameState::GetWaterRise(double ServerTime) const
{
	// Client: the replicated deadline may still move before the submersion; only the server clock decides it started
	if (!HasAuthority() && Phase < EExpeditionPhase::Submerging)
	{
		return 0.0;
	}
	return ExpeditionClock::WaterRiseCm(ClockState, ServerTime, GetParams());
}

double AExpeditionGameState::GetCurrentWaterRise() const
{
	return GetWaterRise(GetServerWorldTimeSeconds());
}

double AExpeditionGameState::GetTimeBeforeSubmersion() const
{
	return ExpeditionClock::TimeBeforeSubmersion(ClockState, GetServerWorldTimeSeconds());
}

double AExpeditionGameState::GetSubmersionEndTime() const
{
	return ExpeditionClock::SubmersionEndTime(ClockState, GetParams());
}

int32 AExpeditionGameState::GetRemainingZoneCount() const
{
	const int32 ZonesCount = GetParams().ZonesCount;
	return ZonesCount > 0 ? FMath::Max(0, ZonesCount - ClockState.BonusedZoneIds.Num()) : -1;
}

void AExpeditionGameState::RefreshPhase()
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World || Phase == EExpeditionPhase::Resolved)
	{
		return;
	}

	const FExpeditionClockParams Params = GetParams();
	const double Now = GetServerWorldTimeSeconds();
	const EExpeditionPhase Target = ExpeditionClock::PhaseAt(ClockState, Now, Params);

	// Phases only move forward, one step at a time so every server listener sees each of them
	// (clients only receive the latest replicated phase and may skip steps)
	while (Phase < Target)
	{
		SetPhase(static_cast<EExpeditionPhase>(static_cast<uint8>(Phase) + 1));
	}

	FTimerManager& Timers = World->GetTimerManager();
	Timers.ClearTimer(PhaseTimer);

	double NextTransition = -1.0;
	if (Phase == EExpeditionPhase::Exploring)
	{
		NextTransition = ClockState.FloodStartTime;
	}
	else if (Phase == EExpeditionPhase::Submerging)
	{
		NextTransition = ExpeditionClock::SubmersionEndTime(ClockState, Params);
	}

	if (NextTransition >= 0.0)
	{
		// A timer of rate 0 is cleared: keep a tiny delay, RefreshPhase reschedules if it fires a hair early
		const float Delay = static_cast<float>(FMath::Max(NextTransition - Now, static_cast<double>(UE_KINDA_SMALL_NUMBER)));
		Timers.SetTimer(PhaseTimer, this, &AExpeditionGameState::RefreshPhase, Delay, false);
	}
}

void AExpeditionGameState::SetPhase(EExpeditionPhase NewPhase)
{
	if (NewPhase == Phase)
	{
		return;
	}

	const EExpeditionPhase PreviousPhase = Phase;
	if (NewPhase == EExpeditionPhase::Resolved)
	{
		Resolution = EExpeditionResolution::Submerged;
	}
	Phase = NewPhase;

	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: phase %s -> %s at %.2f s"), *ExpeditionId.ToString(),
		*UEnum::GetValueAsString(PreviousPhase), *UEnum::GetValueAsString(NewPhase), GetServerWorldTimeSeconds());

	OnPhaseChanged.Broadcast(Phase, PreviousPhase);
	if (Phase == EExpeditionPhase::Resolved && Resolution == EExpeditionResolution::Submerged)
	{
		OnSubmersionEnded.Broadcast();
		ResolveDelugeEnd();
	}
}

void AExpeditionGameState::OnRep_ClockState(const FExpeditionClockState& PreviousClockState)
{
	if (ClockState.FloodStartTime != PreviousClockState.FloodStartTime)
	{
		OnFloodDeadlineChanged.Broadcast(ClockState.FloodStartTime, PreviousClockState.FloodStartTime);
	}
}

void AExpeditionGameState::OnRep_Phase(EExpeditionPhase PreviousPhase)
{
	OnPhaseChanged.Broadcast(Phase, PreviousPhase);
	if (Phase == EExpeditionPhase::Resolved && Resolution == EExpeditionResolution::Submerged)
	{
		OnSubmersionEnded.Broadcast();
	}
}

double AExpeditionGameState::GetRestartTime() const
{
	// the day lost (T-173): the travel to the next island, none when the campaign is lost with it
	return IsTeamWiped() ? GetNextTravelTime() : -1.0;
}

void AExpeditionGameState::WatchPlayerCharacter(AActor* Actor)
{
	AIslanderCharacter* Character = Cast<AIslanderCharacter>(Actor);
	if (!Character || !Rules || !Rules->bPermadeath)
	{
		return;
	}
	// AIslanderCharacter::RespawnDelay: 0 disables respawning. Set through reflection (the member is protected):
	// to be replaced by a call on the character once its file is free.
	if (const FFloatProperty* Delay = FindFProperty<FFloatProperty>(AIslanderCharacter::StaticClass(), TEXT("RespawnDelay")))
	{
		Delay->SetPropertyValue_InContainer(Character, 0.0f);
	}
	else
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Expedition: AIslanderCharacter has no RespawnDelay, %s still respawns"), *GetNameSafe(Character));
	}
	if (UIslanderHealthComponent* Health = Character->GetHealthComponent())
	{
		Health->OnDeath.AddUniqueDynamic(this, &AExpeditionGameState::HandlePlayerDeath);
	}
}

void AExpeditionGameState::HandlePlayerDeath(AActor* Killer)
{
	RecordDeadPlayers();
	CheckTeamWiped();
}

void AExpeditionGameState::HandleLogout(AGameModeBase* GameMode, AController* Exiting)
{
	if (!GameMode || GameMode->GetWorld() != GetWorld())
	{
		return;
	}
	// the player state of the leaver is removed after this event: count the team on the next frame
	GetWorldTimerManager().SetTimerForNextTick(this, &AExpeditionGameState::CheckTeamWiped);
}

void AExpeditionGameState::RecordDeadPlayers()
{
	for (const APlayerState* PlayerState : PlayerArray)
	{
		const AIslanderCharacter* Character = PlayerState ? Cast<AIslanderCharacter>(PlayerState->GetPawn()) : nullptr;
		const UIslanderHealthComponent* Health = Character ? Character->GetHealthComponent() : nullptr;
		if (Health && Health->IsDead())
		{
			DeadPlayers.Add(PlayerState);
		}
	}
}

void AExpeditionGameState::CountPlayers(int32& OutDead, int32& OutAlive, int32& OutPending)
{
	OutDead = 0;
	OutAlive = 0;
	OutPending = 0;
	RecordDeadPlayers();
	for (const APlayerState* PlayerState : PlayerArray)
	{
		if (!PlayerState || PlayerState->IsOnlyASpectator() || PlayerState->IsInactive())
		{
			continue;
		}
		if (DeadPlayers.Contains(PlayerState))
		{
			++OutDead;
		}
		else if (Cast<AIslanderCharacter>(PlayerState->GetPawn()))
		{
			++OutAlive;
		}
		else
		{
			// logged in, still loading or not spawned yet: alive until proven otherwise
			++OutPending;
		}
	}
}

void AExpeditionGameState::CheckTeamWiped()
{
	if (!HasAuthority() || IsTeamWiped() || !Rules || !Rules->bPermadeath)
	{
		return;
	}
	if (IsDaySurvived())
	{
		// the day already ended with survivors: the travel is on its way
		return;
	}
	int32 Dead = 0;
	int32 Alive = 0;
	int32 Pending = 0;
	CountPlayers(Dead, Alive, Pending);
	if (!IsWipe(Dead, Alive, Pending))
	{
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: %d player(s) dead, %d alive, %d still loading"), *ExpeditionId.ToString(), Dead, Alive, Pending);
		return;
	}

	GetWorldTimerManager().ClearTimer(DayEndTimer);
	TeamWipedTime = GetServerWorldTimeSeconds();
	// ADR-007 §14 (T-173): the whole crew dead loses the day, not the game. The run counts it (nothing brought back, the
	// loot left on the island, the day spent, a fee per death) and the galleon sails on to the next island; only a
	// contract failed on its milestone day ends the campaign (execution rule: nobody is left to execute, no scene)
	URunSubsystem* Run = URunSubsystem::Get(this);
	const FRunDayReport Report = Run ? Run->EndDay(GetWorld(), 0, Dead, true) : FRunDayReport();
	const FDayEndPlan Plan = ExpeditionDayEnd::Plan(true, Report.Verdict, DayEndDelaysOf(Rules, Run));
	DayEndTravelWait = Plan.TravelWait;
	bExecution = Plan.IsCampaignLost();
	if (bExecution)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: every player is dead at %.2f s on the last day of the contract, quota missed: the campaign is lost (nobody left to execute, no travel)"),
			*ExpeditionId.ToString(), TeamWipedTime);
	}
	else
	{
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: every player is dead at %.2f s: the day is lost, next island in %.1f s"),
			*ExpeditionId.ToString(), TeamWipedTime, Plan.TravelWait);
	}
	ForceNetUpdate();
	OnTeamWiped.Broadcast();
	if (!bExecution)
	{
		GetWorldTimerManager().SetTimer(RestartTimer, this, &AExpeditionGameState::RestartFromBoot,
			FMath::Max(Plan.TravelWait, UE_KINDA_SMALL_NUMBER), false);
	}
}

void AExpeditionGameState::ResolveDelugeEnd()
{
	if (!HasAuthority() || !Rules || IsExpeditionOver())
	{
		return;
	}
	TArray<const AGalleonActor*, TInlineAllocator<2>> Galleons;
	for (TActorIterator<AGalleonActor> It(GetWorld()); It; ++It)
	{
		Galleons.Add(*It);
	}
	if (!Rules->bGalleonSavesAtDelugeEnd || Galleons.IsEmpty())
	{
		// no ship to come back to: the sea keeps rising (T-101), the day ends once it reached its highest level
		const float Delay = Rules->PostSubmersionRiseCmPerSecond > 0.0f ? Rules->PostSubmersionMaxRiseCm / Rules->PostSubmersionRiseCmPerSecond : 0.0f;
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: end of the submersion without galleon, the day ends in %.0f s"), *ExpeditionId.ToString(), Delay);
		GetWorldTimerManager().SetTimer(DayEndTimer, this, &AExpeditionGameState::EndDay, FMath::Max(Delay, UE_KINDA_SMALL_NUMBER), false);
		return;
	}

	// ADR-006: whoever is not aboard at the end of the deluge dies. The array is copied: a death may travel the world.
	const TArray<TObjectPtr<APlayerState>> Players = PlayerArray;
	for (const APlayerState* PlayerState : Players)
	{
		AIslanderCharacter* Character = PlayerState ? Cast<AIslanderCharacter>(PlayerState->GetPawn()) : nullptr;
		UIslanderHealthComponent* Health = Character ? Character->GetHealthComponent() : nullptr;
		if (!Health || Health->IsDead())
		{
			continue;
		}
		const bool bAboard = Galleons.ContainsByPredicate([Character](const AGalleonActor* Galleon) { return Galleon->IsActorAboard(Character); });
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: end of the deluge, %s is %s"), *ExpeditionId.ToString(), *GetNameSafe(Character),
			bAboard ? TEXT("aboard the galleon: survives") : TEXT("not aboard the galleon: dies"));
		if (!bAboard)
		{
			// "Emporté par le déluge" on the death screen (T-135)
			Health->ApplyHealthDamage(1.0e6f, this, EDeathCause::Flood);
		}
	}
	EndDay();
}

void AExpeditionGameState::EndDay()
{
	if (!HasAuthority() || !Rules || IsExpeditionOver())
	{
		return;
	}
	int32 Dead = 0;
	int32 Alive = 0;
	int32 Pending = 0;
	CountPlayers(Dead, Alive, Pending);
	if (Alive + Pending <= 0)
	{
		// nobody left to carry on: a wipe when somebody died, an empty server otherwise
		CheckTeamWiped();
		return;
	}

	DayEndedTime = GetServerWorldTimeSeconds();
	// ADR-007: what is aboard the galleon is what the day brought back; the greed question gives the crew more time; the
	// quota missed on the last day of the contract (owner 2026-10-07): the crew is put on a plank and shot (T-173: the
	// same pure plan as a lost day, ExpeditionDayEnd)
	URunSubsystem* Run = URunSubsystem::Get(this);
	const FRunDayReport Report = Run ? Run->EndDay(GetWorld(), ComputeDayValue(), Dead, false) : FRunDayReport();
	const FDayEndPlan Plan = ExpeditionDayEnd::Plan(false, Report.Verdict, DayEndDelaysOf(Rules, Run));
	DayEndTravelWait = Plan.TravelWait;
	bExecution = Plan.FollowUp == EDayEndFollowUp::Execution;
	const FString FollowUp = bExecution ? FString(TEXT("contract failed: execution of the crew"))
		: FString::Printf(TEXT("next island in %.1f s"), Plan.TravelWait);
	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: the day ends at %.2f s with %d survivor(s) and %d dead, %s"),
		*ExpeditionId.ToString(), DayEndedTime, Alive + Pending, Dead, *FollowUp);
	OnDaySurvived.Broadcast();
	if (bExecution)
	{
		BeginExecution();
		return;
	}
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AExpeditionGameState::RestartFromBoot,
		FMath::Max(DayEndTravelWait, UE_KINDA_SMALL_NUMBER), false);
}

bool AExpeditionGameState::CanDepartFromHelm() const
{
	// the clock runs: not while the crew is still loading the island (T-292), not once the day is over
	return Rules && ClockState.bStarted && (Phase == EExpeditionPhase::Exploring || Phase == EExpeditionPhase::Submerging) && !IsExpeditionOver();
}

bool AExpeditionGameState::SailFromHelm()
{
	if (!HasAuthority() || !Rules || IsExpeditionOver())
	{
		return false;
	}
	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: the galleon sails from the wheel at %.2f s (whoever is ashore is left behind)"),
		*ExpeditionId.ToString(), GetServerWorldTimeSeconds());
	ResolveDelugeEnd();
	return IsExpeditionOver();
}

void AExpeditionGameState::BeginExecution()
{
	// no travel: the end screen of the lost campaign follows the scene (UTapaoUISubsystem), the host starts again
	GetWorldTimerManager().ClearTimer(RestartTimer);
	ForceNetUpdate();
	AExecutionScene::Begin(GetWorld(), AGalleonActor::Get(this));
}

void AExpeditionGameState::DebugExecute()
{
	if (!HasAuthority() || IsExpeditionOver())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(DayEndTimer);
	DayEndedTime = GetServerWorldTimeSeconds();
	DayEndTravelWait = 0.0f;
	bExecution = true;
	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: execution of the crew (test)"), *ExpeditionId.ToString());
	OnDaySurvived.Broadcast();
	BeginExecution();
}

#if !UE_BUILD_SHIPPING
// monke.EndDayAtHelm (the notice of a departure from the wheel) lives with the departure: Galleon/GalleonDepartureComponent.cpp
static FAutoConsoleCommandWithWorld GExecutionCommand(
	TEXT("monke.Execution.Start"),
	TEXT("Ends the day now with the execution of the crew (server, tests): plank, French squad, end screen"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (AExpeditionGameState* GameState = AExpeditionGameState::Get(World))
		{
			GameState->DebugExecute();
		}
	}));

static FAutoConsoleCommandWithWorld GEndDelugeCommand(
	TEXT("monke.EndDeluge"),
	TEXT("Ends the deluge now (server, tests): whoever is aboard the galleon survives the day, the others die"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (AExpeditionGameState* GameState = AExpeditionGameState::Get(World))
		{
			GameState->DebugEndDeluge();
		}
	}));
#endif

void AExpeditionGameState::HurryTravel(float Seconds)
{
	// the day over, survived or lost (T-173: the host sends the galleon on after a lost day too), a travel to come
	if (!HasAuthority() || !IsExpeditionOver() || bExecution)
	{
		return;
	}
	FTimerManager& Timers = GetWorldTimerManager();
	const float Left = Timers.GetTimerRemaining(RestartTimer);
	if (Left > Seconds)
	{
		const float Wait = FMath::Max(Seconds, UE_KINDA_SMALL_NUMBER);
		Timers.SetTimer(RestartTimer, this, &AExpeditionGameState::RestartFromBoot, Wait, false);
		const double DayOverTime = IsTeamWiped() ? TeamWipedTime : DayEndedTime;
		DayEndTravelWait = static_cast<float>(GetServerWorldTimeSeconds() - DayOverTime) + Wait;
		ForceNetUpdate();
		UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: the travel to the next island is hurried, it leaves in %.1f s"), *ExpeditionId.ToString(), Wait);
	}
}

void AExpeditionGameState::OnRep_DayEndedTime()
{
	if (IsDaySurvived())
	{
		OnDaySurvived.Broadcast();
	}
}

double AExpeditionGameState::GetNextTravelTime() const
{
	if (bExecution || !IsExpeditionOver())
	{
		// no travel: the expedition runs, or the campaign is lost (an execution, or a crew wiped on its milestone day)
		return -1.0;
	}
	// a day survived or lost (T-173): the galleon sails on to the next island
	const bool bLost = IsTeamWiped();
	const float Default = Rules ? (bLost ? Rules->GameOverRestartDelay : Rules->DayEndTravelDelay) : 0.0f;
	const float Wait = DayEndTravelWait >= 0.0f ? DayEndTravelWait : Default;
	return (bLost ? TeamWipedTime : DayEndedTime) + Wait;
}

void AExpeditionGameState::OnRep_TeamWipedTime()
{
	if (IsTeamWiped())
	{
		OnTeamWiped.Broadcast();
	}
}

int32 AExpeditionGameState::ComputeDayValue() const
{
	int32 Value = 0;
	for (TActorIterator<AGalleonActor> It(GetWorld()); It; ++It)
	{
		Value += FMath::Max(0, It->GetCargoValue());
	}
	return Value;
}

void AExpeditionGameState::RestartFromBoot()
{
	// ADR-007: what was bought or picked up after the end of the day (greed window) travels too
	if (IsDaySurvived() && !IsTeamWiped())
	{
		if (URunSubsystem* Run = URunSubsystem::Get(this))
		{
			Run->StoreLoadouts(GetWorld(), false);
			Run->SaveRun();
		}
	}
	FString Map = Rules ? Rules->RestartMap.GetLongPackageName() : FString();
	if (Map.IsEmpty() && GConfig)
	{
		FString DefaultMap;
		GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"), TEXT("GameDefaultMap"), DefaultMap, GEngineIni);
		Map = FSoftObjectPath(DefaultMap).GetLongPackageName();
	}
	UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this);
	const EFragmentError Error = Islands ? Islands->TravelToMapURL(Map) : EFragmentError::TravelFailed;
	if (Error != EFragmentError::None)
	{
		UE_LOG(Logmonke_island, Error, TEXT("Expedition %s: the day is over but the travel to the next island failed (map '%s'): the players stay on this map"),
			*ExpeditionId.ToString(), *Map);
	}
}
