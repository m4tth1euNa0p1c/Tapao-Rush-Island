// monke_island

#pragma once

#include "CoreMinimal.h"
#include "ExpeditionIslandTypes.generated.h"

MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeIslands, Log, All);

/** Why an expedition island (fragment, ADR-004) could not be loaded, validated, selected or travelled to */
UENUM(BlueprintType)
enum class EFragmentError : uint8
{
	/** Success */
	None,
	/** The Asset Manager is not initialized yet */
	AssetManagerUnavailable,
	/** DA_IslandCatalog is not registered or could not be loaded */
	CatalogNotFound,
	/** The catalog lists no fragment */
	CatalogEmpty,
	/** The catalog lists fragments, but none of them is valid */
	NoValidFragment,
	/** The catalog has not been loaded yet */
	CatalogNotLoaded,
	/** No island definition carries this fragment id, or it could not be loaded */
	FragmentNotFound,
	/** The fragment id is empty or malformed */
	InvalidFragmentId,
	/** The definition has no map */
	MapMissing,
	/** The definition has no pacifiable zone */
	InvalidZoneCount,
	/** Too few anchorages, duplicated ids, or no anchorage reachable during the submersion (cahier §6.4) */
	InvalidAnchorages,
	/** The fragment is not offered by the catalog */
	NotInCatalog,
	/** No fragment has been selected */
	NoSelection,
	/** Only the server chooses the fragment */
	NoAuthority,
	/** The asynchronous load exceeded its maximum delay */
	LoadTimedOut,
	/** The asynchronous load was cancelled */
	LoadCancelled
};

/** Role of an anchorage of the boat (cahier §6.4: initial beach, cove near the resources, fallback exit) */
UENUM(BlueprintType)
enum class EAnchorageRole : uint8
{
	/** Initial beach: the team and the boat start here */
	Start,
	/** Cove near the resources: short trips, more danger */
	Resources,
	/** Fallback exit: long trips, safe during the end of the expedition */
	Fallback
};

/**
 *  One anchorage of the boat on a fragment, stored as data (never a reference to an actor of the map):
 *  the hub reads it without loading the map; the mooring actors of the map carry the same AnchorageId.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionAnchorage
{
	GENERATED_BODY()

	/** Stable identifier inside the fragment, shared with the mooring actor of the map */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anchorage")
	FName AnchorageId;

	/** Name shown on the tactical map (String Table) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anchorage")
	FText DisplayName;

	/** Role in the placement of the boat */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anchorage")
	EAnchorageRole Role = EAnchorageRole::Start;

	/** Reachable during the whole final submersion (at least one is required, cahier §6.4) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anchorage")
	bool bAvailableDuringSubmersion = false;

	/** Mooring of the boat, in the frame of the fragment map */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Anchorage")
	FTransform Transform;
};

namespace ExpeditionIslands
{
	/** Player facing message of an error (to move to the String Table ST_Fragments) */
	MONKE_ISLAND_API FText GetErrorText(EFragmentError Error);

	/** Short technical name of an error, for logs */
	MONKE_ISLAND_API const TCHAR* LexErrorName(EFragmentError Error);
}
