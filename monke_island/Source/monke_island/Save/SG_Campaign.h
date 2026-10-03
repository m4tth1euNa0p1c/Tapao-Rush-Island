// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Save/SaveTypes.h"
#include "SG_Campaign.generated.h"

/**
 *  Campaign save of the host (cahier §17, §21 SG_Campaign): separate, versioned schema.
 *  Stable internal identifier (FGuid, also the file name) and an editable display name never used as a path.
 *  Holds the hub state the save owns (gold, reserve per category, hub equipment), the ExpeditionInProgress marker
 *  and the ledger of expeditions already applied, which makes ApplyExpeditionResult idempotent.
 *  Only properties flagged SaveGame are written.
 *
 *  Schema history:
 *  - 1: applied expeditions kept as a bare list of ids (LegacyAppliedExpeditionIds);
 *  - 2: ledger entries (ExpeditionLedger) with what each expedition credited or lost.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API USG_Campaign : public USaveGame
{
	GENERATED_BODY()

public:

	/** Schema written by this build */
	static constexpr int32 CurrentSchemaVersion = 2;

	/**
	 *  Schema version of this object (stamped to CurrentSchemaVersion at save, upgraded by Migrate at load).
	 *  The authoritative version at load is the one of the file header: property serialisation skips values equal to the class default.
	 */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save")
	int32 SchemaVersion = CurrentSchemaVersion;

	/** Stable internal identifier: file name, correlation id in the logs */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FGuid CampaignId;

	/** Display name chosen by the host, editable, never a path */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FString DisplayName;

	/** Creation time (UTC) */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FDateTime CreatedUtc;

	/** Last successful save (UTC) */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FDateTime LastSavedUtc;

	/** Campaign gold (never negative) */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Hub")
	int64 Gold = 0;

	/** Reserve per resource category (wood, food, chest…), filled by the economy through ApplyExpeditionResult */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Hub")
	TMap<FName, int32> ReserveByCategory;

	/** Equipment stored at the hub: item id -> count. Committed equipment leaves it at departure */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Hub")
	TMap<FName, int32> HubEquipment;

	/** ExpeditionInProgress marker (§17); inactive at the hub */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	FExpeditionInProgressMarker ExpeditionInProgress;

	/** Every expedition applied or abandoned, oldest first: an ExpeditionId found here is never applied again */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category="Save|Expedition")
	TArray<FExpeditionLedgerEntry> ExpeditionLedger;

	/** Schema 1 only: ids of applied expeditions, moved into ExpeditionLedger by Migrate */
	UPROPERTY(SaveGame, VisibleAnywhere, Category="Save|Legacy")
	TArray<FGuid> LegacyAppliedExpeditionIds;

	/** Ledger entry of an expedition, or nullptr */
	const FExpeditionLedgerEntry* FindLedgerEntry(const FGuid& ExpeditionId) const;

	/** True when the expedition is already in the ledger */
	UFUNCTION(BlueprintPure, Category="Save|Expedition")
	bool IsExpeditionApplied(const FGuid& ExpeditionId) const;

	/** True when an ExpeditionInProgress marker is set */
	UFUNCTION(BlueprintPure, Category="Save|Expedition")
	bool HasExpeditionInProgress() const { return ExpeditionInProgress.IsActive(); }

	/**
	 *  Upgrades an older schema to CurrentSchemaVersion, step by step. Never called with a future version.
	 *  @return true when something was migrated
	 */
	bool Migrate();

	/**
	 *  Checks every value read from disk against bounds (security.md SEC-05): negative counts or gold, too many entries,
	 *  invalid identifiers. A file that fails is treated as damaged, never used.
	 */
	[[nodiscard]] bool ValidateLoadedData(int32 MaxMapEntries, int32 MaxLedgerEntries, int32 MaxDisplayNameLength, FString& OutReason) const;
};
