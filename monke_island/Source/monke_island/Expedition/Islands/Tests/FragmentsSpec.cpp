// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Islands/ExpeditionIslandDefinition.h"
#include "Expedition/Islands/ExpeditionIslandValidation.h"
#include "Expedition/Islands/FragmentRules.h"
#include "Expedition/Islands/IslandCatalog.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Fragments
// ADR-004, docs/features/fragments-tuamotu.md (T-087). Transient objects only: no asset of the project is needed.
BEGIN_DEFINE_SPEC(FFragmentsSpec, "monke_island.Fragments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TStrongObjectPtr<UExpeditionIslandDefinition> Definition;
	TStrongObjectPtr<UIslandCatalog> Catalog;
	TStrongObjectPtr<UFragmentRules> Rules;

	/** Anchorage with an id */
	static FExpeditionAnchorage MakeAnchorage(FName Id, EAnchorageRole Role, bool bSubmersion)
	{
		FExpeditionAnchorage Anchorage;
		Anchorage.AnchorageId = Id;
		Anchorage.Role = Role;
		Anchorage.bAvailableDuringSubmersion = bSubmersion;
		return Anchorage;
	}
END_DEFINE_SPEC(FFragmentsSpec)

void FFragmentsSpec::Define()
{
	BeforeEach([this]()
	{
		Rules.Reset(NewObject<UFragmentRules>());
		Definition.Reset(NewObject<UExpeditionIslandDefinition>(GetTransientPackage(), TEXT("DA_Island_Raraka_01_Test")));
		Definition->FragmentId = TEXT("FRG_Raraka_01");
		Definition->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_01.L_Frag_Raraka_01")));
		Definition->ZoneCount = 3;
		Definition->Anchorages = {
			MakeAnchorage(TEXT("Beach"), EAnchorageRole::Start, false),
			MakeAnchorage(TEXT("Cove"), EAnchorageRole::Resources, false),
			MakeAnchorage(TEXT("Exit"), EAnchorageRole::Fallback, true) };
		Catalog.Reset(NewObject<UIslandCatalog>());
	});

	AfterEach([this]()
	{
		Definition.Reset();
		Catalog.Reset();
		Rules.Reset();
	});

	Describe("Rules", [this]()
	{
		It("defaults to 60 s of load delay and 3 anchorages, one for the submersion", [this]()
		{
			TestEqual(TEXT("timeout"), Rules->LoadTimeoutSeconds, 60.0f);
			TestEqual(TEXT("anchorages"), Rules->MinAnchorageCount, 3);
			TestTrue(TEXT("submersion anchorage"), Rules->bRequireSubmersionAnchorage);
		});
	});

	Describe("PrimaryAssetId", [this]()
	{
		It("is (ExpeditionIsland, FragmentId), whatever the asset name", [this]()
		{
			const FPrimaryAssetId Id = Definition->GetPrimaryAssetId();
			TestEqual(TEXT("type"), Id.PrimaryAssetType, FPrimaryAssetType(TEXT("ExpeditionIsland")));
			TestEqual(TEXT("name"), Id.PrimaryAssetName, FName(TEXT("FRG_Raraka_01")));
			TestEqual(TEXT("helper"), UExpeditionIslandDefinition::MakePrimaryAssetId(TEXT("FRG_Raraka_01")), Id);
		});

		It("falls back to the asset name while the fragment id is empty", [this]()
		{
			Definition->FragmentId = NAME_None;
			TestEqual(TEXT("name"), Definition->GetPrimaryAssetId().PrimaryAssetName, FName(TEXT("DA_Island_Raraka_01_Test")));
		});

		It("is invalid on the class default object", [this]()
		{
			TestFalse(TEXT("CDO"), GetDefault<UExpeditionIslandDefinition>()->GetPrimaryAssetId().IsValid());
		});

		It("names the default catalog DA_IslandCatalog", [this]()
		{
			TestEqual(TEXT("catalog"), UIslandCatalog::GetDefaultCatalogId(), FPrimaryAssetId(TEXT("IslandCatalog"), TEXT("DA_IslandCatalog")));
		});
	});

	Describe("Valid definition", [this]()
	{
		It("passes with an id, a map, 3 zones and 3 anchorages", [this]()
		{
			TArray<FText> Issues;
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get(), &Issues), EFragmentError::None);
			TestEqual(TEXT("issues"), Issues.Num(), 0);
		});

		It("passes with null rules (class defaults)", [this]()
		{
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateDefinition(Definition.Get(), nullptr), EFragmentError::None);
		});
	});

	Describe("Invalid definition", [this]()
	{
		It("reports a null definition", [this]()
		{
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateDefinition(nullptr, Rules.Get()), EFragmentError::FragmentNotFound);
		});

		It("reports a missing map", [this]()
		{
			Definition->Map.Reset();
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::MapMissing);
		});

		It("reports an empty or malformed fragment id", [this]()
		{
			Definition->FragmentId = NAME_None;
			TestEqual(TEXT("none"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::InvalidFragmentId);
			Definition->FragmentId = TEXT("FRG_A?listen");
			TestEqual(TEXT("url chars"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::InvalidFragmentId);
			Definition->FragmentId = FName(FString::ChrN(ExpeditionIslands::MaxFragmentIdLength + 1, TEXT('A')));
			TestEqual(TEXT("too long"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::InvalidFragmentId);
		});

		It("reports no zone", [this]()
		{
			Definition->ZoneCount = 0;
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::InvalidZoneCount);
		});

		It("reports too few anchorages", [this]()
		{
			Definition->Anchorages.SetNum(2);
			Definition->Anchorages[1].bAvailableDuringSubmersion = true;
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::InvalidAnchorages);
		});

		It("reports a duplicated anchorage id", [this]()
		{
			Definition->Anchorages[1].AnchorageId = TEXT("Beach");
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::InvalidAnchorages);
		});

		It("reports no anchorage for the final submersion, unless the rules allow it", [this]()
		{
			Definition->Anchorages[2].bAvailableDuringSubmersion = false;
			TestEqual(TEXT("required"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::InvalidAnchorages);
			Rules->bRequireSubmersionAnchorage = false;
			TestEqual(TEXT("relaxed"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get()), EFragmentError::None);
		});

		It("lists every problem and returns the first", [this]()
		{
			Definition->FragmentId = NAME_None;
			Definition->Map.Reset();
			Definition->ZoneCount = 0;
			TArray<FText> Issues;
			TestEqual(TEXT("first"), ExpeditionIslands::ValidateDefinition(Definition.Get(), Rules.Get(), &Issues), EFragmentError::InvalidFragmentId);
			TestEqual(TEXT("issues"), Issues.Num(), 3);
		});
	});

	Describe("Catalog", [this]()
	{
		It("reports a missing catalog", [this]()
		{
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateCatalog(nullptr), EFragmentError::CatalogNotFound);
		});

		It("reports an empty catalog", [this]()
		{
			TArray<FText> Issues;
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateCatalog(Catalog.Get(), &Issues), EFragmentError::CatalogEmpty);
			TestEqual(TEXT("issues"), Issues.Num(), 1);
		});

		It("accepts a catalog with one fragment", [this]()
		{
			Catalog->Fragments.Add(UExpeditionIslandDefinition::MakePrimaryAssetId(TEXT("FRG_Raraka_01")));
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateCatalog(Catalog.Get()), EFragmentError::None);
		});

		It("reports a catalog whose entries are all wrong", [this]()
		{
			Catalog->Fragments.Add(FPrimaryAssetId(TEXT("Map"), TEXT("Lvl_Raraka")));
			Catalog->Fragments.Add(FPrimaryAssetId());
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateCatalog(Catalog.Get()), EFragmentError::NoValidFragment);
		});

		It("keeps the good entries and lists the wrong and duplicated ones", [this]()
		{
			const FPrimaryAssetId Good = UExpeditionIslandDefinition::MakePrimaryAssetId(TEXT("FRG_Raraka_01"));
			Catalog->Fragments = { Good, Good, FPrimaryAssetId(TEXT("Map"), TEXT("Lvl_Raraka")) };
			TArray<FText> Issues;
			TestEqual(TEXT("error"), ExpeditionIslands::ValidateCatalog(Catalog.Get(), &Issues), EFragmentError::None);
			TestEqual(TEXT("issues"), Issues.Num(), 2);
		});
	});

	Describe("Travel URL", [this]()
	{
		It("is the map package with the Island option", [this]()
		{
			const TValueOrError<FString, EFragmentError> URL = ExpeditionIslands::BuildTravelURL(Definition.Get());
			TestTrue(TEXT("value"), URL.HasValue());
			if (URL.HasValue())
			{
				TestEqual(TEXT("url"), URL.GetValue(), FString(TEXT("/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_01?Island=FRG_Raraka_01")));
			}
		});

		It("fails without a map, an id or a definition", [this]()
		{
			TestEqual(TEXT("null"), ExpeditionIslands::BuildTravelURL(nullptr).GetError(), EFragmentError::FragmentNotFound);
			Definition->Map.Reset();
			TestEqual(TEXT("map"), ExpeditionIslands::BuildTravelURL(Definition.Get()).GetError(), EFragmentError::MapMissing);
			Definition->FragmentId = NAME_None;
			TestEqual(TEXT("id"), ExpeditionIslands::BuildTravelURL(Definition.Get()).GetError(), EFragmentError::InvalidFragmentId);
		});

		It("reads back the fragment id of the option, and rejects malformed values", [this]()
		{
			TestEqual(TEXT("good"), ExpeditionIslands::ParseFragmentOption(TEXT("FRG_Raraka_01")), FName(TEXT("FRG_Raraka_01")));
			TestEqual(TEXT("null"), ExpeditionIslands::ParseFragmentOption(nullptr), FName());
			TestEqual(TEXT("empty"), ExpeditionIslands::ParseFragmentOption(TEXT("")), FName());
			TestEqual(TEXT("injection"), ExpeditionIslands::ParseFragmentOption(TEXT("FRG_01?Game=/Script/Engine.GameMode")), FName());
			const FString TooLong = FString::ChrN(ExpeditionIslands::MaxFragmentIdLength + 1, TEXT('A'));
			TestEqual(TEXT("too long"), ExpeditionIslands::ParseFragmentOption(*TooLong), FName());
		});
	});

	Describe("PickRandom", [this]()
	{
		const TArray<FName> Ids = { TEXT("FRG_Raraka_01"), TEXT("FRG_Raraka_02"), TEXT("FRG_Raraka_03"), TEXT("FRG_Raraka_04") };

		It("draws the same fragment with the same seed", [this, Ids]()
		{
			for (int32 Seed = 0; Seed < 50; ++Seed)
			{
				const TValueOrError<FName, EFragmentError> A = ExpeditionIslands::PickFragmentId(Ids, Seed, NAME_None);
				const TValueOrError<FName, EFragmentError> B = ExpeditionIslands::PickFragmentId(Ids, Seed, NAME_None);
				if (!TestTrue(TEXT("value"), A.HasValue() && B.HasValue()))
				{
					return;
				}
				TestEqual(*FString::Printf(TEXT("seed %d"), Seed), A.GetValue(), B.GetValue());
			}
		});

		It("reaches every fragment over many seeds", [this, Ids]()
		{
			TSet<FName> Seen;
			for (int32 Seed = 0; Seed < 200; ++Seed)
			{
				Seen.Add(ExpeditionIslands::PickFragmentId(Ids, Seed, NAME_None).GetValue());
			}
			TestEqual(TEXT("all drawn"), Seen.Num(), Ids.Num());
		});

		It("never draws the previous fragment twice in a row", [this, Ids]()
		{
			FName Previous = NAME_None;
			for (int32 Seed = 0; Seed < 200; ++Seed)
			{
				const FName Drawn = ExpeditionIslands::PickFragmentId(Ids, Seed, Previous).GetValue();
				if (!TestNotEqual(*FString::Printf(TEXT("seed %d"), Seed), Drawn, Previous))
				{
					return;
				}
				Previous = Drawn;
			}
		});

		It("keeps the excluded index out of every draw", [this]()
		{
			for (int32 Seed = 0; Seed < 100; ++Seed)
			{
				const int32 Index = ExpeditionIslands::PickIndex(3, Seed, 1);
				TestTrue(*FString::Printf(TEXT("seed %d in range"), Seed), Index == 0 || Index == 2);
			}
			TestTrue(TEXT("excluded out of range ignored"), FMath::IsWithin(ExpeditionIslands::PickIndex(3, 7, 9), 0, 3));
		});

		It("draws the only fragment of a one-entry catalog, even if it was the previous one", [this]()
		{
			const TArray<FName> One = { TEXT("FRG_Raraka_01") };
			const TValueOrError<FName, EFragmentError> Drawn = ExpeditionIslands::PickFragmentId(One, 42, TEXT("FRG_Raraka_01"));
			TestTrue(TEXT("value"), Drawn.HasValue());
			if (Drawn.HasValue())
			{
				TestEqual(TEXT("only"), Drawn.GetValue(), FName(TEXT("FRG_Raraka_01")));
			}
			TestEqual(TEXT("index"), ExpeditionIslands::PickIndex(1, 42, 0), 0);
		});

		It("reports an empty catalog clearly", [this]()
		{
			const TValueOrError<FName, EFragmentError> Drawn = ExpeditionIslands::PickFragmentId(TArray<FName>(), 42, NAME_None);
			TestTrue(TEXT("error"), Drawn.HasError());
			if (Drawn.HasError())
			{
				TestEqual(TEXT("CatalogEmpty"), Drawn.GetError(), EFragmentError::CatalogEmpty);
				TestFalse(TEXT("player message"), ExpeditionIslands::GetErrorText(Drawn.GetError()).IsEmpty());
			}
			TestEqual(TEXT("index"), ExpeditionIslands::PickIndex(0, 42, INDEX_NONE), static_cast<int32>(INDEX_NONE));
		});

		It("ignores a previous fragment that left the catalog", [this, Ids]()
		{
			TestTrue(TEXT("value"), ExpeditionIslands::PickFragmentId(Ids, 3, TEXT("FRG_Gone_09")).HasValue());
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
