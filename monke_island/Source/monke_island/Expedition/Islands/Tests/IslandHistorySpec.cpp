// monke_island
#include "Misc/AutomationTest.h"
#include "Expedition/Islands/ExpeditionIslandValidation.h"
#include "Expedition/Islands/FragmentRules.h"

#if WITH_AUTOMATION_TESTS
BEGIN_DEFINE_SPEC(FIslandHistorySpec, "monke_island.Fragments.History",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FIslandHistorySpec)

void FIslandHistorySpec::Define()
{
	It("defaults to eight played islands", [this]()
	{
		TestEqual(TEXT("default"), GetDefault<UFragmentRules>()->RecentIslandCount, 8);
	});
	It("excludes the last eight over a deterministic sequence", [this]()
	{
		TArray<FName> Ids, History;
		for (int32 Index = 0; Index < 20; ++Index)
		{
			Ids.Add(FName(*FString::Printf(TEXT("FRG_%02d"), Index)));
		}
		for (int32 Seed = -100; Seed < 100; ++Seed)
		{
			const FName Pick = ExpeditionIslands::PickFragmentId(Ids, Seed, History, 8).GetValue();
			TestEqual(TEXT("replay"), Pick, ExpeditionIslands::PickFragmentId(Ids, Seed, History, 8).GetValue());
			TestFalse(TEXT("not recently played"), History.Contains(Pick));
			ExpeditionIslands::RememberPlayedIsland(History, Pick, 8);
			TestTrue(TEXT("bounded"), History.Num() <= 8);
		}
	});
	It("cycles small catalogs by least recent play without immediate repeats", [this]()
	{
		const TArray<FName> Ids = { TEXT("A"), TEXT("B"), TEXT("C") };
		TArray<FName> History;
		for (int32 Seed = 0; Seed < 50; ++Seed)
		{
			const FName Pick = ExpeditionIslands::PickFragmentId(Ids, Seed, History, 8).GetValue();
			if (History.Num() >= 3)
			{
				TestEqual(TEXT("cycle"), Pick, History[History.Num() - 3]);
			}
			ExpeditionIslands::RememberPlayedIsland(History, Pick, 8);
		}
	});
	It("uses the latest occurrence and ignores removed islands", [this]()
	{
		const TArray<FName> Ids = { TEXT("A"), TEXT("B"), TEXT("C") };
		const TArray<FName> History = { TEXT("A"), TEXT("B"), TEXT("C"), TEXT("A"), TEXT("Gone") };
		TestEqual(TEXT("oldest still present"), ExpeditionIslands::PickFragmentId(Ids, 7, History, 8).GetValue(), FName(TEXT("B")));
	});
	It("handles empty singleton and disabled history", [this]()
	{
		TArray<FName> History = { TEXT("A") };
		TestTrue(TEXT("empty error"), ExpeditionIslands::PickFragmentId(TArray<FName>(), 0, History, 8).HasError());
		TestEqual(TEXT("singleton"), ExpeditionIslands::PickFragmentId(History, 0, History, 8).GetValue(), FName(TEXT("A")));
		const TArray<FName> Ids = { TEXT("A"), TEXT("B") };
		for (int32 Seed = 0; Seed < 20; ++Seed)
		{
			TestEqual(TEXT("disabled"), ExpeditionIslands::PickFragmentId(Ids, Seed, History, 0).GetValue(),
				ExpeditionIslands::PickFragmentId(Ids, Seed, NAME_None).GetValue());
		}
		ExpeditionIslands::RememberPlayedIsland(History, TEXT("B"), -1);
		TestTrue(TEXT("negative cleared"), History.IsEmpty());
	});
	It("trims oldest entries when capacity shrinks and rejects malformed ids", [this]()
	{
		TArray<FName> History = { TEXT("A"), TEXT("B"), TEXT("C") };
		ExpeditionIslands::RememberPlayedIsland(History, TEXT("D"), 2);
		TestEqual(TEXT("size"), History.Num(), 2);
		TestEqual(TEXT("oldest"), History[0], FName(TEXT("C")));
		ExpeditionIslands::RememberPlayedIsland(History, TEXT("Bad?listen"), 2);
		TestEqual(TEXT("invalid ignored"), History.Last(), FName(TEXT("D")));
	});
}
#endif
