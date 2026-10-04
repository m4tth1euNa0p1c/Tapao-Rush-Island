#pragma once
#include "CoreMinimal.h"
#include "Core/DeathCause.h"

class UExpeditionRules;
class UIslanderHealthComponent;

namespace ExpeditionEvents
{
	/** Effective editable DA fields, sorted by name, excluding UObject identity. Local fingerprint, not a signature. */
	MONKE_ISLAND_API FString RulesHash(const UExpeditionRules& Rules);
	/** ProjectVersion from game config, limited to a non-personal token. */
	MONKE_ISLAND_API FString BuildVersion();
	MONKE_ISLAND_API FString SafeBuildToken(const FString& Value);
	/** Server-only death event; does not expose actor names, controller names or personal paths. */
	MONKE_ISLAND_API void LogDeath(const UIslanderHealthComponent& Health, EDeathCause Cause);
}
