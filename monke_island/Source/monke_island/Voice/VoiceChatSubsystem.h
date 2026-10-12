// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Online/CoreOnline.h"
#include "Subsystems/WorldSubsystem.h"
#include "Voice/VoiceTypes.h"
#include "VoiceChatSubsystem.generated.h"

class APawn;
class APlayerState;
class IOnlineSession;
class IOnlineVoice;
class USceneComponent;
class USoundAttenuation;
class UVOIPTalker;
class UVoiceChatComponent;
class UVoiceRules;

/**
 *  Proximity voice chat of a game world (T-263, docs/features/chat-vocal.md), built on the engine's VoIP (Voice module,
 *  OnlineSubsystem NULL; nothing is captured, encoded or played by hand here).
 *  - Every machine that plays (not a dedicated server): a local voice session on the world's NULL online subsystem
 *    (the engine's voice only runs inside a session, and the game hosts and joins by address, without one), every
 *    player registered in it; a UVOIPTalker per remote player, its voice coming from the head of that player's pawn
 *    and fading with distance (DA_VoiceRules: full within 3 m, silent from 25 m); who is talking (the engine's talking
 *    state) for the HUD and for the server.
 *  - Every update (UpdateIntervalSeconds) it drives the UVoiceChatComponent of each player controller: the local one
 *    (key, mode, report), and on the server each one (noise of the voice, mute of the dead).
 *  - Ticks only while the outgoing voice must be dropped before the network sends it: the microphone test (heard by
 *    the player alone) and a game without network (nothing takes the voice away).
 *  Debug (non shipping): monke.Voice.Test [1|0], monke.Voice.FakeTalk 1, monke.Voice.Dump; the engine's
 *  OSS.VoiceLoopback 1.
 */
UCLASS()
class MONKE_ISLAND_API UVoiceChatSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Rules loaded by default */
	static const TCHAR* DefaultRulesPath;

	/** Name of the local voice session */
	static const FName VoiceSessionName;

	/** Subsystem of the world of an object, nullptr without a game world */
	static UVoiceChatSubsystem* Get(const UObject* WorldContext);

	/** monke.Voice.FakeTalk: a held push-to-talk key counts as talking (noise test without a microphone); never in
	 *  Shipping */
	static bool IsFakeTalkOn();

	/** Rules in use (DA_VoiceRules, or the class defaults when it is missing) */
	const UVoiceRules& GetRules() const;

	/** The engine hears the local player's voice (above the threshold, while it is sent) */
	bool IsLocalTalking() const { return bLocalTalking; }

	/** The engine of this machine hears this remote player's voice (the listen server hears every client) */
	bool IsHeardTalking(const APlayerState* PlayerState) const;

	/** The voice session of this machine is ready: the engine captures, relays and plays voices */
	bool IsVoiceSessionReady() const { return bSessionReady; }

	/** Indicator of the local microphone for the HUD */
	EVoiceIndicator GetLocalIndicator() const;

	/** Players heard near the local player, one "Name parle" per line, nearest first; empty when nobody talks */
	const FText& GetNearbyTalkersText() const { return NearbyText; }

	/** The local voice chat asks that the outgoing voice be dropped every frame (microphone test, no network) */
	void SetSuppressOutgoingVoice(bool bSuppress) { bSuppressOutgoing = bSuppress; }

	/** Writes the state of the voice chat (counts and flags, no identity) */
	void DumpState(FOutputDevice& Ar) const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;

	virtual bool IsTickable() const override;

	/** The microphone test also runs in the settings menu opened from a paused solo game */
	virtual bool IsTickableWhenPaused() const override { return true; }

	virtual TStatId GetStatId() const override;

private:

	/** Voice of a remote player on this machine */
	struct FRemoteTalker
	{
		TWeakObjectPtr<APlayerState> PlayerState;
		TWeakObjectPtr<UVOIPTalker> Talker;
		TWeakObjectPtr<USceneComponent> Anchor;
		TWeakObjectPtr<APawn> Pawn;
		bool bParked = false;
		bool bWarnedShared = false;
	};

	/** Every UpdateIntervalSeconds: the components, the session, the talkers, the HUD names */
	void Update();

	/** Creates the local voice session once (NULL subsystem), or adopts the game session of another platform */
	void EnsureVoiceSession();

	/** A game without network needs the voice only for a pawn's noise or the microphone test: no microphone in a menu */
	bool NeedsVoiceAlone() const;

	/** The NULL session answered */
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	/** The engine's talking state of a player changed */
	void HandleTalkingStateChanged(FUniqueNetIdRef TalkerId, bool bIsTalking);

	/** Registers the players in the voice session, gives each remote player its talker */
	void SyncPlayers();

	/** Keeps the talker of a remote player registered and its voice on the head of its pawn */
	void UpdateTalker(FRemoteTalker& Entry);

	/** Rebuilds the names of the players heard near the local player when they changed */
	void UpdateNearbyText();

	/** Attenuation of the voices (DA_VoiceRules) */
	USoundAttenuation* GetAttenuation();

	/** Component on the pawn the voice comes from, and its socket (none: the root, offset by the rules) */
	USceneComponent* FindHead(APawn* Pawn, FName& OutSocket) const;

	/** A warning once per subsystem */
	void WarnOnce(bool& bWarned, const FString& Message) const;

	/** Rules (DA_VoiceRules or the class defaults) */
	UPROPERTY(Transient)
	TObjectPtr<UVoiceRules> Rules;

	/** Attenuation built from the rules */
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Attenuation;

	/** Interfaces of the world's online subsystem (weak: the subsystem owns them) */
	TWeakPtr<IOnlineSession, ESPMode::ThreadSafe> SessionInterface;
	TWeakPtr<IOnlineVoice, ESPMode::ThreadSafe> VoiceInterface;

	/** Remote players and their voices */
	TArray<FRemoteTalker> RemoteTalkers;

	/** Players registered in the voice session */
	TArray<FUniqueNetIdRepl> RegisteredIds;

	/** Remote players the engine hears talking */
	TSet<FUniqueNetIdWrapper> TalkingIds;

	/** The local talker of the engine */
	FUniqueNetIdWrapper LocalTalkerId;

	/** Component of the local player controller */
	TWeakObjectPtr<UVoiceChatComponent> LocalComponent;

	/** HUD names, and what they were built from */
	FText NearbyText;
	TArray<FString> NearbyNames;

	FTimerHandle UpdateTimer;
	FDelegateHandle CreateSessionHandle;
	FDelegateHandle TalkingHandle;

	bool bSessionReady = false;
	bool bSessionPending = false;
	bool bSessionFailed = false;
	bool bOwnSession = false;
	bool bLocalTalking = false;
	bool bSuppressOutgoing = false;
	mutable bool bWarnedNoSubsystem = false;
	mutable bool bWarnedNoVoice = false;
	mutable bool bWarnedOtherPlatform = false;
};
