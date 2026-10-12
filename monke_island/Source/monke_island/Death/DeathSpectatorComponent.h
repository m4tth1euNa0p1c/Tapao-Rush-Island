// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "DeathSpectatorComponent.generated.h"

class AActor;
class APawn;
class APlayerController;
class APlayerState;
class UDeathRules;
struct FMinimalViewInfo;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDeathFollowedPlayerChangedSignature, APlayerState*, FollowedPlayer);

/** What the local player's screen shows of death (T-135) */
UENUM(BlueprintType)
enum class EDeathScreen : uint8
{
	/** Alive, or the day is over: its scenes and its summary take over */
	None,
	/** "TU ES MORT" and the cause, until the camera follows a living teammate */
	Dead,
	/** The camera follows a living teammate */
	Following,
	/** NAUFRAGÉ: the whole crew is dead (solo: at the death) */
	Wreck,
};

/**
 *  Death and spectator of a player (T-135; cahier §4 "Les morts deviennent spectateurs, sans influencer l'expédition",
 *  §19 "Mort : cause courte, liste de coéquipiers observables"), on its player controller.
 *  Server authoritative: when the possessed pawn dies, after DeathScreenSeconds the server points the controller's
 *  view at a living teammate (SetViewTarget, sent to the owner by the engine; the server's view point follows him so
 *  what surrounds him stays relevant), changes it on the owner's request (ServerCycle, rate limited) and goes on to the
 *  next one when the followed teammate dies or leaves. Only the view changes, never the world. The owner's camera
 *  manager puts the camera behind the followed teammate (ApplyFollowView). A new living pawn (respawn) ends it.
 *  It follows nobody without an expedition with permadeath (test maps respawn) nor once the expedition is over (the
 *  departure scene, NAUFRAGÉ). Values in DA_DeathRules (UDeathRules).
 */
UCLASS(ClassGroup=(Death), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UDeathSpectatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UDeathSpectatorComponent();

	/** Rules of the death screen and of the follow camera (DA_DeathRules); the class defaults when it is missing */
	UPROPERTY(EditDefaultsOnly, Category="Death")
	TSoftObjectPtr<UDeathRules> RulesAsset;

	/** Fires on the server and on the owner when the followed teammate changes (nullptr: nobody) */
	UPROPERTY(BlueprintAssignable, Category="Death")
	FDeathFollowedPlayerChangedSignature OnFollowedPlayerChanged;

	/** Rules in use */
	const UDeathRules& GetRules() const;

	/** Teammate the camera follows (replicated to the owner), nullptr while alive or on one's own body */
	UFUNCTION(BlueprintPure, Category="Death")
	APlayerState* GetFollowedPlayer() const { return FollowedPlayer; }

	/** Pawn of the followed teammate, nullptr when nobody is followed */
	APawn* GetFollowedPawn() const;

	/** Living teammates the dead owner may follow, by player id (replicated to the owner; empty while alive) */
	const TArray<TObjectPtr<APlayerState>>& GetObservablePlayers() const { return ObservablePlayers; }

	/** The pawn of the owner is dead */
	UFUNCTION(BlueprintPure, Category="Death")
	bool IsOwnerDead() const;

	/** Short cause of the death of the owner's pawn (cahier §19), empty while alive */
	UFUNCTION(BlueprintPure, Category="Death")
	FText DescribeOwnerDeath() const;

	/** Owner (local player): follow the next (Direction > 0) or the previous living teammate; false when not dead */
	UFUNCTION(BlueprintCallable, Category="Death")
	bool RequestCycle(int32 Direction);

	/** Camera of the owner: when the view target is a teammate's pawn, places the view behind him (true); any other
	 *  view (one's own body, a scene camera) is left as it is (false) */
	bool ApplyFollowView(const AActor* ViewTarget, FMinimalViewInfo& InOutPOV, float DeltaTime);

	/**
	 *  Pure: index in CandidateIds (player ids, ascending) of the teammate to follow after CurrentId in Direction
	 *  (> 0 next, < 0 previous), round the end; when CurrentId is not among them (none yet, or he just died) the next
	 *  one in order after it. INDEX_NONE when there is no candidate.
	 */
	static int32 PickFollowIndex(const TArray<int32>& CandidateIds, int32 CurrentId, int32 Direction);

	/** Pure: what the local player's screen shows; NAUFRAGÉ only when the whole crew is dead (T-135) */
	static EDeathScreen DecideScreen(bool bLocalDead, bool bTeamWiped, bool bDayOver, bool bFollowing);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Owner: follow the next (1) or the previous (-1) living teammate */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerCycle(int8 Direction);

	/** Owner: tells the screens */
	UFUNCTION()
	void OnRep_FollowedPlayer();

	/** Server: the controller possesses another pawn (its death is watched, a living one ends the spectator) */
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** Server: the possessed pawn died */
	UFUNCTION()
	void HandleOwnerDeath(AActor* Killer);

private:

	/** Server, every RefreshSeconds while the owner is dead: whom he may follow, the end of the death screen, the death
	 *  or the departure of the followed teammate */
	void Refresh();

	/** Server: follows the next living teammate in Direction; nobody followed when nobody is left (the view stays) */
	void FollowStep(int32 Direction);

	/** Server: the camera follows this teammate's pawn */
	void Follow(APlayerState* Target);

	/** Server: nobody followed any more (the view stays where it is: a living body sets its own on possession) */
	void StopFollowing();

	/** Server: the living teammates of the owner, by player id */
	void GatherLivingTeammates(TArray<APlayerState*>& Out) const;

	/** Server: a dead player may follow a teammate now (an expedition with permadeath that is not over) */
	bool CanFollow() const;

	/** Owning player controller, nullptr when the owner is not one */
	APlayerController* GetPlayerController() const;

	/** Unbinds the death of the watched pawn */
	void StopWatchingPawn();

	/** Followed teammate (owner only) */
	UPROPERTY(ReplicatedUsing=OnRep_FollowedPlayer)
	TObjectPtr<APlayerState> FollowedPlayer;

	/** Living teammates the owner may follow (owner only), refreshed by the server while the owner is dead */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<APlayerState>> ObservablePlayers;

	/** Rules loaded at BeginPlay (nullptr: the class defaults) */
	UPROPERTY(Transient)
	TObjectPtr<UDeathRules> Rules;

	/** Server: pawn whose death is watched */
	TWeakObjectPtr<APawn> WatchedPawn;

	/** Server: pawn of the followed teammate when he was chosen */
	TWeakObjectPtr<APawn> FollowedPawn;

	/** Server times (s): the owner's death, the followed teammate seen dead, the last request of the owner */
	double OwnerDeathTime = -1.0;
	double FollowedDeathTime = -1.0;
	double LastCycleTime = -1.0;

	/** Server: Refresh while the owner is dead */
	FTimerHandle RefreshTimer;

	/** Owner's camera: the teammate it was placed behind, and its smoothed rotation */
	TWeakObjectPtr<const AActor> FollowViewTarget;
	FRotator FollowRotation = FRotator::ZeroRotator;
};
