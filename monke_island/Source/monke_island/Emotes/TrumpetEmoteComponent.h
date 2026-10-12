// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/RequestRateLimiter.h"
#include "TrumpetEmoteComponent.generated.h"

class AFlagCeremonySite;
class UAudioComponent;
class UBeanPuppetComponent;
class UIslanderInventoryComponent;
class UStaticMeshComponent;
class UTexture2D;
class UTrumpetEmoteRules;

/** Part of the trumpet playing */
UENUM(BlueprintType)
enum class ETrumpetPhase : uint8
{
	None,
	Equip,
	/** The single fanfare clip of the free emote V, removed by T-333 (the call on A plays the loop): no longer played */
	Fanfare,
	/** The play loop, as long as it lasts: a flag ceremony, or the trumpet call */
	Ritual,
	Unequip
};

/** What the trumpet plays */
UENUM(BlueprintType)
enum class ETrumpetPlayKind : uint8
{
	None,
	/** The flag ceremony (key F on a flag site): as long as the ritual, the bean held still */
	Ceremony,
	/** The trumpet call (key A, T-333): the bugle call, the stamina given back, then the cooldown */
	Call
};

/** One start of the trumpet, replicated as a whole: every machine plays it from these values */
USTRUCT()
struct FTrumpetPlay
{
	GENERATED_BODY()

	/** Changes at each start (server), so that a start replays even with the values of the last one */
	UPROPERTY()
	uint8 Id = 0;

	/** What plays */
	UPROPERTY()
	ETrumpetPlayKind Kind = ETrumpetPlayKind::None;

	/** Length of the play loop between the equip and the put away (s) */
	UPROPERTY()
	float LoopSeconds = 0.0f;

	/** Server world time of the start (s) */
	UPROPERTY()
	double StartTime = 0.0;

	/** The loop was cut short (ceremony over or interrupted, call broken) this long after the start (s); < 0 while it
	 *  runs to its end */
	UPROPERTY()
	float CutAfter = -1.0f;
};

/**
 *  Herald trumpet of the bean. Every start goes through the server, which puts the item in hand away (taken back at the
 *  end) and replicates the play as a whole (FTrumpetPlay); every machine plays the clips (UBeanAnimDriverComponent),
 *  shows the trumpet in the right hand and sounds it, in third person (the scene reads from behind).
 *  - Trumpet call (key A, T-333, docs/features/trumpet-call.md): the owner asks, the server checks the cooldown and the
 *    bean (alive, on foot, out of the water, standing, nothing else playing), then the call plays CallSeconds: the
 *    military bugle call, the stamina given back on the server and on the owning client alike (both simulate it), the
 *    walk left free. The server breaks it when the bean can no longer play or takes an item in hand. The start of the
 *    last call is replicated to the owner for the icon of the HUD (STapaoSpellSlot).
 *  - Flag ceremony (key F on a flag site, AFlagCeremonySite): the request goes to the server through this component of
 *    the pawn; the site then makes it play for the whole ritual, the bean held still, and stops it early when the
 *    ceremony is interrupted.
 */
UCLASS(ClassGroup=(Islander), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UTrumpetEmoteComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UTrumpetEmoteComponent();

	/** Trumpet, timing, sounds, call and keys (DA_TrumpetEmoteRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	TObjectPtr<UTrumpetEmoteRules> Rules;

	/** Asks the server for the trumpet call (key A); nothing is asked while this machine sees it cannot start */
	UFUNCTION(BlueprintCallable, Category="Trumpet")
	void RequestCall();

	/** Asks the server to start the flag ceremony of the area the pawn stands in (key F there) */
	UFUNCTION(BlueprintCallable, Category="Trumpet")
	void RequestCeremony();

	/** Server: plays for a flag ceremony of Seconds (equip, play loop, put away), whatever played before; false when
	 *  the pawn cannot play (dead, seated, swimming, ragdoll) */
	bool StartRitual(float Seconds);

	/** Server: the ceremony is over or interrupted: the trumpet goes away now */
	void StopRitual();

	/** True while the play loop plays on this machine (a flag ceremony or the trumpet call): the anim driver loops its
	 *  clip at its own pace */
	UFUNCTION(BlueprintPure, Category="Trumpet")
	bool IsRitualPlaying() const { return IsPlaying(); }

	/** True while the trumpet call plays on this machine (equip, bugle call, put away) */
	UFUNCTION(BlueprintPure, Category="Trumpet")
	bool IsCallPlaying() const { return IsPlaying() && LocalKind == ETrumpetPlayKind::Call; }

	/** True while the bean is held in the ceremony (owner request 2026-10-07): third person view, no walking, jumping,
	 *  crouching nor tumbling until the trumpet goes away. The call holds nothing */
	UFUNCTION(BlueprintPure, Category="Trumpet")
	bool IsRitualLocked() const { return bHoldStill; }

	/** Part playing on this machine, with its elapsed time and duration (s) */
	ETrumpetPhase GetPhase(float& OutElapsed, float& OutDuration) const;

	/** The trumpet is playing on this machine */
	UFUNCTION(BlueprintPure, Category="Trumpet")
	bool IsPlaying() const { return LocalKind != ETrumpetPlayKind::None && LocalStart >= 0.0 && GetElapsed() < GetTotal(); }

	/** The call is set up (rules with its action): the HUD shows its icon */
	bool HasCall() const;

	/** Seconds before the next call may start, 0 once ready (server and owner) */
	UFUNCTION(BlueprintPure, Category="Trumpet|Call")
	float GetCallCooldownLeft() const;

	/** Share of the cooldown still to run: 1 as a call starts, 0 once ready (server and owner) */
	UFUNCTION(BlueprintPure, Category="Trumpet|Call")
	float GetCallCooldownFraction() const;

	/** Share of the call still to play while it plays on this machine (1 at its start, 0 at its end); < 0 otherwise */
	UFUNCTION(BlueprintPure, Category="Trumpet|Call")
	float GetCallFractionLeft() const;

	/** The call could start now as far as this machine knows: ready, nothing playing, the bean free to play */
	UFUNCTION(BlueprintPure, Category="Trumpet|Call")
	bool CanCallNow() const;

	/** Key of the call as the local player has it mapped ("A"), for the HUD */
	FText GetCallKeyText() const;

	/** Picture of the call for the HUD, or nullptr */
	UTexture2D* GetCallIcon() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UFUNCTION(Server, Reliable)
	void ServerRequestCall();

	UFUNCTION(Server, Reliable)
	void ServerRequestCeremony(AFlagCeremonySite* Site);

	UFUNCTION()
	void OnRep_PlayState(const FTrumpetPlay& PreviousPlay);

	/** Server: a new play of Kind with a loop of LoopSeconds, whatever played before; the item in hand is put away */
	void StartPlay(ETrumpetPlayKind Kind, float LoopSeconds);

	/** Server: the loop of the play running ends now (the trumpet goes away) */
	void CutPlay();

	/** Can the pawn play at all now (alive, on foot, out of the water, standing) */
	bool CanPlayNow() const;

	/** Server: the call breaks when the bean can no longer play or takes an item in hand */
	void CheckCallOnServer();

	/** Server and owning client: the stamina of the call given so far on this machine */
	void UpdateCallStamina();

	/** Every machine: starts the local playback of PlayState, Late seconds after its start */
	void StartLocal(float Late);

	/** Every machine: the loop ends now, or where the server cut it (no later than it would have) */
	void StopLocalLoop();

	/** Server: the item put away comes back at the end of the playback on this machine */
	void ScheduleFinish();

	/** Server: the playback ends, the item put away comes back */
	void FinishOnServer();

	/** Server: one more request of this player allowed now (RequestBurst, RequestsPerSecond) */
	bool ConsumeRequest(const TCHAR* What);

	void TryBindInput();

	float GetElapsed() const;
	float GetTotal() const;

	/** Time of the server (the game state's estimate on a client), in which the plays and the cooldown are dated */
	double GetServerNow() const;

	/** Length of a whole play (s): equip, loop (to where it was cut), put away */
	float TotalOf(const FTrumpetPlay& InPlay) const;

	/** Trumpet on the right hand of the puppet, shown while playing */
	void UpdateTrumpet();

	/** Enters or leaves the views of a play on this machine: the third person forced (ceremony and call), the movement
	 *  keys ignored by its own controller (ceremony only) */
	void SetPlayView(bool bForceThirdPerson, bool bHold);

	/** What plays, replicated as a whole (server) */
	UPROPERTY(ReplicatedUsing=OnRep_PlayState)
	FTrumpetPlay PlayState;

	/** Server time of the start of the last call (cooldown), replicated to the owner for the HUD */
	UPROPERTY(Replicated)
	double CallStartTime = -1.0e9;

	/** What plays on this machine (None when idle) */
	ETrumpetPlayKind LocalKind = ETrumpetPlayKind::None;

	/** World time of the start on this machine, < 0 when idle */
	double LocalStart = -1.0;

	/** World time the play loop ends on this machine */
	double LocalLoopEnd = 0.0;

	/** World time of the next fanfare of the ceremony on this machine */
	double NextRitualFanfare = 0.0;

	/** Bugle call sounded for the current call */
	bool bSounded = false;

	/** Stamina the current call gave back on this machine so far */
	float CallStaminaGiven = 0.0f;

	/** This play forces the third person view on this machine */
	bool bViewForced = false;

	/** This play holds the bean still on this machine (ceremony): its own controller ignores the movement keys */
	bool bHoldStill = false;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Trumpet;

	/** Last fanfare of the ceremony */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Sound;

	/** Bugle call of the current call (faded out when the call breaks) */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> CallSoundComponent;

	TWeakObjectPtr<UBeanPuppetComponent> Puppet;
	TWeakObjectPtr<UIslanderInventoryComponent> Inventory;

	/** Server: slot put away for the play (INDEX_NONE: none) */
	int32 PutAwaySlot = INDEX_NONE;
	FTimerHandle EndTimer;

	/** Server: requests of this player (call, ceremony) */
	FRequestRateLimiter RequestLimiter;

	/** Key of the call as mapped for the local player, once read from the input subsystem */
	mutable FText CallKeyName;

	bool bInputBound = false;
	int32 BindAttempts = 0;
	FTimerHandle BindTimer;
};
