// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/RequestRateLimiter.h"
#include "Expedition/Core/ExpeditionTypes.h"
#include "monke_islandGameMode.generated.h"

/**
 *  Base of the game modes of monke_island (BP_IslandGameMode, BP_FirstPersonGameMode, the boot map's
 *  AExpeditionBootGameMode, the test map's AMonkeTestGameMode). Server only.
 *  Session gate (T-315; cahier §16, §24.2, §24.5; security review T-269 M4, m6), with UCrewSessionSubsystem:
 *  - PreLogin: a lobby map (bOpenToNewPlayers, the boot map) lets anyone in within MaxPlayers (DefaultGame.ini: 4);
 *    once an expedition was launched, the other maps let only the known crew in, with a clear message to the others
 *    (« Expédition en cours »); a spectator login (SpectatorOnly=1) is always refused, it would pass the limit;
 *  - the expedition counts as launched when the boot map travels to an island, or when the clock of an expedition
 *    starts on a map opened directly (PIE);
 *  - the session goes on across the server travels of the host (ProcessServerTravel): the crew reconnecting at each
 *    island passes; any other world starts a new session;
 *  - ChangeName: the server keeps a bounded name (MaxPlayerNameLength characters, controls and invisible characters
 *    removed), and a rename asked by a client is limited in rate.
 */
UCLASS(abstract)
class MONKE_ISLAND_API Amonke_islandGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	Amonke_islandGameMode();

	/** Lobby map: anyone may join within MaxPlayers, even once an expedition was launched (the boot map between two
	 *  islands stands for the safe zone of cahier §24.2). Off: once an expedition was launched, only its crew */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Session")
	bool bOpenToNewPlayers = false;

	/** Longest player name the server keeps (characters; cahier §20, review m6) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Session", meta=(ClampMin="1", ClampMax="64"))
	int32 MaxPlayerNameLength = 20;

	/** Renames a client may ask for at once (ServerChangeName), then NameChangeRefillPerSecond given back each second;
	 *  a rename over the limit is ignored (security.md SEC-03) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Session", meta=(ClampMin="1"))
	float NameChangeBurst = 3.0f;

	/** See NameChangeBurst (per second) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Session", meta=(ClampMin="0"))
	float NameChangeRefillPerSecond = 0.1f;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	virtual void InitGameState() override;

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

	virtual void PostLogin(APlayerController* NewPlayer) override;

	virtual void Logout(AController* Exiting) override;

	virtual void ChangeName(AController* Controller, const FString& NewName, bool bNameChange) override;

	virtual void ProcessServerTravel(const FString& URL, bool bAbsolute = false) override;

	/** Name the server keeps for a player, empty when nothing printable is left (see CrewSession::SanitizePlayerName) */
	FString SanitizePlayerName(const FString& NewName) const;

protected:

	/** Server: the phase of the expedition of this map moved; its clock started = the expedition is launched */
	UFUNCTION()
	void HandleExpeditionPhaseChanged(EExpeditionPhase NewPhase, EExpeditionPhase PreviousPhase);

private:

	/** Rename limit of each player (ServerChangeName), dropped at its logout */
	TMap<TWeakObjectPtr<AController>, FRequestRateLimiter> NameChangeLimiters;
};
