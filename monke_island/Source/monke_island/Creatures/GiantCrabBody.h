// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Creatures/GiantCrabTypes.h"
#include "Engine/EngineTypes.h"
#include "GiantCrabBody.generated.h"

class ACharacter;
class UGiantCrabRules;
class UPrimitiveComponent;
class UShapeComponent;
class USkeletalMeshComponent;

/** A hit body built from the rules: its shape, the bone that carries it, what a blow there counts as */
USTRUCT()
struct FGiantCrabBodyPart
{
	GENERATED_BODY()

	/** Capsule or box attached to the bone */
	UPROPERTY(Transient)
	TObjectPtr<UShapeComponent> Shape;

	/** Bone that carries it */
	UPROPERTY(Transient)
	FName Bone;

	/** Zone of a blow there */
	UPROPERTY(Transient)
	EGiantCrabZone Zone = EGiantCrabZone::Shell;
};

/**
 *  Hit bodies of the giant crab (T-261, owner 2026-10-09: "ajoute hit box physique du crabe afin que je puisse le taper
 *  partout et pas traverser ses textures"): capsules and boxes built on every machine from UGiantCrabRules::HitBodies,
 *  attached to the bones of SK_GiantCrab so that they follow its animation (query only, no simulation, no tick).
 *  They block the Pawn channel (never walkable: nobody rides the crab), the cutlass finds them (object type Pawn), the
 *  shots of the rules' channel stop on the shell, eyes, belly and claw joints (and on the legs and claws with
 *  bLimbsBlockShots), the camera channel is ignored otherwise. A blow gets the zone of the body it struck, else of its
 *  bone, else of the body nearest its point.
 *  Characters (T-305, T-328): the bodies follow the clip of each machine and, at head height, they pressed the
 *  capsules down onto the sand ("is stuck and failed to move", normal Z -0.98). The moves of the characters around the
 *  crab ignore them (mask filter AIslanderCharacter::AnimatedHitBodyMaskFilter, added to their capsule by the crab,
 *  IgnoreAnimatedBodies); upright pillars fixed on the root (UGiantCrabRules::PawnPillars: leg tips, claws at rest), from
 *  under the sand to above the tallest character, stop them instead, the same on every machine, alone or in a network
 *  session: they only ever push a character aside. The ground under the shell, between the legs, stays free.
 *  Console (non shipping): monke.GiantCrab.ShowBodies 1 draws the bodies and the pillars of the crabs that appear
 *  afterwards.
 */
UCLASS(ClassGroup=(Creatures))
class MONKE_ISLAND_API UGiantCrabBodyComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UGiantCrabBodyComponent();

	/** Mask filter of the fixed pillars that stop the pawns: a query may ignore them with it (the second look of the
	 *  firearms, which looks for the bodies on the Pawn channel) */
	static constexpr uint8 PawnBlockerMaskFilter = 2;

	/** Builds the bodies of Rules on the bones of Mesh, and the upright pillars that stop the pawns (the previous ones are
	 *  removed); returns how many bodies were built */
	int32 Build(const UGiantCrabRules& Rules, USkeletalMeshComponent* Mesh);

	/** The pillars stop the pawns or let them through (every machine, from the replicated state: through during a
	 *  charge, whose speed no prediction follows) */
	void SetBlockersEnabled(bool bEnabled);

	/** Upright pillars that stop the pawns (fixed on the root) */
	const TArray<TObjectPtr<UShapeComponent>>& GetBlockers() const { return Blockers; }

	/** How deep (cm) Character's capsule is pressed into a shape of the crab that stops its moves: a pillar while they
	 *  are on, a body unless its moves ignore them; 0 when free. OutAway: the way out (unit, the MTD), OutShape: that
	 *  shape */
	float FindPin(const ACharacter& Character, FVector& OutAway, FName& OutShape) const;

	/** Character's moves ignore the animated bodies of every giant crab from now on (its capsule's move ignore mask);
	 *  true when it did not already */
	static bool IgnoreAnimatedBodies(ACharacter& Character);

	/** Zone of a blow: the body Component when it is one of them, else the bone Bone, else the body nearest Point
	 *  (bHasPoint) within the rules' snap distance; the shell otherwise. OutBone: the bone of that body, or Bone */
	EGiantCrabZone ResolveZone(const UPrimitiveComponent* Component, FName Bone, const FVector& Point, bool bHasPoint, FName& OutBone) const;

	/** Shots pass through the bodies from now on (a dead crab on the sand); the characters still bump into them */
	void StopBlockingShots();

	/** The body carried by Bone, nullptr when none */
	const UShapeComponent* FindBody(FName Bone) const;

	/** Bodies built */
	const TArray<FGiantCrabBodyPart>& GetParts() const { return Parts; }

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Removes the bodies built */
	void Clear();

	/** Bodies built */
	UPROPERTY(Transient)
	TArray<FGiantCrabBodyPart> Parts;

	/** Upright pillars on the root that stop the pawns */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShapeComponent>> Blockers;

	/** The pillars stop the pawns now */
	bool bBlockersEnabled = true;

	/** Rules values kept at build time */
	float SnapDistance = 80.0f;
	FString LegToken = TEXT("leg");
	TEnumAsByte<ECollisionChannel> ShotChannel = ECC_Camera;
};
