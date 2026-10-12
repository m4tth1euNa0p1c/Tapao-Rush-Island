// monke_island

#include "Misc/AutomationTest.h"
#include "Boats/BoatRules.h"
#include "Boats/BoatSeat.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** A rider seated as it should be: sat on this machine, on the hull of its boat, movement off */
	BoatSeat::FRiderState BoatSeatSpecSeated()
	{
		BoatSeat::FRiderState State;
		State.bSeated = true;
		State.bBoatReady = true;
		State.bApplied = true;
		State.bOnSeatHull = true;
		State.bOnBoatHull = true;
		State.bMovementOff = true;
		return State;
	}

	/** The same rider the moment its seat is cleared (F, galleon deck, sinking), nothing given back yet */
	BoatSeat::FRiderState BoatSeatSpecCleared()
	{
		BoatSeat::FRiderState State = BoatSeatSpecSeated();
		State.bSeated = false;
		State.bOnSeatHull = false;
		return State;
	}

	/** Comparable value of an action */
	int32 BoatSeatSpecCode(BoatSeat::EAction Action)
	{
		return static_cast<int32>(Action);
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Boats.Seat
BEGIN_DEFINE_SPEC(FBoatSeatSpec, "monke_island.Boats.Seat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FBoatSeatSpec)

void FBoatSeatSpec::Define()
{
	Describe("Seated", [this]()
	{
		It("keeps a rider seated as it should be, on the server and on a client", [this]()
		{
			BoatSeat::FRiderState State = BoatSeatSpecSeated();
			TestEqual(TEXT("server"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::None));
			State.bAuthority = false;
			TestEqual(TEXT("client"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::None));
		});

		It("sits a rider whose seat is not applied yet, or who lost its hull or its movement lock", [this]()
		{
			BoatSeat::FRiderState NotYet = BoatSeatSpecSeated();
			NotYet.bApplied = false;
			NotYet.bOnSeatHull = false;
			NotYet.bOnBoatHull = false;
			NotYet.bMovementOff = false;
			TestEqual(TEXT("seat replicated, not applied"), BoatSeatSpecCode(BoatSeat::Resolve(NotYet)), BoatSeatSpecCode(BoatSeat::EAction::Sit));

			BoatSeat::FRiderState OffHull = BoatSeatSpecSeated();
			OffHull.bOnSeatHull = false;
			TestEqual(TEXT("off its hull"), BoatSeatSpecCode(BoatSeat::Resolve(OffHull)), BoatSeatSpecCode(BoatSeat::EAction::Sit));

			BoatSeat::FRiderState OtherHull = OffHull;
			OtherHull.bOnBoatHull = true;
			TestEqual(TEXT("on another boat"), BoatSeatSpecCode(BoatSeat::Resolve(OtherHull)), BoatSeatSpecCode(BoatSeat::EAction::Sit));

			// e.g. a possession restart puts the default movement back on a client
			BoatSeat::FRiderState Moving = BoatSeatSpecSeated();
			Moving.bMovementOff = false;
			Moving.bAuthority = false;
			TestEqual(TEXT("movement back on"), BoatSeatSpecCode(BoatSeat::Resolve(Moving)), BoatSeatSpecCode(BoatSeat::EAction::Sit));
		});

		It("waits for a boat that does not know its seats yet on this machine", [this]()
		{
			BoatSeat::FRiderState State = BoatSeatSpecSeated();
			State.bAuthority = false;
			State.bApplied = false;
			State.bBoatReady = false;
			TestEqual(TEXT("late boat on a client"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::Wait));
		});

		It("throws a knocked out rider off its seat on the server, a client waits for the replicated seat", [this]()
		{
			BoatSeat::FRiderState State = BoatSeatSpecSeated();
			State.bRagdoll = true;
			TestEqual(TEXT("server: off the seat"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::LeaveSeat));
			State.bAuthority = false;
			TestEqual(TEXT("client: waits"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::None));
		});
	});

	Describe("SeatedNowhere", [this]()
	{
		It("frees a rider whose seat was cleared: off the hull, movement and collision back", [this]()
		{
			BoatSeat::FRiderState State = BoatSeatSpecCleared();
			TestEqual(TEXT("server"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::Release));
			State.bAuthority = false;
			TestEqual(TEXT("client, which sat it"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::Release));
		});

		It("frees on the server a player left on a barque without a seat (T-182: MOVE_None, attached at the rowing post)", [this]()
		{
			BoatSeat::FRiderState State;
			State.bOnBoatHull = true;
			State.bMovementOff = true;
			TestEqual(TEXT("movement off"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::Release));
			State.bMovementOff = false;
			TestEqual(TEXT("carried along while walking"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::Release));
		});

		It("frees a player whose seat still holds its movement off, even off the hull", [this]()
		{
			BoatSeat::FRiderState State = BoatSeatSpecCleared();
			State.bOnBoatHull = false;
			TestEqual(TEXT("movement off by the seat"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::Release));
		});

		It("never undoes on a client a hull the server attached", [this]()
		{
			BoatSeat::FRiderState State;
			State.bAuthority = false;
			State.bOnBoatHull = true;
			State.bMovementOff = true;
			TestEqual(TEXT("replicated attachment"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::None));
		});

		It("leaves alone a movement switched off by something else (crab claw, flight crash)", [this]()
		{
			BoatSeat::FRiderState State;
			State.bMovementOff = true;
			TestEqual(TEXT("not the seat's"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::None));
		});

		It("lets the ragdoll keep the body: off the hull only, the rest once it stands up", [this]()
		{
			BoatSeat::FRiderState State = BoatSeatSpecCleared();
			State.bRagdoll = true;
			TestEqual(TEXT("knocked out on the hull: off it"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::Detach));
			State.bOnBoatHull = false;
			TestEqual(TEXT("off the hull, still knocked out: wait"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::None));
			State.bRagdoll = false;
			State.bMovementOff = false;
			TestEqual(TEXT("stood up: movement and collision back"), BoatSeatSpecCode(BoatSeat::Resolve(State)), BoatSeatSpecCode(BoatSeat::EAction::Release));
		});

		It("does nothing for a free player", [this]()
		{
			TestEqual(TEXT("free"), BoatSeatSpecCode(BoatSeat::Resolve(BoatSeat::FRiderState())), BoatSeatSpecCode(BoatSeat::EAction::None));
		});
	});

	Describe("GalleonStart", [this]()
	{
		It("BoatStart -> PlaceOnDeck -> MoorBoat -> F twice leaves the rower free, never carried by the barque", [this]()
		{
			// BoatStart: at the oars, seat applied
			TestEqual(TEXT("seated at the oars"), BoatSeatSpecCode(BoatSeat::Resolve(BoatSeatSpecSeated())), BoatSeatSpecCode(BoatSeat::EAction::None));
			// PlaceOnDeck (LeaveBoat): the boat clears the seat, the rider gives everything back
			TestEqual(TEXT("leaves the seat for the deck"), BoatSeatSpecCode(BoatSeat::Resolve(BoatSeatSpecCleared())), BoatSeatSpecCode(BoatSeat::EAction::Release));
			// released: nothing hangs on the barque any more, MoorBoat moves it alone
			const BoatSeat::FRiderState OnDeck{};
			TestEqual(TEXT("free on the deck"), BoatSeatSpecCode(BoatSeat::Resolve(OnDeck)), BoatSeatSpecCode(BoatSeat::EAction::None));
			// down the ladder, F: takes the oars again
			BoatSeat::FRiderState Taking;
			Taking.bSeated = true;
			TestEqual(TEXT("F sits"), BoatSeatSpecCode(BoatSeat::Resolve(Taking)), BoatSeatSpecCode(BoatSeat::EAction::Sit));
			// F again: stands up
			TestEqual(TEXT("F stands up"), BoatSeatSpecCode(BoatSeat::Resolve(BoatSeatSpecCleared())), BoatSeatSpecCode(BoatSeat::EAction::Release));
		});
	});

	Describe("StandUp", [this]()
	{
		It("stands a rider up StandUpLiftCm above where it sat (DA_BoatRules), never lower", [this]()
		{
			const UBoatRules* Rules = GetDefault<UBoatRules>();
			TestTrue(TEXT("a lift is set"), Rules->StandUpLiftCm > 0.0f);
			const FVector Seated(120.0, -40.0, 85.0);
			TestTrue(TEXT("lifted"), BoatSeat::StandUpLocation(Seated, Rules->StandUpLiftCm).Equals(Seated + FVector(0.0, 0.0, Rules->StandUpLiftCm)));
			TestTrue(TEXT("never lowered"), BoatSeat::StandUpLocation(Seated, -50.0f).Equals(Seated));
		});
	});
}

#endif
