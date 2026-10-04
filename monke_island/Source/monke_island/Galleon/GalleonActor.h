// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/IslanderInteractable.h"
#include "GalleonActor.generated.h"

class ABoatActor;
class ACharacter;
class UBoxComponent;
class UGalleonRules;
class UIslandWaterSubsystem;
class UStaticMeshComponent;

/**
 *  The crew's galleon (cahier §24.8, ADR-005), V1 kinematic: anchored where the spawner put it, it follows the sea
 *  level (and the flood) with a cosmetic sway computed on every machine from the server clock. Walkable decks and
 *  cabin (complex collision of SM_Galleon). The interaction key at the boarding ladder takes a player up to the deck
 *  from the water or the barque, and back down. AboardVolume tells who is aboard, CargoVolume what lies aboard; the
 *  barque is moored when it rests at the dock. The helm, the hoist and the departure will build on these.
 */
UCLASS()
class MONKE_ISLAND_API AGalleonActor : public AActor, public IIslanderInteractable
{
	GENERATED_BODY()

public:

	AGalleonActor();

	/** Returns the galleon of a world, or nullptr */
	static AGalleonActor* Get(const UObject* WorldContextObject);

	/** Meshes, volumes, sway and mooring (DA_GalleonRules), replicated with the actor */
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category="Galleon")
	TObjectPtr<UGalleonRules> Rules;

	/** True when the actor stands in the volume of the players aboard */
	UFUNCTION(BlueprintPure, Category="Galleon")
	bool IsActorAboard(const AActor* Actor) const;

	/** Objects with a value lying aboard (UValuableComponent in the cargo volume) */
	UFUNCTION(BlueprintCallable, Category="Galleon")
	void GetCargoAboard(TArray<AActor*>& OutCargo) const;

	/** Value of the cargo aboard */
	UFUNCTION(BlueprintPure, Category="Galleon")
	int32 GetCargoValue() const;

	/** Where the barque lies when moored (world) */
	UFUNCTION(BlueprintPure, Category="Galleon")
	FTransform GetDockTransform() const;

	/** True when this barque rests at the dock: close and nearly still */
	UFUNCTION(BlueprintPure, Category="Galleon")
	bool IsBoatMoored(const ABoatActor* Boat) const;

	/** The barque resting at the dock, or nullptr */
	ABoatActor* FindMooredBoat() const;

	/** Server: brings a barque to the dock, still */
	void MoorBoat(ABoatActor* Boat);

	/** Server: puts a character on the deck (place Index of the crew), standing up from a boat seat first */
	void PlaceOnDeck(ACharacter* Character, int32 Index);

	/** Hull (walkable decks, sockets helm / cargo_hatch / boarding_ladder / barque_dock / bean_spawn) */
	UFUNCTION(BlueprintPure, Category="Galleon")
	UStaticMeshComponent* GetHull() const { return Hull; }

	// IIslanderInteractable: the boarding ladder
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

private:

	/** What the ladder offers this pawn now */
	enum class ELadderMove : uint8 { None, Board, Leave };
	ELadderMove GetLadderMove(const APawn* User) const;

	/** Server: moves a character and what it holds */
	void TeleportCharacter(ACharacter* Character, const FVector& Location, float Yaw) const;

	/** Applies the meshes and the volumes of the rules */
	void ApplyRules();

	/** Anchor point: the ship swings and heaves under it */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Anchor;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Hull;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Sails;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Rigging;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Flag;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Wheel;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Rudder;

	/** Players aboard (shape only, queried) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> AboardVolume;

	/** Cargo aboard (shape only, queried) */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBoxComponent> CargoVolume;

	UPROPERTY(Transient)
	TObjectPtr<UIslandWaterSubsystem> WaterSubsystem;

	bool bRulesApplied = false;
};
