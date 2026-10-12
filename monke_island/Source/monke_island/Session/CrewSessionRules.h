// monke_island

#pragma once

#include "CoreMinimal.h"

/** Why the host refuses a connection (cahier §16: distinct errors for the player; T-315) */
enum class ECrewJoinRefusal : uint8
{
	/** Let in */
	None,
	/** An expedition was launched and this player is not one of its crew: joining only at the lobby or between two
	 *  expeditions (cahier §16, §24.2 step 2, §24.5) */
	ExpeditionInProgress,
	/** Every place of the session is taken (MaxPlayers, the host included) */
	SessionFull,
	/** Asked to join as a spectator (SpectatorOnly=1): the game has no such place, it would pass the player limit */
	SpectatorOnly
};

/** Name of a refusal, for the logs */
MONKE_ISLAND_API const TCHAR* LexToString(ECrewJoinRefusal Refusal);

/** How a join failed, seen from the joining machine (cahier §16: refused, version, expired, lost) */
enum class EJoinFailureKind : uint8
{
	/** The host refused the login: its message, or the engine's (full, identity) */
	Refused,
	/** The host and this machine do not run the same version */
	Version,
	/** No answer from the host while joining: wrong address, host gone, firewall */
	Timeout,
	/** The connection dropped while joining */
	Lost
};

/**
 *  Pure rules of the host's session (T-315, security review T-269 M4 and m6): who may join, the key of a crew member,
 *  the name a player keeps, the messages of a refusal. Without a world: tested by monke_island.Session.*.
 */
namespace CrewSession
{
	/** Longest message of a refusal shown in the menu (characters): the text comes from the host, over the network */
	inline constexpr int32 MaxFailureMessageChars = 200;

	/**
	 *  Key of a player in the crew roster: its unique net id when it has one (OSS Null: stable for the whole run of the
	 *  game, across the travels of the session), else its name as the server keeps it. Empty when it has neither.
	 *  Never written to a log nor a file (security.md §6, M7).
	 */
	MONKE_ISLAND_API FString MakeCrewKey(const FString& UniqueId, const FString& PlayerName);

	/**
	 *  Name the server keeps for a player (cahier §20, review m6): control and invisible characters removed (C0, DEL,
	 *  C1, zero-width, bidirectional controls, separators, tags, unpaired surrogates), runs of spaces turned into one
	 *  and trimmed, at most two combining marks in a row, at most MaxChars characters (code points). Empty when nothing
	 *  printable is left.
	 */
	MONKE_ISLAND_API FString SanitizePlayerName(const FString& Raw, int32 MaxChars);

	/** Text from the network made safe to show: the same filter as a name, at most MaxChars characters */
	MONKE_ISLAND_API FString SanitizeMessage(const FString& Raw, int32 MaxChars);

	/** Short name of the map of a travel URL or a package ("/Game/Maps/L_Boot.L_Boot?listen" -> "L_Boot"), the PIE
	 *  prefix removed ("UEDPIE_0_L_Boot" -> "L_Boot") */
	MONKE_ISLAND_API FString MapShortName(const FString& URLOrPackage);

	/** Message sent to a refused player (French, shown by its menu); empty for None */
	MONKE_ISLAND_API FText GetRefusalMessage(ECrewJoinRefusal Refusal, int32 MaxPlayers);

	/** Message for the menu of a player whose join failed: by its kind, and for a refusal from the error the engine
	 *  received (the engine's own strings in French, the host's message kept once filtered, a generic text otherwise) */
	MONKE_ISLAND_API FText DescribeJoinFailure(EJoinFailureKind Kind, const FString& EngineError);
}

/**
 *  Who may join the host's session (T-315; cahier §16 « Rejoindre seulement au hub », §24.2, §24.5), kept by the
 *  host's game instance across the travels of the session (UCrewSessionSubsystem). Pure: tested without a world.
 *  - Lobby maps (L_Boot, later the galleon at the safe zone) let anyone in, within MaxPlayers.
 *  - Once an expedition was launched (the boot travels to an island, or the clock of an island starts), the other maps
 *    let only the known crew in: those present at each launch and those who logged in on such a map. A member who
 *    drops can come back; the travel from island to island reconnects the crew, which passes.
 *  - A new session (a world not reached by a server travel of this one: the menu, « Recommencer ») forgets it all.
 */
struct MONKE_ISLAND_API FCrewRoster
{
	/** An expedition was launched in this session */
	bool bExpeditionLaunched = false;

	/** Keys of the known crew (CrewSession::MakeCrewKey) */
	TSet<FString> Members;

	/** Short name of the map a server travel of this session goes to, empty otherwise */
	FString PendingTravelMap;

	/** A server world of the host starts (its game mode's InitGame). Reached by the pending travel of this session, the
	 *  session goes on (true); otherwise a new session starts: nothing launched, nobody known (false) */
	bool BeginServerWorld(const FString& WorldShortName);

	/** The host sends everybody to this map (ProcessServerTravel) */
	void NoteServerTravel(const FString& DestinationShortName);

	/** The session ends (a menu, a client world): nothing launched, nobody known */
	void Reset();

	/** The crew leaves for an expedition: these players are the crew, added to those already known (a member who missed
	 *  a travel stays known) */
	void Launch(TConstArrayView<FString> PresentKeys);

	/** A player completed its login: on a map that takes only the crew of a launched session (not a lobby), it is one
	 *  of them (it passed the gate when it was open, or it was known) */
	void NoteLogin(const FString& Key, bool bLobby);

	/** Whether this key is a known crew member (an empty key never is) */
	bool IsMember(const FString& Key) const;

	/** What a connection gets: a spectator never, a stranger not on a crew-only map of a launched session, nobody when
	 *  the session is full */
	ECrewJoinRefusal Evaluate(const FString& Key, bool bLobby, bool bSpectatorOnly, bool bSessionFull) const;
};
