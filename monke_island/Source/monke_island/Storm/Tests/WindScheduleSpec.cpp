// monke_island

#include "Misc/AutomationTest.h"
#include "Storm/WindMath.h"
#include "Storm/WindRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Wind
BEGIN_DEFINE_SPEC(FWindScheduleSpec, "monke_island.Wind.Schedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWindRules* Rules = nullptr;
END_DEFINE_SPEC(FWindScheduleSpec)

void FWindScheduleSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UWindRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Gusts (T-067)", [this]()
	{
		It("blow for one minute, at least 5 min after the previous one", [this]()
		{
			double Start = Rules->FirstGustSeconds;
			for (int32 Index = 0; Index < 40; ++Index)
			{
				const double Next = WindMath::NextGustStart(*Rules, Index, Start);
				const double Calm = Next - (Start + Rules->GustDurationSeconds);
				if (!TestTrue(FString::Printf(TEXT("calm after gust %d"), Index + 1), Calm >= Rules->MinGapSeconds - 0.001 && Calm <= Rules->MinGapSeconds + Rules->MaxExtraGapSeconds + 0.001))
				{
					break;
				}
				Start = Next;
			}
			TestEqual(TEXT("one minute"), Rules->GustDurationSeconds, 60.0f);
			TestEqual(TEXT("5 min apart"), Rules->MinGapSeconds, 300.0f);
		});

		It("are random but the same on every machine (seeded)", [this]()
		{
			const double A = WindMath::NextGustStart(*Rules, 3, 1000.0);
			TestEqual(TEXT("same seed, same draw"), WindMath::NextGustStart(*Rules, 3, 1000.0), A);
			TestNotEqual(TEXT("each gust its own draw"), WindMath::NextGustStart(*Rules, 4, 1000.0), A);
		});

		It("finds the gust blowing at a time, and calm between gusts", [this]()
		{
			const double Middle = Rules->FirstGustSeconds + Rules->GustDurationSeconds * 0.5;
			const FWindGust Gust = WindMath::LastGustAt(*Rules, Middle);
			TestEqual(TEXT("first gust"), Gust.Index, 0);
			TestEqual(TEXT("plateau"), WindMath::GustEnvelope(*Rules, Gust, Middle), 1.0f);
			TestEqual(TEXT("before any gust"), WindMath::LastGustAt(*Rules, Rules->FirstGustSeconds - 1.0).Index, INDEX_NONE);
			const double AfterIt = Rules->FirstGustSeconds + Rules->GustDurationSeconds + 10.0;
			TestEqual(TEXT("calm after it"), WindMath::MeanStrengthAt(*Rules, AfterIt, -1.0), Rules->CalmStrength);
		});
	});

	Describe("Escalation", [this]()
	{
		It("rises one level every 6 gusts", [this]()
		{
			TestEqual(TEXT("gusts 1-6"), WindMath::LevelOf(*Rules, 5), 0);
			TestEqual(TEXT("gust 7"), WindMath::LevelOf(*Rules, 6), 1);
			TestEqual(TEXT("gust 13"), WindMath::LevelOf(*Rules, 12), 2);
			TestTrue(TEXT("stronger"), WindMath::GustStrength(*Rules, 6) > WindMath::GustStrength(*Rules, 5));
		});

		It("turns ultra violent and never stops from the deadline day on", [this]()
		{
			const double Deadline = 2220.0;
			TestTrue(TEXT("calm or gust before"), WindMath::MeanStrengthAt(*Rules, Deadline - 1.0, Deadline) < Rules->DeadlineStrength);
			for (double T = Deadline + Rules->DeadlineRampSeconds; T < Deadline + 1200.0; T += 37.0)
			{
				if (!TestEqual(FString::Printf(TEXT("ultra violent at %.0f s"), T), WindMath::MeanStrengthAt(*Rules, T, Deadline), Rules->DeadlineStrength, 0.001f))
				{
					break;
				}
			}
		});
	});

	Describe("Shelter", [this]()
	{
		It("recognises the buildings and the bushes", [this]()
		{
			TestTrue(TEXT("minka"), WindMath::HasKeyword(TEXT("SM_Minka_A"), Rules->BuildingMeshKeywords));
			TestTrue(TEXT("naupaka"), WindMath::HasKeyword(TEXT("SM_Naupaka_B"), Rules->BushMeshKeywords));
			TestFalse(TEXT("terrain"), WindMath::HasKeyword(TEXT("SM_Raraka_Terrain_3_4"), Rules->BuildingMeshKeywords));
			TestTrue(TEXT("a building shelters more than a bush"), Rules->BuildingShelterFactor < Rules->BushShelterFactor);
		});
	});
}

#endif
