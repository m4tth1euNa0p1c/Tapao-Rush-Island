// monke_island

#include "Misc/AutomationTest.h"
#include "Props/LootCrate/LootContainer.h"
#include "Props/LootCrate/LootCrateMath.h"
#include "Props/LootCrate/LootCrateRules.h"

#if WITH_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Items/NetRestComponent.h"
#include "Items/PhysicsPropActor.h"
#include "Props/LootCrate/LootCrateActor.h"
#include "TimerManager.h"

namespace LootCrateSpecHelpers
{
	/** A crate or an object in a plain tree, to count what lies aboard without a world */
	struct FNode
	{
		FString Name;
		TArray<FNode*> Inside;
	};

	/** Names of what AppendTree lists from Root */
	TArray<FString> List(FNode* Root, int32 MaxDepth)
	{
		TArray<FNode*> Out;
		LootContainers::AppendTree(Root, Out, [](FNode* Node, TArray<FNode*>& OutChildren) { OutChildren.Append(Node->Inside); }, MaxDepth);
		TArray<FString> Names;
		for (const FNode* Node : Out)
		{
			Names.Add(Node->Name);
		}
		return Names;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.LootCrate
BEGIN_DEFINE_SPEC(FLootCrateSpec, "monke_island.LootCrate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** World of the network tests, without a net driver: the dormancy the crate asks for is read on the actor */
	UWorld* World = nullptr;

	/** A crate in the air, its rules and meshes loaded as in the game */
	ALootCrateActor* SpawnCrate()
	{
		return World->SpawnActor<ALootCrateActor>(FVector(0.0, 0.0, 200.0), FRotator::ZeroRotator);
	}

	/** A 20 cm valuable prop that fits in the crate, its body simulating as on the ground */
	APhysicsPropActor* SpawnSmallProp(const FVector& Location)
	{
		APhysicsPropActor* Prop = World->SpawnActor<APhysicsPropActor>(Location, FRotator::ZeroRotator);
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		if (Prop && Cube)
		{
			Prop->GetMesh()->SetStaticMesh(Cube);
			Prop->GetMesh()->SetWorldScale3D(FVector(0.2));
			Prop->GetMesh()->SetSimulatePhysics(true);
		}
		return Prop;
	}

	/**
	 *  The crate's body falls asleep and its network rest takes its first look (one tick of the world's timers, the only one
	 *  of the test): the crate stops replicating, as it does at rest in the game
	 */
	bool PutToRest(ALootCrateActor* Crate)
	{
		Crate->GetBody()->PutAllRigidBodiesToSleep();
		// the component's looks are a timer set during this frame: the manager keeps it pending until its next tick, and
		// it ticks once a frame (FTimerManager::HasBeenTickedThisFrame): a tick to take it in, the next frame, then time
		FTimerManager& Timers = World->GetTimerManager();
		Timers.Tick(0.0f);
		++GFrameCounter;
		Timers.Tick(2.0f * GetDefault<UNetRestComponent>()->CheckInterval);
		return Crate->NetDormancy == DORM_DormantAll;
	}
END_DEFINE_SPEC(FLootCrateSpec)

void FLootCrateSpec::Define()
{
	Describe("Defaults", [this]()
	{
		It("hold 12 objects and 60 kg in an 8 kg crate (owner request 2026-10-09)", [this]()
		{
			const ULootCrateRules* Rules = GetDefault<ULootCrateRules>();
			TestEqual(TEXT("max items"), Rules->MaxItems, 12);
			TestEqual(TEXT("max content"), Rules->MaxContentKg, 60.0f);
			TestEqual(TEXT("empty mass"), Rules->EmptyMassKg, 8.0f);
			TestTrue(TEXT("the inside fits in the body"), LootCrateMath::FitsInside(Rules->InnerSizeCm, Rules->BodyHalfSizeCm * 2.0));
		});
	});

	Describe("Capacity", [this]()
	{
		It("takes objects until the twelfth", [this]()
		{
			TestTrue(TEXT("empty"), LootCrateMath::CanAccept(0, 0.0f, 1.0f, 12, 60.0f));
			TestTrue(TEXT("eleven inside"), LootCrateMath::CanAccept(11, 11.0f, 1.0f, 12, 60.0f));
			TestFalse(TEXT("twelve inside"), LootCrateMath::CanAccept(12, 12.0f, 1.0f, 12, 60.0f));
		});

		It("takes objects up to the mass limit, not beyond", [this]()
		{
			TestTrue(TEXT("exactly 60 kg"), LootCrateMath::CanAccept(3, 50.0f, 10.0f, 12, 60.0f));
			TestFalse(TEXT("60.5 kg"), LootCrateMath::CanAccept(3, 50.0f, 10.5f, 12, 60.0f));
			TestFalse(TEXT("one heavy object alone"), LootCrateMath::CanAccept(0, 0.0f, 75.0f, 12, 60.0f));
		});

		It("refuses a negative mass or count", [this]()
		{
			TestFalse(TEXT("negative mass"), LootCrateMath::CanAccept(0, 0.0f, -1.0f, 12, 60.0f));
			TestFalse(TEXT("negative count"), LootCrateMath::CanAccept(-1, 0.0f, 1.0f, 12, 60.0f));
		});
	});

	Describe("Mass", [this]()
	{
		It("weighs the crate with what it holds", [this]()
		{
			TestEqual(TEXT("empty"), LootCrateMath::TotalMassKg(8.0f, 0.0f), 8.0f);
			TestEqual(TEXT("full"), LootCrateMath::TotalMassKg(8.0f, 60.0f), 68.0f);
			TestEqual(TEXT("negative values ignored"), LootCrateMath::TotalMassKg(-2.0f, -5.0f), 0.0f);
		});
	});

	Describe("Size", [this]()
	{
		const FVector Inner(74.4, 39.4, 26.6);

		It("lets in what fits, turned any way", [this, Inner]()
		{
			TestTrue(TEXT("coil of rope 30 x 30 x 8"), LootCrateMath::FitsInside(FVector(30.0, 30.0, 8.0), Inner));
			TestTrue(TEXT("rolled mat standing up 10 x 30 x 70"), LootCrateMath::FitsInside(FVector(10.0, 30.0, 70.0), Inner));
			TestTrue(TEXT("exactly the inside"), LootCrateMath::FitsInside(Inner, Inner));
		});

		It("keeps out what is too long or too big", [this, Inner]()
		{
			TestFalse(TEXT("oar 180 cm"), LootCrateMath::FitsInside(FVector(180.0, 8.0, 8.0), Inner));
			TestFalse(TEXT("cube of 40 cm"), LootCrateMath::FitsInside(FVector(40.0, 40.0, 40.0), Inner));
		});
	});

	Describe("Empty", [this]()
	{
		It("puts a single object straight in front, at the distance", [this]()
		{
			const FVector2D Spot = LootCrateMath::EmptySpot(0, 1, 70.0f, 22.0f, 6, 30.0f);
			TestEqual(TEXT("ahead"), Spot.X, 70.0, 0.01);
			TestEqual(TEXT("centred"), Spot.Y, 0.0, 0.01);
		});

		It("spreads a row symmetrically at the same distance", [this]()
		{
			const FVector2D First = LootCrateMath::EmptySpot(0, 3, 70.0f, 22.0f, 6, 30.0f);
			const FVector2D Last = LootCrateMath::EmptySpot(2, 3, 70.0f, 22.0f, 6, 30.0f);
			TestEqual(TEXT("mirror"), First.Y, -Last.Y, 0.01);
			TestEqual(TEXT("same distance"), First.Size(), 70.0, 0.01);
			TestEqual(TEXT("same distance"), Last.Size(), 70.0, 0.01);
		});

		It("puts twelve objects on two rows, never on top of one another", [this]()
		{
			TArray<FVector2D> Spots;
			for (int32 Index = 0; Index < 12; ++Index)
			{
				Spots.Add(LootCrateMath::EmptySpot(Index, 12, 70.0f, 22.0f, 6, 30.0f));
			}
			TestEqual(TEXT("first row"), Spots[5].Size(), 70.0, 0.01);
			TestEqual(TEXT("second row further"), Spots[6].Size(), 100.0, 0.01);
			double Closest = TNumericLimits<double>::Max();
			for (int32 A = 0; A < Spots.Num(); ++A)
			{
				for (int32 B = A + 1; B < Spots.Num(); ++B)
				{
					Closest = FMath::Min(Closest, FVector2D::Distance(Spots[A], Spots[B]));
				}
			}
			TestTrue(FString::Printf(TEXT("closest two %.1f cm apart"), Closest), Closest >= 20.0);
		});
	});

	Describe("Lid", [this]()
	{
		It("opens, stays open, shuts, then is over", [this]()
		{
			TestEqual(TEXT("start"), LootCrateMath::LidOpenFraction(0.0f, 0.2f, 0.35f, 0.3f), 0.0f, 0.001f);
			TestEqual(TEXT("open"), LootCrateMath::LidOpenFraction(0.2f, 0.2f, 0.35f, 0.3f), 1.0f, 0.001f);
			TestEqual(TEXT("held"), LootCrateMath::LidOpenFraction(0.4f, 0.2f, 0.35f, 0.3f), 1.0f, 0.001f);
			const float Closing = LootCrateMath::LidOpenFraction(0.7f, 0.2f, 0.35f, 0.3f);
			TestTrue(TEXT("closing"), Closing > 0.0f && Closing < 1.0f);
			TestTrue(TEXT("over"), LootCrateMath::LidOpenFraction(0.86f, 0.2f, 0.35f, 0.3f) < 0.0f);
			TestTrue(TEXT("not started"), LootCrateMath::LidOpenFraction(-0.1f, 0.2f, 0.35f, 0.3f) < 0.0f);
		});
	});

	Describe("Contents", [this]()
	{
		using namespace LootCrateSpecHelpers;

		It("counts a crate with what it holds, a crate inside a crate too", [this]()
		{
			FNode Coin{ TEXT("Coin") }, Pearl{ TEXT("Pearl") }, Bowl{ TEXT("Bowl") }, Shell{ TEXT("Shell") };
			FNode Small{ TEXT("Small"), { &Shell } };
			FNode Crate{ TEXT("Crate"), { &Coin, &Pearl, &Small } };
			const TArray<FString> Names = List(&Crate, 4);
			TestEqual(TEXT("crate + 3 + 1 inside the small one"), Names.Num(), 5);
			TestTrue(TEXT("the shell of the small crate"), Names.Contains(TEXT("Shell")));
			TestEqual(TEXT("a loose object alone"), List(&Bowl, 4).Num(), 1);
		});

		It("never lists an object twice, even through a loop", [this]()
		{
			FNode A{ TEXT("A") }, B{ TEXT("B") };
			A.Inside.Add(&B);
			B.Inside.Add(&A);
			TestEqual(TEXT("A then B, once each"), List(&A, 8).Num(), 2);
		});

		It("stops at the depth limit", [this]()
		{
			FNode Deep{ TEXT("Deep") };
			FNode Middle{ TEXT("Middle"), { &Deep } };
			FNode Top{ TEXT("Top"), { &Middle } };
			TestEqual(TEXT("depth 1"), List(&Top, 1).Num(), 2);
			TestEqual(TEXT("depth 0"), List(&Top, 0).Num(), 1);
		});

		It("ignores a missing node", [this]()
		{
			TestEqual(TEXT("null"), List(nullptr, 4).Num(), 0);
		});
	});

	Describe("Network", [this]()
	{
		It("rests on the network like any free object, its body telling at once when it wakes (T-316)", [this]()
		{
			const ALootCrateActor* Crate = GetDefault<ALootCrateActor>();
			TestNotNull(TEXT("network rest: dormant at rest, glued to the galleon's deck or the barque's hull"), Crate->GetNetRest());
			TestTrue(TEXT("on the simulating body"), Crate->GetRootComponent() == Crate->GetBody());
			TestTrue(TEXT("wake events"), Crate->GetBody()->BodyInstance.bGenerateWakeEvents);
		});

		Describe("In a world", [this]()
		{
			BeforeEach([this]()
			{
				const UWorld::InitializationValues Init = UWorld::InitializationValues()
					.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(true)
					.CreateNavigation(false).CreateAISystem(false);
				World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
				GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
				World->InitializeActorsForPlay(FURL());
				World->GetWorldSettings()->NotifyBeginPlay();
			});

			AfterEach([this]()
			{
				World->EndPlay(EEndPlayReason::Quit);
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
				World = nullptr;
			});

			It("wakes a sleeping crate when an object goes in, so every client sees it inside (T-316)", [this]()
			{
				ALootCrateActor* Crate = SpawnCrate();
				APhysicsPropActor* Coil = SpawnSmallProp(FVector(0.0, 300.0, 200.0));
				if (!TestNotNull(TEXT("crate"), Crate) || !TestTrue(TEXT("a prop that simulates"), Coil && Coil->GetMesh()->IsSimulatingPhysics()))
				{
					return;
				}
				if (!TestTrue(TEXT("at rest, the crate stops replicating"), PutToRest(Crate)))
				{
					return;
				}
				TestTrue(TEXT("stored"), Crate->StoreItem(nullptr, Coil));
				TestEqual(TEXT("one object inside"), Crate->GetItemCount(), 1);
				TestTrue(TEXT("awake again: its contents and its lid reach the clients"), Crate->NetDormancy == DORM_Awake);
				TestTrue(TEXT("the object hidden inside"), Coil->IsHidden() && Coil->GetAttachParentActor() == Crate);
			});

			It("wakes a sleeping crate when it is emptied, and what comes out (T-316)", [this]()
			{
				ALootCrateActor* Crate = SpawnCrate();
				APhysicsPropActor* Coil = SpawnSmallProp(FVector(0.0, 300.0, 200.0));
				if (!TestNotNull(TEXT("crate"), Crate) || !TestTrue(TEXT("stored while awake"), Coil && Crate->StoreItem(nullptr, Coil)))
				{
					return;
				}
				if (!TestTrue(TEXT("at rest, the crate stops replicating"), PutToRest(Crate)))
				{
					return;
				}
				TestEqual(TEXT("one object out"), Crate->EmptyContents(FVector(300.0, 0.0, 200.0)), 1);
				TestTrue(TEXT("awake again: the clients see it emptied"), Crate->NetDormancy == DORM_Awake);
				TestTrue(TEXT("the object free and shown"), !Coil->IsHidden() && !Coil->GetAttachParentActor() && Coil->GetMesh()->IsSimulatingPhysics());
				TestTrue(TEXT("the object replicates"), Coil->NetDormancy <= DORM_Awake);
			});
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
