// monke_island

#include "Expedition/Pacification/PacificationTypes.h"
#include "GameFramework/Pawn.h"

void FSermonState::Begin(APawn* InOfficiant, const FVector& InStartLocation, double InServerTime, bool bInHasController)
{
	bActive = true;
	Officiant = InOfficiant;
	StartLocation = InStartLocation;
	StartServerTime = InServerTime;
	bHadControllerAtStart = bInHasController;
}

void FSermonState::Reset()
{
	bActive = false;
	Officiant = nullptr;
	StartLocation = FVector::ZeroVector;
	StartServerTime = 0.0;
	bHadControllerAtStart = false;
}

double FSermonState::ElapsedSeconds(double ServerTime) const
{
	return bActive ? FMath::Max(0.0, ServerTime - StartServerTime) : 0.0;
}

float FSermonState::Progress01(double ServerTime, float Duration) const
{
	if (!bActive)
	{
		return 0.0f;
	}
	if (Duration <= 0.0f)
	{
		return 1.0f;
	}
	return FMath::Clamp(static_cast<float>(ElapsedSeconds(ServerTime) / Duration), 0.0f, 1.0f);
}

bool FSermonState::IsComplete(double ServerTime, float Duration) const
{
	return bActive && ElapsedSeconds(ServerTime) >= static_cast<double>(Duration);
}

bool FPacificationLogic::IsWithinTolerance(const FVector& Start, const FVector& Current, float HorizontalToleranceCm, float VerticalToleranceCm)
{
	const FVector Delta = Current - Start;
	return Delta.SizeSquared2D() <= FMath::Square(static_cast<double>(HorizontalToleranceCm))
		&& FMath::Abs(Delta.Z) <= static_cast<double>(VerticalToleranceCm);
}

ESermonInterruptReason FPacificationLogic::EvaluateSermon(const FSermonState& State, const FSermonObservation& Observation, float HorizontalToleranceCm, float VerticalToleranceCm)
{
	if (!State.bActive)
	{
		return ESermonInterruptReason::None;
	}
	if (Observation.bZoneSubmerged)
	{
		return ESermonInterruptReason::ZoneSubmerged;
	}
	if (!Observation.bOfficiantValid || !Observation.bOfficiantConnected)
	{
		return ESermonInterruptReason::OfficiantLost;
	}
	if (!Observation.bOfficiantAlive)
	{
		return ESermonInterruptReason::OfficiantDied;
	}
	if (!Observation.bHolding)
	{
		return ESermonInterruptReason::Released;
	}
	if (Observation.bDamaged)
	{
		return ESermonInterruptReason::Damaged;
	}
	if (!IsWithinTolerance(State.StartLocation, Observation.Location, HorizontalToleranceCm, VerticalToleranceCm))
	{
		return ESermonInterruptReason::Moved;
	}
	if (Observation.bThreatInPerimeter)
	{
		return ESermonInterruptReason::ThreatInPerimeter;
	}
	return ESermonInterruptReason::None;
}

bool FPacificationLogic::CanPlantFlag(EZoneState ZoneState, int32 LivingDesignatedThreats)
{
	return ZoneState == EZoneState::Contested && LivingDesignatedThreats <= 0;
}

bool FPacificationLogic::AcceptsSermon(EZoneState ZoneState, bool bFlagPlanted)
{
	return ZoneState == EZoneState::Securing && bFlagPlanted;
}
