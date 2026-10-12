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
enum class EGalleonDepartureCue : uint8;
struct FStreamableHandle;

/**
 *  The crew's galleon (cahier §24.8, ADR-005), V1 kinematic: anchored where the spawner put it, it follows the sea
 *  level (and the flood) with a cosmetic sway computed on every machine from the server clock. Walkable decks and
 *  cabin (complex collision of SM_Galleon).
 *  The sway is purely visual (network review T-268 R-04, T-317; cahier §24.3 « son balancement est cosmétique »): the
 *  collision hull (Hull: decks, ladder, volumes, sockets) never rolls, it only follows the water up and down; a copy
 *  of the hull that does not collide (HullVisual), with the sails, the rigging, the flag, the wheel and the rudder,
 *  rolls, pitches and heaves around the crew's deck (DeckSpawnSocket). The players standing on the deck keep a still
 *  base, so their movement draws no correction, and the objects lying there fall asleep (dormancy).
 *  The interaction key at the boarding ladder takes a player up to the deck
 *  from the water or the barque, and back down. AboardVolume tells who is aboard, CargoVolume what lies aboard; the
 *  barque is moored when it rests at the dock. The wheel raises the anchor (T-286): the departure, its vote and its
 *  notice are UGalleonDepartureComponent's (on the expedition game state), its key UGalleonCrewComponent's; the
 *  galleon tells where the wheel is and plays the bell, the foghorn and the ticks of the departure.
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

	/** Objects with a value lying aboard (UValuableComponent in the cargo volume), those inside a loot crate aboard
	 *  included (T-258) */
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

	/** True when this pawn, alive and aboard, stands at the wheel (HelmReach of the helm socket) */
	bool IsAtHelm(const APawn* User) const;

	/** Every machine: plays a sound of the departure for the local player, 2D (the whole crew hears it, ashore too);
	 *  silent on a dedicated server or while the sound is still loading */
	void PlayDepartureCue(EGalleonDepartureCue Cue) const;

	/** Collision hull (walkable decks, sockets helm / cargo_hatch / boarding_ladder / barque_dock / bean_spawn): the base
	 *  of whoever stands aboard; never rendered, never sways (it only follows the water level) */
	UFUNCTION(BlueprintPure, Category="Galleon")
	UStaticMeshComponent* GetHull() const { return Hull; }

	/** Visual hull: the same mesh, no collision, swaying on every machine (sails, rigging, flag, wheel and rudder on it) */
	UFUNCTION(BlueprintPure, Category="Galleon")
	UStaticMeshComponent* GetHullVisual() const { return HullVisual; }

	/**
	 *  Pure (T-317): the sway of the visual hull at ServerTime, as its transform relative to the collision hull: the roll
	 *  (around the bow axis +Y), the pitch and the heave of the rules, turning around Pivot (hull frame, cm), which only
	 *  heaves. Tested by monke_island.Galleon.Sway.
	 */
	static FTransform ComputeVisualSway(const UGalleonRules& InRules, double ServerTime, const FVector& Pivot);

	// IIslanderInteractable: the boarding ladder, and the prompt of the wheel (its key is UGalleonCrewComponent's)
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** What the ladder offers this pawn now */
	enum class ELadderMove : uint8 { None, Board, Leave };
	ELadderMove GetLadderMove(const APawn* User) const;

	/** True when the wheel does something for this pawn now: raise the anchor, or cancel a vote or a notice */
	bool IsHelmUsableBy(const APawn* User) const;

	/** Server: moves a character and what it holds, its view turned on its own machine too (R-11) */
	void TeleportCharacter(ACharacter* Character, const FVector& Location, float Yaw) const;

	/** Every machine but a dedicated server: loads the sounds of the departure ahead (asynchronous) */
	void LoadDepartureSounds();

	/** The sounds of the departure, kept loaded */
	TSharedPtr<FStreamableHandle> DepartureSoundsHandle;

	/** Applies the meshes and the volumes of the rules */
	void ApplyRules();

	/** Anchor point on the water: it follows the sea level and the flood */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Anchor;

	/** Collision hull (complex collision of SM_Galleon), hidden, still on the anchor */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Hull;

	/** Visual hull (the same mesh, no collision), swaying on the anchor; the visual parts hang on it */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> HullVisual;

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

	/** Point the visual hull turns around: the crew's deck (DeckSpawnSocket, hull frame), set with the rules */
	FVector SwayPivot = FVector::ZeroVector;

	bool bRulesApplied = false;
};
