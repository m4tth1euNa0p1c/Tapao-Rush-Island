// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Creatures/GiantCrabTypes.h"
#include "GameFramework/Character.h"

/** Constants and small helpers shared by GiantCrab.cpp (the hunter) and GiantCrabMoves.cpp (boss moves, damage); not
 *  gameplay values (those live in UGiantCrabRules), only the kit's measures and technical tolerances */
namespace GiantCrabInternal
{
	/** Kit cycles (crab-anims.json): 0.8 m per sideways cycle of 0.8 s, 0.6 m per forward cycle of 1.2 s */
	inline constexpr float KitSideSpeed = 100.0f;
	inline constexpr float KitForwardSpeed = 50.0f;

	/** Slowest play rate of the walk clips */
	inline constexpr float MinLocomotionRate = 0.3f;

	/** A prey this far above or below the crab is out of its claws (roof, boat, cliff) */
	inline constexpr float MaxReachHeight = 250.0f;

	/** The caught one hangs this far under the grab socket (the socket is at the bean's spine, kit notes) */
	inline constexpr float CaughtDrop = 45.0f;

	/** Looks under its shell this often (s) */
	inline constexpr float UnderShellInterval = 0.1f;

	/** A charge that has run this long and moves at less than this share of its speed has met a wall */
	inline constexpr float ChargeStallSeconds = 0.4f;
	inline constexpr float ChargeStallShare = 0.15f;

	/** A charge this close to its end stops (cm) */
	inline constexpr float ChargeArrivalCm = 60.0f;

	/** Freeing a pinned character (T-328): a push out of a shape slides it on the ground when this share of it is level;
	 *  a spot found for it stands it this high above the ground (cm) */
	inline constexpr float LevelPushShare = 0.7f;
	inline constexpr float FreeSpotLiftCm = 2.0f;

	/** Sockets and notifies of the kit */
	inline const FName GrabSocket(TEXT("SOCKET_ClawGrab_R"));
	inline const FName MouthSocket(TEXT("SOCKET_Mouth"));
	inline const FName ClawSnapNotify(TEXT("AN_Claw_Snap"));

	inline float PreyRadius(const AActor* Actor)
	{
		const ACharacter* Character = Cast<ACharacter>(Actor);
		return Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleRadius() : 30.0f;
	}

	inline float PreyHalfHeight(const ACharacter* Character)
	{
		return Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 55.0f;
	}

	/** States whose clip runs at the attack play rate of the phase */
	inline bool UsesAttackRate(EGiantCrabState State)
	{
		return State == EGiantCrabState::Attack || State == EGiantCrabState::Throw || State == EGiantCrabState::ChargeWindup
			|| State == EGiantCrabState::BellySlam;
	}
}
