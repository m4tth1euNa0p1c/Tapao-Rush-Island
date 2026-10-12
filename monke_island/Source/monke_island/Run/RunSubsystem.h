// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Run/RunContract.h"
#include "Run/RunDifficulty.h"
#include "Run/RunLoadout.h"
#include "RunSubsystem.generated.h"

class AActor;
class APawn;
class APlayerState;
class URunRules;
class UWorld;
struct FActorsInitializedParams;
struct FRunCampaignUnlocks;
enum class ERunUnlock : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRunDayEnded, const FRunDayReport&);

/**
 *  The run kept between the islands (ADR-007): zone, day of the contract, value brought back, treasury, greed question.
 *  Lives on the game instance, so it survives the travels L_Boot -> island -> L_Boot; only the server's copy counts,
 *  the clients read URunStateComponent on the expedition game state, which PushState fills.
 *  R-03: the "Run" slot is read at the first use by a machine that keeps its run (standalone or host), never by a client;
 *  a client keeps no run of its own while it plays on a host (what its main menu read is set aside, untouched on disk).
 *  AExpeditionGameState calls BeginDay at its BeginPlay and EndDay / the wipe at the end of the day.
 *  It also keeps what each player carries between the islands (bought weapons, rounds left), handed out once per island
 *  and kept when a player leaves (R-41), and the treasury the shop spends; everything goes to the "Run" slot (URunSaveGame)
 *  through MonkeSaveSlot (checked before reading, written as a transaction, damaged file put aside, T-302).
 *  Console (server, not in Shipping): monke.Run.Status, monke.Run.Stay, monke.Run.Leave, monke.Run.New, monke.Run.Money <n>.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API URunSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	URunSubsystem();

	/**
	 *  The subsystem of the game instance of this world, or null. On a machine that keeps its run, the "Run" slot is read
	 *  at the first call; on a client the run stays empty (the host's run is URunStateComponent, R-03): only the rules
	 *  (GetRules) are worth reading there
	 */
	static URunSubsystem* Get(const UObject* WorldContext);

	/** True on a machine that keeps the run (standalone or server); a client never reads nor writes its own (R-03) */
	static bool KeepsRunOn(ENetMode NetMode) { return NetMode != NM_Client; }

	/** DA_RunRules, loaded once */
	const URunRules* GetRules() const;

	/** Current run */
	const FRunContractState& GetState() const { return State; }

	/** Summary of the last day that ended */
	const FRunDayReport& GetLastDayReport() const { return LastDayReport; }

	/**
	 *  For the player: what went wrong with the save of the run (damaged file put aside, last valid copy taken, newer
	 *  version kept apart, unreadable file, write failed); empty when all went well, cleared by the next good save
	 */
	const FText& GetLoadNotice() const { return LoadNotice; }

	/** Starts a fresh run: zone 1, day 1, empty treasury (JOUER with no campaign, monke.Run.New); refused on a client */
	void StartNewRun();

	/**
	 *  Server, start of an island: starts a run when there is none (or the last one was won), answers a pending greed
	 *  question with bGreedDefaultStay, then fills the run state of the world
	 */
	void BeginDay(UWorld* World);

	/** Server, end of the day: counts the value brought back, the deaths and the wipe, then fills the run state */
	FRunDayReport EndDay(UWorld* World, int32 DayValue, int32 Deaths, bool bWiped);

	/** Server: answers the greed question (stay one more day, or leave the zone); false when nothing was asked */
	bool Choose(UWorld* World, bool bStay);

	/** Time the crew has to answer the greed question (DA_RunRules) */
	float GetGreedChoiceSeconds() const;

	/** Spawn multiplier of the day: the zone and the greed day of the contract, the surplus of the campaign and the mob
	 *  count of the day's difficulty; 1 without a run */
	float GetDangerScale() const;

	/** Multiplier of the tiki out of the bushes by day (campaign), 1 without a run */
	float GetDayBushScale() const;

	/** True when the expedition of the day starts late so the night falls early (the night share of the day's
	 *  difficulty, else the early night of the campaign), with the solar hours to draw the start from */
	bool GetEarlyNightStartHours(FFloatInterval& OutHours) const;

	/** Loot value multiplier of the day (zone, greed), 1 without a run */
	float GetLootValueScale() const;

	/** Time before the deluge multiplier: the zone and the day's difficulty, 1 without a run */
	float GetFloodDelayScale() const;

	/** Difficulty of the day played (DA_RunRules.DifficultyByDay); neutral without a run (day 0) */
	FRunDayDifficulty GetDayDifficulty() const;

	/** GetDayDifficulty of the run of this world (neutral without a game instance or a run: test maps, clients) */
	static FRunDayDifficulty DifficultyOf(const UObject* WorldContext);

	/** Name of the zone of the contract */
	FText GetZoneName() const;

	/** Day of the campaign being played, from 1 (days played + 1, whatever the contract); 0 without a run */
	int32 GetCampaignDay() const;

	/** True when the content is open on the day played; always without a run (test maps show everything) */
	bool IsUnlocked(ERunUnlock Content) const;

	/** Days the content opens (DA_RunRules, the class defaults without the asset) */
	const FRunCampaignUnlocks& GetCampaignUnlocks() const;

	/** IsUnlocked of the run of this world (true without a game instance or a run) */
	static bool IsContentUnlocked(const UObject* WorldContext, ERunUnlock Content);

	/** GetCampaignDay of the run of this world on the server (0 without one, and always 0 on a client: the interface
	 *  reads URunStateComponent::CampaignDayOf) */
	static int32 CampaignDayOf(const UObject* WorldContext);

	/** Copies the run to URunStateComponent of the world's game state (server) */
	void PushState(UWorld* World) const;

	/** Every day that ends */
	FOnRunDayEnded OnDayEnded;

	/** Key of a player in the loadouts: its unique net id, else its name */
	static FString PlayerKey(const APlayerState* PlayerState);

	/**
	 *  Server: the equipment kept for this player, handed out once on an island (R-41): a second pawn of the same player
	 *  on that island (back after leaving, spawned again) gets null, unless the player left carrying something, which is
	 *  then his to take back. Same as ClaimLoadout (IslanderInventoryComponent calls it on a const subsystem).
	 */
	const FRunLoadout* FindLoadout(const FString& Key) const;

	/** Server: hands out the equipment kept for this player once on this island (see FindLoadout); null for the default kit */
	const FRunLoadout* ClaimLoadout(const FString& Key);

	/** Server: takes Amount from the treasury; false (nothing taken) when it is short or there is no run */
	bool TrySpend(UWorld* World, int32 Amount);

	/** Server: adds money to the treasury (console, tests) */
	void AddMoney(UWorld* World, int32 Amount);

	/** Server, test tool (monke.Run.Day): the run on a day of the campaign, earlier contracts met; what that day opens
	 *  shows on the next island */
	void JumpToDay(UWorld* World, int32 CampaignDay);

	/** Writes the run to the "Run" slot (as a transaction, the last valid copy kept); nothing on a client, nor over a
	 *  file that could not be read */
	void SaveRun();

	/** Text of monke.Run.Status */
	FString Describe() const;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	friend class FRunSubsystemSpec;

	/** DA_RunRules (set in DefaultGame.ini or here) */
	UPROPERTY(Config)
	FSoftObjectPath RunRulesPath;

	/** Loaded rules */
	UPROPERTY(Transient)
	mutable TObjectPtr<URunRules> Rules;

	/** The load was tried (a missing asset is reported once, the class defaults stand in) */
	mutable bool bRulesLoadTried = false;

	/** Rules copied for the calculator */
	FRunContractRules ContractRules() const;

	/** The run */
	FRunContractState State;

	/** Last day */
	FRunDayReport LastDayReport;

	/** Equipment of the players between the islands, by PlayerKey */
	TMap<FString, FRunLoadout> Loadouts;

	/** The slot was read (or found empty) for this machine's own run */
	bool bLoaded = false;

	/** The slot may be written: read, or found damaged and put aside (false before the read, on a client, or when the
	 *  file could neither be read nor put aside) */
	bool bSlotWritable = false;

	/** See GetLoadNotice */
	FText LoadNotice;

	/** Folder of the slot, empty for Saved/SaveGames (the tests use a transient folder) */
	FString SlotDirectory;

	/** Players whose kept equipment was handed out in HandedOutWorld (R-41) */
	TSet<FString> HandedOutKeys;

	/** Island of HandedOutKeys */
	TWeakObjectPtr<const UWorld> HandedOutWorld;

	/** Island whose departing players are watched (R-41) */
	TWeakObjectPtr<UWorld> WatchedWorld;

	/** UWorld::OnActorDestroyed of WatchedWorld */
	FDelegateHandle ActorDestroyedHandle;

	/** FWorldDelegates::OnWorldInitializedActors */
	FDelegateHandle ActorsInitializedHandle;

	/** World of this game instance */
	const UWorld* GetCurrentWorld() const;

	/** Reads the slot once on a machine that keeps its run, forgets the run on a client (R-03) */
	void SyncWithWorld(const UWorld* World);
	void SyncWithNetMode(ENetMode NetMode);

	/** A world of this game instance is there: its run is read (a machine that keeps it) or set aside in memory, never
	 *  saved (a client, R-03) */
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);
	void ForgetRun();

	/** Reads the "Run" slot (MonkeSaveSlot: checks, bounds, damaged file put aside, notice for the player) */
	void LoadRun();

	/** The run may be written now */
	bool CanWriteRun() const;

	/** Server: keeps (or forgets, when lost on the island) what this player's pawn carries */
	void KeepLoadoutOf(const APlayerState* PlayerState, const APawn* Pawn, bool bWiped);

	/** Keeps Carried for the player, or forgets what was kept when null; it is his to take again (R-41) */
	void RecordLoadout(const FString& Key, const FRunLoadout* Carried);

	/** End of the day: what was handed out on this island to a player now gone, whose departure kept nothing, is lost */
	void ForgetHandedOutAbsent(const TSet<FString>& PresentKeys);

	/** Server, start of an island: the pawn of a player who leaves keeps its loadout for his return (R-41) */
	void WatchLeavers(UWorld* World);
	void HandleActorDestroyed(AActor* Actor);

public:

	/** Server: copies what the living players carry; the dead (and everybody on a wipe) lose it (ADR-007 §14); what was
	 *  handed out on this island to a player now gone is lost too (R-41) */
	void StoreLoadouts(UWorld* World, bool bWiped);
};
