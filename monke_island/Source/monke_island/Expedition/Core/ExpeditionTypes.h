// monke_island

#pragma once

#include "CoreMinimal.h"
#include "ExpeditionTypes.generated.h"

/** Phase of an expedition (cahier §6.1, §21): decided by the server, replicated to every machine */
UENUM(BlueprintType)
enum class EExpeditionPhase : uint8
{
	/** The expedition has not started yet: no clock */
	Preparing,
	/** The clock runs, zones can still add time before the flood deadline */
	Exploring,
	/** The flood deadline is passed: the water rises, irreversible */
	Submerging,
	/** The expedition is over (L3: end of the submersion; extraction comes later) */
	Resolved
};

/** Why an expedition reached the Resolved phase */
UENUM(BlueprintType)
enum class EExpeditionResolution : uint8
{
	/** Not resolved yet */
	None,
	/** The final rise of the water ended */
	Submerged
};

/** Answer of the server to a zone bonus request (cahier §6.1, §6.2) */
UENUM(BlueprintType)
enum class EZoneBonusResult : uint8
{
	/** First pacification of this zone before the deadline: +ZoneBonus for the whole team */
	Granted,
	/** This zone already gave its bonus in this expedition: nothing added */
	AlreadyGranted,
	/** The validation does not strictly precede the flood deadline: the flood wins, nothing added */
	TooLate,
	/** The expedition clock has not started */
	NotStarted,
	/** The zone has no identifier */
	InvalidZone,
	/** Every pacifiable zone of the rules already gave its bonus */
	ZoneLimitReached
};
