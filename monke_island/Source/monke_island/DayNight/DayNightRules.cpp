// monke_island

#include "DayNight/DayNightRules.h"

UDayNightRules::UDayNightRules()
{
	// User request (T-067, deviation from cahier §5): day 12 min, dusk 15 s, night 6 min, dawn 15 s (18 min 30 a day).
	// The hours are those where the sun of Raraka (16.18 S, day 276) crosses +/- TwilightElevationDeg (6 deg), so
	// the phases (Day / Dusk / Night / Dawn) last exactly these times; the cycle starts at the beginning of the day.
	Keys = {
		{0.0f, 6.3225f},      // sun at +6 deg, morning: day
		{720.0f, 17.6775f},   // +6 deg, evening: dusk
		{735.0f, 18.5140f},   // -6 deg: night
		{1095.0f, 29.4860f},  // -6 deg, next morning: dawn
		{1110.0f, 30.3225f}}; // +6 deg: the next day
}
