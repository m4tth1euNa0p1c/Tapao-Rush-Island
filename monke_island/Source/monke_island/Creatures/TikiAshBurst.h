// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TikiAshBurst.generated.h"

class APawn;
class UStaticMeshComponent;
class UTikiRules;

/** What the Veilleur des Cendres throws or leaves (T-275) */
UENUM()
enum class ETikiAshBurstKind : uint8
{
	/** A lump of ash thrown on an arc: it bursts into a cloud that blinds the players inside */
	Cloud,
	/** Embers left on the ground: they burn the foes standing in them */
	Embers
};

/**
 *  Ash of the Veilleur des Cendres (T-275). Server authoritative: the server spawns it (Launch), decides when it bursts,
 *  who is blinded (a veil on that player's screen, UTikiVeilSubsystem) or burnt (damage every EmberTickSeconds to the
 *  foes of the tiki standing in it), and destroys it. Every machine draws it from the replicated launch: the lump on its
 *  arc (the same parabola everywhere, no movement replicated), then the cloud or the embers (meshes and effects of
 *  DA_TikiRules Watcher.Ash). A lump is never homing: it lands where it was aimed. Tick only during the flight.
 */
UCLASS(NotBlueprintable)
class MONKE_ISLAND_API ATikiAshBurst : public AActor
{
	GENERATED_BODY()

public:

	ATikiAshBurst();

	/** Server: a lump thrown by Thrower from From that bursts into a cloud at To, or embers laid at To at once */
	static ATikiAshBurst* Launch(UWorld* World, UTikiRules* InRules, ETikiAshBurstKind InKind, APawn* Thrower, const FVector& From, const FVector& To);

	/** What it is */
	ETikiAshBurstKind GetKind() const { return Kind; }

	/** True once the lump has burst (the cloud, the embers) */
	bool HasBurst() const { return bBurst; }

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Rules (DA_TikiRules): values, look and sounds of the ash */
	UPROPERTY(Replicated)
	TObjectPtr<UTikiRules> Rules;

	/** Cloud or embers */
	UPROPERTY(Replicated)
	ETikiAshBurstKind Kind = ETikiAshBurstKind::Cloud;

	/** Where the lump leaves the hand and where it lands (the actor stands there) */
	UPROPERTY(Replicated)
	FVector_NetQuantize LaunchFrom;

	UPROPERTY(Replicated)
	FVector_NetQuantize LandAt;

	/** Time of the flight (s), 0 for embers; server time of the throw */
	UPROPERTY(Replicated)
	float FlightSeconds = 0.0f;

	UPROPERTY(Replicated)
	float LaunchServerTime = 0.0f;

	/** It has burst: the cloud or the embers are there */
	UPROPERTY(ReplicatedUsing=OnRep_Burst)
	bool bBurst = false;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	/** The lump in flight, and the cloud or the patch of embers (every machine but a dedicated server) */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Lump;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Patch;

	/** When each player was last blinded by this cloud (server) */
	TMap<TWeakObjectPtr<APawn>, double> LastBlindTime;

	FTimerHandle BurstTimer;
	FTimerHandle PulseTimer;
	FTimerHandle EffectTimer;

	/** The burst is drawn on this machine */
	bool bShown = false;

	/** Server: the lump bursts here */
	void Burst();

	/** Server: blinds the players in the cloud, burns the foes in the embers */
	void Pulse();

	/** Every machine: the lump starts its flight */
	void ShowFlight();

	/** Every machine: the cloud or the embers, their sound and effect */
	void ShowBurst();

	/** Every machine: the effect of the cloud or of the embers again, while it lasts */
	void SpawnEffect();

	UFUNCTION()
	void OnRep_Burst();

	/** Every machine: the veil on the screen of Victim when it is the local player there */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBlind(APawn* Victim, float Seconds);

	/** Server world time (s) */
	double ServerNow() const;
};
