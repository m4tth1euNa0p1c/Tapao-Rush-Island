// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LootCrateRules.generated.h"

class USoundAttenuation;
class USoundBase;
class UStaticMesh;

/**
 *  Wooden loot crate (T-258, lot L2, cahier §5-6; maquette caisse-transport v0.4.0 of the second PC, 0.80 x 0.45 x 0.30 m):
 *  a box the players carry like any heavy object and fill with the loot, to bring it in bulk from the land to the barque.
 *  Every value here, none in the code (DA_LootCrateRules; an actor without rules uses these defaults).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API ULootCrateRules : public UDataAsset
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------ look (pivot of SM_LootCrate: middle of its bottom)

	/** Body of the crate (SM_LootCrate: sockets Lid, LootZone, Slot_00..29, Grab_L / Grab_R, Carry, BoatSlot); used when
	 *  the actor's mesh component has none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UStaticMesh> CrateMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Props/LootCrate/Meshes/SM_LootCrate.SM_LootCrate")));

	/** Lid (SM_LootCrate_Lid: pivot under the middle of its board); used when the actor's lid component has none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Look")
	TSoftObjectPtr<UStaticMesh> LidMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Props/LootCrate/Meshes/SM_LootCrate_Lid.SM_LootCrate_Lid")));

	// ------------------------------------------------------------------ body

	/** Half size of the physics box (cm): the outside of the planks, 80 x 45 x 30 cm (the ropes stand out a little) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body")
	FVector BodyHalfSizeCm = FVector(40.0, 22.5, 15.0);

	/** Mass of the empty crate, lid included (kg); what lies inside adds its own */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin="0.1", Units="kg"))
	float EmptyMassKg = 8.0f;

	/** Damping of the body as it moves and turns */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin="0"))
	float LinearDamping = 0.1f;

	/** See LinearDamping */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Body", meta=(ClampMin="0"))
	float AngularDamping = 0.3f;

	// ------------------------------------------------------------------ capacity

	/** Objects the crate holds at most */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capacity", meta=(ClampMin="1"))
	int32 MaxItems = 12;

	/** Mass of the objects inside at most (kg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capacity", meta=(ClampMin="0", Units="kg"))
	float MaxContentKg = 60.0f;

	/** Inside of the crate (cm, full size: 74.4 x 39.4 x 26.6): an object goes in when its box fits, turned any way */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capacity")
	FVector InnerSizeCm = FVector(74.4, 39.4, 26.6);

	/** Only objects with a value (UValuableComponent: the loot); false lets in any free object that fits */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capacity")
	bool bOnlyValuables = false;

	/** Actors with this tag never go in a crate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Capacity")
	FName RefusedTag = TEXT("NoCrate");

	// ------------------------------------------------------------------ lid

	/** Socket of the body where the closed lid sits (its pivot); LidClosedCm when the mesh has no such socket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lid")
	FName LidSocket = TEXT("Lid");

	/** See LidSocket (cm, frame of the body mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lid")
	FVector LidClosedCm = FVector(0.0, 0.0, 30.0);

	/** Hinge of the lid (cm, frame of the body mesh): a line along X on the top edge of the front face (+Y; the ropes the
	 *  bean holds are on the back face, -Y); the lid opens away from that line */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lid")
	FVector LidHingeCm = FVector(0.0, 22.5, 30.0);

	/** How far the lid opens when an object goes in or the crate is emptied (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lid", meta=(ClampMin="0", ClampMax="180", Units="deg"))
	float LidOpenAngleDeg = 105.0f;

	/** Time to open, to stay open and to shut (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lid", meta=(ClampMin="0", Units="s"))
	float LidOpenSeconds = 0.2f;

	/** See LidOpenSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lid", meta=(ClampMin="0", Units="s"))
	float LidHoldSeconds = 0.35f;

	/** See LidOpenSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Lid", meta=(ClampMin="0", Units="s"))
	float LidCloseSeconds = 0.3f;

	// ------------------------------------------------------------------ emptying

	/** Crouched, the interaction key empties the crate (standing, it lifts it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Empty")
	bool bEmptyWhenCrouched = true;

	/** The objects come out in a fan in front of the crate, toward the player: distance from its centre (cm), angle between
	 *  two neighbours (deg), objects per row and gap between two rows (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Empty", meta=(ClampMin="0", Units="cm"))
	float EmptyDistanceCm = 70.0f;

	/** See EmptyDistanceCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Empty", meta=(ClampMin="0", ClampMax="60", Units="deg"))
	float EmptySpreadDeg = 22.0f;

	/** See EmptyDistanceCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Empty", meta=(ClampMin="1"))
	int32 EmptyPerRow = 6;

	/** See EmptyDistanceCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Empty", meta=(ClampMin="0", Units="cm"))
	float EmptyRowStepCm = 30.0f;

	/** Each object is put this high above the ground found under its place (cm): a short drop, no damage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Empty", meta=(ClampMin="0", Units="cm"))
	float EmptyDropHeightCm = 3.0f;

	// ------------------------------------------------------------------ texts

	/** Prompt of the interaction key on the crate, empty hands ({Count} objects, {Mass} kg with the crate) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText CarryPrompt = NSLOCTEXT("LootCrate", "Carry", "Porter la caisse ({Count} {Count}|plural(one=objet,other=objets), {Mass} kg)");

	/** Prompt crouched, when the crate holds something ({Count} objects) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText EmptyPrompt = NSLOCTEXT("LootCrate", "Empty", "Vider la caisse ({Count} {Count}|plural(one=objet,other=objets))");

	/** Prompt while holding an object aimed at the crate ({Count} inside, {Max} at most) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText StorePrompt = NSLOCTEXT("LootCrate", "Store", "Ranger dans la caisse ({Count}/{Max})");

	// ------------------------------------------------------------------ sounds (optional)

	/** Played where the crate is when an object goes in (none when empty) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundBase> StoreSound;

	/** Played where the crate is when it is emptied (none when empty) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundBase> EmptySound;

	/** Attenuation of both sounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sounds")
	TObjectPtr<USoundAttenuation> SoundAttenuation;
};
