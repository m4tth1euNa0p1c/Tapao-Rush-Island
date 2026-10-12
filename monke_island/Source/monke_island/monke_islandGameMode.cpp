// Copyright Epic Games, Inc. All Rights Reserved.

#include "monke_islandGameMode.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Session/CrewSessionRules.h"
#include "Session/CrewSessionSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

Amonke_islandGameMode::Amonke_islandGameMode()
{
}

void Amonke_islandGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// T-315: reached by a server travel of the session, the crew and the launch are kept; any other world starts anew
	if (UCrewSessionSubsystem* Crew = UCrewSessionSubsystem::Get(this))
	{
		Crew->BeginServerWorld(*GetWorld());
	}
}

void Amonke_islandGameMode::InitGameState()
{
	Super::InitGameState();
	// a map opened directly (PIE, a test) launches its expedition when its clock starts: from then on, crew only
	if (AExpeditionGameState* Expedition = Cast<AExpeditionGameState>(GameState))
	{
		Expedition->OnPhaseChanged.AddUniqueDynamic(this, &Amonke_islandGameMode::HandleExpeditionPhaseChanged);
	}
}

void Amonke_islandGameMode::HandleExpeditionPhaseChanged(EExpeditionPhase NewPhase, EExpeditionPhase PreviousPhase)
{
	if (NewPhase == EExpeditionPhase::Preparing || PreviousPhase != EExpeditionPhase::Preparing || bOpenToNewPlayers || !GameState)
	{
		return;
	}
	if (UCrewSessionSubsystem* Crew = UCrewSessionSubsystem::Get(this))
	{
		Crew->LaunchExpedition(*GameState);
	}
}

void Amonke_islandGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	// the engine first: identity type, places ("Server full."), GameModePreLoginEvent
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);

	const UCrewSessionSubsystem* Crew = UCrewSessionSubsystem::Get(this);
	const ECrewJoinRefusal Refusal = Crew ? Crew->EvaluateLogin(*this, bOpenToNewPlayers, Options, UniqueId, MaxPlayerNameLength)
		: ECrewJoinRefusal::None;
	if (Refusal == ECrewJoinRefusal::None)
	{
		return;
	}
	// the refused player's menu shows this text (UCrewSessionSubsystem::ConsumeJoinFailure on its machine)
	ErrorMessage = CrewSession::GetRefusalMessage(Refusal, GameSession ? GameSession->MaxPlayers : 0).ToString();
	// no name, no address, no id in the logs (security.md §6, M7)
	UE_LOG(LogMonkeSession, Log, TEXT("Session: connection refused (%s; %d player(s) in, %d crew member(s) known, expedition %s)"),
		LexToString(Refusal), GetNumPlayers(), Crew ? Crew->GetCrewCount() : 0,
		Crew && Crew->IsExpeditionLaunched() ? TEXT("launched") : TEXT("not launched"));
}

void Amonke_islandGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	UCrewSessionSubsystem* Crew = UCrewSessionSubsystem::Get(this);
	if (Crew && NewPlayer && NewPlayer->PlayerState)
	{
		Crew->NoteLogin(*NewPlayer->PlayerState, bOpenToNewPlayers);
	}
}

void Amonke_islandGameMode::Logout(AController* Exiting)
{
	NameChangeLimiters.Remove(Exiting);
	Super::Logout(Exiting);
}

FString Amonke_islandGameMode::SanitizePlayerName(const FString& NewName) const
{
	return CrewSession::SanitizePlayerName(NewName, MaxPlayerNameLength);
}

void Amonke_islandGameMode::ChangeName(AController* Controller, const FString& NewName, bool bNameChange)
{
	APlayerState* PlayerState = Controller ? Controller->PlayerState.Get() : nullptr;
	if (!PlayerState)
	{
		return;
	}
	FString Name = SanitizePlayerName(NewName);
	if (bNameChange)
	{
		// a rename asked by the player (ServerChangeName, Shipping included): never empty, limited in rate; the logs
		// carry its id, never a name (M7)
		if (Name.IsEmpty() || Name == PlayerState->GetPlayerName())
		{
			UE_LOG(LogMonkeSession, Verbose, TEXT("Session: rename of player %d ignored (empty once filtered, or unchanged)"), PlayerState->GetPlayerId());
			return;
		}
		FRequestRateLimiter& Limiter = NameChangeLimiters.FindOrAdd(Controller);
		if (!Limiter.Consume(GetWorld()->GetTimeSeconds(), NameChangeBurst, NameChangeRefillPerSecond))
		{
			UE_LOG(LogMonkeSession, Verbose, TEXT("Session: rename of player %d refused, too frequent (%d refused)"), PlayerState->GetPlayerId(), Limiter.Refused);
			return;
		}
	}
	else if (Name.IsEmpty())
	{
		// the login name had nothing printable: the engine's default name ("Player" and the player id)
		Name = SanitizePlayerName(FString::Printf(TEXT("%s%i"), *DefaultPlayerName.ToString(), PlayerState->GetPlayerId()));
	}
	if (Name.IsEmpty())
	{
		return;
	}
	Super::ChangeName(Controller, Name, bNameChange);
}

void Amonke_islandGameMode::ProcessServerTravel(const FString& URL, bool bAbsolute)
{
	Super::ProcessServerTravel(URL, bAbsolute);
	// the session goes on on the next map: the crew reconnecting there is let in (T-315)
	if (UCrewSessionSubsystem* Crew = UCrewSessionSubsystem::Get(this))
	{
		Crew->NoteServerTravel(URL, *GetWorld());
	}
}
