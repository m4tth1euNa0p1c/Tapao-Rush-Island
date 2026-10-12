// monke_island

#include "Misc/AutomationTest.h"
#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonRules.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Galleon.Sway
// The sway of the galleon is visual only (network review T-268 R-04, T-317): the collision hull never rolls, the visual
// hull rolls, pitches and heaves around the crew's deck with the values of DA_GalleonRules
BEGIN_DEFINE_SPEC(FGalleonSwaySpec, "monke_island.Galleon.Sway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UGalleonRules> Rules;
END_DEFINE_SPEC(FGalleonSwaySpec)

void FGalleonSwaySpec::Define()
{
	BeforeEach([this]()
	{
		Rules.Reset(NewObject<UGalleonRules>(GetTransientPackage()));
	});

	AfterEach([this]()
	{
		Rules.Reset();
	});

	It("turns around the crew's deck, which only heaves, within the heave of the rules", [this]()
	{
		const FVector Pivot(0.0, 120.0, 260.0);
		for (double Time = 0.0; Time < 30.0; Time += 0.37)
		{
			const FTransform Sway = AGalleonActor::ComputeVisualSway(*Rules, Time, Pivot);
			const FVector Moved = Sway.TransformPosition(Pivot) - Pivot;
			if (!TestTrue(*FString::Printf(TEXT("pivot only heaves at %.2f s"), Time), FMath::IsNearlyZero(Moved.X, 0.01) && FMath::IsNearlyZero(Moved.Y, 0.01))
				|| !TestTrue(*FString::Printf(TEXT("heave within %.0f cm at %.2f s"), Rules->HeaveCm, Time), FMath::Abs(Moved.Z) <= Rules->HeaveCm + 0.01))
			{
				return;
			}
		}
	});

	It("rolls and pitches no more than the rules, and still visibly", [this]()
	{
		double MaxRoll = 0.0;
		double MaxPitch = 0.0;
		for (double Time = 0.0; Time < 60.0; Time += 0.1)
		{
			const FRotator Rotation = AGalleonActor::ComputeVisualSway(*Rules, Time, FVector::ZeroVector).Rotator();
			MaxRoll = FMath::Max(MaxRoll, FMath::Abs(static_cast<double>(Rotation.Pitch)));
			MaxPitch = FMath::Max(MaxPitch, FMath::Abs(static_cast<double>(Rotation.Roll)));
		}
		TestTrue(TEXT("roll within the rules"), MaxRoll <= Rules->RollDeg + 0.01);
		TestTrue(TEXT("pitch within the rules"), MaxPitch <= Rules->PitchDeg + 0.01);
		TestTrue(TEXT("the roll stays visible"), MaxRoll >= Rules->RollDeg * 0.9);
	});

	It("gives the same pose for the same server time, and none without sway", [this]()
	{
		const FVector Pivot(10.0, -40.0, 300.0);
		TestTrue(TEXT("same time, same pose"), AGalleonActor::ComputeVisualSway(*Rules, 12.5, Pivot).Equals(AGalleonActor::ComputeVisualSway(*Rules, 12.5, Pivot)));
		Rules->RollDeg = 0.0f;
		Rules->PitchDeg = 0.0f;
		Rules->HeaveCm = 0.0f;
		TestTrue(TEXT("still"), AGalleonActor::ComputeVisualSway(*Rules, 7.0, Pivot).Equals(FTransform::Identity, 0.001));
		TestTrue(TEXT("a time that is not finite"), AGalleonActor::ComputeVisualSway(*Rules, std::numeric_limits<double>::quiet_NaN(), Pivot).IsValid());
	});
}

#endif
