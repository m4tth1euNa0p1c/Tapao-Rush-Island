// monke_island

#include "Misc/AutomationTest.h"
#include "Ceremony/FlagCeremonyMath.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Ceremony
BEGIN_DEFINE_SPEC(FFlagCeremonySpec, "monke_island.Ceremony.Flag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFlagCeremonySpec)

void FFlagCeremonySpec::Define()
{
	Describe("Raising", [this]()
	{
		It("goes up at a steady pace for the length of the ritual, then stays up", [this]()
		{
			TestEqual(TEXT("start"), FlagCeremonyMath::Progress(100.0, 100.0, 60.0f), 0.0f);
			TestEqual(TEXT("half way"), FlagCeremonyMath::Progress(100.0, 130.0, 60.0f), 0.5f, 0.0001f);
			TestEqual(TEXT("done"), FlagCeremonyMath::Progress(100.0, 175.0, 60.0f), 1.0f);
			TestEqual(TEXT("before the start"), FlagCeremonyMath::Progress(100.0, 90.0, 60.0f), 0.0f);
			TestEqual(TEXT("flag at mid height"), FlagCeremonyMath::FlagHeight(0.5f, 90.0f, 350.0f), 220.0f, 0.001f);
			TestEqual(TEXT("seconds left"), FlagCeremonyMath::SecondsLeft(0.25f, 60.0f), 45.0f, 0.001f);
			TestEqual(TEXT("one minute"), FlagCeremonyMath::Countdown(60.0f), FString(TEXT("1:00")));
			TestEqual(TEXT("rounded up"), FlagCeremonyMath::Countdown(41.2f), FString(TEXT("0:42")));
			TestEqual(TEXT("over"), FlagCeremonyMath::Countdown(-3.0f), FString(TEXT("0:00")));
		});
	});

	Describe("Officiant", [this]()
	{
		It("stands in the area within its radius and height", [this]()
		{
			const FVector Site(1000, 0, 50);
			TestTrue(TEXT("near the stones"), FlagCeremonyMath::IsInArea(Site, FVector(1300, 200, 120), 450.0f, 300.0f));
			TestFalse(TEXT("too far"), FlagCeremonyMath::IsInArea(Site, FVector(1500, 0, 50), 450.0f, 300.0f));
			TestFalse(TEXT("on a roof"), FlagCeremonyMath::IsInArea(Site, FVector(1100, 0, 500), 450.0f, 300.0f));
		});

		It("moved only beyond the tolerance (cahier: 10 cm), or lifted by a jump", [this]()
		{
			const FVector Start(0, 0, 100);
			TestFalse(TEXT("jitter"), FlagCeremonyMath::HasMoved(Start, FVector(6, 5, 101), 10.0f));
			TestTrue(TEXT("a step"), FlagCeremonyMath::HasMoved(Start, FVector(12, 0, 100), 10.0f));
			TestTrue(TEXT("a jump"), FlagCeremonyMath::HasMoved(Start, FVector(0, 0, 145), 10.0f));
		});
	});

	Describe("Ring", [this]()
	{
		It("spreads the villagers evenly from the phase", [this]()
		{
			TestEqual(TEXT("first"), FlagCeremonyMath::SlotAngle(0, 4, 30.0f), 30.0f);
			TestEqual(TEXT("third"), FlagCeremonyMath::SlotAngle(2, 4, 30.0f), 210.0f);
			TestTrue(TEXT("east"), FlagCeremonyMath::RingPoint(FVector::ZeroVector, 0.0f, 380.0f).Equals(FVector(380, 0, 0), 0.01));
			TestTrue(TEXT("north"), FlagCeremonyMath::RingPoint(FVector(0, 0, 10), 90.0f, 380.0f).Equals(FVector(0, 380, 10), 0.01));
		});

		It("keeps the places off the stones", [this]()
		{
			const FVector Cairn(100, 0, 0);
			const FVector Pushed = FlagCeremonyMath::KeepOffCairn(FVector(150, 0, 20), Cairn, 120.0f);
			TestTrue(TEXT("pushed out along the line from the stones"), Pushed.Equals(FVector(220, 0, 20), 0.01));
			TestTrue(TEXT("left alone outside"), FlagCeremonyMath::KeepOffCairn(FVector(400, 0, 0), Cairn, 120.0f).Equals(FVector(400, 0, 0)));
		});

		It("comes round the outside of the ring, never across it", [this]()
		{
			const FVector Centre = FVector::ZeroVector;
			// from the far west to the east slot: round the lane, then in
			const TArray<FVector> Way = FlagCeremonyMath::RingApproach(Centre, FVector(-3000, 100, 0), 0.0f, 380.0f, 490.0f, 35.0f);
			TestTrue(TEXT("ends on the slot"), Way.Last().Equals(FVector(380, 0, 0), 0.01));
			for (int32 Index = 0; Index + 1 < Way.Num(); ++Index)
			{
				TestTrue(TEXT("lane points on the lane"), FMath::IsNearlyEqual(FVector::Dist2D(Way[Index], Centre), 490.0, 0.5));
				// a segment of the lane never comes closer to the centre than the ring
				const FVector Middle = (Way[Index] + Way[Index + 1]) * 0.5;
				TestTrue(TEXT("outside the ring"), FVector::Dist2D(Middle, Centre) > 380.0);
			}
			// already on its side: straight in
			const TArray<FVector> Short = FlagCeremonyMath::RingApproach(Centre, FVector(2000, 50, 0), 0.0f, 380.0f, 490.0f, 35.0f);
			TestEqual(TEXT("straight to the slot"), Short.Num(), 1);
		});
	});
}

#endif
