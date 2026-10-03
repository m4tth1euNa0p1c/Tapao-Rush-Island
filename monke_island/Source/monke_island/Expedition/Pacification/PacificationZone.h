// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Expedition/Pacification/PacificationTypes.h"
#include "PacificationZone.generated.h"

class APawn;
class ARoyalFlag;
class AController;
class UDamageType;
class UPacificationRules;
class USceneComponent;

/** Zone pacified: the expedition clock grants its bonus (once per expedition and zone) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnZonePacifiedSignature, FName, ZoneId, double, ServerTime);

/** Native twin of OnZonePacified, for C++ systems (expedition clock) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnZonePacifiedNative, FName /*ZoneId*/, double /*ServerTime*/);

/** Sermon progress moved (0 when it was reset, 1 when it completed) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnZoneProgressChangedSignature, FName, ZoneId, float, Progress01);

/** Zone state changed */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnZoneStateChangedSignature, FName, ZoneId, EZoneState, NewState);

/** Sermon interrupted and reset to zero (server only) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSermonInterruptedSignature, FName, ZoneId, ESermonInterruptReason, Reason);

/** Native twin of OnSermonInterrupted */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSermonInterruptedNative, FName /*ZoneId*/, ESermonInterruptReason /*Reason*/);

/**
 *  A pacifiable zone (cahier §6.2, §22 « Sermon »): designated threats fixed when the objective starts, a flag socket
 *  where the British flag is planted once they are all dead, then a 20 s sermon recited by one officiant standing
 *  still. Server authoritative: every request goes through TryPlantFlag / TryStartSermon / StopSermon (called by the
 *  PlayerController server RPC), the server checks the interruptions every frame of the sermon and validates the
 *  deadline itself; client animations or sounds never validate anything. OnZonePacified fires once per zone.
 *  The zone does not grant time itself: the expedition clock listens to OnZonePacified.
 */
UCLASS(Blueprintable)
class MONKE_ISLAND_API APacificationZone : public AActor
{
	GENERATED_BODY()

public:

	APacificationZone();

	/** Starts the objective with these designated threats, fixed for the rest of the expedition (server, Undiscovered only) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pacification")
	EPacificationRequestResult BeginObjective(const TArray<AActor*>& Threats);

	/** Plants the flag on the socket for this pawn when every designated threat is dead (server) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pacification")
	EPacificationRequestResult TryPlantFlag(APawn* Planter);

	/** Starts the sermon for this pawn at the foot of the planted flag; one officiant per flag (server) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pacification")
	EPacificationRequestResult TryStartSermon(APawn* Officiant);

	/** The pawn released the sermon action: resets the progress if it was the officiant (server) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pacification")
	void StopSermon(APawn* Officiant);

	/** The zone went under water: stops any sermon, no pacification any more (server) */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Pacification")
	void SetSubmerged();

	/** Runs the server checks of the sermon at this server time: interruptions, then the deadline (called every frame of the sermon) */
	void ServerUpdateSermon(double ServerTime);

	/** Identifier of the zone, unique in the expedition */
	UFUNCTION(BlueprintPure, Category="Pacification")
	FName GetZoneId() const { return ZoneId; }

	/** Current state of the zone */
	UFUNCTION(BlueprintPure, Category="Pacification")
	EZoneState GetZoneState() const { return ZoneState; }

	/** Planted flag, or null */
	UFUNCTION(BlueprintPure, Category="Pacification")
	ARoyalFlag* GetFlag() const { return Flag; }

	/** Player reciting the sermon, or null */
	UFUNCTION(BlueprintPure, Category="Pacification")
	APawn* GetOfficiant() const { return Sermon.bActive ? Sermon.Officiant.Get() : nullptr; }

	/** Share of the sermon recited (0-1), smooth on clients from the replicated start time */
	UFUNCTION(BlueprintPure, Category="Pacification")
	float GetSermonProgress01() const;

	/** Designated threats still alive (replicated count, for the HUD) */
	UFUNCTION(BlueprintPure, Category="Pacification")
	int32 GetRemainingThreats() const { return RemainingThreats; }

	/** Whether the pacification bonus was already announced for this zone */
	UFUNCTION(BlueprintPure, Category="Pacification")
	bool WasPacificationAwarded() const { return bPacificationAwarded; }

	/** Rules of the pacification */
	const UPacificationRules* GetRules() const { return Rules; }

	/** Sets the rules (server, before the objective starts; used by spawners and tests) */
	void SetRules(const UPacificationRules* InRules);

	/** Sets the zone identifier (before the objective starts; used by spawners and tests) */
	void SetZoneId(FName InZoneId) { ZoneId = InZoneId; }

	/** Whether a designated threat still counts as alive: valid, not being destroyed, and IPacificationParticipant says so */
	virtual bool IsThreatAlive(AActor* Threat) const;

	/** Whether the officiant still counts as alive (same test as the threats by default) */
	virtual bool IsOfficiantAlive(APawn* Officiant) const;

	/** Number of designated threats still alive (server) */
	int32 CountLivingThreats() const;

	/** Whether a living designated threat stands inside the perimeter around the flag socket (server) */
	bool IsThreatInPerimeter() const;

	/** Zone pacified (server only, once) */
	UPROPERTY(BlueprintAssignable, Category="Pacification")
	FOnZonePacifiedSignature OnZonePacified;

	/** Zone pacified, for C++ listeners (server only, once) */
	FOnZonePacifiedNative OnZonePacifiedNative;

	/** Sermon progress moved (server and clients) */
	UPROPERTY(BlueprintAssignable, Category="Pacification")
	FOnZoneProgressChangedSignature OnZoneProgressChanged;

	/** Zone state changed (server and clients) */
	UPROPERTY(BlueprintAssignable, Category="Pacification")
	FOnZoneStateChangedSignature OnZoneStateChanged;

	/** Sermon interrupted (server only) */
	UPROPERTY(BlueprintAssignable, Category="Pacification")
	FOnSermonInterruptedSignature OnSermonInterrupted;

	/** Sermon interrupted, for C++ listeners (server only) */
	FOnSermonInterruptedNative OnSermonInterruptedNative;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Root of the zone */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pacification")
	TObjectPtr<USceneComponent> Root;

	/** Socket where the flag is planted: the centre of the perimeter and of the flag reach */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pacification")
	TObjectPtr<USceneComponent> FlagSocket;

	/** Rules of the pacification (DA_PacificationRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Pacification")
	TObjectPtr<const UPacificationRules> Rules;

	/** Flag spawned on the socket */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pacification")
	TSubclassOf<ARoyalFlag> FlagClass;

	/** Identifier of the zone, unique in the expedition */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Category="Pacification")
	FName ZoneId;

	/** Threats placed in the level, designated when the objective starts on BeginPlay */
	UPROPERTY(EditInstanceOnly, Category="Pacification")
	TArray<TObjectPtr<AActor>> InitialThreats;

	/** Starts the objective on BeginPlay with InitialThreats (otherwise a discovery trigger calls BeginObjective) */
	UPROPERTY(EditAnywhere, Category="Pacification")
	bool bBeginObjectiveOnBeginPlay = false;

private:

	/** Current state */
	UPROPERTY(ReplicatedUsing=OnRep_ZoneState)
	EZoneState ZoneState = EZoneState::Undiscovered;

	/** Running sermon */
	UPROPERTY(ReplicatedUsing=OnRep_Sermon)
	FSermonState Sermon;

	/** Replicated sermon progress, sent in ProgressReplicationStep steps (0-1) */
	UPROPERTY(ReplicatedUsing=OnRep_SermonProgress)
	float SermonProgress = 0.0f;

	/** Planted flag */
	UPROPERTY(Replicated)
	TObjectPtr<ARoyalFlag> Flag;

	/** Designated threats still alive */
	UPROPERTY(Replicated)
	int32 RemainingThreats = 0;

	/** Designated threats fixed at the start of the objective (server only, not owned) */
	TArray<TWeakObjectPtr<AActor>> DesignatedThreats;

	/** Officiant whose damage and destruction events are bound (server only) */
	TWeakObjectPtr<APawn> BoundOfficiant;

	/** True once OnZonePacified was broadcast: never twice for this zone */
	bool bPacificationAwarded = false;

	/** Server time used for the sermon */
	double GetServerTime() const;

	/** Changes the state and tells the listeners (server) */
	void SetZoneState(EZoneState NewState);

	/** Stops the sermon, resets the progress to zero and tells why (server) */
	void InterruptSermon(ESermonInterruptReason Reason);

	/** Validates the sermon: state Pacified and OnZonePacified once (server) */
	void CompleteSermon(double ServerTime);

	/** Updates the replicated progress when it moved enough (server) */
	void SetSermonProgress(float NewProgress, bool bForce);

	/** Binds the damage and destruction events of the officiant (server) */
	void BindOfficiant(APawn* Officiant);

	/** Unbinds them (server) */
	void UnbindOfficiant();

	/** Horizontal distance from the flag socket (cm) */
	double HorizontalDistanceToSocket(const FVector& Location) const;

	/** Damage taken by the officiant */
	UFUNCTION()
	void HandleOfficiantDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	/** Officiant pawn destroyed */
	UFUNCTION()
	void HandleOfficiantDestroyed(AActor* DestroyedActor);

	/** State replicated */
	UFUNCTION()
	void OnRep_ZoneState();

	/** Sermon replicated */
	UFUNCTION()
	void OnRep_Sermon();

	/** Progress replicated */
	UFUNCTION()
	void OnRep_SermonProgress();
};
