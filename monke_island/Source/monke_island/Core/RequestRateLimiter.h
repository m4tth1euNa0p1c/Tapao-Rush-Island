// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Token bucket of a request a client may repeat (security norm SEC-03, security review T-269 M1): Burst requests at
 *  once, then RefillPerSecond given back each second. One per player and per kind of request, kept by the server on the
 *  component the request reaches; a request over the limit is refused, logged in Verbose by the caller, and nothing
 *  else happens (no disconnection: a player who mashes a key is never punished). Pure, the caller gives the server's
 *  world time: tested without a world (monke_island.Interaction.RateLimit).
 */
struct FRequestRateLimiter
{
	/** Requests still allowed now (-1: full, before the first request) */
	double Tokens = -1.0;

	/** World time of the last refill (s) */
	double LastRefill = 0.0;

	/** Requests refused so far */
	int32 Refused = 0;

	/** Whether a request at Now passes (it takes a token). Burst counts at least 1; a clock that went back (a new world)
	 *  or a time that is not finite gives nothing back */
	bool Consume(double Now, float Burst, float RefillPerSecond)
	{
		const double Capacity = FMath::Max(1.0, FMath::IsFinite(Burst) ? static_cast<double>(Burst) : 1.0);
		const double Refill = FMath::IsFinite(RefillPerSecond) ? FMath::Max(0.0, static_cast<double>(RefillPerSecond)) : 0.0;
		if (Tokens < 0.0)
		{
			Tokens = Capacity;
			LastRefill = FMath::IsFinite(Now) ? Now : 0.0;
		}
		else if (FMath::IsFinite(Now))
		{
			Tokens = FMath::Min(Capacity, Tokens + FMath::Max(0.0, Now - LastRefill) * Refill);
			LastRefill = Now;
		}
		if (Tokens >= 1.0)
		{
			Tokens -= 1.0;
			return true;
		}
		++Refused;
		return false;
	}
};
