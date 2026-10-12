// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExecutionScene.generated.h"

class AFortSoldier;
class AGalleonActor;
class UAudioComponent;
class UCameraComponent;
class UExecutionRules;
class UStaticMeshComponent;

/**
 *  The execution of a crew that missed the quota on the last day of its contract (owner request 2026-10-07: "si quota
 *  pas atteint au jour jalon, alors animation des beans mis sur une planche et soldats français les fusillent";
 *  ERunDayVerdict::Executed, DA_ExecutionRules). Spawned by the server on the galleon when the day ends. After the
 *  title, in the black, the living players are put on a plank run out over the sea and a squad of French soldiers comes
 *  on deck facing them; the squad shoulders, fires a volley, the balls kill and throw the bodies into the sea. Every
 *  machine watches through the camera of this actor (UDayTransitionSubsystem); the end screen follows
 *  (UTapaoUISubsystem). Attached to the hull: it sways with the ship.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API AExecutionScene : public AActor
{
	GENERATED_BODY()

public:

	AExecutionScene();

	/** The scene of this world, or null */
	static AExecutionScene* Get(const UObject* WorldContext);

	/** Server: starts the execution of the living players on Galleon (where the first of them stands without one) */
	static AExecutionScene* Begin(UWorld* World, AGalleonActor* Galleon);

	/** DA_ExecutionRules (the class defaults without it) */
	const UExecutionRules* GetRules() const;

	/** Seconds since the end of the day (server clock), -1 before it is known */
	float GetSceneTime() const;

	/** The crew is on the plank and the squad on deck: the camera of this actor is the view */
	bool IsShowing() const;

	/** The scene is over: black, the end screen comes */
	bool IsOver() const;

	/** Title over the picture now and its opacity (empty when none) */
	FText GetTitle(float& OutAlpha) const;

	/** True for a player put on the plank */
	bool IsCondemned(const APawn* Pawn) const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Server: the crew to the plank, the squad on deck and its orders */
	void PlaceEveryone();

	/** Server: the squad of the rules on deck, each soldier on its victim */
	void SpawnSquad();

	/** Every machine: the plank's mesh, size and place */
	void BuildPlank();

	/** Every machine: the camera along its glide, the drums */
	void UpdatePresentation();

	/** The local player on the plank faces the sea and is seen in third person (its rotation is its machine's) */
	void HoldLocalCondemned();

	/** Frame of the places of the rules (the hull this actor is attached to, or this actor) */
	FTransform Frame() const;

	/** Where a condemned stands and the way it faces (world) */
	FTransform CondemnedSpot(int32 Index, float HalfHeight) const;

	/** DA_ExecutionRules (DefaultGame.ini) */
	UPROPERTY(Config)
	FSoftObjectPath RulesPath;

	UPROPERTY(Transient)
	mutable TObjectPtr<UExecutionRules> Rules;

	mutable bool bRulesLoadTried = false;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Plank;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Drums;

	/** Server time of the end of the day */
	UPROPERTY(Replicated)
	double StartTime = -1.0;

	/** The players put on the plank, in order */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<APawn>> Condemned;

	/** Server: the squad */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AFortSoldier>> Squad;

	bool bPlaced = false;
	bool bPlankBuilt = false;
	bool bDrumsDone = false;
};
