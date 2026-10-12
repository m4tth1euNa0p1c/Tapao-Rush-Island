// monke_island

#pragma once

#include "CoreMinimal.h"

/** Boats domain log (T-182): seats taken and left, and every fix made by the seat guard */
MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeBoat, Log, All);

/**
 *  Pure rule of a player's seat in a boat (T-182), shared by UBoatRiderComponent and the specs (monke_island.Boats.Seat).
 *  Seated: the capsule hangs on the hull of its boat, without physics, movement off (MOVE_None).
 *  Seated nowhere: the capsule never hangs on a boat, and no seat keeps the movement off.
 *  Each machine compares what it sees with the replicated seat and takes the action that restores the rule, whatever
 *  the order in which the seat, the attachment and the movement corrections arrived. A client only undoes what it
 *  applied itself; the server also undoes what it finds without having applied it.
 */
namespace BoatSeat
{
	/** What one machine sees of a player character and the boats */
	struct FRiderState
	{
		/** The replicated seat names a valid boat and a seat index */
		bool bSeated = false;

		/** That boat knows its seats on this machine (it has begun play there) */
		bool bBoatReady = true;

		/** This machine sat the character (movement off, capsule without physics, attached) and has not given it all back */
		bool bApplied = false;

		/** The capsule hangs on the hull of the boat of its seat */
		bool bOnSeatHull = false;

		/** The capsule hangs on the hull of a boat, its own or another */
		bool bOnBoatHull = false;

		/** The character movement is off (MOVE_None) */
		bool bMovementOff = false;

		/** The ragdoll drives the body (tumble, knock-out, flung, dead): it owns the movement and the capsule collision */
		bool bRagdoll = false;

		/** This machine has the authority (server or standalone) */
		bool bAuthority = true;
	};

	/** What the rider does to restore the rule */
	enum class EAction : uint8
	{
		/** The character matches its seat */
		None,

		/** Seated in a boat that does not know its seats here yet: tried again at the next tick */
		Wait,

		/** Sit: movement off, capsule without physics, attached to the seat on the hull */
		Sit,

		/** Server: knocked out on its seat, the rider falls off it (its boat frees the seat, the rower's oars included) */
		LeaveSeat,

		/** Seated nowhere while the ragdoll drives the body: off the hull only, movement and collision once it stands up */
		Detach,

		/** Seated nowhere: off the hull, capsule collision back, movement back (falling) where it was off */
		Release
	};

	/** The action that makes what a machine sees match the seat */
	inline EAction Resolve(const FRiderState& State)
	{
		if (State.bSeated)
		{
			if (State.bRagdoll)
			{
				// knocked out on the seat: the server frees it, a client waits for the replicated seat
				return State.bAuthority ? EAction::LeaveSeat : EAction::None;
			}
			if (!State.bBoatReady)
			{
				return EAction::Wait;
			}
			// applied, on its own hull and still: as it should be; anything else is sat again
			return State.bApplied && State.bOnSeatHull && State.bMovementOff ? EAction::None : EAction::Sit;
		}

		// seated nowhere: on a client, a hull it hangs on without having sat here is the server's replicated attachment
		const bool bHanging = State.bOnBoatHull && (State.bAuthority || State.bApplied);
		if (State.bRagdoll)
		{
			// the ragdoll keeps the movement and the collision: only off the hull, the rest once it stands up
			return bHanging ? EAction::Detach : EAction::None;
		}
		// a movement switched off by something else (crab claw, flight crash) is left alone
		return State.bApplied || bHanging ? EAction::Release : EAction::None;
	}

	/** Where a rider standing up from its seat is put: LiftCm above where it sat (never lower), so its capsule clears the seat */
	inline FVector StandUpLocation(const FVector& Seated, float LiftCm)
	{
		return Seated + FVector(0.0, 0.0, FMath::Max(0.0f, LiftCm));
	}
}
