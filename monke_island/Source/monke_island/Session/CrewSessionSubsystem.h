// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Session/CrewSessionRules.h"
#include "CrewSessionSubsystem.generated.h"

class AGameModeBase;
class AGameStateBase;
class APlayerState;
class UNetDriver;
class UWorld;
struct FUniqueNetIdRepl;

MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeSession, Log, All);

/**
 *  The host's session gate and the joining player's failure (T-315; cahier §16 « Rejoindre seulement au hub », §24.2,
 *  §24.5; security review T-269 M4). One per game instance, on every machine.
 *  - Host: keeps the crew roster (FCrewRoster) across the server travels of the session, and answers the PreLogin of
 *    the game modes (Amonke_islandGameMode): lobby maps let anyone in within MaxPlayers; once an expedition was
 *    launched, the other maps take only the known crew, whose travels from island to island go through.
 *  - Joining player: remembers why its last join failed (the host's message, a full session, a timeout, a version)
 *    until the menu shows it (ConsumeJoinFailure).
 *  Nothing personal is logged: no name, no address, no id (security.md §6, M7).
 */
UCLASS()
class MONKE_ISLAND_API UCrewSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Returns the subsystem of the game instance of the given world, or nullptr */
	static UCrewSessionSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	// ------------------------------------------------------------------ host

	/** Server, from a game mode's InitGame: a server world starts. Reached by a server travel of this session, the
	 *  session goes on; otherwise (the menu hosted, « Recommencer », a map opened in PIE) a new one starts */
	void BeginServerWorld(const UWorld& World);

	/** Server, from a game mode's ProcessServerTravel: the host sends everybody to URL */
	void NoteServerTravel(const FString& URL, const UWorld& World);

	/** Server: the crew leaves for an expedition (the boot travels to an island) or the clock of an island starts: the
	 *  players present are the crew, and the maps that are not a lobby take only them from now on */
	void LaunchExpedition(const AGameStateBase& GameState);

	/** Server: a player completed its login on a map (bLobby: a lobby map, where it does not join the crew yet) */
	void NoteLogin(const APlayerState& PlayerState, bool bLobby);

	/** Server: what a connection asking to join this game mode's map gets (None: let in) */
	ECrewJoinRefusal EvaluateLogin(const AGameModeBase& GameMode, bool bLobby, const FString& Options, const FUniqueNetIdRepl& UniqueId,
		int32 MaxNameChars) const;

	/** Whether an expedition was launched in this session */
	UFUNCTION(BlueprintPure, Category="Session")
	bool IsExpeditionLaunched() const { return Roster.bExpeditionLaunched; }

	/** Players known to the crew of this session */
	UFUNCTION(BlueprintPure, Category="Session")
	int32 GetCrewCount() const { return Roster.Members.Num(); }

	/** Key of a logged-in player in the roster (CrewSession::MakeCrewKey) */
	static FString KeyOf(const APlayerState& PlayerState);

	/** Key of a connection asking to join: its unique id, else the name it sends (filtered as the server keeps it) */
	static FString KeyOfLogin(const FString& Options, const FUniqueNetIdRepl& UniqueId, int32 MaxNameChars);

	// ------------------------------------------------------------------ joining player

	/** Whether the last join of this machine failed with a message not shown yet */
	UFUNCTION(BlueprintPure, Category="Session")
	bool HasJoinFailure() const { return !JoinFailure.IsEmpty(); }

	/** The message of the last failed join, for the menu, then forgotten; empty when there is none */
	UFUNCTION(BlueprintCallable, Category="Session")
	FText ConsumeJoinFailure();

private:

	/** Engine: a network failure, on any game instance */
	void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);

	/** Engine: a map was loaded; a menu or a client world ends the session this machine hosted */
	void HandlePostLoadMap(UWorld* World);

	/** Who may join, kept across the server travels of the session */
	FCrewRoster Roster;

	/** Message of the last failed join (joining machine) */
	FText JoinFailure;

	/** Net driver and kind of that failure: the engine reports a failed join twice (its cause, then a generic
	 *  pending failure); the more precise cause is kept */
	TWeakObjectPtr<UNetDriver> JoinFailureDriver;
	EJoinFailureKind JoinFailureKind = EJoinFailureKind::Refused;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle PostLoadMapHandle;
};
