// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BoatRules.generated.h"

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
	float ForwardQuadDrag = 2.1f;

	/** Water drag across the keel (kg/s): the boat does not slide sideways */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Drag", meta=(ClampMin="0"))
	float LateralDrag = 900.0f;

	/** Peak force of one oar stroke, both oars together (N) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0"))
	float RowForceN = 2400.0f;

	/** Peak yaw torque of a turning stroke (N.m) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0"))
	float TurnTorqueNm = 900.0f;

	/** Duration of one stroke, drive + recovery (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0.1", Units="s"))
	float StrokePeriod = 1.2f;

	/** Fraction of the stroke spent pulling (the rest is the recovery, no force) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0.05", ClampMax="1"))
	float StrokeDriveFraction = 0.45f;

	/** Speed above which the oars stop pushing (cm/s): the boat is slow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rowing", meta=(ClampMin="0"))
	float MaxRowSpeed = 160.0f;

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

	/** Speed lost per second and per keel point resting on the ground (fraction) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching", meta=(ClampMin="0"))
	float BeachedDragPerPoint = 0.35f;

	/** Distance below the keel at which a keel point counts as resting on the ground (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching", meta=(ClampMin="0"))
	float GroundContactDistance = 12.0f;

	/** Distance travelled on the ground between two trail decals (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Trail", meta=(ClampMin="1"))
	float TrailSpacing = 25.0f;

	/** Seconds a trail decal stays before it has faded */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Trail", meta=(ClampMin="0", Units="s"))
	float TrailLifeSpan = 90.0f;

	/** Ground speed under which the hull leaves no trail (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Trail", meta=(ClampMin="0"))
	float TrailMinSpeed = 8.0f;

	/** Size of one trail decal: projection depth, width, length (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Beaching|Trail")
	FVector TrailDecalSize = FVector(30.0f, 14.0f, 22.0f);
};
