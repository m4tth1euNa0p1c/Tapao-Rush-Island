// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "Capture/CaptureTypes.h"
#include "CaptureCage.generated.h"

class ACapturePrisoner;
class APlayerState;
class UBoxComponent;
class UCaptureRules;
class UInventoryItemDefinition;
class UStaticMeshComponent;
class UTextRenderComponent;

/**
 *  A prisoner's cage set on the beach (T-336) for DA_CaptureRules.CageLifetimeSeconds (2 minutes), shared by the whole
 *  crew: a player leading a prisoner within SellRadiusCm presses the interaction key on it ("Mettre le prisonnier en
 *  cage"); the door opens, the prisoner walks in, the door closes, SalePrice crowns go to the crew's treasury
 *  (URunSubsystem::AddMoney, server; the money the merchant takes), the whole crew is told, and the prisoner is taken
 *  away TakenAwaySeconds later. When its time is up the cage goes back to the inventory of the one who set it (when
 *  there is room, else it lies folded on the ground).
 *  Server authoritative: the sale, the money and the time; always relevant (few, short lived), replicates its time, its
 *  door and its occupant; every machine swings the door and shows the time left above it.
 */
UCLASS()
class MONKE_ISLAND_API ACaptureCage : public AActor, public IIslanderInteractable
{
	GENERATED_BODY()

public:

	ACaptureCage();

	/** Capture rules (DA_CaptureRules), set when it is placed and replicated once */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Capture")
	TObjectPtr<UCaptureRules> Rules;

	/** Server: sets a cage down at Transform for Placer; ReturnDefinition comes back to it when the cage goes */
	static ACaptureCage* PlaceCage(UWorld* World, UCaptureRules* InRules, const FTransform& Transform, APawn* Placer,
		const UInventoryItemDefinition* ReturnDefinition);

	/** Its time is up: it is leaving */
	UFUNCTION(BlueprintPure, Category="Capture")
	bool IsGoing() const { return bGoing; }

	/** A prisoner walks in or stands in it: no other sale until it is taken away */
	UFUNCTION(BlueprintPure, Category="Capture")
	bool IsBusy() const { return Occupant != nullptr; }

	/** Seconds before it goes (every machine) */
	UFUNCTION(BlueprintPure, Category="Capture")
	float GetSecondsLeft() const;

	/** In front of its door, outside (world, the prisoner's feet) */
	FVector GetEntryPoint() const;

	/** Middle of its floor (world, the prisoner's feet) */
	FVector GetPrisonerPoint() const;

	/** Yaw of a prisoner standing in it (it faces the door) */
	float GetPrisonerYaw() const;

	/** Server: the prisoner of the sale is in: the door closes, the crew is paid, it is taken away later */
	void OnPrisonerInside(ACapturePrisoner* Prisoner);

	// IIslanderInteractable: the sale of the prisoner led by the player
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

	/** Swings the door, only while it moves */
	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Root at the middle of the floor, on the ground */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> Root;

	/** Bars and floor (SM_Capture_Cage) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CageMesh;

	/** Hinges of the door (socket door_hinge), turned to open it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> DoorHinge;

	/** The door (SM_Capture_Cage_Door) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	/** What the crosshair finds (the bars alone are thin): blocks the visibility trace only */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> InteractionBox;

	/** Time left over the cage */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTextRenderComponent> TimerText;

	/** Server world time it goes (replicated once) */
	UPROPERTY(Replicated)
	double ExpireServerTime = 0.0;

	/** Name of the player who set it (the message of its arrival) */
	UPROPERTY(Replicated)
	FString PlacerName;

	/** Its door is open */
	UPROPERTY(ReplicatedUsing=OnRep_DoorOpen)
	bool bDoorOpen = false;

	/** The prisoner walking in or standing in it */
	UPROPERTY(Replicated)
	TObjectPtr<ACapturePrisoner> Occupant;

	/** Its time is up */
	UPROPERTY(ReplicatedUsing=OnRep_Going)
	bool bGoing = false;

	/** Server: the item that comes back to the one who set it */
	UPROPERTY(Transient)
	TObjectPtr<const UInventoryItemDefinition> ReturnDefinition;

	/** Server: who set it (its player follows its pawns: a respawned player gets the cage back) */
	TWeakObjectPtr<APlayerState> PlacerState;
	TWeakObjectPtr<APawn> PlacerPawn;

	/** Server: who sells the prisoner walking in */
	TWeakObjectPtr<APawn> Seller;

	/** Every machine: the door swings */
	UFUNCTION()
	void OnRep_DoorOpen();

	/** Every machine: the cage hides, the crew is told */
	UFUNCTION()
	void OnRep_Going();

	/** A sale, for the whole crew */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSold(const FString& SellerName, ECaptiveKind Kind, int32 Amount, int32 Treasury);

	/** Every machine: meshes, door hinge, box, clock */
	void ApplyMeshes();

	/** Server: the prisoner of Jailer walks in */
	void StartSale(ACapturePrisoner* Prisoner, APawn* Jailer);

	/** Server: its time is up */
	void Expire();

	/** Server: the sold prisoner is taken away */
	void TakeAway();

	/** Server: a prisoner led close enough goes in by itself (AutoSellRadiusCm) */
	void CheckAutoSell();

	/** Server: the folded cage back to the inventory of the one who set it, else on the ground */
	void ReturnCage();

	/** Every machine with a picture: the time left, turned toward the local view */
	void RefreshTimerText();

	/** World time of the server on this machine */
	double GetServerNow() const;

	/** Every machine: the door at its opening of this frame */
	void ApplyDoor();

	/** Opening of the door on this machine (0 closed, 1 open) */
	float DoorAlpha = 0.0f;

	/** Turn of the hinges with the door closed (from their socket) */
	FQuat DoorClosedRotation = FQuat::Identity;

	FTimerHandle ExpireTimer;
	FTimerHandle TakeAwayTimer;
	FTimerHandle AutoSellTimer;
	FTimerHandle TextTimer;
};
