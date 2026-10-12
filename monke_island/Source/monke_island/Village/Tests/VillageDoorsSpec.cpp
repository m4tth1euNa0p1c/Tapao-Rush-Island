// monke_island

#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Village/VillageDoorMath.h"
#include "Village/VillageDoorsComponent.h"
#include "Village/VillageHouseKit.h"
#include "Village/VillageRules.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** The front shoji pair of the minka (minka_doors.json): bay from x 190 to 350 on the facade, track at 45 cm */
	FVillageHouseDoorway FrontShoji()
	{
		FVillageHouseDoorway Doorway;
		Doorway.Center = FVector(270.0, 0.0, 45.0);
		Doorway.Yaw = 0.0f;
		Doorway.HalfWidth = 80.0f;
		Doorway.Height = 210.0f;
		return Doorway;
	}

	FVillageHousePanel Leaf(UStaticMesh* Mesh, const FVector& Closed, const FVector& Travel, float OpenFraction, int32 Doorway)
	{
		FVillageHousePanel Panel;
		Panel.Mesh = Mesh;
		Panel.ClosedLocation = Closed;
		Panel.Travel = Travel;
		Panel.OpenFraction = OpenFraction;
		Panel.Doorway = Doorway;
		return Panel;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Village
BEGIN_DEFINE_SPEC(FVillageDoorsSpec, "monke_island.Village.Doors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FVillageDoorsSpec)

void FVillageDoorsSpec::Define()
{
	Describe("Trigger", [this]()
	{
		It("opens for a pawn in front of or behind the bay, not beside it", [this]()
		{
			const FVillageHouseDoorway Doorway = FrontShoji();
			// a bean (radius 34) walking up to the facade from the engawa (+Y is outside)
			TestTrue(TEXT("120 cm in front"), VillageDoorMath::IsNearDoorway(Doorway, FVector(270.0, 120.0, 100.0), 34.0f, 150.0f, 30.0f));
			TestTrue(TEXT("inside the room"), VillageDoorMath::IsNearDoorway(Doorway, FVector(300.0, -150.0, 100.0), 34.0f, 150.0f, 30.0f));
			TestFalse(TEXT("too far in front"), VillageDoorMath::IsNearDoorway(Doorway, FVector(270.0, 220.0, 100.0), 34.0f, 150.0f, 30.0f));
			TestFalse(TEXT("beside the bay"), VillageDoorMath::IsNearDoorway(Doorway, FVector(470.0, 50.0, 100.0), 34.0f, 150.0f, 30.0f));
			TestTrue(TEXT("within the side margin"), VillageDoorMath::IsNearDoorway(Doorway, FVector(400.0, 50.0, 100.0), 34.0f, 150.0f, 30.0f));
		});

		It("turns with the track of the doorway and ignores a pawn on the roof", [this]()
		{
			FVillageHouseDoorway Side = FrontShoji();
			// a side shoji: its track runs along -Y (yaw -90), x = 900 is the wall
			Side.Center = FVector(900.0, -90.0, 45.0);
			Side.Yaw = -90.0f;
			TestTrue(TEXT("outside the side wall"), VillageDoorMath::IsNearDoorway(Side, FVector(1020.0, -90.0, 100.0), 34.0f, 150.0f, 30.0f));
			TestFalse(TEXT("along the wall, past the bay"), VillageDoorMath::IsNearDoorway(Side, FVector(900.0, -330.0, 100.0), 34.0f, 150.0f, 30.0f));
			TestFalse(TEXT("on the roof"), VillageDoorMath::IsNearDoorway(Side, FVector(900.0, -90.0, 500.0), 34.0f, 150.0f, 30.0f));
			TestTrue(TEXT("on the ground below the engawa"), VillageDoorMath::IsNearDoorway(Side, FVector(980.0, -90.0, -20.0), 34.0f, 150.0f, 30.0f));
		});
	});

	// T-301, review R-08: the server opens ahead of a running pawn, a client's pawn by half its round trip more
	Describe("Lead", [this]()
	{
		It("adds the share of the slide, the look interval, a margin and the capped half round trip", [this]()
		{
			// the minka: looks every 0.1 s, slides in 0.35 s, half open is enough, 0.05 s margin, half round trip capped at 0.2 s
			TestEqual(TEXT("own pawn or a creature"), VillageDoorMath::LeadSeconds(0.0f, 0.1f, 0.35f, 0.5f, 0.05f, 0.2f), 0.325f, 0.0001f);
			TestEqual(TEXT("client at 100 ms"), VillageDoorMath::LeadSeconds(0.05f, 0.1f, 0.35f, 0.5f, 0.05f, 0.2f), 0.375f, 0.0001f);
			TestEqual(TEXT("huge ping capped"), VillageDoorMath::LeadSeconds(0.5f, 0.1f, 0.35f, 0.5f, 0.05f, 0.2f), 0.525f, 0.0001f);
			TestEqual(TEXT("nothing negative"), VillageDoorMath::LeadSeconds(-1.0f, -1.0f, -1.0f, 2.0f, -1.0f, -1.0f), 0.0f, 0.0001f);
		});

		It("crosses a box with a segment, through it or ending inside", [this]()
		{
			const FVector Min(-10.0, -10.0, -10.0);
			const FVector Max(10.0, 10.0, 10.0);
			TestTrue(TEXT("ends inside"), VillageDoorMath::SegmentHitsBox(FVector(-30.0, 0.0, 0.0), FVector(0.0, 0.0, 0.0), Min, Max));
			TestTrue(TEXT("passes through"), VillageDoorMath::SegmentHitsBox(FVector(-30.0, 0.0, 0.0), FVector(30.0, 0.0, 0.0), Min, Max));
			TestFalse(TEXT("stops short"), VillageDoorMath::SegmentHitsBox(FVector(-30.0, 0.0, 0.0), FVector(-11.0, 0.0, 0.0), Min, Max));
			TestFalse(TEXT("parallel beside it"), VillageDoorMath::SegmentHitsBox(FVector(-30.0, 20.0, 0.0), FVector(30.0, 20.0, 0.0), Min, Max));
			TestTrue(TEXT("a point inside"), VillageDoorMath::SegmentHitsBox(FVector(1.0, 2.0, 3.0), FVector(1.0, 2.0, 3.0), Min, Max));
		});

		It("opens for a sprinter heading for the bay before it reaches the trigger, never behind it", [this]()
		{
			const FVillageHouseDoorway Doorway = FrontShoji();
			// a bean sprinting at 600 cm/s toward the facade (+Y is outside), 0.375 s ahead (client at 100 ms): 225 cm
			const FVector Toward(0.0, -600.0, 0.0);
			TestFalse(TEXT("not near yet"), VillageDoorMath::IsNearDoorway(Doorway, FVector(270.0, 400.0, 100.0), 34.0f, 150.0f, 30.0f));
			TestTrue(TEXT("opens ahead"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 400.0, 100.0), Toward, 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
			TestFalse(TEXT("too far for its lead"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 450.0, 100.0), Toward, 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
			TestFalse(TEXT("running away"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 250.0, 100.0), -Toward, 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
			TestFalse(TEXT("on the roof"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 400.0, 500.0), Toward, 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
			TestTrue(TEXT("standing in it"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 120.0, 100.0), FVector::ZeroVector, 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
		});

		It("opens for a pawn running along the facade into the bay, not for one passing far from it", [this]()
		{
			const FVillageHouseDoorway Doorway = FrontShoji();
			const FVector Along(-600.0, 0.0, 0.0);
			TestTrue(TEXT("along the facade"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(600.0, 100.0, 100.0), Along, 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
			TestFalse(TEXT("far from the facade"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(600.0, 300.0, 100.0), Along, 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
		});

		It("looks no farther than the longest lead, and catches a pawn that would run through the bay", [this]()
		{
			const FVillageHouseDoorway Doorway = FrontShoji();
			// 5000 cm/s for 0.375 s would reach 1875 cm: capped at 400 cm, it ends 300 cm in front, out of the trigger
			TestFalse(TEXT("capped"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 700.0, 100.0), FVector(0.0, -5000.0, 0.0), 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
			TestTrue(TEXT("longer cap"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 700.0, 100.0), FVector(0.0, -5000.0, 0.0), 34.0f, 150.0f, 30.0f, 0.375f, 600.0f));
			TestTrue(TEXT("through the bay"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 300.0, 100.0), FVector(0.0, -2000.0, 0.0), 34.0f, 150.0f, 30.0f, 0.375f, 1000.0f));
			TestFalse(TEXT("a falling pawn does not open it"), VillageDoorMath::IsNearDoorwayAhead(Doorway, FVector(270.0, 400.0, 600.0), FVector(0.0, 0.0, -3000.0), 34.0f, 150.0f, 30.0f, 0.375f, 400.0f));
		});
	});

	Describe("Slide", [this]()
	{
		It("eases a panel from closed to open along its travel", [this]()
		{
			const FVillageHousePanel Panel = Leaf(nullptr, FVector(190.0, 1.8, 45.0), FVector(78.0, 0.0, 0.0), 1.0f, 0);
			TestTrue(TEXT("closed"), VillageDoorMath::PanelLocation(Panel, 0.0f).Equals(FVector(190.0, 1.8, 45.0), 0.01));
			TestTrue(TEXT("open"), VillageDoorMath::PanelLocation(Panel, 1.0f).Equals(FVector(268.0, 1.8, 45.0), 0.01));
			TestTrue(TEXT("halfway"), VillageDoorMath::PanelLocation(Panel, 0.5f).Equals(FVector(229.0, 1.8, 45.0), 0.01));
			TestTrue(TEXT("slow start"), VillageDoorMath::Ease(0.1f) < 0.1f);
			TestTrue(TEXT("clamped"), VillageDoorMath::PanelLocation(Panel, 3.0f).Equals(FVector(268.0, 1.8, 45.0), 0.01));
		});

		It("turns a hinged leaf about its pivot and leaves a sliding one square", [this]()
		{
			FVillageHousePanel Hinged = Leaf(nullptr, FVector(-60.0, 95.0, 102.0), FVector::ZeroVector, 1.0f, 0);
			Hinged.Yaw = 0.0f;
			Hinged.OpenYaw = -100.0f;
			TestEqual(TEXT("closed"), VillageDoorMath::PanelYaw(Hinged, 0.0f), 0.0f);
			TestEqual(TEXT("open"), VillageDoorMath::PanelYaw(Hinged, 1.0f), -100.0f);
			TestTrue(TEXT("its pivot stays"), VillageDoorMath::PanelLocation(Hinged, 1.0f).Equals(FVector(-60.0, 95.0, 102.0), 0.01));
			const FVillageHousePanel Sliding = Leaf(nullptr, FVector::ZeroVector, FVector(78.0, 0.0, 0.0), 1.0f, 0);
			TestEqual(TEXT("sliding"), VillageDoorMath::PanelYaw(Sliding, 1.0f), 0.0f);
		});

		It("steps toward its target without overshooting, both ways", [this]()
		{
			TestEqual(TEXT("a third of 0.3 s"), VillageDoorMath::StepToward(0.0f, 1.0f, 0.1f, 0.3f), 1.0f / 3.0f, 0.001f);
			TestEqual(TEXT("reaches"), VillageDoorMath::StepToward(0.9f, 1.0f, 0.1f, 0.3f), 1.0f);
			TestEqual(TEXT("closes"), VillageDoorMath::StepToward(0.2f, 0.0f, 0.1f, 0.3f), 0.0f);
			TestEqual(TEXT("still"), VillageDoorMath::StepToward(1.0f, 1.0f, 0.1f, 0.3f), 1.0f);
		});

		It("keeps a doorway open while occupied and for the close delay after", [this]()
		{
			TestTrue(TEXT("occupied"), VillageDoorMath::ShouldBeOpen(true, -1.0, 10.0, 1.2f));
			TestTrue(TEXT("just left"), VillageDoorMath::ShouldBeOpen(false, 9.5, 10.0, 1.2f));
			TestFalse(TEXT("left long ago"), VillageDoorMath::ShouldBeOpen(false, 8.0, 10.0, 1.2f));
			TestFalse(TEXT("never visited"), VillageDoorMath::ShouldBeOpen(false, -1.0, 10.0, 1.2f));
		});
	});

	Describe("Contents", [this]()
	{
		It("draws distinct slots, never more than there are", [this]()
		{
			FRandomStream Stream(186);
			const TArray<int32> Three = VillageDoorMath::PickDistinct(14, 3, Stream);
			TestEqual(TEXT("three"), Three.Num(), 3);
			TestTrue(TEXT("distinct"), Three[0] != Three[1] && Three[1] != Three[2] && Three[0] != Three[2]);
			for (const int32 Index : Three)
			{
				TestTrue(TEXT("in range"), Index >= 0 && Index < 14);
			}
			TestEqual(TEXT("clamped"), VillageDoorMath::PickDistinct(2, 5, Stream).Num(), 2);
			TestEqual(TEXT("none"), VillageDoorMath::PickDistinct(4, 0, Stream).Num(), 0);
			FRandomStream A(7);
			FRandomStream B(7);
			TestTrue(TEXT("same seed, same draw"), VillageDoorMath::PickDistinct(14, 3, A) == VillageDoorMath::PickDistinct(14, 3, B));
		});

		It("rests the bottom of an object on its slot", [this]()
		{
			// pivot at the centre of a 40 cm relic, slot on the tokonoma at 55 cm
			const FVector Rested = VillageDoorMath::RestOnSurface(FVector(10.0, 20.0, 55.0), 35.0, 55.0, 0.5f);
			TestTrue(TEXT("bottom on the board"), Rested.Equals(FVector(10.0, 20.0, 75.5), 0.001));
		});

		It("files houses by grid cell, negative coordinates included", [this]()
		{
			TestTrue(TEXT("origin"), VillageDoorMath::CellOf(FVector2D(10.0, 10.0), 2000.0) == FIntPoint(0, 0));
			TestTrue(TEXT("negative"), VillageDoorMath::CellOf(FVector2D(-10.0, -2010.0), 2000.0) == FIntPoint(-1, -2));
		});
	});

	Describe("Kit", [this]()
	{
		It("builds only the houses of its source mesh, and only with a shell", [this]()
		{
			UVillageRules* Rules = NewObject<UVillageRules>();
			UVillageHouseKit* Kit = NewObject<UVillageHouseKit>();
			UStaticMesh* Minka = NewObject<UStaticMesh>();
			UStaticMesh* Kura = NewObject<UStaticMesh>();
			Kit->SourceMesh = Minka;
			Rules->HouseKits.Add(Kit);
			TestNull(TEXT("no shell yet: the house stays as it is"), Rules->FindHouseKit(Minka));
			Kit->ShellMesh = NewObject<UStaticMesh>();
			TestTrue(TEXT("the minka"), Rules->FindHouseKit(Minka) == Kit);
			TestNull(TEXT("a storehouse"), Rules->FindHouseKit(Kura));
			TestNull(TEXT("nothing"), Rules->FindHouseKit(nullptr));
		});

		It("numbers the doorways and panels of every house in the order they are added", [this]()
		{
			UVillageHouseKit* Kit = NewObject<UVillageHouseKit>();
			UStaticMesh* Shoji = NewObject<UStaticMesh>();
			Kit->Doorways = { FrontShoji(), FrontShoji() };
			Kit->Panels = {
				Leaf(Shoji, FVector(190.0, 1.8, 45.0), FVector(78.0, 0.0, 0.0), 1.0f, 0),
				Leaf(Shoji, FVector(268.0, -1.8, 45.0), FVector(-78.0, 0.0, 0.0), 0.0f, 0),
				Leaf(Shoji, FVector(370.0, 1.8, 45.0), FVector(78.0, 0.0, 0.0), 1.0f, 1),
				// a panel of a doorway the kit does not have is skipped
				Leaf(Shoji, FVector(0.0, 0.0, 0.0), FVector(10.0, 0.0, 0.0), 1.0f, 7),
			};
			UVillageDoorsComponent* Doors = NewObject<UVillageDoorsComponent>();
			Doors->AddHouse(Kit, FTransform(FVector(1000.0, 0.0, 0.0)), 0);
			Doors->AddHouse(Kit, FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(-5000.0, 0.0, 0.0)), 1);
			Doors->AddHouse(nullptr, FTransform::Identity, 0);
			TestEqual(TEXT("houses"), Doors->GetHouseCount(), 2);
			TestEqual(TEXT("doorways"), Doors->GetDoorwayCount(), 4);
			TestEqual(TEXT("panels"), Doors->GetPanelCount(), 6);
			TestFalse(TEXT("closed before the game"), Doors->IsDoorwayOpen(0));
		});
	});
}

#endif
