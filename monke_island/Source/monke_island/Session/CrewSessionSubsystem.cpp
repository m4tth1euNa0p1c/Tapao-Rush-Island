// monke_island

#include "Session/CrewSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogMonkeSession);

namespace
{
	/** The engine keeps the first 20 characters of the Name option of a login (AGameModeBase::InitNewPlayer) before the
	 *  game mode filters it: the key of a login without unique id is built the same way */
	constexpr int32 EngineLoginNameChars = 20;
}

UCrewSessionSubsystem* UCrewSessionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UCrewSessionSubsystem>() : nullptr;
}

void UCrewSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UCrewSessionSubsystem::HandleNetworkFailure);
	}
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UCrewSessionSubsystem::HandlePostLoadMap);
}

void UCrewSessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	NetworkFailureHandle.Reset();
	PostLoadMapHandle.Reset();
	Roster.Reset();
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------------------------------- host

FString UCrewSessionSubsystem::KeyOf(const APlayerState& PlayerState)
{
	const FUniqueNetIdRepl& Id = PlayerState.GetUniqueId();
	return CrewSession::MakeCrewKey(Id.IsValid() ? Id.ToString() : FString(), PlayerState.GetPlayerName());
}

FString UCrewSessionSubsystem::KeyOfLogin(const FString& Options, const FUniqueNetIdRepl& UniqueId, int32 MaxNameChars)
{
	if (UniqueId.IsValid())
	{
		return CrewSession::MakeCrewKey(UniqueId.ToString(), FString());
	}
	// no online identity: the name it will get, filtered as Amonke_islandGameMode::ChangeName keeps it
	const FString Name = CrewSession::SanitizePlayerName(UGameplayStatics::ParseOption(Options, TEXT("Name")).Left(EngineLoginNameChars), MaxNameChars);
	return CrewSession::MakeCrewKey(FString(), Name);
}

void UCrewSessionSubsystem::BeginServerWorld(const UWorld& World)
{
	if (World.GetNetMode() == NM_Client)
	{
		return;
	}
	const FString Map = CrewSession::MapShortName(World.GetOutermost()->GetName());
	const bool bHadSession = Roster.bExpeditionLaunched || Roster.Members.Num() > 0;
	if (Roster.BeginServerWorld(Map))
	{
		UE_LOG(LogMonkeSession, Log, TEXT("Session: %s reached by a travel of the session (expedition %s, %d crew member(s) known)"), *Map,
			Roster.bExpeditionLaunched ? TEXT("launched") : TEXT("not launched yet"), Roster.Members.Num());
	}
	else if (bHadSession)
	{
		UE_LOG(LogMonkeSession, Log, TEXT("Session: %s starts a new session, the crew of the previous one is forgotten"), *Map);
	}
}

void UCrewSessionSubsystem::NoteServerTravel(const FString& URL, const UWorld& World)
{
	if (World.GetNetMode() == NM_Client)
	{
		return;
	}
	FString Destination = CrewSession::MapShortName(URL);
	if (Destination.IsEmpty())
	{
		// "?restart" and other options alone: the same map again
		Destination = CrewSession::MapShortName(World.GetOutermost()->GetName());
	}
	Roster.NoteServerTravel(Destination);
	UE_LOG(LogMonkeSession, Verbose, TEXT("Session: server travel to %s, the session goes on there"), *Destination);
}

void UCrewSessionSubsystem::LaunchExpedition(const AGameStateBase& GameState)
{
	if (GameState.GetNetMode() == NM_Client)
	{
		return;
	}
	TArray<FString> Keys;
	for (const APlayerState* PlayerState : GameState.PlayerArray)
	{
		if (PlayerState && !PlayerState->IsABot() && !PlayerState->IsOnlyASpectator())
		{
			Keys.Add(KeyOf(*PlayerState));
		}
	}
	const bool bFirst = !Roster.bExpeditionLaunched;
	const int32 Before = Roster.Members.Num();
	Roster.Launch(Keys);
	UE_LOG(LogMonkeSession, Log, TEXT("Session: expedition %s with %d player(s): %d crew member(s) known (%d new); a map out of the lobby takes only them"),
		bFirst ? TEXT("launched") : TEXT("launched again"), Keys.Num(), Roster.Members.Num(), Roster.Members.Num() - Before);
}

void UCrewSessionSubsystem::NoteLogin(const APlayerState& PlayerState, bool bLobby)
{
	if (PlayerState.GetNetMode() == NM_Client || PlayerState.IsABot())
	{
		return;
	}
	const int32 Before = Roster.Members.Num();
	Roster.NoteLogin(KeyOf(PlayerState), bLobby);
	if (Roster.Members.Num() > Before)
	{
		UE_LOG(LogMonkeSession, Log, TEXT("Session: a player let in before the launch joins the crew (%d known)"), Roster.Members.Num());
	}
}

ECrewJoinRefusal UCrewSessionSubsystem::EvaluateLogin(const AGameModeBase& GameMode, bool bLobby, const FString& Options,
	const FUniqueNetIdRepl& UniqueId, int32 MaxNameChars) const
{
	const bool bSpectatorOnly = UGameplayStatics::GetIntOption(Options, TEXT("SpectatorOnly"), 0) == 1;
	AGameSession* Session = GameMode.GameSession.Get();
	const bool bFull = Session && Session->AtCapacity(false);
	return Roster.Evaluate(KeyOfLogin(Options, UniqueId, MaxNameChars), bLobby, bSpectatorOnly, bFull);
}

void UCrewSessionSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!World || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	const ENetMode Mode = World->GetNetMode();
	if ((Mode == NM_Standalone || Mode == NM_Client)
		&& (Roster.bExpeditionLaunched || Roster.Members.Num() > 0 || !Roster.PendingTravelMap.IsEmpty()))
	{
		Roster.Reset();
		UE_LOG(LogMonkeSession, Log, TEXT("Session: this machine hosts no more (%s), the crew of its session is forgotten"),
			Mode == NM_Client ? TEXT("it joined a host") : TEXT("a world alone"));
	}
}

// ---------------------------------------------------------------------------------------------------- joining player

void UCrewSessionSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	// only a join of this game instance under way: its pending net game (the engine reports it without a world)
	const UGameInstance* GameInstance = GetGameInstance();
	const FWorldContext* Context = GameInstance ? GameInstance->GetWorldContext() : nullptr;
	const UPendingNetGame* Pending = Context ? Context->PendingNetGame.Get() : nullptr;
	if (!NetDriver || !Pending || Pending->NetDriver != NetDriver)
	{
		return;
	}
	EJoinFailureKind Kind = EJoinFailureKind::Refused;
	switch (FailureType)
	{
	case ENetworkFailure::PendingConnectionFailure:
	case ENetworkFailure::FailureReceived:
		Kind = EJoinFailureKind::Refused;
		break;
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
		Kind = EJoinFailureKind::Version;
		break;
	case ENetworkFailure::ConnectionTimeout:
		Kind = EJoinFailureKind::Timeout;
		break;
	case ENetworkFailure::ConnectionLost:
		Kind = EJoinFailureKind::Lost;
		break;
	default:
		return;
	}
	// the engine follows a precise cause (version, timeout) with a generic failure of the same join: the cause stays
	if (Kind == EJoinFailureKind::Refused && JoinFailureDriver.Get() == NetDriver && JoinFailureKind != EJoinFailureKind::Refused && !JoinFailure.IsEmpty())
	{
		return;
	}
	JoinFailure = CrewSession::DescribeJoinFailure(Kind, ErrorString);
	JoinFailureDriver = NetDriver;
	JoinFailureKind = Kind;
	// the text may come from the host: not logged (it is in LogNet already, as the engine received it)
	UE_LOG(LogMonkeSession, Log, TEXT("Session: the join failed (%s), the menu tells the player why"), ENetworkFailure::ToString(FailureType));
}

FText UCrewSessionSubsystem::ConsumeJoinFailure()
{
	FText Message = JoinFailure;
	JoinFailure = FText::GetEmpty();
	JoinFailureDriver.Reset();
	JoinFailureKind = EJoinFailureKind::Refused;
	return Message;
}
