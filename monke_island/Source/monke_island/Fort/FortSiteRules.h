// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FortSiteRules.generated.h"

class UWorld;

/** A place of the atoll a fort can stand on (scripts/map/place_forts.py) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FFortSiteSpot
{
	GENERATED_BODY()

	/** Centre of the fort, on the ground of the heightmap (UE cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	FVector Location = FVector::ZeroVector;

	/** Yaw of the fort (deg): its gate (local +Y) toward the nearest shore */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	float Yaw = 0.0f;
};

/** The places a fort can stand on in one fragment */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FFortFragmentSites
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	FName FragmentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	TArray<FFortSiteSpot> Sites;

	/** A map that builds its fort from these sites when it plays no fragment of the catalog (the test map and its
	 *  section of Raraka, owner 2026-10-08: "ajoute la génération du fort sur cette map") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	TSoftObjectPtr<UWorld> TestMap;
};

/**
 *  The French forts built on the land of the atoll (owner 2026-10-08: "les forts français spawn sur la map de Raraka
 *  Tuamotu et pas sur une île personnalisée", "génère procéduralement"): the land fort level, the places it may stand
 *  on in each fragment (docs/reference/map/raraka/collectibles/fort_sites.json) and how it is set on the ground. The
 *  fort keeps the plateau of its old island (court, moat, buried treasure vault): it stands on it as on a mound, the
 *  highest ground of the atoll under it at TopGroundLocalCm of its frame. DA_FortSiteRules, read by UFortSiteSubsystem.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UFortSiteRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** The fort without its island (LI_FortFrench_Land): plateau, moat, ramparts, buildings, garrison, loot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	TSoftObjectPtr<UWorld> FortLevel;

	/** Forts built in the fragment of an expedition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(ClampMin="0"))
	int32 FortsPerExpedition = 1;

	/** Height, in the fort's frame, the highest ground of the atoll under it is set to (cm): under the floor of the
	 *  treasure vault (66) and the water of the moat (115), the court (165) a little above */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(Units="cm"))
	float TopGroundLocalCm = 50.0f;

	/** The ground is measured at the centre and on these rings (cm), 8 points each */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	TArray<float> GroundRingsCm = { 1500.0f, 3000.0f, 4500.0f };

	/** Share of the ground points (0-1) under the height the fort is set by: 1 = the highest, a little less leaves a
	 *  lone bump out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0", ClampMax="1"))
	float GroundQuantile = 0.9f;

	/** Vegetation taken away around the fort (cm, 2D): ramparts, moat and the plateau around */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0", Units="cm"))
	float ClearRadiusCm = 6500.0f;

	/** No tiki, camp, marae nor temple within this distance of its centre (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0", Units="cm"))
	float QuietRadiusCm = 7000.0f;

	/** Draws of the sites (0: a new draw every expedition) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
	int32 Seed = 0;

	/** Places of the forts, per fragment */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sites")
	TArray<FFortFragmentSites> Fragments;

	/** Sites of a fragment, null when it has none */
	const FFortFragmentSites* FindFragment(FName FragmentId) const
	{
		return Fragments.FindByPredicate([FragmentId](const FFortFragmentSites& Entry) { return Entry.FragmentId == FragmentId; });
	}
};
