// Tidemark plugin

#include "Tidemark/Rules/TidemarkRules.h"

UTidemarkRules::UTidemarkRules()
{
	// The Tapao submersion as the default: 650 cm over 60 s of steps, slow start (exponent 1.6), never down.
	DefaultSchedule = Tidemark::Schedule::MakeRamp(FName(TEXT("Default")), Tidemark::Rules::DefaultScheduleHeightCm,
		0, Tidemark::Rules::DefaultScheduleDurationSteps, Tidemark::Rules::DefaultScheduleExponent, Tidemark::Rules::DefaultHoldRise);
}
