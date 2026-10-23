// Tidemark plugin

#include "Tidemark/Solver/TidemarkSchedule.h"

namespace Tidemark::Schedule
{
	float TermLevelCm(const FSeaLevelTerm& Term, uint32 StepIndex)
	{
		if (StepIndex < Term.StartStep)
		{
			return 0.0f;
		}
		// Local time of the term, in steps. Double: a step index times a period never loses a step this way.
		const double T = static_cast<double>(StepIndex - Term.StartStep);

		switch (Term.Kind)
		{
		case ETidemarkScheduleTerm::Constant:
			return Term.LevelCm;

		case ETidemarkScheduleTerm::Ramp:
		{
			// Same curve as the game's expedition clock: Height * Alpha^max(0.01, Exponent), Alpha clamped to [0, 1].
			const double Alpha = Term.DurationSteps == 0 ? 1.0 : FMath::Clamp(T / static_cast<double>(Term.DurationSteps), 0.0, 1.0);
			const double Exponent = FMath::Max(MinRampExponent, static_cast<double>(Term.Exponent));
			return static_cast<float>(static_cast<double>(Term.LevelCm) * FMath::Pow(Alpha, Exponent));
		}

		case ETidemarkScheduleTerm::Tide:
		{
			if (!(Term.PeriodSteps > 0.0f))
			{
				return 0.0f;
			}
			const double Angle = UE_DOUBLE_TWO_PI * (T + static_cast<double>(Term.PhaseSteps)) / static_cast<double>(Term.PeriodSteps);
			return static_cast<float>(static_cast<double>(Term.AmplitudeCm) * FMath::Sin(Angle));
		}

		case ETidemarkScheduleTerm::Surge:
		{
			if (!(Term.WidthSteps > 0.0f))
			{
				return 0.0f;
			}
			// Gaussian in absolute steps: CenterStep is a step of the run, not an offset from StartStep.
			const double Z = (static_cast<double>(StepIndex) - static_cast<double>(Term.CenterStep)) / static_cast<double>(Term.WidthSteps);
			return static_cast<float>(static_cast<double>(Term.PeakCm) * FMath::Exp(-0.5 * Z * Z));
		}

		default:
			return 0.0f;
		}
	}

	float LevelCm(const FSeaLevelSchedule& Schedule, uint32 StepIndex)
	{
		// Fixed order of summation (the order of the terms): the same bits on every machine.
		double Level = 0.0;
		for (const FSeaLevelTerm& Term : Schedule.Terms)
		{
			Level += static_cast<double>(TermLevelCm(Term, StepIndex));
		}
		return static_cast<float>(Level);
	}

	float HeldLevelCm(const FSeaLevelSchedule& Schedule, uint32 StepIndex, float PreviousHeldCm)
	{
		const float Level = LevelCm(Schedule, StepIndex);
		return Schedule.bHoldRise ? FMath::Max(PreviousHeldCm, Level) : Level;
	}

	uint32 StepsFromSeconds(float Seconds, float StepSeconds)
	{
		if (!(Seconds > 0.0f) || !(StepSeconds > 0.0f) || !FMath::IsFinite(Seconds))
		{
			return 0;
		}
		const double Ratio = static_cast<double>(Seconds) / static_cast<double>(StepSeconds);
		const double Steps = FMath::Floor(Ratio * (1.0 + StepRoundingTolerance));
		return static_cast<uint32>(FMath::Clamp(Steps, 0.0, static_cast<double>(MAX_uint32)));
	}

	FSeaLevelSchedule MakeRamp(FName Id, float HeightCm, uint32 StartStep, uint32 DurationSteps, float Exponent, bool bHoldRise)
	{
		FSeaLevelSchedule Schedule;
		Schedule.Id = Id;
		Schedule.bHoldRise = bHoldRise;
		FSeaLevelTerm& Term = Schedule.Terms.AddDefaulted_GetRef();
		Term.Kind = ETidemarkScheduleTerm::Ramp;
		Term.LevelCm = HeightCm;
		Term.StartStep = StartStep;
		Term.DurationSteps = DurationSteps;
		Term.Exponent = Exponent;
		return Schedule;
	}

	FSeaLevelSchedule MakeSurge(FName Id, float PeakCm, float CenterStep, float WidthSteps, bool bHoldRise)
	{
		FSeaLevelSchedule Schedule;
		Schedule.Id = Id;
		Schedule.bHoldRise = bHoldRise;
		FSeaLevelTerm& Term = Schedule.Terms.AddDefaulted_GetRef();
		Term.Kind = ETidemarkScheduleTerm::Surge;
		Term.PeakCm = PeakCm;
		Term.CenterStep = CenterStep;
		Term.WidthSteps = WidthSteps;
		return Schedule;
	}
}
