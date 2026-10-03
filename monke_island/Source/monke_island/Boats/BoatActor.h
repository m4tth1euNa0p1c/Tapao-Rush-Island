// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "BoatActor.generated.h"

class ACharacter;
class UBoatRules;
class UBoxComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class UIslandWaterSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBoatSunkSignature, ABoatActor*, Boat);

/**
 *  Physical rowboat (cahier §5, §6.4, §6.5; ADR-003): floats on the island sea level through buoyancy points,
 *  rolls under the players' weight, rows stroke by stroke, takes water and sinks when overloaded,
 *  beaches on the sand and leaves a trail on the ground.
 *  Server authoritative: load, flooding, sinking, seats and the rowing input live on the server; the hull
 *  simulates everywhere with the same forces from replicated state and the server corrects it.
 *  Mesh sockets: Seat_*, Oarlock_L / Oarlock_R (see docs/art/briefs/barque.md).
 *  Interaction key (F, IIslanderInteractable): sits on the closest free seat, or stands up when seated in it.
 */
UCLASS()
class MONKE_ISLAND_API ABoatActor : public AActor, public IIslanderInteractable
{
	GENERATED_BODY()

public:

	ABoatActor();

	/** Rules of this boat (DA_BoatRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boat")
	TObjectPtr<UBoatRules> Rules;

	/** Deferred decal material of the trail left on the sand */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boat|Beaching")
	TObjectPtr<UMaterialInterface> TrailDecalMaterial;

	/** Broadcast on the server and every client when the boat sinks */
	UPROPERTY(BlueprintAssignable, Category="Boat")
	FBoatSunkSignature OnSunk;

	/** Returns the free seat closest to Location within MaxDistance, or INDEX_NONE */
	int32 FindFreeSeat(const FVector& Location, float MaxDistance) const;

	/** Seats a character (server only); the first seated player takes the oars */
	bool SitCharacter(ACharacter* Character, int32 SeatIndex);

	/** Makes a seated character stand up on the floor (server only) */
	void StandCharacter(ACharacter* Character);

	/** Sets the rowing input of a rower (server only): X turns right, Y rows forward */
	void SetRowInput(const ACharacter* Rower, const FVector2D& Input);

	/** Socket of a seat on the hull, NAME_None when out of range */
	FName GetSeatSocket(int32 SeatIndex) const { return SeatSockets.IsValidIndex(SeatIndex) ? SeatSockets[SeatIndex] : NAME_None; }

	/** Height of a seated character's capsule centre above its seat socket (cm) */
	float GetSeatedCapsuleOffset() const;

	/** Returns the hull */
	UFUNCTION(BlueprintPure, Category="Boat")
	UStaticMeshComponent* GetHull() const { return Hull; }

	/** Load aboard: players + objects (kg) */
	UFUNCTION(BlueprintPure, Category="Boat")
	float GetLoadKg() const { return LoadKg; }

	/** Load above which the boat floods (kg) */
	UFUNCTION(BlueprintPure, Category="Boat")
	float GetMaxLoadKg() const;

	/** Water aboard (kg) */
	UFUNCTION(BlueprintPure, Category="Boat")
	float GetBilgeWaterKg() const { return BilgeWaterKg; }

	/** Water aboard in [0, 1] of the amount that sinks the boat */
	UFUNCTION(BlueprintPure, Category="Boat")
	float GetBilgeFraction() const;

	/** True once the boat has sunk */
	UFUNCTION(BlueprintPure, Category="Boat")
	bool IsSunk() const { return bSunk; }

	/** True when this character holds the oars */
	UFUNCTION(BlueprintPure, Category="Boat")
	bool IsRower(const ACharacter* Character) const;

	/** Number of seats (Seat_* sockets) */
	int32 GetSeatCount() const { return SeatSockets.Num(); }

	/** True when a player occupies this seat (replicated) */
	bool IsSeatOccupied(int32 SeatIndex) const { return SeatOccupants.IsValidIndex(SeatIndex) && SeatOccupants[SeatIndex] != nullptr; }

	/** Spawns a boat on the water in front of a location (server); returns nullptr when no water is found */
	static ABoatActor* SpawnOnWaterNear(UWorld* World, const FVector& From, const FVector& Forward, TSubclassOf<ABoatActor> BoatClass);

	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;

	virtual bool CanInteract_Implementation(APawn* User) const override;

	virtual void Interact_Implementation(APawn* User) override;

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Plays the sinking on clients */
	UFUNCTION()
	void OnRep_Sunk();

private:

	/** Buoyancy, damping and water drag */
	void ApplyWaterForces(float DeltaSeconds, float SeaLevel);

	/** Stroke forces from the rowing input */
	void ApplyRowing(float DeltaSeconds);

	/** Weight of the players aboard and of the water shipped */
	void ApplyLoadForces(const TArray<ACharacter*>& People);

	/** Characters standing or seated aboard */
	void GatherPeople(TArray<ACharacter*>& OutPeople, float& OutObjectsKg) const;

	/** Load, flooding and sinking (server) */
	void UpdateLoad(float DeltaSeconds, int32 PersonCount, float ObjectsKg, float SeaLevel);

	/** Keel contacts: friction on the ground and the sand trail */
	void UpdateGroundContact(float DeltaSeconds);

	/** Oars and water aboard */
	void UpdateVisuals(float DeltaSeconds);

	/** Sinks the boat: buoyancy cut, everyone thrown out, destroyed later (server) */
	void Sink();

	/** Common part of the sinking on every machine */
	void PlaySinking();

	/** Removes dead or invalid occupants and hands the oars over (server) */
	void ValidateSeats();

	/** Hull, simulates physics (root) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Hull;

	/** Port oar, pivot at the oarlock */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> OarLeft;

	/** Starboard oar, pivot at the oarlock */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> OarRight;

	/** Water shipped, a plane rising inside the hull */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> BilgeWater;

	/** Volume of the hull interior: what overlaps it is aboard */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> CargoVolume;

	/** Occupant of each seat (nullptr when free) */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ACharacter>> SeatOccupants;

	/** Seat of the rower, INDEX_NONE when nobody rows */
	UPROPERTY(Replicated)
	int32 RowerSeat = INDEX_NONE;

	/** Rowing input of the rower (X turn right, Y forward), replicated for the forces and the oars */
	UPROPERTY(Replicated)
	FVector2D RowInput = FVector2D::ZeroVector;

	/** Load aboard (kg) */
	UPROPERTY(Replicated)
	float LoadKg = 0.0f;

	/** Water aboard (kg) */
	UPROPERTY(Replicated)
	float BilgeWaterKg = 0.0f;

	/** The boat sank */
	UPROPERTY(ReplicatedUsing=OnRep_Sunk)
	bool bSunk = false;

	/** Seat socket names found on the hull */
	TArray<FName> SeatSockets;

	/** Buoyancy points in local space */
	TArray<FVector> FloatPoints;

	/** Keel points used for the ground contact and the trail, in local space */
	TArray<FVector> KeelPoints;

	/** Ground distance travelled by each keel point since its last decal (cm) */
	TArray<float> TrailDistance;

	/** Phase of each oar's stroke [0, 1) */
	FVector2D StrokePhase = FVector2D::ZeroVector;

	/** Elapsed time for the swell */
	float SwellTime = 0.0f;

	/** Time accumulated since the last load update (s) */
	float LoadUpdateTimer = 0.0f;

	/** Provides the sea level */
	UPROPERTY(Transient)
	TObjectPtr<UIslandWaterSubsystem> WaterSubsystem;
};
