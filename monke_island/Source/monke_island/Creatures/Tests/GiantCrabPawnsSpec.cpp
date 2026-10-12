// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/GiantCrabMath.h"
#include "Creatures/GiantCrabRules.h"

#if WITH_DEV_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Creatures.GiantCrab.Pawns (T-328, L5: no character pinned under the
// giant crab; the upright pillars that stop them, the time a character is pinned, where it is freed to)
BEGIN_DEFINE_SPEC(FGiantCrabPawnsSpec, "monke_island.Creatures.GiantCrab.Pawns", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	/** A bean walks between two legs (AIslanderCharacter's capsule, 34 cm) */
	static constexpr float BeanWidthCm = 68.0f;
	/** Above the tallest character standing or jumping (a tiki 1.35 m) */
	static constexpr float TallestCm = 200.0f;

	const UGiantCrabRules* Rules = nullptr;

	const FGiantCrabPawnPillar* FindPillar(const TCHAR* Name) const
	{
		return Rules->PawnPillars.FindByPredicate([Name](const FGiantCrabPawnPillar& Pillar) { return Pillar.Name == FName(Name); });
	}

	const FGiantCrabHitBody* FindBody(const FString& Bone) const
	{
		return Rules->HitBodies.FindByPredicate([&Bone](const FGiantCrabHitBody& Body) { return Body.Bone == FName(*Bone); });
	}

	/** Room left between two pillars (cm) */
	double Gap(const TCHAR* A, const TCHAR* B) const
	{
		const FGiantCrabPawnPillar* First = FindPillar(A);
		const FGiantCrabPawnPillar* Second = FindPillar(B);
		return First && Second ? FVector2D::Distance(First->CenterCm, Second->CenterCm) - First->RadiusCm - Second->RadiusCm : -1.0;
	}

	/** True when Point lies in one of the pillars whose name starts with Prefix */
	bool IsCovered(const FVector2D& Point, const FString& Prefix) const
	{
		for (const FGiantCrabPawnPillar& Pillar : Rules->PawnPillars)
		{
			if (Pillar.Name.ToString().StartsWith(Prefix) && FVector2D::Distance(Point, Pillar.CenterCm) <= Pillar.RadiusCm + 0.01)
			{
				return true;
			}
		}
		return false;
	}
END_DEFINE_SPEC(FGiantCrabPawnsSpec)

void FGiantCrabPawnsSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = GetDefault<UGiantCrabRules>();
	});

	Describe("Pillars", [this]()
	{
		It("stand upright on the root where the limbs stand at rest, from under the sand to above everyone", [this]()
		{
			// the kit faces +Y, the crab +X; the mesh 100 cm under the capsule's centre
			const FTransform MeshInRoot(FRotator(0.0, -90.0, 0.0), FVector(0.0, 0.0, -100.0));
			float HalfHeight = 0.0f;
			const FVector Foot = GiantCrabMath::PillarInRoot(FVector2D(230.0, 186.6), -150.0f, 260.0f, MeshInRoot, HalfHeight);
			TestEqual(TEXT("the front left foot: ahead of the capsule, on its left"), Foot, FVector(186.6, -230.0, -45.0), 0.01f);
			TestEqual(TEXT("half of 4.1 m"), HalfHeight, 205.0f, 0.01f);
			float Swapped = 0.0f;
			TestEqual(TEXT("bottom and top in any order"), GiantCrabMath::PillarInRoot(FVector2D(230.0, 186.6), 260.0f, -150.0f, MeshInRoot, Swapped), Foot, 0.01f);
			TestEqual(TEXT("same height"), Swapped, HalfHeight, 0.01f);
		});

		It("reach from under the sand to above the tallest character, so that they only ever push aside", [this]()
		{
			TestFalse(TEXT("pillars"), Rules->PawnPillars.IsEmpty());
			for (const FGiantCrabPawnPillar& Pillar : Rules->PawnPillars)
			{
				// the straight part of the capsule spans the whole height of a character standing or jumping
				TestTrue(*FString::Printf(TEXT("%s under the sand"), *Pillar.Name.ToString()), Rules->PawnPillarBottomCm + Pillar.RadiusCm < 0.0f);
				TestTrue(*FString::Printf(TEXT("%s above everyone"), *Pillar.Name.ToString()), Rules->PawnPillarTopCm - Pillar.RadiusCm >= TallestCm);
				TestTrue(*FString::Printf(TEXT("%s has a size"), *Pillar.Name.ToString()), Pillar.RadiusCm >= 5.0f);
			}
			TestTrue(TEXT("the crab still stops the characters"), Rules->bHitBodiesBlockPawns);
		});

		It("stand at the eight leg tips", [this]()
		{
			for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
			{
				for (int32 Leg = 1; Leg <= 4; ++Leg)
				{
					const FString Name = FString::Printf(TEXT("leg_%s%d"), Side, Leg);
					const FGiantCrabPawnPillar* Pillar = FindPillar(*Name);
					const FGiantCrabHitBody* Dactyl = FindBody(Name + TEXT("_dactyl"));
					if (TestNotNull(*FString::Printf(TEXT("pillar %s"), *Name), Pillar) && TestNotNull(*FString::Printf(TEXT("dactyl of %s"), *Name), Dactyl))
					{
						TestEqual(*FString::Printf(TEXT("%s on its foot"), *Name), FVector(Pillar->CenterCm, 0.0), FVector(Dactyl->CenterCm.X, Dactyl->CenterCm.Y, 0.0), 0.01f);
						TestTrue(*FString::Printf(TEXT("%s wider than the dactyl"), *Name), Pillar->RadiusCm > Dactyl->RadiusCm);
					}
				}
			}
		});

		It("cover both claw hands, corners included", [this]()
		{
			for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
			{
				const FGiantCrabHitBody* Hand = FindBody(FString::Printf(TEXT("claw_%s_hand"), Side));
				if (!TestNotNull(TEXT("hand body"), Hand))
				{
					continue;
				}
				const FString Prefix = FString::Printf(TEXT("claw_%s_hand"), Side);
				const FVector2D Center(Hand->CenterCm.X, Hand->CenterCm.Y);
				const FVector2D Half(Hand->BoxSizeCm.X * 0.5, Hand->BoxSizeCm.Y * 0.5);
				for (const FVector2D& Corner : { FVector2D(Half.X, Half.Y), FVector2D(-Half.X, Half.Y), FVector2D(Half.X, -Half.Y), FVector2D(-Half.X, -Half.Y),
					FVector2D(Half.X, 0.0), FVector2D(-Half.X, 0.0) })
				{
					TestTrue(*FString::Printf(TEXT("%s point %s"), *Prefix, *Corner.ToString()), IsCovered(Center + Corner, Prefix));
				}
				TestNotNull(TEXT("its pincer"), FindPillar(*FString::Printf(TEXT("claw_%s_finger"), Side)));
			}
		});

		It("leave the ways under the belly open for a bean: between the legs, behind, beside the claws", [this]()
		{
			for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
			{
				const auto Leg = [Side](int32 Index) { return FString::Printf(TEXT("leg_%s%d"), Side, Index); };
				TestTrue(*FString::Printf(TEXT("%s 1-2"), Side), Gap(*Leg(1), *Leg(2)) >= BeanWidthCm);
				TestTrue(*FString::Printf(TEXT("%s 2-3"), Side), Gap(*Leg(2), *Leg(3)) >= BeanWidthCm);
				TestTrue(*FString::Printf(TEXT("%s 3-4"), Side), Gap(*Leg(3), *Leg(4)) >= BeanWidthCm);
				TestTrue(*FString::Printf(TEXT("%s claw - leg 1"), Side), Gap(*FString::Printf(TEXT("claw_%s_hand_back"), Side), *Leg(1)) >= BeanWidthCm);
			}
			TestTrue(TEXT("behind"), Gap(TEXT("leg_l4"), TEXT("leg_r4")) >= BeanWidthCm);
		});
	});

	Describe("Pinned characters", [this]()
	{
		It("count the time a character is pressed in, back to 0 as soon as it is out; a touch is no pin", [this]()
		{
			TestEqual(TEXT("pressed 5 cm in"), GiantCrabMath::PinnedTime(0.0f, 5.0f, 3.0f, 0.25f), 0.25f, 0.001f);
			TestEqual(TEXT("still pressed"), GiantCrabMath::PinnedTime(0.25f, 5.0f, 3.0f, 0.25f), 0.5f, 0.001f);
			TestEqual(TEXT("a touch"), GiantCrabMath::PinnedTime(0.5f, 2.0f, 3.0f, 0.25f), 0.0f);
			TestEqual(TEXT("out"), GiantCrabMath::PinnedTime(0.5f, 0.0f, 3.0f, 0.25f), 0.0f);
			TestEqual(TEXT("no time back"), GiantCrabMath::PinnedTime(0.5f, 5.0f, 3.0f, -1.0f), 0.5f, 0.001f);
			TestEqual(TEXT("no depth, no pin, even without a threshold"), GiantCrabMath::PinnedTime(0.0f, 0.0f, 0.0f, 0.25f), 0.0f);
		});

		It("are freed within about a second by default, never for a touch, out of the footprint of the pillars", [this]()
		{
			const FGiantCrabPawnRules& Pawns = Rules->Pawns;
			TestTrue(TEXT("their moves ignore the animated bodies"), Pawns.bIgnoreAnimatedBodies);
			TestTrue(TEXT("the server frees them"), Pawns.bFreePinned);
			TestTrue(TEXT("a touch is no pin"), Pawns.PinDepthCm >= 1.0f);
			TestTrue(TEXT("freed within 1.5 s"), Pawns.PinnedSeconds <= 1.5f);
			TestTrue(TEXT("looked at often enough"), Pawns.CheckSeconds <= Pawns.PinnedSeconds * 0.5f);
			// the pin of the journal (1.5 s at 13 cm, the bean of 18:34:30) is freed after the default time
			float Time = 0.0f;
			int32 Looks = 0;
			while (Time < Pawns.PinnedSeconds && Looks < 100)
			{
				Time = GiantCrabMath::PinnedTime(Time, 13.0f, Pawns.PinDepthCm, Pawns.CheckSeconds);
				++Looks;
			}
			TestTrue(TEXT("freed at a look"), Time >= Pawns.PinnedSeconds && Looks <= FMath::CeilToInt(Pawns.PinnedSeconds / Pawns.CheckSeconds) + 1);
			double Footprint = 0.0;
			for (const FGiantCrabPawnPillar& Pillar : Rules->PawnPillars)
			{
				Footprint = FMath::Max(Footprint, Pillar.CenterCm.Size() + Pillar.RadiusCm);
			}
			TestTrue(TEXT("freed out of the footprint"), Pawns.ClearRadiusCm >= Footprint);
			TestTrue(TEXT("the characters looked at reach past it"), Pawns.CheckRadiusCm >= Pawns.ClearRadiusCm + 300.0f);
		});
	});

	Describe("Free spots", [this]()
	{
		It("go straight away from the crab, out of its footprint, never toward it", [this]()
		{
			const FVector2D Spot = GiantCrabMath::FreeSpot(FVector2D(1000.0, 0.0), FVector2D(1100.0, 0.0), FVector2D(-1.0, 0.0), 384.0f, 0.0f);
			TestEqual(TEXT("straight away"), FVector(Spot, 0.0), FVector(1384.0, 0.0, 0.0), 0.01f);
			TestEqual(TEXT("out of the footprint"), GiantCrabMath::FreeSpotDistance(100.0f, 384.0f, 10.0f), 384.0f, 0.001f);
			TestEqual(TEXT("already farther: a little farther still"), GiantCrabMath::FreeSpotDistance(500.0f, 384.0f, 10.0f), 510.0f, 0.001f);
		});

		It("turn each side in turn all the way round, each angle once", [this]()
		{
			TestEqual(TEXT("30 deg: 12 spots"), GiantCrabMath::FreeSpotTries(30.0f), 12);
			const float Expected[] = { 0.0f, 30.0f, -30.0f, 60.0f, -60.0f, 90.0f, -90.0f, 120.0f, -120.0f, 150.0f, -150.0f, 180.0f };
			for (int32 Try = 0; Try < static_cast<int32>(UE_ARRAY_COUNT(Expected)); ++Try)
			{
				TestEqual(*FString::Printf(TEXT("try %d"), Try), GiantCrabMath::FreeSpotAngle(Try, 30.0f), Expected[Try], 0.001f);
			}
			TestEqual(TEXT("45 deg: 8 spots"), GiantCrabMath::FreeSpotTries(45.0f), 8);
			TestEqual(TEXT("50 deg: 7 spots, the last one 150 deg left"), GiantCrabMath::FreeSpotTries(50.0f), 7);
			TestEqual(TEXT("the last of 50 deg"), GiantCrabMath::FreeSpotAngle(6, 50.0f), -150.0f, 0.001f);
			TestEqual(TEXT("half turns: straight and behind"), GiantCrabMath::FreeSpotTries(180.0f), 2);
			// UE: +90 deg turns from +X to +Y, to the right seen from above
			const FVector2D Right = GiantCrabMath::FreeSpot(FVector2D::ZeroVector, FVector2D(100.0, 0.0), FVector2D(-1.0, 0.0), 384.0f, 90.0f);
			TestEqual(TEXT("turned a quarter to the right"), FVector(Right, 0.0), FVector(0.0, 384.0, 0.0), 0.01f);
		});

		It("take the crab's back when the character stands on its centre", [this]()
		{
			const FVector2D Behind = GiantCrabMath::FreeSpot(FVector2D::ZeroVector, FVector2D(0.2, 0.1), FVector2D(-1.0, 0.0), 384.0f, 0.0f);
			TestEqual(TEXT("its back"), FVector(Behind, 0.0), FVector(-384.0, 0.0, 0.0), 0.01f);
			const FVector2D Anywhere = GiantCrabMath::FreeSpot(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, 384.0f, 0.0f);
			TestEqual(TEXT("no back either: +X"), FVector(Anywhere, 0.0), FVector(384.0, 0.0, 0.0), 0.01f);
		});

		It("slide a character out of a pillar, never along a body that pressed it down", [this]()
		{
			TestTrue(TEXT("a pillar pushes level"), GiantCrabMath::IsLevelPush(FVector(0.0, 1.0, 0.0), 0.7f));
			TestFalse(TEXT("the claw over the bean of 18:34:30"), GiantCrabMath::IsLevelPush(FVector(0.09, 0.20, -0.98).GetSafeNormal(), 0.7f));
			TestFalse(TEXT("no way out"), GiantCrabMath::IsLevelPush(FVector::ZeroVector, 0.7f));
		});
	});
}

#endif
