// monke_island

#include "Misc/AutomationTest.h"
#include "Village/VillageHouseNav.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** A small house: door (0) - hall (1) - kitchen (2), hall - step (3) - room (4) - alcove (5); room 4 also opens on 6 */
	TArray<FVillageHouseNode> TestNodes()
	{
		const FVector Points[] = { FVector(95, 70, 0), FVector(85, 270, 0), FVector(105, 545, 0), FVector(225, 270, 45),
			FVector(460, 285, 45), FVector(630, 600, 45), FVector(600, 270, 45) };
		const TArray<int32> Links[] = { { 1 }, { 2, 3 }, {}, { 4 }, { 5, 6 }, {}, {} };
		TArray<FVillageHouseNode> Nodes;
		for (int32 Index = 0; Index < 7; ++Index)
		{
			FVillageHouseNode& Node = Nodes.AddDefaulted_GetRef();
			Node.Location = Points[Index];
			Node.Links = Links[Index];
		}
		Nodes[2].Activity = TEXT("work");
		Nodes[5].Activity = TEXT("pray");
		return Nodes;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Village.HouseNav
BEGIN_DEFINE_SPEC(FHouseNavSpec, "monke_island.Village.HouseNav",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FHouseNavSpec)

void FHouseNavSpec::Define()
{
	Describe("Waypoints", [this]()
	{
		It("finds the nearest node, a step of floor counting more than the same distance across", [this]()
		{
			const TArray<FVillageHouseNode> Nodes = TestNodes();
			TestEqual(TEXT("in the hall"), VillageHouseNav::NearestNode(Nodes, FVector(90, 300, 0)), 1);
			// 55 cm from the step node across, 45 cm above the hall node: the floor it stands on wins
			TestEqual(TEXT("on the raised floor"), VillageHouseNav::NearestNode(Nodes, FVector(170, 270, 45)), 3);
			TestEqual(TEXT("no node"), VillageHouseNav::NearestNode({}, FVector::ZeroVector), static_cast<int32>(INDEX_NONE));
		});

		It("walks the links both ways, by the shortest way", [this]()
		{
			const TArray<FVillageHouseNode> Nodes = TestNodes();
			const TArray<int32> In = VillageHouseNav::FindPath(Nodes, 0, 5);
			TestEqual(TEXT("door to alcove"), In, TArray<int32>({ 0, 1, 3, 4, 5 }));
			const TArray<int32> Out = VillageHouseNav::FindPath(Nodes, 6, 2);
			TestEqual(TEXT("back through the links listed the other way"), Out, TArray<int32>({ 6, 4, 3, 1, 2 }));
			TestEqual(TEXT("already there"), VillageHouseNav::FindPath(Nodes, 4, 4), TArray<int32>({ 4 }));
		});

		It("gives no way to a node it cannot reach or out of range", [this]()
		{
			TArray<FVillageHouseNode> Nodes = TestNodes();
			Nodes[3].Links.Reset();
			Nodes[1].Links = { 2 };
			TestTrue(TEXT("cut off"), VillageHouseNav::FindPath(Nodes, 0, 5).IsEmpty());
			TestTrue(TEXT("out of range"), VillageHouseNav::FindPath(Nodes, 0, 12).IsEmpty());
		});

		It("backs off to the reachable node farthest from the threat", [this]()
		{
			const TArray<FVillageHouseNode> Nodes = TestNodes();
			TestEqual(TEXT("threat at the door"), VillageHouseNav::FarthestNode(Nodes, 3, FVector(95, 0, 0)), 5);
			TestEqual(TEXT("routine points"), VillageHouseNav::ActivityNodes(Nodes), TArray<int32>({ 2, 5 }));
		});
	});

	Describe("Inside", [this]()
	{
		It("tells the inside of the house, grown by a margin", [this]()
		{
			const FBox Interior(FVector(0, -720, -50), FVector(900, 0, 400));
			TestTrue(TEXT("in a room"), VillageHouseNav::IsInside(Interior, FVector(450, -300, 45), 0.0f));
			TestFalse(TEXT("on the engawa"), VillageHouseNav::IsInside(Interior, FVector(450, 60, 45), 0.0f));
			TestTrue(TEXT("on the threshold, with the margin"), VillageHouseNav::IsInside(Interior, FVector(450, 30, 45), 40.0f));
			TestFalse(TEXT("no inside"), VillageHouseNav::IsInside(FBox(ForceInit), FVector::ZeroVector, 100.0f));
		});
	});
}

#endif
