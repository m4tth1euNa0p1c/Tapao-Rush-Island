// monke_island

#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"
#include "Fort/FortGarrisonMath.h"
#include "Fort/FortSoldierRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Fort.Garrison
BEGIN_DEFINE_SPEC(FFortGarrisonSpec, "monke_island.Fort.Garrison",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** The bastioned cordon of the fort of the maquette (m), Y turned to Unreal's (cm) like the setup does */
	static TArray<FVector2D> FortOutline()
	{
		const double Trace[][2] = {
			{ 24.2, 14.0 }, { 29.2, 14.0 }, { 31.7, 27.7 }, { 18.0, 25.2 }, { 18.0, 20.2 }, { -18.0, 20.2 }, { -18.0, 25.2 },
			{ -31.7, 27.7 }, { -29.2, 14.0 }, { -24.2, 14.0 }, { -24.2, -14.0 }, { -29.2, -14.0 }, { -31.7, -27.7 }, { -18.0, -25.2 },
			{ -18.0, -20.2 }, { 18.0, -20.2 }, { 18.0, -25.2 }, { 31.7, -27.7 }, { 29.2, -14.0 }, { 24.2, -14.0 } };
		TArray<FVector2D> Outline;
		for (const auto& Point : Trace)
		{
			Outline.Emplace(Point[0] * 100.0, -Point[1] * 100.0);
		}
		return Outline;
	}

END_DEFINE_SPEC(FFortGarrisonSpec)

void FFortGarrisonSpec::Define()
{
	Describe("Outline", [this]()
	{
		It("knows the court, the bastions and the outside of the fort", [this]()
		{
			const TArray<FVector2D> Fort = FortOutline();
			TestTrue(TEXT("court"), FortGarrisonMath::IsInside(Fort, FVector2D(0.0, 0.0)));
			TestTrue(TEXT("rampart by the gate"), FortGarrisonMath::IsInside(Fort, FVector2D(0.0, 1900.0)));
			TestTrue(TEXT("bastion tip"), FortGarrisonMath::IsInside(Fort, FVector2D(2900.0, -2500.0)));
			TestFalse(TEXT("out of the gate"), FortGarrisonMath::IsInside(Fort, FVector2D(0.0, 2340.0)));
			TestFalse(TEXT("ditch between two bastions"), FortGarrisonMath::IsInside(Fort, FVector2D(2700.0, 0.0)));
			TestFalse(TEXT("glacis"), FortGarrisonMath::IsInside(Fort, FVector2D(0.0, -4000.0)));
		});

		It("brings a point outside back inside, past the edge by the margin", [this]()
		{
			const TArray<FVector2D> Fort = FortOutline();
			const FVector2D Gate(0.0, 2600.0);
			const FVector2D Held = FortGarrisonMath::ClampInside(Fort, Gate, 80.0);
			TestTrue(TEXT("inside"), FortGarrisonMath::IsInside(Fort, Held));
			TestTrue(TEXT("on the curtain, 80 cm in"), FMath::IsNearlyEqual(Held.Y, 1940.0, 1.0) && FMath::IsNearlyEqual(Held.X, 0.0, 1.0));
			const FVector2D Court(500.0, 300.0);
			TestEqual(TEXT("a point inside stays"), FortGarrisonMath::ClampInside(Fort, Court, 80.0), Court);
		});

		It("works whatever the turn of the outline", [this]()
		{
			TArray<FVector2D> Square = { { 0.0, 0.0 }, { 1000.0, 0.0 }, { 1000.0, 1000.0 }, { 0.0, 1000.0 } };
			const FVector2D Out(1500.0, 500.0);
			const FVector2D A = FortGarrisonMath::ClampInside(Square, Out, 50.0);
			Algo::Reverse(Square);
			const FVector2D B = FortGarrisonMath::ClampInside(Square, Out, 50.0);
			TestTrue(TEXT("same point"), A.Equals(B, 0.5));
			TestTrue(TEXT("50 cm in"), A.Equals(FVector2D(950.0, 500.0), 0.5));
		});
	});

	Describe("Fight", [this]()
	{
		It("fires in range, closes in out of range or out of sight", [this]()
		{
			TestEqual(TEXT("in range"), FortGarrisonMath::PickMove(900.0f, true, 6, true, 140.0f, 300.0f, 1500.0f), EFortSoldierMove::Fire);
			TestEqual(TEXT("too far"), FortGarrisonMath::PickMove(2000.0f, true, 6, true, 140.0f, 300.0f, 1500.0f), EFortSoldierMove::Close);
			TestEqual(TEXT("hidden"), FortGarrisonMath::PickMove(900.0f, false, 6, true, 140.0f, 300.0f, 1500.0f), EFortSoldierMove::Close);
		});

		It("reloads when it is not pressed, takes steel when pressed or out of shots", [this]()
		{
			TestEqual(TEXT("not loaded"), FortGarrisonMath::PickMove(900.0f, true, 5, false, 140.0f, 300.0f, 1500.0f), EFortSoldierMove::Reload);
			TestEqual(TEXT("pressed"), FortGarrisonMath::PickMove(100.0f, true, 5, false, 140.0f, 300.0f, 1500.0f), EFortSoldierMove::Melee);
			TestEqual(TEXT("no shot left (owner: then it attacks)"), FortGarrisonMath::PickMove(900.0f, true, 0, false, 140.0f, 300.0f, 1500.0f),
				EFortSoldierMove::Melee);
			TestEqual(TEXT("too close to aim"), FortGarrisonMath::PickMove(200.0f, true, 5, true, 140.0f, 300.0f, 1500.0f), EFortSoldierMove::Melee);
		});

		It("hits more often close than far", [this]()
		{
			TestEqual(TEXT("near"), FortGarrisonMath::HitChance(300.0f, 500.0f, 1500.0f, 0.55f, 0.15f), 0.55f);
			TestEqual(TEXT("far"), FortGarrisonMath::HitChance(2000.0f, 500.0f, 1500.0f, 0.55f, 0.15f), 0.15f);
			TestTrue(TEXT("between"), FMath::IsNearlyEqual(FortGarrisonMath::HitChance(1000.0f, 500.0f, 1500.0f, 0.55f, 0.15f), 0.35f, 0.001f));
		});

		It("sees in front, not behind", [this]()
		{
			TestTrue(TEXT("in front"), FortGarrisonMath::IsInView(FVector::ForwardVector, FVector(100.0, 60.0, 0.0), 75.0f));
			TestFalse(TEXT("behind"), FortGarrisonMath::IsInView(FVector::ForwardVector, FVector(-100.0, 10.0, 0.0), 75.0f));
		});
	});

	Describe("Patrol", [this]()
	{
		It("walks to another post each time", [this]()
		{
			for (int32 Step = 0; Step < 20; ++Step)
			{
				const int32 Next = FortGarrisonMath::NextPatrolPoint(6, 2, Step / 20.0f);
				TestTrue(TEXT("another post"), Next != 2 && Next >= 0 && Next < 6);
			}
			TestEqual(TEXT("a single post"), FortGarrisonMath::NextPatrolPoint(1, 0, 0.7f), 0);
			TestEqual(TEXT("no post"), FortGarrisonMath::NextPatrolPoint(0, -1, 0.5f), INDEX_NONE);
		});

		It("plays the patrol clip faster when it walks faster, within the bean's limits", [this]()
		{
			TestTrue(TEXT("patrol pace"), FMath::IsNearlyEqual(FortGarrisonMath::LocomotionRate(98.0f, 49.0f, 0.6f, 3.2f), 2.0f, 0.001f));
			TestEqual(TEXT("capped"), FortGarrisonMath::LocomotionRate(600.0f, 112.0f, 0.6f, 3.2f), 3.2f);
		});

		It("gives the officer more health and the skirmisher the sabre (roles of the rules)", [this]()
		{
			UFortSoldierRules* Rules = NewObject<UFortSoldierRules>();
			FFortSoldierRoleRules& Officer = Rules->Roles.AddDefaulted_GetRef();
			Officer.Role = EFortSoldierRole::Officer;
			Officer.HealthScale = 2.0f;
			Officer.bBayonet = false;
			TestEqual(TEXT("officer"), Rules->GetRole(EFortSoldierRole::Officer).HealthScale, 2.0f);
			TestEqual(TEXT("a kind not listed plays the defaults"), Rules->GetRole(EFortSoldierRole::Grenadier).HealthScale, 1.0f);
			TestTrue(TEXT("blows"), Rules->GetBlow(EFortSoldierBlow::Combo) == &Rules->Combo && Rules->GetBlow(EFortSoldierBlow::None) == nullptr);
		});
	});

	Describe("Garrison of the day (owner 2026-10-09: tout devient plus dur)", [this]()
	{
		It("completes or thins the garrison placed in the fort to the soldiers the day wants", [this]()
		{
			TestEqual(TEXT("day 6: two more than the seven placed"), FortGarrisonMath::GarrisonChange(7, 9), 2);
			TestEqual(TEXT("day 3: as placed"), FortGarrisonMath::GarrisonChange(7, 7), 0);
			TestEqual(TEXT("a smaller garrison asked: two leave"), FortGarrisonMath::GarrisonChange(7, 5), -2);
			TestEqual(TEXT("no wish (0): as placed"), FortGarrisonMath::GarrisonChange(7, 0), 0);
			TestEqual(TEXT("nobody placed to clone"), FortGarrisonMath::GarrisonChange(0, 9), 0);
		});

		It("posts the soldiers added after those the placed ones took, around the patrol", [this]()
		{
			TestEqual(TEXT("first added: the 8th post"), FortGarrisonMath::SpawnPost(0, 7, 17), 7);
			TestEqual(TEXT("second added"), FortGarrisonMath::SpawnPost(1, 7, 17), 8);
			TestEqual(TEXT("around the patrol"), FortGarrisonMath::SpawnPost(12, 7, 17), 2);
			TestEqual(TEXT("no post"), FortGarrisonMath::SpawnPost(0, 7, 0), INDEX_NONE);
		});
	});

	Describe("Guards of the buildings (T-332, owner 2026-10-11: plus de soldats dans les bâtiments)", [this]()
	{
		It("asks the guards of the day, the last entry for the days after, the test count without a campaign", [this]()
		{
			const TArray<int32> ByDay = { 4, 4, 4, 5, 5, 5, 6, 6, 6, 6 };
			TestEqual(TEXT("day 1"), FortGarrisonMath::BuildingGuardsOfDay(ByDay, 1, 3), 4);
			TestEqual(TEXT("day 4"), FortGarrisonMath::BuildingGuardsOfDay(ByDay, 4, 3), 5);
			TestEqual(TEXT("day 10"), FortGarrisonMath::BuildingGuardsOfDay(ByDay, 10, 3), 6);
			TestEqual(TEXT("past the table: its last entry"), FortGarrisonMath::BuildingGuardsOfDay(ByDay, 14, 3), 6);
			TestEqual(TEXT("no campaign (day 0)"), FortGarrisonMath::BuildingGuardsOfDay(ByDay, 0, 3), 3);
			TestEqual(TEXT("no table"), FortGarrisonMath::BuildingGuardsOfDay(TArray<int32>(), 5, 3), 3);
			TestEqual(TEXT("a negative entry posts none"), FortGarrisonMath::BuildingGuardsOfDay(TArray<int32>{ -2 }, 1, 3), 0);
			TestEqual(TEXT("a negative test count posts none"), FortGarrisonMath::BuildingGuardsOfDay(ByDay, 0, -1), 0);
		});

		It("posts one guard per post at most, the fort held under its most soldiers", [this]()
		{
			TestEqual(TEXT("day 3 of the owner: 4 guards on top of 7"), FortGarrisonMath::BuildingGuardsToPost(4, 7, 18, 12), 4);
			TestEqual(TEXT("fewer posts than wished"), FortGarrisonMath::BuildingGuardsToPost(6, 7, 18, 3), 3);
			TestEqual(TEXT("the fort full: 12 of the court + 6 = 18"), FortGarrisonMath::BuildingGuardsToPost(8, 12, 18, 12), 6);
			TestEqual(TEXT("the fort already over its most"), FortGarrisonMath::BuildingGuardsToPost(4, 20, 18, 12), 0);
			TestEqual(TEXT("no limit"), FortGarrisonMath::BuildingGuardsToPost(8, 30, 0, 12), 8);
			TestEqual(TEXT("no post"), FortGarrisonMath::BuildingGuardsToPost(4, 7, 18, 0), 0);
			TestEqual(TEXT("none wished"), FortGarrisonMath::BuildingGuardsToPost(-3, 7, 18, 12), 0);
		});

		It("spreads the guards over the buildings: one each first, then a second one each", [this]()
		{
			// posts: barracks 0, 1, 2 ; guardhouse 3 ; governor 4, 5 ; tower 6
			const TArray<FName> Posts = { TEXT("barracks"), TEXT("barracks"), TEXT("barracks"), TEXT("guardhouse"), TEXT("governor"),
				TEXT("governor"), TEXT("tower") };
			TestEqual(TEXT("one per building first"), FortGarrisonMath::SpreadOverBuildings(Posts, 4, 0), TArray<int32>({ 0, 3, 4, 6 }));
			TestEqual(TEXT("then their second posts"), FortGarrisonMath::SpreadOverBuildings(Posts, 6, 0), TArray<int32>({ 0, 3, 4, 6, 1, 5 }));
			TestEqual(TEXT("every post at most"), FortGarrisonMath::SpreadOverBuildings(Posts, 20, 0).Num(), Posts.Num());
			TestEqual(TEXT("from another building"), FortGarrisonMath::SpreadOverBuildings(Posts, 2, 2), TArray<int32>({ 4, 6 }));
			TestEqual(TEXT("a start past the buildings turns round"), FortGarrisonMath::SpreadOverBuildings(Posts, 1, 5), TArray<int32>({ 3 }));
			TestEqual(TEXT("a negative start turns round"), FortGarrisonMath::SpreadOverBuildings(Posts, 1, -1), TArray<int32>({ 6 }));
			TestTrue(TEXT("none wished"), FortGarrisonMath::SpreadOverBuildings(Posts, 0, 0).IsEmpty());
			TestTrue(TEXT("no post"), FortGarrisonMath::SpreadOverBuildings(TArray<FName>(), 4, 0).IsEmpty());
			const TArray<int32> All = FortGarrisonMath::SpreadOverBuildings(Posts, 7, 1);
			TArray<int32> Sorted = All;
			Sorted.Sort();
			TestEqual(TEXT("each post once"), Sorted, TArray<int32>({ 0, 1, 2, 3, 4, 5, 6 }));
		});

		It("holds its building: answers the alarm only close to it, the patrol always", [this]()
		{
			TestTrue(TEXT("the patrol, far"), FortGarrisonMath::AnswersAlarm(false, 4000.0, 1500.0f));
			TestTrue(TEXT("a guard, the foe by its door"), FortGarrisonMath::AnswersAlarm(true, 900.0, 1500.0f));
			TestFalse(TEXT("a guard, the foe across the court"), FortGarrisonMath::AnswersAlarm(true, 2600.0, 1500.0f));
			TestTrue(TEXT("a guard without a radius: every alarm"), FortGarrisonMath::AnswersAlarm(true, 9000.0, 0.0f));
		});

		It("keeps a guard within its hold, and lets one pushed out walk back", [this]()
		{
			TestTrue(TEXT("inside"), FortGarrisonMath::KeepsToHold(300.0, 100.0, 600.0f));
			TestFalse(TEXT("a step out"), FortGarrisonMath::KeepsToHold(650.0, 590.0, 600.0f));
			TestTrue(TEXT("pushed out, a step back"), FortGarrisonMath::KeepsToHold(700.0, 750.0, 600.0f));
			TestFalse(TEXT("pushed out, a step farther"), FortGarrisonMath::KeepsToHold(800.0, 750.0, 600.0f));
			TestTrue(TEXT("no hold"), FortGarrisonMath::KeepsToHold(5000.0, 10.0, 0.0f));

			const FVector Post(1000.0, 0.0, 200.0);
			const FVector Far(2000.0, 0.0, 50.0);
			const FVector Held = FortGarrisonMath::ClampIntoDisc(Post, 600.0f, Far);
			TestTrue(TEXT("a goal outside brought to the edge, its height kept"), Held.Equals(FVector(1600.0, 0.0, 50.0), 0.01));
			TestEqual(TEXT("a goal inside stays"), FortGarrisonMath::ClampIntoDisc(Post, 600.0f, FVector(1200.0, 300.0, 0.0)), FVector(1200.0, 300.0, 0.0));
			TestEqual(TEXT("no radius: anywhere"), FortGarrisonMath::ClampIntoDisc(Post, 0.0f, Far), Far);
		});

		It("ships values that keep the fort reasonable (DA_FortSoldierRules defaults)", [this]()
		{
			const UFortSoldierRules* Rules = GetDefault<UFortSoldierRules>();
			TestEqual(TEXT("ten days of guards"), Rules->BuildingGuardsByDay.Num(), 10);
			for (const int32 Guards : Rules->BuildingGuardsByDay)
			{
				TestTrue(TEXT("a few guards a day"), Guards >= 0 && Guards <= 8);
			}
			// the court of day 10 holds 12 soldiers (DA_RunRules): room left for the guards, never past 20
			TestTrue(TEXT("most soldiers of a fort"), Rules->MaxSoldiersPerFort > 12 && Rules->MaxSoldiersPerFort <= 20);
			TestTrue(TEXT("a guard answers closer than the alarm of the fort carries"), Rules->BuildingAlarmRadiusCm < Rules->AlarmRadiusCm);
			TestTrue(TEXT("a hold of a few metres"), Rules->BuildingHoldRadiusCm >= 300.0f && Rules->BuildingHoldRadiusCm <= 1500.0f);
		});
	});
}

#endif
