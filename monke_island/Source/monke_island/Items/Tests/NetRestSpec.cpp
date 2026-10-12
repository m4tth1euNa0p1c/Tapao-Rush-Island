// monke_island

#include "Misc/AutomationTest.h"
#include "Items/NetRestMath.h"

#if WITH_AUTOMATION_TESTS

// T-312, T-294, T-295 (review R-04, R-05, R-28): a free object stops replicating at rest, and on a base that moves
// under it (the rolling deck of the galleon) it is glued there once still on it
// Run with: ./scripts/ue.ps1 test -Filter monke_island.Items.NetRest
BEGIN_DEFINE_SPEC(FNetRestSpec, "monke_island.Items.NetRest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FNetRestSpec)

void FNetRestSpec::Define()
{
	Describe("State", [this]()
	{
		It("sleeps when asleep, and stays awake when it moves, is held, is glued or someone else moves it", [this]()
		{
			using NetRestMath::Decide;
			TestTrue(TEXT("asleep on the ground"), Decide(true, false, false, false, true) == ENetRestState::Dormant);
			TestTrue(TEXT("rolling"), Decide(true, false, false, true, true) == ENetRestState::Awake);
			TestTrue(TEXT("held by a player"), Decide(true, true, false, true, true) == ENetRestState::Awake);
			// glued on the deck or in the barque: awake, its movement off (a dormant one would keep a base its client remade)
			TestTrue(TEXT("glued"), Decide(true, false, true, true, true) == ENetRestState::Awake);
			TestTrue(TEXT("held while glued"), Decide(true, true, true, true, true) == ENetRestState::Awake);
			// in a crate, carried by a villager, a corpse laid down: their own code moves it, its changes must go out
			TestTrue(TEXT("not simulating"), Decide(false, false, false, false, true) == ENetRestState::Awake);
			TestTrue(TEXT("dormancy off"), Decide(true, false, false, false, false) == ENetRestState::Awake);
		});

		It("replicates a held object more often, a glued one hardly ever", [this]()
		{
			TestEqual(TEXT("held"), NetRestMath::AwakeRate(true, false, 30.0f, 60.0f, 2.0f), 60.0f);
			TestEqual(TEXT("held while glued"), NetRestMath::AwakeRate(true, true, 30.0f, 60.0f, 2.0f), 60.0f);
			TestEqual(TEXT("glued"), NetRestMath::AwakeRate(false, true, 30.0f, 60.0f, 2.0f), 2.0f);
			TestEqual(TEXT("moving"), NetRestMath::AwakeRate(false, false, 30.0f, 60.0f, 2.0f), 30.0f);
		});
	});

	Describe("Base", [this]()
	{
		It("tells an object still on its base from one sliding or tipping on it", [this]()
		{
			const FTransform Before(FRotator(0.0f, 30.0f, 0.0f), FVector(120.0, -40.0, 15.0));
			const FTransform Crept(FRotator(0.0f, 30.5f, 0.0f), FVector(120.6, -40.4, 15.0));
			const FTransform Slid(FRotator(0.0f, 30.0f, 0.0f), FVector(125.0, -40.0, 15.0));
			const FTransform Tipped(FRotator(10.0f, 30.0f, 0.0f), FVector(120.0, -40.0, 15.0));
			TestTrue(TEXT("crept a little"), NetRestMath::IsStillOnBase(Before, Crept, 1.5f, 2.0f));
			TestFalse(TEXT("slid 5 cm"), NetRestMath::IsStillOnBase(Before, Slid, 1.5f, 2.0f));
			TestFalse(TEXT("tipped 10 degrees"), NetRestMath::IsStillOnBase(Before, Tipped, 1.5f, 2.0f));
		});

		It("counts the rest time while still, from zero again after a move", [this]()
		{
			TestEqual(TEXT("grows"), NetRestMath::AccumulateRest(0.5f, true, 0.25f), 0.75f, 0.0001f);
			TestEqual(TEXT("back to zero"), NetRestMath::AccumulateRest(0.5f, false, 0.25f), 0.0f, 0.0001f);
		});

		It("glues an object still long enough on a base that moves, never on a still one", [this]()
		{
			TestTrue(TEXT("rolling deck"), NetRestMath::ShouldGlue(0.75f, 0.75f, true));
			TestFalse(TEXT("not long enough"), NetRestMath::ShouldGlue(0.5f, 0.75f, true));
			TestFalse(TEXT("still base: it falls asleep instead"), NetRestMath::ShouldGlue(5.0f, 0.75f, false));
			TestFalse(TEXT("gluing off"), NetRestMath::ShouldGlue(5.0f, 0.0f, true));
		});

		It("sees the galleon's deck move between two looks, not a still floor", [this]()
		{
			const FTransform Deck(FRotator(0.0f, 0.0f, 0.0f), FVector(0.0, 0.0, 0.0));
			// a quarter second of a 1 degree roll over a few seconds, and of a 6 cm heave
			const FTransform Rolled(FRotator(0.0f, 0.0f, 0.2f), FVector(0.0, 0.0, 0.0));
			const FTransform Heaved(FRotator(0.0f, 0.0f, 0.0f), FVector(0.0, 0.0, 1.2));
			TestTrue(TEXT("rolled"), NetRestMath::HasBaseMoved(Deck, Rolled, 0.05f, 0.01f));
			TestTrue(TEXT("heaved"), NetRestMath::HasBaseMoved(Deck, Heaved, 0.05f, 0.01f));
			TestFalse(TEXT("still"), NetRestMath::HasBaseMoved(Deck, Deck, 0.05f, 0.01f));
		});
	});

	Describe("Follow", [this]()
	{
		It("gives a glued object the speed of its base, none without time", [this]()
		{
			TestTrue(TEXT("linear"), NetRestMath::FollowVelocity(FVector(0.0, 0.0, 0.0), FVector(1.0, 0.0, 0.5), 0.1f).Equals(FVector(10.0, 0.0, 5.0), 0.001));
			TestTrue(TEXT("no time"), NetRestMath::FollowVelocity(FVector::ZeroVector, FVector(1.0, 0.0, 0.0), 0.0f).IsZero());
			const FQuat From = FQuat::Identity;
			const FQuat To(FVector::UpVector, FMath::DegreesToRadians(9.0f));
			const FVector Spin = NetRestMath::FollowAngularVelocity(From, To, 0.1f);
			TestTrue(TEXT("about the vertical"), Spin.GetSafeNormal().Equals(FVector::UpVector, 0.001));
			TestEqual(TEXT("90 degrees per second"), static_cast<float>(FMath::RadiansToDegrees(Spin.Size())), 90.0f, 0.01f);
			TestTrue(TEXT("still"), NetRestMath::FollowAngularVelocity(To, To, 0.1f).IsNearlyZero());
		});
	});
}

#endif
