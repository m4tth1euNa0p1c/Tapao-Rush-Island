// monke_island

#include "Run/RunSubsystem.h"
#include "Run/RunRules.h"
#include "Run/RunStateComponent.h"
#include "Run/RunSaveGame.h"
#include "Run/SaveSlotFile.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UObject/Package.h"
#include "monke_island.h"

#define LOCTEXT_NAMESPACE "RunSave"

namespace
{
	/** Save slot of the run */
	const TCHAR* RunSlot = TEXT("Run");

	/** DA_RunRules when DefaultGame.ini names none */
	const TCHAR* DefaultRunRulesPath = TEXT("/Game/MonkeIsland/Expedition/Data/DA_RunRules.DA_RunRules");

	const TCHAR* VerdictName(ERunDayVerdict Verdict)
	{
		switch (Verdict)
		{
		case ERunDayVerdict::NextDay: return TEXT("next day");
		case ERunDayVerdict::QuotaMetChoose: return TEXT("quota met, leave or stay (monke.Run.Leave / monke.Run.Stay)");
		case ERunDayVerdict::ContractComplete: return TEXT("contract complete, next zone");
		case ERunDayVerdict::ContractFailed: return TEXT("contract failed");
		case ERunDayVerdict::Victory: return TEXT("victory");
		case ERunDayVerdict::Executed: return TEXT("contract failed, the crew is executed: the run is lost");
		default: return TEXT("nothing counted");
		}
	}

	/** Files of the "Run" slot in Directory (Saved/SaveGames when empty) */
	TValueOrError<MonkeSaveSlot::FSlotPaths, ESaveSlotError> MakeRunPaths(const FString& Directory)
	{
		return MonkeSaveSlot::MakePaths(Directory.IsEmpty() ? MonkeSaveSlot::DefaultDirectory() : Directory, RunSlot);
	}

	/** Subject of the messages about this save */
	FText RunSaveName()
	{
		return LOCTEXT("RunSaveName", "La sauvegarde de la partie");
	}

#if !UE_BUILD_SHIPPING
	URunSubsystem* SubsystemOf(UWorld* World)
	{
		return World ? URunSubsystem::Get(World) : nullptr;
	}

	FAutoConsoleCommandWithWorld StatusCommand(TEXT("monke.Run.Status"), TEXT("Prints the run: zone, day, quota, treasury (ADR-007)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const URunSubsystem* Run = SubsystemOf(World))
			{
				UE_LOG(Logmonke_island, Display, TEXT("%s"), *Run->Describe());
			}
		}));

	FAutoConsoleCommandWithWorld StayCommand(TEXT("monke.Run.Stay"), TEXT("Quota met: one more day in the zone (more loot, more danger)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (URunSubsystem* Run = SubsystemOf(World))
			{
				Run->Choose(World, true);
			}
		}));

	FAutoConsoleCommandWithWorld LeaveCommand(TEXT("monke.Run.Leave"), TEXT("Quota met: leave the zone now, contract complete"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (URunSubsystem* Run = SubsystemOf(World))
			{
				Run->Choose(World, false);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs MoneyCommand(TEXT("monke.Run.Money"), TEXT("Adds crowns to the treasury: monke.Run.Money 500"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (URunSubsystem* Run = SubsystemOf(World))
			{
				Run->AddMoney(World, Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 500);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs DayCommand(TEXT("monke.Run.Day"),
		TEXT("Puts the run on a day of the campaign, earlier contracts met: monke.Run.Day 7 (shows on the next island)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (URunSubsystem* Run = SubsystemOf(World))
			{
				Run->JumpToDay(World, Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1);
			}
		}));

	FAutoConsoleCommandWithWorld NewCommand(TEXT("monke.Run.New"), TEXT("Starts a fresh run: zone 1, day 1"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (URunSubsystem* Run = SubsystemOf(World))
			{
				Run->StartNewRun();
				Run->PushState(World);
			}
		}));
#endif
}

URunSubsystem::URunSubsystem()
	: RunRulesPath(DefaultRunRulesPath)
{
}

URunSubsystem* URunSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	URunSubsystem* Run = GameInstance ? GameInstance->GetSubsystem<URunSubsystem>() : nullptr;
	if (Run)
	{
		// R-03: the machine that keeps the run reads its slot here, once; a client keeps none
		Run->SyncWithWorld(World);
	}
	return Run;
}

void URunSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (RunRulesPath.IsNull())
	{
		RunRulesPath = FSoftObjectPath(DefaultRunRulesPath);
	}
	// R-03: nothing is read here (a client would read its own run): the slot is read once the world of a machine that
	// keeps its run is there (its actors initialised, or the first Get), and a client world forgets what the main menu read
	ActorsInitializedHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &URunSubsystem::HandleWorldActorsInitialized);
}

void URunSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(ActorsInitializedHandle);
	ActorsInitializedHandle.Reset();
	WatchLeavers(nullptr);
	Super::Deinitialize();
}

const UWorld* URunSubsystem::GetCurrentWorld() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetWorld() : nullptr;
}

void URunSubsystem::HandleWorldActorsInitialized(const FActorsInitializedParams& Params)
{
	// before any actor of the world reads the run (also the readers that skip Get): read on a machine that keeps its run,
	// set aside on a client (a PIE client's first world is already a client world: nothing is read there)
	SyncWithWorld(Params.World);
}

void URunSubsystem::SyncWithWorld(const UWorld* World)
{
	if (World && World->IsGameWorld() && World->GetGameInstance() == GetGameInstance())
	{
		SyncWithNetMode(World->GetNetMode());
	}
}

void URunSubsystem::SyncWithNetMode(ENetMode NetMode)
{
	if (!KeepsRunOn(NetMode))
	{
		ForgetRun();
	}
	else if (!bLoaded)
	{
		LoadRun();
	}
}

void URunSubsystem::ForgetRun()
{
	if (!bLoaded && !State.bActive && Loadouts.IsEmpty())
	{
		return;
	}
	State = FRunContractState();
	LastDayReport = FRunDayReport();
	Loadouts.Reset();
	HandedOutKeys.Reset();
	LoadNotice = FText::GetEmpty();
	bLoaded = false;
	bSlotWritable = false;
	UE_LOG(Logmonke_island, Log, TEXT("Run: this machine plays on a host: its own run stays on disk, untouched, and the host's run is shown (R-03)"));
}

void URunSubsystem::LoadRun()
{
	bLoaded = true;
	bSlotWritable = false;
	LoadNotice = FText::GetEmpty();
	const TValueOrError<MonkeSaveSlot::FSlotPaths, ESaveSlotError> Paths = MakeRunPaths(SlotDirectory);
	if (Paths.HasError())
	{
		UE_LOG(Logmonke_island, Error, TEXT("Run: the folder of the save slot '%s' is refused (%s): the run will not be saved"), RunSlot,
			LexToString(Paths.GetError()));
		return;
	}
	int32 Fixes = 0;
	const MonkeSaveSlot::FReadResult Result = MonkeSaveSlot::Read(Paths.GetValue(), URunSaveGame::StaticClass(), URunSaveGame::CurrentVersion,
		[&Fixes](USaveGame& Loaded)
		{
			return CastChecked<URunSaveGame>(&Loaded)->SanitizeLoaded(Fixes);
		}, FDateTime::Now());
	bSlotWritable = Result.bWritable;
	LoadNotice = MonkeSaveSlot::DescribeForPlayer(Result, RunSaveName(), LOCTEXT("RunFresh", "une nouvelle partie commencera"));
	const URunSaveGame* Save = Cast<URunSaveGame>(Result.Object);
	if (!Save)
	{
		if (Result.MainError != ESaveSlotError::NotFound)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Run: no valid save in slot '%s' (%s), a new run will start"), RunSlot, LexToString(Result.MainError));
		}
		return;
	}
	State = Save->State;
	LastDayReport = Save->LastDayReport;
	Loadouts = Save->Loadouts;
	if (Fixes > 0)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Run: %d value(s) of the save were out of bounds and were fixed"), Fixes);
	}
	UE_LOG(Logmonke_island, Log, TEXT("Run: loaded from the save of %s%s. %s"), *Save->Saved.ToString(),
		Result.bLegacy ? TEXT(" (older format, converted at the next save, the old file kept as the last valid copy)") : TEXT(""), *Describe());
}

bool URunSubsystem::CanWriteRun() const
{
	const UWorld* World = GetCurrentWorld();
	return bLoaded && bSlotWritable && (!World || KeepsRunOn(World->GetNetMode()));
}

void URunSubsystem::SaveRun()
{
	if (!CanWriteRun())
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Run: not saved (client, slot not read, or slot left untouched)"));
		return;
	}
	const TValueOrError<MonkeSaveSlot::FSlotPaths, ESaveSlotError> Paths = MakeRunPaths(SlotDirectory);
	if (Paths.HasError())
	{
		return;
	}
	URunSaveGame* Save = NewObject<URunSaveGame>(GetTransientPackage());
	Save->Version = URunSaveGame::CurrentVersion;
	Save->State = State;
	Save->LastDayReport = LastDayReport;
	Save->Loadouts = Loadouts;
	Save->Saved = FDateTime::Now();
	const ESaveSlotError Error = MonkeSaveSlot::Write(Paths.GetValue(), *Save, URunSaveGame::CurrentVersion, FDateTime::Now());
	if (Error != ESaveSlotError::None)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Run: the save slot '%s' could not be written (%s), its last valid copy is kept"), RunSlot, LexToString(Error));
		LoadNotice = MonkeSaveSlot::DescribeWriteFailureForPlayer(RunSaveName());
		return;
	}
	LoadNotice = FText::GetEmpty();
}

FString URunSubsystem::PlayerKey(const APlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return FString();
	}
	const FUniqueNetIdRepl& Id = PlayerState->GetUniqueId();
	return Id.IsValid() ? Id.ToString() : PlayerState->GetPlayerName();
}

const FRunLoadout* URunSubsystem::FindLoadout(const FString& Key) const
{
	// R-41: handed out once; IslanderInventoryComponent::RestoreRunLoadout asks through a const subsystem
	return const_cast<URunSubsystem*>(this)->ClaimLoadout(Key);
}

const FRunLoadout* URunSubsystem::ClaimLoadout(const FString& Key)
{
	const UWorld* World = GetCurrentWorld();
	if (Key.IsEmpty() || (World && !KeepsRunOn(World->GetNetMode())))
	{
		return nullptr;
	}
	if (HandedOutWorld.Get() != World)
	{
		HandedOutKeys.Reset();
		HandedOutWorld = World;
	}
	bool bAlreadyHandedOut = false;
	HandedOutKeys.Add(Key, &bAlreadyHandedOut);
	if (bAlreadyHandedOut)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Run: this player's kept equipment was already handed out on this island: the default kit (R-41)"));
		return nullptr;
	}
	return Loadouts.Find(Key);
}

void URunSubsystem::RecordLoadout(const FString& Key, const FRunLoadout* Carried)
{
	if (Key.IsEmpty())
	{
		return;
	}
	if (Carried)
	{
		Loadouts.Add(Key, *Carried);
	}
	else
	{
		Loadouts.Remove(Key);
	}
	// what is recorded now is his to take again (a player back after leaving takes what he left with)
	HandedOutKeys.Remove(Key);
}

void URunSubsystem::KeepLoadoutOf(const APlayerState* PlayerState, const APawn* Pawn, bool bWiped)
{
	const FString Key = PlayerKey(PlayerState);
	if (Key.IsEmpty())
	{
		return;
	}
	const AIslanderCharacter* Character = Cast<AIslanderCharacter>(Pawn);
	const UIslanderHealthComponent* Health = Character ? Character->GetHealthComponent() : nullptr;
	const UIslanderInventoryComponent* Inventory = Character ? Character->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	if (bWiped || !Inventory || (Health && Health->IsDead()))
	{
		// lost on the island: the next day starts with the default kit
		RecordLoadout(Key, nullptr);
		UE_LOG(Logmonke_island, Log, TEXT("Run: player %d loses its equipment (%s)"), PlayerState->GetPlayerId(),
			bWiped ? TEXT("crew wiped") : (Inventory ? TEXT("dead") : TEXT("no character")));
		return;
	}
	// T-088: the key must stay the same through the travels (the unique net id; the name only without online subsystem)
	const FRunLoadout Carried = Inventory->ExportLoadout();
	RecordLoadout(Key, &Carried);
	UE_LOG(Logmonke_island, Log, TEXT("Run: player %d keeps %d item(s) for the next island%s"), PlayerState->GetPlayerId(), Carried.Items.Num(),
		PlayerState->GetUniqueId().IsValid() ? TEXT("") : TEXT(" (no unique net id: kept by player name)"));
}

void URunSubsystem::StoreLoadouts(UWorld* World, bool bWiped)
{
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || !KeepsRunOn(World->GetNetMode()))
	{
		return;
	}
	TSet<FString> Present;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		const FString Key = PlayerKey(PlayerState);
		if (Key.IsEmpty())
		{
			continue;
		}
		Present.Add(Key);
		KeepLoadoutOf(PlayerState, PlayerState->GetPawn(), bWiped);
	}
	if (HandedOutWorld.Get() == World)
	{
		ForgetHandedOutAbsent(Present);
	}
}

void URunSubsystem::ForgetHandedOutAbsent(const TSet<FString>& PresentKeys)
{
	// R-41: handed out on this island to a player now gone whose departure kept nothing: his pawn took it away
	for (auto It = HandedOutKeys.CreateIterator(); It; ++It)
	{
		if (!PresentKeys.Contains(*It))
		{
			Loadouts.Remove(*It);
			It.RemoveCurrent();
		}
	}
}

void URunSubsystem::WatchLeavers(UWorld* World)
{
	UWorld* Watched = WatchedWorld.Get();
	if (Watched == World && (ActorDestroyedHandle.IsValid() || !World))
	{
		return;
	}
	if (Watched && ActorDestroyedHandle.IsValid())
	{
		Watched->RemoveOnActorDestroyedHandler(ActorDestroyedHandle);
	}
	ActorDestroyedHandle.Reset();
	WatchedWorld = World;
	if (World)
	{
		ActorDestroyedHandle = World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &URunSubsystem::HandleActorDestroyed));
	}
}

void URunSubsystem::HandleActorDestroyed(AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	// R-41: a player whose connection closed (no UPlayer left): APlayerController::Destroyed takes his pawn away
	// (PawnLeavingGame) before the game mode's Logout, and UWorld::OnActorDestroyed comes before the pawn's EndPlay:
	// its inventory is still whole here
	if (!Controller || Controller->Player || !Controller->IsActorBeingDestroyed() || !State.bActive || Pawn->GetWorld() != WatchedWorld.Get()
		|| !KeepsRunOn(Pawn->GetWorld()->GetNetMode()))
	{
		return;
	}
	const APlayerState* PlayerState = Pawn->GetPlayerState();
	if (!PlayerState)
	{
		return;
	}
	KeepLoadoutOf(PlayerState, Pawn, /*bWiped*/ false);
	UE_LOG(Logmonke_island, Log, TEXT("Run: player %d left the game, what it carried waits for its return (R-41)"), PlayerState->GetPlayerId());
}

bool URunSubsystem::TrySpend(UWorld* World, int32 Amount)
{
	SyncWithWorld(World);
	if (World && !KeepsRunOn(World->GetNetMode()))
	{
		return false;
	}
	if (!State.bActive)
	{
		// a map without expedition (L_Test_Flat): the shop still works on a fresh run
		StartNewRun();
	}
	if (Amount < 0 || State.Treasury < Amount)
	{
		return false;
	}
	State.Treasury -= Amount;
	PushState(World);
	SaveRun();
	return true;
}

void URunSubsystem::AddMoney(UWorld* World, int32 Amount)
{
	SyncWithWorld(World);
	if (World && !KeepsRunOn(World->GetNetMode()))
	{
		return;
	}
	if (!State.bActive)
	{
		StartNewRun();
	}
	State.Treasury = static_cast<int32>(FMath::Clamp(static_cast<int64>(State.Treasury) + Amount, int64(0), int64(URunSaveGame::MaxCrowns)));
	UE_LOG(Logmonke_island, Log, TEXT("Run: treasury %d"), State.Treasury);
	PushState(World);
	SaveRun();
}

void URunSubsystem::JumpToDay(UWorld* World, int32 CampaignDay)
{
	SyncWithWorld(World);
	if (World && !KeepsRunOn(World->GetNetMode()))
	{
		return;
	}
	FRunContractCalculator::JumpToDay(ContractRules(), State, CampaignDay);
	LastDayReport = FRunDayReport();
	PushState(World);
	SaveRun();
	UE_LOG(Logmonke_island, Log, TEXT("Run: campaign day %d (test). %s"), GetCampaignDay(), *Describe());
}

const URunRules* URunSubsystem::GetRules() const
{
	if (!Rules && !bRulesLoadTried)
	{
		bRulesLoadTried = true;
		Rules = Cast<URunRules>(RunRulesPath.TryLoad());
		if (!Rules)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Run: no DA_RunRules at '%s', default contract values used"), *RunRulesPath.ToString());
		}
	}
	return Rules;
}

FRunContractRules URunSubsystem::ContractRules() const
{
	if (const URunRules* Loaded = GetRules())
	{
		return Loaded->MakeContractRules();
	}
	// the class defaults hold the zones of ADR-007
	return GetDefault<URunRules>()->MakeContractRules();
}

void URunSubsystem::StartNewRun()
{
	// the slot is read first: the fresh run then replaces it through a normal save (the old file stays as the last valid copy)
	const UWorld* World = GetCurrentWorld();
	SyncWithWorld(World);
	if (World && !KeepsRunOn(World->GetNetMode()))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Run: a client does not start a run, the host's run counts (R-03)"));
		return;
	}
	State = FRunContractCalculator::StartRun(ContractRules());
	const URunRules* Loaded = GetRules();
	State.Treasury = FMath::Max(0, (Loaded ? Loaded : GetDefault<URunRules>())->StartingTreasury);
	LastDayReport = FRunDayReport();
	Loadouts.Reset();
	HandedOutKeys.Reset();
	SaveRun();
	UE_LOG(Logmonke_island, Log, TEXT("Run: new run. %s"), *Describe());
}

void URunSubsystem::BeginDay(UWorld* World)
{
	SyncWithWorld(World);
	if (World && !KeepsRunOn(World->GetNetMode()))
	{
		return;
	}
	// R-41: a player who leaves this island keeps what he carries for his return
	WatchLeavers(World);
	if (!State.bActive || State.Outcome != ERunOutcome::InProgress)
	{
		StartNewRun();
	}
	if (State.bAwaitingChoice)
	{
		const bool bStay = GetRules() ? GetRules()->bGreedDefaultStay : GetDefault<URunRules>()->bGreedDefaultStay;
		UE_LOG(Logmonke_island, Log, TEXT("Run: the greed question was not answered, the crew %s"), bStay ? TEXT("stays") : TEXT("leaves the zone"));
		Choose(nullptr, bStay);
	}
	UE_LOG(Logmonke_island, Log, TEXT("Run: day begins (campaign day %d, danger x%.2f; difficulty of the day: %s). %s"), GetCampaignDay(), GetDangerScale(),
		*RunDifficulty::Describe(GetDayDifficulty()), *Describe());
	PushState(World);
}

FRunDayReport URunSubsystem::EndDay(UWorld* World, int32 DayValue, int32 Deaths, bool bWiped)
{
	SyncWithWorld(World);
	if (World && !KeepsRunOn(World->GetNetMode()))
	{
		return FRunDayReport();
	}
	if (State.bActive && State.Outcome == ERunOutcome::InProgress && !State.bAwaitingChoice)
	{
		StoreLoadouts(World, bWiped);
	}
	const FRunDayReport Report = FRunContractCalculator::EndDay(ContractRules(), State, DayValue, Deaths, bWiped);
	if (Report.Verdict != ERunDayVerdict::Invalid)
	{
		LastDayReport = Report;
		UE_LOG(Logmonke_island, Log, TEXT("Run: day %d ended (%s), %d brought back, %d dead, fees %d, contract %d / %d -> %s. %s"),
			Report.Day, bWiped ? TEXT("crew wiped") : TEXT("survivors"), Report.DayValue, Report.Deaths, Report.MedicalFees,
			Report.ContractValue, Report.Quota, VerdictName(Report.Verdict), *Describe());
		OnDayEnded.Broadcast(Report);
	}
	else
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Run: end of day ignored (no run in progress or a choice pending). %s"), *Describe());
	}
	PushState(World);
	SaveRun();
	return Report;
}

bool URunSubsystem::Choose(UWorld* World, bool bStay)
{
	SyncWithWorld(World);
	if (World && !KeepsRunOn(World->GetNetMode()))
	{
		return false;
	}
	int32 Payout = 0;
	const ERunDayVerdict Verdict = FRunContractCalculator::Choose(ContractRules(), State, bStay, &Payout);
	if (Verdict == ERunDayVerdict::Invalid)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Run: no greed question pending"));
		return false;
	}
	UE_LOG(Logmonke_island, Log, TEXT("Run: the crew %s (%s, paid %d). %s"), bStay ? TEXT("stays one more day") : TEXT("leaves the zone"),
		VerdictName(Verdict), Payout, *Describe());
	// the summary of the day tells what the answer led to
	LastDayReport.Verdict = Verdict;
	LastDayReport.Payout = Payout;
	PushState(World);
	SaveRun();
	return true;
}

float URunSubsystem::GetGreedChoiceSeconds() const
{
	const URunRules* Loaded = GetRules();
	return FMath::Max(0.0f, (Loaded ? Loaded : GetDefault<URunRules>())->GreedChoiceSeconds);
}

float URunSubsystem::GetDangerScale() const
{
	// the zone and the greed day of ADR-007, the extra tiki of the campaign from its day, and the mobs of the day's
	// difficulty (owner 2026-10-09: "tout devient plus dur")
	return State.bActive ? FRunContractCalculator::DangerScale(ContractRules(), State) * RunCampaign::MobScale(GetCampaignUnlocks(), GetCampaignDay())
		* GetDayDifficulty().MobCountScale : 1.0f;
}

float URunSubsystem::GetDayBushScale() const
{
	return RunCampaign::DayBushScale(GetCampaignUnlocks(), GetCampaignDay());
}

bool URunSubsystem::GetEarlyNightStartHours(FFloatInterval& OutHours) const
{
	// the night share of the day's difficulty sets the start hour itself; else the early night of the campaign
	const FRunDayDifficulty Difficulty = GetDayDifficulty();
	if (Difficulty.NightShare > 0.0f)
	{
		const URunRules* Loaded = GetRules();
		const URunRules* DayRules = Loaded ? Loaded : GetDefault<URunRules>();
		const float Hour = RunDifficulty::NightShareStartHour(DayRules->NightFallHour, DayRules->ExpeditionSolarHours * GetFloodDelayScale(), Difficulty.NightShare);
		OutHours = FFloatInterval(Hour, Hour);
		return true;
	}
	return RunCampaign::EarlyNightStartHours(GetCampaignUnlocks(), GetCampaignDay(), OutHours);
}

float URunSubsystem::GetLootValueScale() const
{
	return State.bActive ? FRunContractCalculator::LootValueScale(ContractRules(), State) : 1.0f;
}

float URunSubsystem::GetFloodDelayScale() const
{
	// the zone of the contract and the day's difficulty (the deluge comes sooner on the hard days)
	return State.bActive ? FRunContractCalculator::FloodDelayScale(ContractRules(), State) * GetDayDifficulty().FloodDelayScale : 1.0f;
}

FRunDayDifficulty URunSubsystem::GetDayDifficulty() const
{
	const URunRules* Loaded = GetRules();
	return (Loaded ? Loaded : GetDefault<URunRules>())->GetDayDifficulty(GetCampaignDay());
}

FRunDayDifficulty URunSubsystem::DifficultyOf(const UObject* WorldContext)
{
	const URunSubsystem* Run = Get(WorldContext);
	return Run ? Run->GetDayDifficulty() : FRunDayDifficulty();
}

FText URunSubsystem::GetZoneName() const
{
	const FRunContractRules Contract = ContractRules();
	return Contract.Zones.IsValidIndex(State.ZoneIndex) ? Contract.Zones[State.ZoneIndex].DisplayName : FText::GetEmpty();
}

int32 URunSubsystem::GetCampaignDay() const
{
	return State.bActive ? State.DaysPlayed + 1 : 0;
}

const FRunCampaignUnlocks& URunSubsystem::GetCampaignUnlocks() const
{
	const URunRules* Loaded = GetRules();
	return (Loaded ? Loaded : GetDefault<URunRules>())->Campaign;
}

bool URunSubsystem::IsUnlocked(ERunUnlock Content) const
{
	return RunCampaign::IsUnlocked(GetCampaignUnlocks(), Content, GetCampaignDay());
}

bool URunSubsystem::IsContentUnlocked(const UObject* WorldContext, ERunUnlock Content)
{
	const URunSubsystem* Run = Get(WorldContext);
	return !Run || Run->IsUnlocked(Content);
}

int32 URunSubsystem::CampaignDayOf(const UObject* WorldContext)
{
	const URunSubsystem* Run = Get(WorldContext);
	return Run ? Run->GetCampaignDay() : 0;
}

void URunSubsystem::PushState(UWorld* World) const
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (URunStateComponent* Component = URunStateComponent::Get(World))
	{
		const FRunContractRules Contract = ContractRules();
		Component->SetFromContract(State, FRunContractCalculator::DaysOfZone(Contract, State.ZoneIndex), FMath::Max(1, Contract.Zones.Num()),
			GetZoneName(), LastDayReport);
	}
}

FString URunSubsystem::Describe() const
{
	if (!State.bActive)
	{
		return TEXT("no run");
	}
	const FRunContractRules Contract = ContractRules();
	return FString::Printf(TEXT("zone %d/%d %s, contract %d, day %d/%d%s, quota %d/%d%s, treasury %d, days played %d, %s"),
		State.ZoneIndex + 1, Contract.Zones.Num(), *GetZoneName().ToString(), State.ContractNumber, State.DayInContract,
		FRunContractCalculator::DaysOfZone(Contract, State.ZoneIndex), State.bGreedDay ? TEXT(" (greed)") : TEXT(""), State.ContractValue, State.Quota,
		State.bAwaitingChoice ? TEXT(", choice pending") : TEXT(""), State.Treasury, State.DaysPlayed,
		*UEnum::GetValueAsString(State.Outcome));
}

#undef LOCTEXT_NAMESPACE
