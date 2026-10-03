// monke_island

#include "UI/FloodTimerMath.h"

#define LOCTEXT_NAMESPACE "MonkeFloodTimer"

namespace FloodTimerMath
{
	FFloodTimerSnapshot Evaluate(const FFloodTimerInput& Input)
	{
		FFloodTimerSnapshot Snapshot;
		if (!Input.bActive)
		{
			return Snapshot;
		}
		if (Input.bGameOver)
		{
			Snapshot.Mode = EFloodTimerMode::GameOver;
			Snapshot.SecondsLeft = FMath::Max(0.0, Input.RestartTime - Input.Now);
			return Snapshot;
		}
		if (Input.bResolved || (Input.bSubmersionStarted && Input.Now >= Input.SubmersionEndTime))
		{
			Snapshot.Mode = EFloodTimerMode::Over;
			return Snapshot;
		}
		if (Input.bSubmersionStarted)
		{
			Snapshot.Mode = EFloodTimerMode::Evacuation;
			Snapshot.SecondsLeft = FMath::Max(0.0, Input.SubmersionEndTime - Input.Now);
			return Snapshot;
		}
		// Not confirmed yet: 0:00 at most, never a negative time nor an evacuation the server did not decide
		Snapshot.Mode = EFloodTimerMode::Countdown;
		Snapshot.SecondsLeft = FMath::Max(0.0, Input.FloodStartTime - Input.Now);
		return Snapshot;
	}

	int32 DisplaySeconds(double Seconds)
	{
		// A millisecond of tolerance keeps 165.0000001 at 2:45 (float noise of server times)
		return FMath::Max(0, FMath::CeilToInt(Seconds - 0.001));
	}

	FText FormatClock(double Seconds)
	{
		const int32 Total = DisplaySeconds(Seconds);
		FNumberFormattingOptions TwoDigits;
		TwoDigits.MinimumIntegralDigits = 2;
		TwoDigits.UseGrouping = false;
		FNumberFormattingOptions Plain;
		Plain.UseGrouping = false;
		return FText::Format(LOCTEXT("ClockFormat", "{0}:{1}"), FText::AsNumber(Total / 60, &Plain), FText::AsNumber(Total % 60, &TwoDigits));
	}

	bool IsZoneBonus(double NewFloodStartTime, double PreviousFloodStartTime)
	{
		// The start of the expedition moves the deadline from 0 (never started); a bonus moves a real deadline later
		return PreviousFloodStartTime > 0.0 && NewFloodStartTime > PreviousFloodStartTime + 0.001;
	}
}

void FFloodTimerPresenter::Configure(const TArray<float>& InThresholds, bool bInRearmAfterExtension)
{
	Thresholds.Reset();
	for (const float Threshold : InThresholds)
	{
		if (Threshold > 0.0f)
		{
			Thresholds.AddUnique(Threshold);
		}
	}
	Thresholds.Sort([](float A, float B) { return A > B; });
	bRearmAfterExtension = bInRearmAfterExtension;
	Reset();
}

void FFloodTimerPresenter::Reset()
{
	Fired.Init(false, Thresholds.Num());
	bCountdownSeen = false;
	LastSecondsLeft = 0.0;
	Mode = EFloodTimerMode::Hidden;
}

bool FFloodTimerPresenter::HasFired(float Threshold) const
{
	const int32 Index = Thresholds.IndexOfByKey(Threshold);
	return Index != INDEX_NONE && Fired[Index];
}

FFloodTimerEvents FFloodTimerPresenter::Update(const FFloodTimerSnapshot& Snapshot)
{
	FFloodTimerEvents Events;
	if (Snapshot.Mode != Mode)
	{
		Events.bModeChanged = true;
		Events.PreviousMode = Mode;
		Mode = Snapshot.Mode;
	}
	if (Mode != EFloodTimerMode::Countdown)
	{
		return Events;
	}

	const double Left = Snapshot.SecondsLeft;
	if (!bCountdownSeen)
	{
		// Late join or a timer created mid-expedition: the thresholds already passed stay silent
		for (int32 Index = 0; Index < Thresholds.Num(); ++Index)
		{
			Fired[Index] = Left <= Thresholds[Index];
		}
		bCountdownSeen = true;
		LastSecondsLeft = Left;
		return Events;
	}

	if (bRearmAfterExtension && Left > LastSecondsLeft + 0.5)
	{
		// The deadline moved later: the thresholds now ahead fire once more, each once
		for (int32 Index = 0; Index < Thresholds.Num(); ++Index)
		{
			if (Left > Thresholds[Index])
			{
				Fired[Index] = false;
			}
		}
	}
	LastSecondsLeft = Left;

	// Every unfired threshold reached is spent; only the most urgent one alerts (one sound per update at most)
	for (int32 Index = 0; Index < Thresholds.Num(); ++Index)
	{
		if (!Fired[Index] && Left <= Thresholds[Index])
		{
			Fired[Index] = true;
			Events.AlertThreshold = Thresholds[Index];
		}
	}
	return Events;
}

#undef LOCTEXT_NAMESPACE
