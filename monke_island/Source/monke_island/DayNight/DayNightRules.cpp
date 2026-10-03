// monke_island

#include "DayNight/DayNightRules.h"

UDayNightRules::UDayNightRules()
{
	// Cahier §5: day at the start, dusk at 120 s, night at 240 s; then dawn and back to the start hour
	Keys = {
		{0.0f, 9.0f},
		{120.0f, 18.25f},
		{240.0f, 19.5f},
		{480.0f, 29.5f},
		{600.0f, 33.0f}};
}
