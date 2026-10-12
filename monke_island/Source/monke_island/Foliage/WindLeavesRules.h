// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WindLeavesRules.generated.h"

class UMaterialInterface;
class UMaterialParameterCollection;
class UStaticMesh;

/**
 *  Tunable rules of the leaves carried by the wind around the local player (DA_WindLeavesRules).
 *  Cosmetic and local: never replicated, nothing on a dedicated server.
 *  The wind is read from MPC_Wind (WindStrength, WindDirection written by the storm, FoliageBreeze).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UWindLeavesRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Leaf meshes (small two-sided cards); one instanced component each, the leaves share them at random */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals")
	TArray<TSoftObjectPtr<UStaticMesh>> LeafMeshes;

	/** Material forced on the leaf meshes (empty: the meshes' own) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals")
	TSoftObjectPtr<UMaterialInterface> LeafMaterial;

	/** Wind parameters shared with the foliage materials (MPC_Wind) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind")
	TSoftObjectPtr<UMaterialParameterCollection> WindParameters;

	/** Leaves alive at most around the player (budget: one instance each, updated in one batch a frame) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Budget", meta=(ClampMin="0", ClampMax="2000"))
	int32 MaxLeaves = 260;

	/** Leaves born per second in calm weather (light breeze) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float SpawnRateCalm = 3.0f;

	/** Leaves born per second at full storm strength (WindStrength 1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0"))
	float SpawnRateStorm = 60.0f;

	/** Radius around the camera where leaves are born (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="100", Units="cm"))
	float SpawnRadius = 2200.0f;

	/** Leaves farther than this from the camera are recycled (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="100", Units="cm"))
	float RecycleRadius = 3200.0f;

	/** Height above the ground where leaves are born, min / max (cm): from the bushes to the palm crowns */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawn")
	FVector2D SpawnHeight = FVector2D(120.0f, 1400.0f);

	/** Lifetime of a leaf, min / max (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawn")
	FVector2D LifeSeconds = FVector2D(7.0f, 15.0f);

	/** Uniform scale of a leaf mesh, min / max */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawn")
	FVector2D LeafScale = FVector2D(0.7f, 1.35f);

	/** Wind speed carried by the leaves at WindStrength 1 (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion", meta=(ClampMin="0", Units="cm/s"))
	float WindSpeedAtFullStrength = 1100.0f;

	/** Wind strength felt in calm weather (0-1), also the foliage breeze written to MPC_Wind.FoliageBreeze */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion", meta=(ClampMin="0", ClampMax="1"))
	float BreezeStrength = 0.12f;

	/** World yaw the breeze blows toward when MPC_Wind has no direction (degrees): 180 = trade wind toward the west */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion")
	float BreezeTowardYawDeg = 180.0f;

	/** Swirl of the air around the mean wind (cm/s at full strength, half of it in calm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion", meta=(ClampMin="0", Units="cm/s"))
	float Turbulence = 260.0f;

	/** Size of the swirls (cm): larger is smoother */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion", meta=(ClampMin="10", Units="cm"))
	float TurbulenceSize = 450.0f;

	/** Speed at which a leaf falls in still air (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion", meta=(ClampMin="0", Units="cm/s"))
	float FallSpeed = 85.0f;

	/** How fast a leaf takes the speed of the air (1/s): light leaves follow the gusts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion", meta=(ClampMin="0.01"))
	float AirResponse = 1.8f;

	/** Tumbling speed, min / max (degrees/s), faster with the wind */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion")
	FVector2D SpinDegPerSecond = FVector2D(60.0f, 420.0f);

	/** Fraction of the wind speed at which a leaf drifts once fallen on the water */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Motion", meta=(ClampMin="0", ClampMax="1"))
	float WaterDrift = 0.05f;

	/** Leaves cast shadows (off by default: costs shadow-map invalidations for a barely visible gain) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Budget")
	bool bCastShadow = false;

	/** Distance beyond which the leaves are not drawn (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Budget", meta=(ClampMin="0", Units="cm"))
	float CullDistance = 4500.0f;
};
