// monke_island

#pragma once

#include "CoreMinimal.h"
#include "VoiceTypes.generated.h"

/** Proximity voice chat (T-263): voice session, microphone, talkers, the noise of the voice */
MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeVoice, Log, All);

/** What the HUD shows of the local player's microphone (security.md §6: an indicator while the microphone is open) */
UENUM(BlueprintType)
enum class EVoiceIndicator : uint8
{
	/** Microphone closed: nothing shown */
	None,
	/** Microphone open (key held, or voice activation), nobody heard yet */
	Open,
	/** The player is heard: by the others and by the mobs */
	Talking,
	/** Microphone test: the voice comes back to the player alone */
	Test,
	/** Dead: the voice no longer goes out */
	Dead
};
