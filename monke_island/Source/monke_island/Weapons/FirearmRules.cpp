// monke_island

#include "Weapons/FirearmRules.h"

float FFirearmClip::FindEventTime(FName Name, int32 Occurrence) const
{
	for (const FFirearmClipEvent& Event : Events)
	{
		if (Event.Name == Name && Occurrence-- == 0)
		{
			return Event.Time;
		}
	}
	return -1.0f;
}

int32 FFirearmClip::CountEvents(FName Name) const
{
	int32 Count = 0;
	for (const FFirearmClipEvent& Event : Events)
	{
		Count += Event.Name == Name ? 1 : 0;
	}
	return Count;
}

FFirearmClip FFirearmClip::Make(EFirearmGesture InGesture, int32 Frames, int32 Fps, bool bInLoop, std::initializer_list<FFirearmEventDef> InEvents)
{
	FFirearmClip Clip;
	Clip.Gesture = InGesture;
	Clip.Duration = static_cast<float>(FMath::Max(Frames, 1)) / static_cast<float>(FMath::Max(Fps, 1));
	Clip.bLoop = bInLoop;
	for (const FFirearmEventDef& Event : InEvents)
	{
		Clip.Events.Emplace(Event.Name, Event.Frame, Fps);
	}
	Clip.Events.StableSort([](const FFirearmClipEvent& A, const FFirearmClipEvent& B) { return A.Time < B.Time; });
	return Clip;
}

const FFirearmClip* UFirearmRules::FindClip(EFirearmGesture Gesture) const
{
	return Clips.FindByPredicate([Gesture](const FFirearmClip& Clip) { return Clip.Gesture == Gesture; });
}
