// monke_island

#include "Misc/AutomationTest.h"
#include "Fort/FortSiteMath.h"
#include "Fort/FortSiteRules.h"
#include "Misc/PackageName.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Fort.Site
BEGIN_DEFINE_SPEC(FFortSiteSpec, "monke_island.Fort.Site",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FFortSiteSpec)

void FFortSiteSpec::Define()
{
	Describe("Draw", [this]()
	{
		It("draws different sites, never more than the fragment has", [this]()
		{
			FRandomStream Stream(17);
			const TArray<int32> Two = FortSiteMath::PickSites(3, 2, Stream);
			TestEqual(TEXT("two"), Two.Num(), 2);
			TestTrue(TEXT("different"), Two.Num() == 2 && Two[0] != Two[1]);
			for (const int32 Index : Two)
			{
				TestTrue(TEXT("in the fragment"), Index >= 0 && Index < 3);
			}
			FRandomStream Again(17);
			TestEqual(TEXT("clamped"), FortSiteMath::PickSites(1, 4, Again).Num(), 1);
			FRandomStream None(3);
			TestEqual(TEXT("none asked"), FortSiteMath::PickSites(3, 0, None).Num(), 0);
			FRandomStream Empty(3);
			TestEqual(TEXT("no site"), FortSiteMath::PickSites(0, 1, Empty).Num(), 0);
		});

		It("draws the same sites for the same seed", [this]()
		{
			FRandomStream First(1767);
			FRandomStream Second(1767);
			TestTrue(TEXT("same"), FortSiteMath::PickSites(3, 3, First) == FortSiteMath::PickSites(3, 3, Second));
		});
	});

	Describe("Ground", [this]()
	{
		It("sets the fort by the high ground, a lone bump left out", [this]()
		{
			TArray<double> Heights = { 100.0, 120.0, 110.0, 130.0, 105.0, 115.0, 125.0, 108.0, 118.0, 400.0 };
			TestEqual(TEXT("highest"), FortSiteMath::HeightAtQuantile(Heights, 1.0f, 0.0), 400.0);
			TestEqual(TEXT("0.9 leaves the bump"), FortSiteMath::HeightAtQuantile(Heights, 0.9f, 0.0), 130.0);
			TestEqual(TEXT("lowest"), FortSiteMath::HeightAtQuantile(Heights, 0.0f, 0.0), 100.0);
			TestEqual(TEXT("no ground: the heightmap"), FortSiteMath::HeightAtQuantile({}, 0.9f, 42.0), 42.0);
		});

		It("measures the centre and 8 points per ring", [this]()
		{
			const TArray<float> Rings = { 1500.0f, 3000.0f };
			const TArray<FVector> Probes = FortSiteMath::GroundProbes(FVector(100.0, 200.0, 50.0), Rings);
			TestEqual(TEXT("count"), Probes.Num(), 17);
			TestTrue(TEXT("centre"), Probes[0].Equals(FVector(100.0, 200.0, 50.0)));
			TestEqual(TEXT("first ring"), FVector::Dist2D(Probes[1], Probes[0]), 1500.0, 0.01);
			TestEqual(TEXT("second ring"), FVector::Dist2D(Probes[16], Probes[0]), 3000.0, 0.01);
		});

		It("puts the high ground at its height in the fort's frame, the gate turned to the shore", [this]()
		{
			const FTransform Level = FortSiteMath::LevelTransform(FVector(1000.0, -2000.0, 80.0), 90.0f, 230.0, 50.0f);
			TestEqual(TEXT("origin under the ground"), Level.GetLocation().Z, 180.0, 0.01);
			TestEqual(TEXT("ground in the frame"), Level.InverseTransformPosition(FVector(1000.0, -2000.0, 230.0)).Z, 50.0, 0.01);
			// the gate (local +Y) of a fort turned 90 degrees faces world -X
			const FVector Gate = Level.TransformVectorNoScale(FVector(0.0, 1.0, 0.0));
			TestTrue(TEXT("gate"), Gate.Equals(FVector(-1.0, 0.0, 0.0), 0.001));
		});
	});

	Describe("Level", [this]()
	{
		It("names the fort level the same way on every machine, as a valid package name", [this]()
		{
			const FString Name = FortSiteMath::LevelName(TEXT("LI_FortFrench_Land"), TEXT("FRG_Raraka_07"), 2, 0x1234ABCD);
			TestEqual(TEXT("name"), Name, FString(TEXT("LI_FortFrench_Land_FRG_Raraka_07_2_1234ABCD")));
			TestEqual(TEXT("stable"), FortSiteMath::LevelName(TEXT("LI_FortFrench_Land"), TEXT("FRG_Raraka_07"), 2, 0x1234ABCD), Name);
			FText Reason;
			TestTrue(TEXT("valid"), FPackageName::IsValidLongPackageName(TEXT("/Game/MonkeIsland/Maps/Fragments/") + Name, false, &Reason));
			const FString Odd = FortSiteMath::LevelName(TEXT("LI_Fort"), TEXT("FRG 1/2"), 0, -1);
			TestFalse(TEXT("no space"), Odd.Contains(TEXT(" ")));
			TestFalse(TEXT("no slash"), Odd.Contains(TEXT("/")));
		});

		It("finds the sites of a fragment", [this]()
		{
			UFortSiteRules* Rules = NewObject<UFortSiteRules>();
			FFortFragmentSites& Entry = Rules->Fragments.AddDefaulted_GetRef();
			Entry.FragmentId = TEXT("FRG_Raraka_07");
			Entry.Sites.AddDefaulted();
			TestNotNull(TEXT("found"), Rules->FindFragment(TEXT("FRG_Raraka_07")));
			TestNull(TEXT("unknown"), Rules->FindFragment(TEXT("FRG_Raraka_99")));
		});
	});
}

#endif
