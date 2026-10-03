// monke_island

#include "Misc/AutomationTest.h"
#include "Village/VillageMath.h"
#include "Village/VillageRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Village
BEGIN_DEFINE_SPEC(FVillageFragmentsSpec, "monke_island.Village.Fragments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FVillageFragmentsSpec)

void FVillageFragmentsSpec::Define()
{
	Describe("Fragment", [this]()
	{
		It("is the one whose arrival is nearest to a player", [this]()
		{
			const TArray<FVector> Arrivals = { FVector(0, 0, 0), FVector(33100, 0, 0), FVector(500000, 0, 0) };
			TestEqual(TEXT("in the boat at the second arrival"), VillageMath::PickFragment(Arrivals, { FVector(33000, 500, 0) }, 100000.0), 1);
			TestEqual(TEXT("the nearest of two players wins"), VillageMath::PickFragment(Arrivals, { FVector(480000, 0, 0), FVector(200, 0, 0) }, 100000.0), 0);
		});

		It("is none when nobody is near an arrival (a PIE at the PlayerStart of Motutapu)", [this]()
		{
			const TArray<FVector> Arrivals = { FVector(0, 0, 0), FVector(400000, 0, 0) };
			TestEqual(TEXT("too far"), VillageMath::PickFragment(Arrivals, { FVector(0, 259500, 0) }, 100000.0), static_cast<int32>(INDEX_NONE));
			TestEqual(TEXT("no player"), VillageMath::PickFragment(Arrivals, {}, 100000.0), static_cast<int32>(INDEX_NONE));
		});
	});

	Describe("Clearing", [this]()
	{
		It("tests a turned rectangle in its own frame, margin included", [this]()
		{
			FVillageClearArea Area;
			Area.Shape = EVillageClearShape::Rect;
			Area.Center = FVector2D(1000, 1000);
			Area.HalfSize = FVector2D(400, 100);
			Area.Yaw = 90.0f;
			// turned a quarter: the long side runs along Y
			TestTrue(TEXT("along the long side"), VillageMath::IsInArea(Area, FVector2D(1000, 1350), 0.0f));
			TestFalse(TEXT("across, beyond the short side"), VillageMath::IsInArea(Area, FVector2D(1150, 1000), 0.0f));
			TestTrue(TEXT("across, within the margin"), VillageMath::IsInArea(Area, FVector2D(1150, 1000), 60.0f));
		});

		It("tests a disc and a corridor", [this]()
		{
			FVillageClearArea Disc;
			Disc.Shape = EVillageClearShape::Circle;
			Disc.Center = FVector2D(0, 0);
			Disc.HalfSize = FVector2D(1100, 0);
			TestTrue(TEXT("on the square"), VillageMath::IsInArea(Disc, FVector2D(1000, 0), 0.0f));
			TestFalse(TEXT("off the square"), VillageMath::IsInArea(Disc, FVector2D(1200, 0), 50.0f));
			TestTrue(TEXT("crown touching the square"), VillageMath::IsInArea(Disc, FVector2D(1200, 0), 150.0f));

			FVillageClearArea Road;
			Road.Shape = EVillageClearShape::Path;
			Road.HalfSize = FVector2D(225, 0);
			Road.Points = { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 1000) };
			TestTrue(TEXT("on the first leg"), VillageMath::IsInArea(Road, FVector2D(500, 200), 0.0f));
			TestTrue(TEXT("on the second leg"), VillageMath::IsInArea(Road, FVector2D(1200, 600), 0.0f));
			TestFalse(TEXT("inside the bend, away from both legs"), VillageMath::IsInArea(Road, FVector2D(500, 500), 0.0f));
			TestFalse(TEXT("beyond the end"), VillageMath::IsInArea(Road, FVector2D(1000, 1400), 100.0f));
		});

		It("bounds an area with its pad", [this]()
		{
			FVillageClearArea Road;
			Road.Shape = EVillageClearShape::Path;
			Road.HalfSize = FVector2D(200, 0);
			Road.Points = { FVector2D(0, 0), FVector2D(1000, 500) };
			const FBox2D Box = VillageMath::AreaBounds(Road, 300.0f);
			TestTrue(TEXT("min"), Box.Min.Equals(FVector2D(-500, -500)));
			TestTrue(TEXT("max"), Box.Max.Equals(FVector2D(1500, 1000)));
		});

		It("names the plants by mesh prefix, the most precise rule first", [this]()
		{
			const UVillageRules* Rules = GetDefault<UVillageRules>();
			TestEqual(TEXT("fallen palm"), Rules->GetPlant(TEXT("SM_CoconutPalm_Fallen_02")), EVillagePlant::Fallen);
			TestEqual(TEXT("juvenile palm"), Rules->GetPlant(TEXT("SM_CoconutPalm_Juvenile_01")), EVillagePlant::Young);
			TestEqual(TEXT("sprout"), Rules->GetPlant(TEXT("SM_CoconutPalm_Sprout")), EVillagePlant::Ground);
			TestEqual(TEXT("palm"), Rules->GetPlant(TEXT("SM_CoconutPalm_Tall_03")), EVillagePlant::Palm);
			TestEqual(TEXT("young pandanus"), Rules->GetPlant(TEXT("SM_Pandanus_Young_A")), EVillagePlant::Young);
			TestEqual(TEXT("pandanus"), Rules->GetPlant(TEXT("SM_Pandanus_B")), EVillagePlant::Bush);
			TestEqual(TEXT("fern"), Rules->GetPlant(TEXT("SM_Fern_01")), EVillagePlant::Ground);
		});

		It("keeps the margins of village_clearing.py and clears only the vegetation", [this]()
		{
			const UVillageRules* Rules = GetDefault<UVillageRules>();
			TestEqual(TEXT("a bush 3 m from a house"), Rules->GetMargin(EVillageClearKind::Building, EVillagePlant::Bush), 300.0f);
			TestEqual(TEXT("a fern 30 cm from the road"), Rules->GetMargin(EVillageClearKind::Road, EVillagePlant::Ground), 30.0f);
			TestEqual(TEXT("the widest margin"), Rules->GetMaxMargin(), 500.0f);
			TestTrue(TEXT("a palm"), Rules->IsClearable(TEXT("/Game/MonkeIsland/Environment/Vegetation/Palms/Meshes/SM_CoconutPalm_Tall_03.SM_CoconutPalm_Tall_03")));
			TestTrue(TEXT("coral on the beach"), Rules->IsClearable(TEXT("/Game/MonkeIsland/Environment/Props/Coral/Meshes/SM_CoralBlock_01.SM_CoralBlock_01")));
			TestFalse(TEXT("a house of a village"), Rules->IsClearable(TEXT("/Game/MonkeIsland/Environment/Village/Meshes/SM_Minka.SM_Minka")));
		});
	});

	Describe("Doors", [this]()
	{
		It("put the gate along the yaw of the door marker", [this]()
		{
			const FVector Gate = VillageMath::GateOf(FVector(100, 100, 50), 90.0f, 513.0f);
			TestTrue(TEXT("gate"), Gate.Equals(FVector(100, 613, 50), 0.01));
		});
	});
}

#endif
