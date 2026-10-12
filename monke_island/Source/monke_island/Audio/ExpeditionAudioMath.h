// monke_island
#pragma once

#include "CoreMinimal.h"

namespace ExpeditionAudioMath
{
	/** Only a real downward crossing fires. An initial/late replicated sample does not. */
	inline bool Crossed(double Previous, double Current, double Threshold)
	{
		return FMath::IsFinite(Previous) && FMath::IsFinite(Current)
			&& Previous > Threshold && Current <= Threshold && Current >= 0.0;
	}

	/** No tick at zero, on the initial sample, or while the timer grows after a zone bonus. */
	inline bool CountdownTick(double Previous, double Current, int32 Maximum)
	{
		return FMath::IsFinite(Previous) && FMath::IsFinite(Current) && Current > 0.0
			&& Current <= Maximum && FMath::CeilToInt(Current) < FMath::CeilToInt(Previous);
	}

	/** Only an increase in item count plays pickup; rearranging/equipping does not. */
	inline bool GainedItems(int32 Previous, int32 Current)
	{
		return Previous >= 0 && Current > Previous;
	}
}
