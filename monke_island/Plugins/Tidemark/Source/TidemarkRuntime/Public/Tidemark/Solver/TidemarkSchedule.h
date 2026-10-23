// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "TidemarkSchedule.generated.h"

/** Kind of a sea level term. Every kind is a pure function of the step index. */
UENUM(BlueprintType)
enum class ETidemarkScheduleTerm : uint8
{
	/** LevelCm from StartStep onward. */
	Constant,
	/** LevelCm * Alpha^Exponent, Alpha = (Step - StartStep) / DurationSteps clamped to [0, 1] (same curve as the expedition clock). */
	Ramp,
	/** AmplitudeCm * sin(2 pi (Step - StartStep + PhaseSteps) / PeriodSteps) from StartStep onward. */
	Tide,
	/** PeakCm * exp(-((Step - CenterStep) / WidthSteps)^2 / 2): a gaussian bump, WidthSteps is its standard deviation. */
	Surge
};

/**
 * One term of a sea level schedule. Fields not used by Kind are ignored.
 * Steps are simulation steps (StepSeconds each, UTidemarkRules), never seconds: the schedule is the same on every
 * machine whatever the frame rate. A term contributes 0 before StartStep.
 */
USTRUCT(BlueprintType)
struct TIDEMARKRUNTIME_API FSeaLevelTerm
{
	GENERATED_BODY()

	/** What this term computes. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule")
	ETidemarkScheduleTerm Kind = ETidemarkScheduleTerm::Constant;

	/** Constant: the level. Ramp: the height reached at the end of the ramp (cm). */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule", meta = (Units = "cm"))
	float LevelCm = 0.0f;

	/** First step the term contributes at (all kinds). */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule")
	uint32 StartStep = 0;

	/** Ramp: steps from StartStep to the full height. 0 = a jump to LevelCm at StartStep. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule")
	uint32 DurationSteps = 0;

	/** Ramp: exponent of the curve (1 = linear, > 1 = slow start then fast), floored at 0.01 like the expedition clock. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule", meta = (ClampMin = "0.01", ClampMax = "10"))
	float Exponent = 1.0f;

	/** Tide: half height of the swing (cm). */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule", meta = (Units = "cm"))
	float AmplitudeCm = 0.0f;

	/** Tide: steps of one full cycle. 0 or less = no tide. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule", meta = (ClampMin = "0"))
	float PeriodSteps = 0.0f;

	/** Tide: offset of the cycle, in steps (PhaseSteps = PeriodSteps / 4 starts at high tide). */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule")
	float PhaseSteps = 0.0f;

	/** Surge: height at CenterStep (cm). */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule", meta = (Units = "cm"))
	float PeakCm = 0.0f;

	/** Surge: step of the peak. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule", meta = (ClampMin = "0"))
	float CenterStep = 0.0f;

	/** Surge: standard deviation of the bump, in steps (60.7 % of the peak at Center +- Width). 0 or less = no surge. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule", meta = (ClampMin = "0"))
	float WidthSteps = 0.0f;
};

/**
 * Sea level over the simulation steps: the sum of its terms, evaluated by Tidemark::Schedule::LevelCm.
 * Pure data, no state: a scenario.json carries one and a replay gets the same level at every step.
 * The level is relative to the base sea level of the map (0 = the sea where the grid was baked).
 */
USTRUCT(BlueprintType)
struct TIDEMARKRUNTIME_API FSeaLevelSchedule
{
	GENERATED_BODY()

	/** Name of the schedule, written in scenario.json and the reports. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule")
	FName Id;

	/** Terms summed at every step. */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule")
	TArray<FSeaLevelTerm> Terms;

	/**
	 * The water never goes down: Tidemark::Schedule::HeldLevelCm keeps the highest level reached so far
	 * (moved from the game's FloodSchedule::HoldRise). UTidemarkRules::bHoldRise can force it on every schedule.
	 */
	UPROPERTY(EditAnywhere, Category = "Tidemark|Schedule")
	bool bHoldRise = true;
};

namespace Tidemark::Schedule
{
	/** Lowest exponent of a ramp, as in the expedition clock (ExpeditionClock::WaterRiseCm). */
	constexpr double MinRampExponent = 0.01;

	/**
	 * Relative slack of StepsFromSeconds: 0.05 as a float is 0.05000000074, so 600 s / 0.05 s is 11 999.9998 and the
	 * error grows with the step count (1.5e-8 per step). A ratio within 1e-6 of a whole step (about 16 float ulps,
	 * 0.07 step after an hour at 0.05 s) counts as that step. Same rounding on every machine.
	 */
	constexpr double StepRoundingTolerance = 1.0e-6;

	/** Contribution of one term at StepIndex (cm). Pure. */
	TIDEMARKRUNTIME_API float TermLevelCm(const FSeaLevelTerm& Term, uint32 StepIndex);

	/** Sea level at StepIndex (cm): the sum of the terms, in their order. Pure, no state. */
	TIDEMARKRUNTIME_API float LevelCm(const FSeaLevelSchedule& Schedule, uint32 StepIndex);

	/**
	 * Sea level applied at StepIndex when the previous applied level was PreviousHeldCm: with bHoldRise the level
	 * never goes down (max of both), otherwise LevelCm. Pure: the caller keeps the held level.
	 */
	TIDEMARKRUNTIME_API float HeldLevelCm(const FSeaLevelSchedule& Schedule, uint32 StepIndex, float PreviousHeldCm);

	/**
	 * Steps elapsed after Seconds of clock at StepSeconds per step: floor(Seconds / StepSeconds x (1 + StepRoundingTolerance)),
	 * computed in double. 0 for a negative time or a non-positive step. Never fed with DeltaTime.
	 */
	TIDEMARKRUNTIME_API uint32 StepsFromSeconds(float Seconds, float StepSeconds);

	/** Schedule with a single ramp: 0 at StartStep, HeightCm after DurationSteps, curve HeightCm * Alpha^Exponent. */
	TIDEMARKRUNTIME_API FSeaLevelSchedule MakeRamp(FName Id, float HeightCm, uint32 StartStep, uint32 DurationSteps, float Exponent, bool bHoldRise = true);

	/** Schedule with a single gaussian surge of PeakCm at CenterStep, standard deviation WidthSteps. */
	TIDEMARKRUNTIME_API FSeaLevelSchedule MakeSurge(FName Id, float PeakCm, float CenterStep, float WidthSteps, bool bHoldRise = false);
}
