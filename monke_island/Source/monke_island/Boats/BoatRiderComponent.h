// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

	/** Seats the owner (server); called by the boat */
	void SetSeat(ABoatActor* Boat, int32 SeatIndex);

	/** Boat the owner is seated in, or nullptr */
	UFUNCTION(BlueprintPure, Category="Boat")
	ABoatActor* GetSeatedBoat() const { return SeatedBoat; }

	/** True while seated */
	UFUNCTION(BlueprintPure, Category="Boat")
	bool IsSeated() const { return SeatedBoat != nullptr; }

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

	/** Attaches / detaches the owner and switches its movement and collision */
	void ApplySeat();

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

	/** Boat the seat was last applied for, to restore movement on stand up */
	UPROPERTY(Transient)
	TObjectPtr<ABoatActor> AppliedBoat;

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

	/** Input is bound on the owning client */
	bool bInputBound = false;

	/** The rowing context is currently added */
	bool bRowingContextActive = false;

	/** Handle of the HUD draw delegate */
	FDelegateHandle HUDHandle;
};
