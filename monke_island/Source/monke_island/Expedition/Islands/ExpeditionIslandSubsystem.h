// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Expedition/Islands/ExpeditionIslandTypes.h"
#include "ExpeditionIslandSubsystem.generated.h"

class UExpeditionIslandDefinition;
class UFragmentRules;
class UIslandCatalog;
class UWorld;
struct FStreamableHandle;

/** Result of a catalog load: the error (None on success) and the valid fragments offered, in catalog order */
DECLARE_DELEGATE_TwoParams(FOnIslandCatalogLoaded, EFragmentError /*Error*/, const TArray<UExpeditionIslandDefinition*>& /*Definitions*/);

/** Result of a definition load: the error (None on success) and the definition, null on error */
DECLARE_DELEGATE_TwoParams(FOnIslandDefinitionLoaded, EFragmentError /*Error*/, UExpeditionIslandDefinition* /*Definition*/);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FIslandCatalogLoadedSignature, EFragmentError, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FIslandSelectionChangedSignature, FName, NewFragmentId, FName, PreviousFragmentId);

/**
 *  Expedition islands (ADR-004), one per game instance on every machine.
 *  - Loads DA_IslandCatalog then its definitions asynchronously through the Asset Manager: every load ends once,
 *    with success, failure or cancellation, within DA_FragmentRules' maximum delay (never LoadSynchronous).
 *  - Keeps the fragment chosen at the hub. Only the server chooses; the chosen id reaches the clients through
 *    AExpeditionGameState::FragmentId, and each client loads the definition locally by its id.
 *  - Gives T-088 the ServerTravel URL of the chosen fragment, and tells which fragment an opened world plays.
 */
UCLASS()
class MONKE_ISLAND_API UExpeditionIslandSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Returns the subsystem of the game instance of the given world, or nullptr */
	static UExpeditionIslandSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	/**
	 *  Server: players logged in the world this game instance left by its last travel (ServerTravel or seamless), 0 when
	 *  unknown; reading it forgets it. The boot map waits for them before travelling on (T-141: a slow client coming
	 *  back to L_Boot after an expedition is not left behind).
	 */
	int32 TakeTravelPlayerCount();

	/**
	 *  Server, before the first frame of a fragment world: when its URL names the fragment (?Island=) and the definition
	 *  is loaded and plays this map, gives its Start anchorage to the boat start (UBoatStartSubsystem::SetArrivalOverride).
	 *  True when injected.
	 */
	bool InjectStartAnchorage(UWorld* World) const;

	/**
	 *  Loads the catalog and its definitions (once; a failed load can be retried). Callback runs once: right away
	 *  when the catalog is already loaded or cannot be requested, later otherwise. OnCatalogLoaded fires too.
	 */
	void LoadCatalog(FOnIslandCatalogLoaded Callback = FOnIslandCatalogLoaded());

	/** Blueprint entry of LoadCatalog: listen to OnCatalogLoaded */
	UFUNCTION(BlueprintCallable, Category="Expedition|Islands")
	void RequestCatalog() { LoadCatalog(); }

	/** Whether the catalog finished loading with at least one valid fragment */
	UFUNCTION(BlueprintPure, Category="Expedition|Islands")
	bool IsCatalogLoaded() const { return CatalogState == ECatalogState::Loaded; }

	/** Error of the last catalog load (None while never loaded or loaded fine) */
	UFUNCTION(BlueprintPure, Category="Expedition|Islands")
	EFragmentError GetCatalogError() const { return CatalogError; }

	/** Valid fragments offered by the loaded catalog, in catalog order (empty before the load) */
	UFUNCTION(BlueprintPure, Category="Expedition|Islands")
	TArray<UExpeditionIslandDefinition*> GetOfferedDefinitions() const;

	/** Runtime rules: those of the loaded catalog, else the class defaults of UFragmentRules */
	const UFragmentRules& GetRules() const;

	/**
	 *  Loads one definition by its fragment id (a client reads the id from the game state). Callback runs once:
	 *  right away when the definition is cached or the request is invalid, later otherwise.
	 */
	void LoadDefinition(FName FragmentId, FOnIslandDefinitionLoaded Callback);

	/** Definition already loaded for this fragment id, or nullptr */
	UFUNCTION(BlueprintPure, Category="Expedition|Islands")
	UExpeditionIslandDefinition* FindLoadedDefinition(FName FragmentId) const;

	/** Server: chooses the fragment of the next expedition among the loaded catalog */
	UFUNCTION(BlueprintCallable, Category="Expedition|Islands")
	EFragmentError SelectFragment(FName FragmentId);

	/** Server: forgets the chosen fragment */
	UFUNCTION(BlueprintCallable, Category="Expedition|Islands")
	void ClearSelection();

	/** Chosen fragment id, None when nothing is chosen */
	UFUNCTION(BlueprintPure, Category="Expedition|Islands")
	FName GetSelectedFragmentId() const;

	/** Chosen definition, or nullptr */
	UFUNCTION(BlueprintPure, Category="Expedition|Islands")
	UExpeditionIslandDefinition* GetSelectedDefinition() const { return SelectedDefinition; }

	/** ServerTravel URL of the chosen fragment (<map>?Island=<id>), for T-088 */
	UFUNCTION(BlueprintCallable, Category="Expedition|Islands")
	EFragmentError GetTravelURL(FString& OutURL) const;

	/**
	 *  Server: draws a fragment of the loaded catalog from Seed and chooses it (T-094: random island at launch until
	 *  the hub of L4). Never the previously drawn fragment twice in a row, unless the catalog offers only one.
	 */
	UFUNCTION(BlueprintCallable, Category="Expedition|Islands")
	EFragmentError PickRandomIsland(int32 Seed);

	/** Fragment drawn last by PickRandomIsland in this game instance, None before the first draw */
	UFUNCTION(BlueprintPure, Category="Expedition|Islands")
	FName GetLastPickedFragmentId() const { return LastPickedFragmentId; }

	/**
	 *  Server: travels to the chosen fragment (<map>?Island=<id>, read by the boat of T-093): OpenLevel-like client
	 *  travel when alone, ServerTravel when hosting (the listen option is kept). Refused on a client.
	 */
	UFUNCTION(BlueprintCallable, Category="Expedition|Islands")
	EFragmentError TravelToSelectedIsland();

	/** Server: travels to a map URL (fragment or fallback map), see TravelToSelectedIsland */
	EFragmentError TravelToMapURL(const FString& URL);

	/**
	 *  Fragment played by a world, None when the world is not a fragment: the Island option of its URL, else the
	 *  chosen or loaded definition of its map, else the asset registry (a map opened directly, without the hub).
	 *  Reads the registry tags only: nothing is loaded. Called once by the expedition game state, not every frame.
	 */
	FName ResolveFragmentIdForWorld(const UWorld* World) const;

	/** Fires when a catalog load ends, with its error (None on success) */
	UPROPERTY(BlueprintAssignable, Category="Expedition|Islands")
	FIslandCatalogLoadedSignature OnCatalogLoaded;

	/** Fires on the server when the chosen fragment changes */
	UPROPERTY(BlueprintAssignable, Category="Expedition|Islands")
	FIslandSelectionChangedSignature OnSelectionChanged;

private:

	/** Progress of the catalog load */
	enum class ECatalogState : uint8
	{
		NotLoaded,
		Loading,
		Loaded,
		Failed
	};

	/** One pending definition load */
	struct FDefinitionRequest
	{
		FName FragmentId;
		FOnIslandDefinitionLoaded Callback;
		TSharedPtr<FStreamableHandle> Handle;
		FTimerHandle Timeout;
	};

	/** The catalog asset arrived: validates it, then loads its definitions */
	void HandleCatalogAssetLoaded(uint32 Generation);

	/** The definitions of the catalog arrived: keeps the valid ones */
	void HandleCatalogDefinitionsLoaded(uint32 Generation, TArray<FPrimaryAssetId> Ids);

	/** A catalog load was cancelled by the streaming manager */
	void HandleCatalogCancelled(uint32 Generation);

	/** The catalog load exceeded its delay */
	void HandleCatalogTimeout(uint32 Generation);

	/** Ends the catalog load once: state, log, callbacks, event */
	void FinishCatalog(EFragmentError Error);

	/** A definition arrived */
	void HandleDefinitionLoaded(uint32 RequestId);

	/** A definition load was cancelled by the streaming manager */
	void HandleDefinitionCancelled(uint32 RequestId);

	/** A definition load exceeded its delay */
	void HandleDefinitionTimeout(uint32 RequestId);

	/** Ends a definition load once */
	void FinishDefinition(uint32 RequestId, EFragmentError Error, UExpeditionIslandDefinition* Definition);

	/** Starts the maximum delay of a load on the game instance timers */
	FTimerHandle StartTimeout(FTimerDelegate Delegate) const;

	/** Clears a timer of the game instance */
	void ClearTimeout(FTimerHandle& Handle) const;

	/** Whether this game instance may choose the fragment (not a client) */
	bool HasSelectionAuthority() const;

	/** Loaded catalog (DA_IslandCatalog) */
	UPROPERTY(Transient)
	TObjectPtr<UIslandCatalog> Catalog;

	/** Valid fragments of the catalog, in catalog order */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UExpeditionIslandDefinition>> OfferedDefinitions;

	/** Every definition loaded, by fragment id */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UExpeditionIslandDefinition>> LoadedDefinitions;

	/** Chosen fragment (server) */
	UPROPERTY(Transient)
	TObjectPtr<UExpeditionIslandDefinition> SelectedDefinition;

	/** Progress of the catalog load */
	ECatalogState CatalogState = ECatalogState::NotLoaded;

	/** Error of the last catalog load */
	EFragmentError CatalogError = EFragmentError::None;

	/** Bumped by every catalog load: late answers of an older load are ignored */
	uint32 CatalogGeneration = 0;

	/** Callers waiting for the catalog */
	TArray<FOnIslandCatalogLoaded> CatalogCallbacks;

	/** Streaming of the catalog asset, then of its definitions */
	TSharedPtr<FStreamableHandle> CatalogHandle;

	/** Maximum delay of the catalog load */
	FTimerHandle CatalogTimeout;

	/** Pending definition loads by request id */
	TMap<uint32, FDefinitionRequest> DefinitionRequests;

	/** Next definition request id */
	uint32 NextRequestId = 1;

	/** Fragment drawn last: the next draw avoids it */
	FName LastPickedFragmentId;

	/** A world of this game instance is about to be replaced: remembers its players (server) */
	void RecordTravelPlayers(UWorld* World);

	/** Players of the world left by the last travel, 0 when unknown */
	int32 TravelPlayerCount = 0;

	/** Handles of the travel delegates */
	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle SeamlessTravelHandle;
};
