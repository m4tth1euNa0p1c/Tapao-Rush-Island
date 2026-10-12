// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "BoatRiderComponent.generated.h"

class ABoatActor;
class ACharacter;
class AHUD;
class UCanvas;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 *  Lets a player character use the boats (ADR-003): holds the replicated seat (F goes through the shared
 *  interaction key, ABoatActor implements IIslanderInteractable), ZQSD / WASD row while holding the oars
 *  (no stamina spent), and a small HUD shows the load, the water aboard and the drowning warning. Added at runtime to every player pawn by UBoatSubsystem, so the
 *  character classes stay untouched; its input actions and mapping contexts are built in code.
 *  The seat is server authoritative and replicated; requests go through this component (owned pawn).
 *  Seat guard (T-182, BoatSeat::Resolve): on every machine the character is kept matching its seat. Seated, it hangs on
 *  the hull with its movement off; seated nowhere, it never hangs on a boat nor keeps the movement off of a seat,
 *  whatever the order of the replicated events (LogMonkeBoat says so when the guard has to fix it). A rider knocked
 *  out on its seat falls off it.
 */
UCLASS(ClassGroup=(Boat), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UBoatRiderComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UBoatRiderComponent();

	/** Priority of the rowing mapping context: above the character's so ZQSD row instead of walking */
	UPROPERTY(EditDefaultsOnly, Category="Boat|Input")
	int32 RowingContextPriority = 10;

	/** Seconds between two rowing input updates sent to the server while it changes */
	UPROPERTY(EditDefaultsOnly, Category="Boat|Input", meta=(ClampMin="0.02", Units="s"))
	float RowInputSendInterval = 0.1f;

	/** Seconds between two sends of an unchanged rowing input while the keys are held (review R-09): the input goes
	 *  unreliably, a lost packet no longer leaves the barque still while Z is held */
	UPROPERTY(EditDefaultsOnly, Category="Boat|Input", meta=(ClampMin="0.05", Units="s"))
	float RowInputResendInterval = 0.25f;

	/** Client: the server threw the owner off a sinking boat (review R-12): the same launch on its own machine, once
	 *  this machine has taken it off its seat, so its predicted movement is not corrected */
	UFUNCTION(Client, Reliable)
	void ClientLaunch(FVector_NetQuantize10 Velocity, bool bXYOverride, bool bZOverride);

	/** Seats the owner (server); called by the boat */
	void SetSeat(ABoatActor* Boat, int32 SeatIndex);

	/** Server: the owner leaves any boat before being moved elsewhere (galleon deck, ladder): its seat is freed, the
	 *  capsule taken off a hull it hangs on, movement and collision given back (T-182) */
	void LeaveBoat();

	/** Boat the owner is seated in, or nullptr */
	UFUNCTION(BlueprintPure, Category="Boat")
	ABoatActor* GetSeatedBoat() const { return SeatedBoat; }

	/** True while seated (in a boat that still exists) */
	UFUNCTION(BlueprintPure, Category="Boat")
	bool IsSeated() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Rowing input of the owner (X turn right, Y forward) */
	UFUNCTION(Server, Unreliable)
	void ServerSetRowInput(FVector2D Input);

	/** Final rowing input, sent reliably when the keys are released */
	UFUNCTION(Server, Reliable)
	void ServerStopRowing();

	/** Applies the replicated seat on every machine */
	UFUNCTION()
	void OnRep_Seat();

private:

	/** Builds the input actions and binds them once the owner is locally controlled */
	bool TryBindInput();

	/** Adds or removes the rowing context with the seat */
	void UpdateRowingContext();

	/** Makes the owner match its replicated seat (BoatSeat::Resolve): sits it, takes it off the boat, or waits for the
	 *  boat; bFromGuard when the tick checks it, which logs what the seat changes should already have done (T-182) */
	void ReconcileSeat(bool bFromGuard);

	/** Attaches the owner to its seat, movement off, capsule without physics, view in third person */
	void SitOnSeat(ACharacter* Character);

	/** Takes the owner off the boat hull it hangs on and gives back movement, capsule collision and view;
	 *  bKeepRagdoll: only off the hull, the ragdoll keeps the movement and the collision until it stands up */
	void ReleaseFromBoat(ACharacter* Character, bool bKeepRagdoll);

	/** Input handlers */
	void OnRowKey(const FInputActionValue& Value, int32 Index, bool bPressed);

	/** Draws the boat and drowning lines on the local HUD */
	void DrawHUD(AHUD* HUD, UCanvas* Canvas);

	/** Boat the owner is seated in (server authoritative) */
	UPROPERTY(ReplicatedUsing=OnRep_Seat)
	TObjectPtr<ABoatActor> SeatedBoat;

	/** Seat index in SeatedBoat */
	UPROPERTY(ReplicatedUsing=OnRep_Seat)
	int32 SeatIndex = INDEX_NONE;

	/** Boat the seat was last applied for on this machine */
	UPROPERTY(Transient)
	TObjectPtr<ABoatActor> AppliedBoat;

	/** This machine sat the owner (movement off, capsule without physics, attached) and has not given it all back */
	bool bSeatApplied = false;

	/** Taken off the hull while knocked out: movement and collision come back once the ragdoll stands up */
	bool bReleaseAfterRagdoll = false;

	/** Input built in code (no asset) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputAction>> RowActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RowingContext;

	/** Direction of each row action (X turn right, Y forward) */
	TArray<FVector2D> RowDirections;

	/** Directions currently held (one per row action) */
	TArray<FVector2D> HeldDirections;


	/** Rowing input last sent and time since it was sent */
	FVector2D SentRowInput = FVector2D::ZeroVector;
	float TimeSinceRowSend = 0.0f;

	/** Launch the server asked for (ClientLaunch), applied once this machine took the owner off its seat */
	FVector PendingLaunch = FVector::ZeroVector;
	bool bPendingLaunchXY = false;
	bool bPendingLaunchZ = false;
	bool bHasPendingLaunch = false;
	double PendingLaunchTime = 0.0;

	/** Applies the pending launch when the owner is off its seat here; dropped after a second */
	void ApplyPendingLaunch();

	/** Input is bound on the owning client */
	bool bInputBound = false;

	/** The rowing context is currently added */
	bool bRowingContextActive = false;

	/** Handle of the HUD draw delegate */
	FDelegateHandle HUDHandle;
};
