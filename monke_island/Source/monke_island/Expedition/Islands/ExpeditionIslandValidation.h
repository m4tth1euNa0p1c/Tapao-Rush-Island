// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Expedition/Islands/ExpeditionIslandTypes.h"
#include "Templates/ValueOrError.h"

class UExpeditionIslandDefinition;
class UFragmentRules;
class UIslandCatalog;

/**
 *  Pure rules of the expedition islands (ADR-004): validation of a definition and of the catalog, travel URL.
 *  No world, no loading: callable from tests, from IsDataValid and from the subsystem.
 */
namespace ExpeditionIslands
{
	/** Name of the travel URL option that carries the fragment id (<map>?Island=FRG_Raraka_01, contract with T-093) */
	MONKE_ISLAND_API extern const TCHAR* const FragmentOptionKey;

	/** Longest fragment id accepted from a URL option (ids are short: FRG_<Ile>_<NN>) */
	constexpr int32 MaxFragmentIdLength = 64;

	/**
	 *  First problem of a definition, None when it can be offered and travelled to.
	 *  Rules may be null: the class defaults of UFragmentRules apply. OutIssues (optional) receives every problem.
	 */
	[[nodiscard]] MONKE_ISLAND_API EFragmentError ValidateDefinition(const UExpeditionIslandDefinition* Definition, const UFragmentRules* Rules, TArray<FText>* OutIssues = nullptr);

	/** First problem of a catalog itself (missing, empty, malformed ids, duplicates); its definitions are checked when loaded */
	[[nodiscard]] MONKE_ISLAND_API EFragmentError ValidateCatalog(const UIslandCatalog* Catalog, TArray<FText>* OutIssues = nullptr);

	/** Whether a fragment id is well formed: not empty, short, a valid name */
	[[nodiscard]] MONKE_ISLAND_API bool IsValidFragmentId(FName FragmentId);

	/** Travel URL of a definition for ServerTravel: <long package name of the map>?Island=<FragmentId> */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<FString, EFragmentError> BuildTravelURL(const UExpeditionIslandDefinition* Definition);

	/**
	 *  Index drawn among Count fragments from Seed (same seed, same draw), never ExcludedIndex while another one
	 *  exists. INDEX_NONE when Count <= 0; ExcludedIndex out of range excludes nothing.
	 */
	[[nodiscard]] MONKE_ISLAND_API int32 PickIndex(int32 Count, int32 Seed, int32 ExcludedIndex);

	/** Fragment drawn among FragmentIds from Seed, never PreviousFragmentId twice in a row unless it is the only one */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<FName, EFragmentError> PickFragmentId(TConstArrayView<FName> FragmentIds, int32 Seed, FName PreviousFragmentId);

	/**
	 *  Index drawn from Seed with a chance proportional to its weight (same seed, same draw); weights <= 0 are never
	 *  drawn; never ExcludedIndex while another drawable one exists. INDEX_NONE when nothing is drawable.
	 */
	[[nodiscard]] MONKE_ISLAND_API int32 PickWeightedIndex(TConstArrayView<float> Weights, int32 Seed, int32 ExcludedIndex);

	/** PickFragmentId with a weight per fragment (the fort island weighs more, T-205); Weights as long as FragmentIds */
	[[nodiscard]] MONKE_ISLAND_API TValueOrError<FName, EFragmentError> PickWeightedFragmentId(TConstArrayView<FName> FragmentIds,
		TConstArrayView<float> Weights, int32 Seed, FName PreviousFragmentId);

	/** Fragment id carried by the options of a travel URL (Island=...), None when absent or malformed */
	[[nodiscard]] MONKE_ISLAND_API FName ParseFragmentOption(const TCHAR* OptionValue);
}
