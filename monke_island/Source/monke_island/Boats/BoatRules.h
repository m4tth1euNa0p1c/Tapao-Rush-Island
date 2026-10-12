// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "BoatRules.generated.h"

class UMaterialInterface;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;

/**
 *  Tunable rules of a rowboat (DA_BoatRules): mass, buoyancy, load and overload, rowing, beaching, sand trail.
 *  Spec: docs/features/boat-rowing.md (ADR-003 for the deviations from the cahier).
 *  Masses in kg, lengths in cm, forces in newtons, torques in N.m (converted to Unreal units by the boat).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UBoatRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Mass of the empty hull (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mass", meta=(ClampMin="1"))
	float HullMassKg = 220.0f;

	/** Fastest the hull may be pushed out of an overlap (cm/s): a player or a creature bumping into it, a
	 *  ragdoll landing on it; the engine default is unlimited and throws the boat (T-063) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mass", meta=(ClampMin="0"))
	float MaxDepenetrationVelocity = 100.0f;

	/** Weight of one player aboard, standing or seated (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load", meta=(ClampMin="0"))
	float PersonMassKg = 60.0f;

	/** Load (players + objects) above which the boat takes water (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load", meta=(ClampMin="0"))
	float MaxLoadKg = 500.0f;

	/** Water taken per second as soon as the boat is overloaded (kg/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load|Flooding", meta=(ClampMin="0"))
	float FloodRateKgPerS = 15.0f;

	/** Extra water per second for each kilogram above MaxLoadKg (kg/s per kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load|Flooding", meta=(ClampMin="0"))
	float FloodRatePerKgOver = 0.6f;

	/** Water bailed out per second once the load is back under the limit (kg/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load|Flooding", meta=(ClampMin="0"))
	float DrainRateKgPerS = 6.0f;

	/** Water shipped per second while a gunwale is under the sea surface (kg/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load|Flooding", meta=(ClampMin="0"))
	float SwampRateKgPerS = 80.0f;

	/** Water aboard at which the boat sinks and is lost (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load|Flooding", meta=(ClampMin="1"))
	float FloodCapacityKg = 450.0f;

	/** Seconds a sunk boat stays in the world before it is destroyed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load|Flooding", meta=(ClampMin="0", Units="s"))
	float SunkLifeSpan = 20.0f;

	/** Fraction of the buoyancy left once sunk */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load|Flooding", meta=(ClampMin="0", ClampMax="1"))
	float SunkBuoyancyFactor = 0.2f;

	/** Upward speed the seated players are thrown off a sinking boat with (cm/s); a client launches its own pawn the
	 *  same way, so it is not corrected (review R-12) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Load|Flooding", meta=(ClampMin="0", Units="cm/s"))
	float SinkEjectUpSpeed = 300.0f;

	/** Buoyancy of the fully immersed hull, keel to gunwale, expressed as the mass it can hold up (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buoyancy", meta=(ClampMin="1"))
	float FloatCapacityKg = 1100.0f;

	/** Hull depth from the keel to the gunwale (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buoyancy", meta=(ClampMin="1"))
	float HullDepthCm = 80.0f;

	/** Height of the keel in the boat's local space (cm, the mesh origin is the designed waterline) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buoyancy")
	float KeelLocalZ = -30.0f;

	/** Height of the gunwale in the boat's local space (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buoyancy")
	float GunwaleLocalZ = 50.0f;

	/** Hull length and beam used to place the buoyancy points (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buoyancy", meta=(ClampMin="1"))
	FVector2D HullSize = FVector2D(560.0f, 220.0f);

	/** Vertical damping of each buoyancy point while immersed (kg/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buoyancy", meta=(ClampMin="0"))
	float VerticalDampingPerPoint = 120.0f;

	/** Centre of mass offset, below the origin for stability (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buoyancy")
	FVector CenterOfMassOffset = FVector(0.0f, 0.0f, -15.0f);

	/** Angular damping of the hull body */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Buoyancy", meta=(ClampMin="0"))
	float AngularDamping = 1.6f;

	/** Linear water drag along the keel (kg/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drag", meta=(ClampMin="0"))
	float ForwardDrag = 40.0f;

	/** Quadratic water drag along the keel (kg/cm), caps the rowing speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drag", meta=(ClampMin="0"))
	float ForwardQuadDrag = 1.3f;

	/** Water drag of a fully loaded boat relative to the empty one (it sits deeper): heavier is slower,
	 *  about 1 / sqrt(scale) of the empty speed at MaxLoadKg (2.8 -> 60 %) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drag", meta=(ClampMin="1"))
	float FullLoadDragScale = 2.8f;

	/** Water drag across the keel (kg/s): the boat does not slide sideways */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drag", meta=(ClampMin="0"))
	float LateralDrag = 900.0f;

	/** Peak force of one oar stroke, both oars together (N) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0"))
	float RowForceN = 3400.0f;

	/** Peak yaw torque of a turning stroke (N.m) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0"))
	float TurnTorqueNm = 1100.0f;

	/** Duration of one stroke, drive + recovery (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0.1", Units="s"))
	float StrokePeriod = 1.2f;

	/** Fraction of the stroke spent pulling (the rest is the recovery, no force) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0.05", ClampMax="1"))
	float StrokeDriveFraction = 0.45f;

	/** Speed above which the oars stop pushing when empty (cm/s), lower when loaded (FullLoadDragScale) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0"))
	float MaxRowSpeed = 260.0f;

	/** Oar sweep angle on each side of the perpendicular (degrees) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing|Animation", meta=(ClampMin="0", ClampMax="80"))
	float OarSweepDeg = 35.0f;

	/** Oar pitch while the blade is in the water / lifted for the recovery (degrees, negative = blade down) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing|Animation")
	FVector2D OarPitchDeg = FVector2D(-26.0f, -8.0f);

	/** Gentle swell: roll and pitch torques (N.m) and their periods (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Swell", meta=(ClampMin="0"))
	FVector2D SwellTorqueNm = FVector2D(60.0f, 40.0f);

	/** Periods of the swell roll and pitch (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Swell", meta=(ClampMin="0.1"))
	FVector2D SwellPeriod = FVector2D(4.5f, 6.1f);

	/** Reach from which F seats a player on a free seat (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats", meta=(ClampMin="0"))
	float SeatReach = 220.0f;

	/** Prefix of the seat sockets of the hull mesh (Seat_0, Seat_1...) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats")
	FString SeatSocketPrefix = TEXT("Seat_");

	/** Height of the seated character's capsule centre above the seat socket (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats")
	float SeatedCapsuleOffsetCm = 60.0f;

	/** A rider standing up from its seat (F, galleon deck, sinking) is lifted this much above where it sat, so its capsule
	 *  clears the seat before it falls back on the floor, free (cm, T-182) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats", meta=(ClampMin="0", Units="cm"))
	float StandUpLiftCm = 35.0f;

	/** Only the rower sits (owner request 2026-10-03): F takes the oars when nobody rows; the other players stand
	 *  and ride on the boat physically, they cannot sit */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats")
	bool bOnlyRowerSits = true;

	/** Rowing post, where the rower sits (cm, SM_Barque mesh space, +X bow): centred between the gunwales, 0.35 m
	 *  forward of the oarlocks so the bean's short arms (0.32 m) reach the oar handles (maquette bean-anim-library,
	 *  clip A_Bean_Boat_Row). The bean's feet (mesh origin) are put on this point */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats")
	FVector RowerSeatLocation = FVector(41.7, 0.0, 7.2);

	/** The rower faces the stern, as a real rower and as the clip was made; false faces the bow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats")
	bool bRowerFacesStern = true;

	/** Rowing crate the rower sits on (maquette bean-anim-library PROP_RowingCrate, pivot at the bottom centre);
	 *  none when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats")
	TObjectPtr<UStaticMesh> RowingCrateMesh;

	/** Placement of the rowing crate on the floor (cm, SM_Barque mesh space) and its scale: its top is under the
	 *  rower's pelvis (16 cm above the rower's feet in the clip) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats")
	FTransform RowingCrateTransform = FTransform(FRotator::ZeroRotator, FVector(41.7, 0.0, -10.0), FVector(1.0, 1.0, 0.84));

	/** Oar splashes (UOarSplashComponent, cosmetic): material of the droplets (translucent, per instance custom data 0
	 *  = opacity); no splash when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash")
	TObjectPtr<UMaterialInterface> SplashDropletMaterial;

	/** Material of the foam rings on the water (plane, per instance custom data 0 = opacity) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash")
	TObjectPtr<UMaterialInterface> SplashRingMaterial;

	/** Blade entering the water */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash")
	TObjectPtr<USoundBase> OarSplashSound;

	/** Blade leaving the water, dripping */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash")
	TObjectPtr<USoundBase> OarDripSound;

	/** Attenuation of the oar sounds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash")
	TObjectPtr<USoundAttenuation> OarSoundAttenuation;

	/** Middle of the oar blade from the pivot, along the oar (cm, SM_Barque_Oar mesh space) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash")
	float OarBladeCenterCm = 135.0f;

	/** Water along the hull while the boat moves on the water, looped; its volume and pitch follow the speed (owner
	 *  2026-10-07: "ajoute son de barque quand elle navigue sur l'eau"); silent on the sand */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> WakeSound;

	/** Speed of the full wake (cm/s) and its volume then */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="1", Units="cm/s"))
	float WakeFullSpeed = 350.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0"))
	float WakeVolume = 0.7f;

	/** Pitch of the wake at rest (X) and at full speed (Y) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound")
	FVector2D WakePitch = FVector2D(0.85, 1.15);

	/** The wake comes and goes over this time (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sound", meta=(ClampMin="0.05", Units="s"))
	float WakeFadeSeconds = 0.4f;

	/** Droplets thrown when a blade enters the water */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash", meta=(ClampMin="0", ClampMax="60"))
	int32 SplashDroplets = 16;

	/** Speed of those droplets (cm/s), up and away from the blade */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash", meta=(ClampMin="0"))
	float SplashSpeed = 280.0f;

	/** Size of a droplet (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash", meta=(ClampMin="0.1"))
	FVector2D DropletSizeCm = FVector2D(1.5f, 4.0f);

	/** Life of a droplet (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash", meta=(ClampMin="0.05", Units="s"))
	float DropletLife = 0.75f;

	/** Foam ring of the entry: final radius (cm) and life (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash", meta=(ClampMin="1"))
	float RingRadiusCm = 55.0f;

	/** See RingRadiusCm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash", meta=(ClampMin="0.05", Units="s"))
	float RingLife = 1.1f;

	/** While the blade drives through the water: a little spray and a small eddy every this often (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars|Splash", meta=(ClampMin="0.02", Units="s"))
	float WakeInterval = 0.08f;

	/** The oars follow the rower's clip (A_Bean_Boat_Row) when it sends its angles, instead of the procedural stroke */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Oars")
	bool bOarsFollowRower = true;

	/** Extra height of the rower over RowerSeatLocation (cm), to fine tune the sitting pose on the post */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Seats")
	float RowerHeightOffsetCm = 0.0f;

	/** Speed lost per second and per keel point resting on the ground (fraction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching", meta=(ClampMin="0"))
	float BeachedDragPerPoint = 0.35f;

	/** Distance below the keel at which a keel point counts as resting on the ground (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching", meta=(ClampMin="0"))
	float GroundContactDistance = 12.0f;

	/** Push of one player shoving the hull on foot (N): a person leaning into a boat, about 450 N */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Push", meta=(ClampMin="0", Units="N"))
	float PushForcePerPersonN = 450.0f;

	/** Gap between a player's capsule and the hull within which walking into it pushes it (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Push", meta=(ClampMin="0", Units="cm"))
	float PushReach = 45.0f;

	/** How straight a player must walk into the hull to push it (cosine between the walk and the hull direction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Push", meta=(ClampMin="0", ClampMax="1"))
	float PushMinAlignment = 0.5f;

	/** Pushers counted at most (the hull has room for so many hands) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Push", meta=(ClampMin="1"))
	int32 MaxPushers = 4;

	/** Share of the boat's weight lifted off the sand while it is pushed or worked free (bow raised, keel skidding):
	 *  the sand friction drops by as much */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Push", meta=(ClampMin="0", ClampMax="0.9"))
	float SkidLiftFraction = 0.75f;

	/** Friction of a wet keel on coral sand, used to size the pushes (the engine contact does the actual work) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Push", meta=(ClampMin="0", ClampMax="1.5"))
	float SandFriction = 0.6f;

	/** Rowing without moving for this long with the keel on the ground frees the boat (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Unstick", meta=(ClampMin="0", Units="s"))
	float UnstickAfterSeconds = 2.0f;

	/** Speed under which a rowing, grounded boat counts as stuck (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Unstick", meta=(ClampMin="0", Units="cm/s"))
	float StuckSpeed = 15.0f;

	/** Push toward deeper water while freeing a stuck boat (N): a rower working the oars against the sand */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Unstick", meta=(ClampMin="0", Units="N"))
	float UnstickForceN = 1100.0f;

	/** Distance at which the water depth is probed around the hull to find the way out (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Unstick", meta=(ClampMin="50", Units="cm"))
	float UnstickProbeDistance = 450.0f;

	/** Distance travelled on the ground between two trail decals (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Trail", meta=(ClampMin="1"))
	float TrailSpacing = 25.0f;

	/** Seconds a trail decal stays before it has faded */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Trail", meta=(ClampMin="0", Units="s"))
	float TrailLifeSpan = 90.0f;

	/** Objects put in the barque are stowed (owner request 2026-10-04): once an object rests in the cargo volume it
	 *  rides with the hull, whatever the speed, the swell or the storm, until a player takes it (no collision added).
	 *  An object with a UNetRestComponent (props, corpses) is glued to the hull on every machine, its movement no longer
	 *  replicated meanwhile (T-295, review R-05); the others are carried by the server alone, as before */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo")
	bool bContainLooseCargo = true;

	/** An object counts at rest in the hull under this speed against it (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo", meta=(ClampMin="0", Units="cm/s", EditCondition="bContainLooseCargo"))
	float CargoStowMaxSpeed = 80.0f;

	/** Time at rest before the object is stowed (s): one thrown across the hull flies on */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Cargo", meta=(ClampMin="0", Units="s", EditCondition="bContainLooseCargo"))
	float CargoStowDelay = 0.3f;

	/** Ground speed under which the hull leaves no trail (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Trail", meta=(ClampMin="0"))
	float TrailMinSpeed = 8.0f;

	/** Size of one trail decal: projection depth, width, length (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Trail")
	FVector TrailDecalSize = FVector(30.0f, 14.0f, 22.0f);

	/** How a client follows the hull the server simulates (T-295, ADR-003 B-07, review R-05). PredictiveInterpolation:
	 *  the client's hull is driven by velocity toward the server's state carried ahead by the ping, the same forces
	 *  applied in between (swell on the server clock, rowing, load). Default: the engine's older correction, kept as a
	 *  fallback if the PIE trial at 150 ms says so */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network")
	EPhysicsReplicationMode PhysicsReplicationMode = EPhysicsReplicationMode::PredictiveInterpolation;

	/** Network updates per second of the barque (the engine default is 100; review R-05: 30 to 60) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="5", ClampMax="100"))
	float NetUpdateFrequency = 30.0f;
};
