// monke_island

#pragma once

#include "CoreMinimal.h"
#include "AmmoTypes.generated.h"

/** Kind of ammunition a firearm's pouch takes and a pickup gives (owner request 2026-10-09: ammunition models) */
UENUM(BlueprintType)
enum class EAmmoKind : uint8
{
	/** Takes nothing (a weapon without ammunition) */
	None,
	/** Musket balls and paper cartridges (APirateMusketWeapon) */
	MusketBall,
	/** Pistol charges, single and double pistols (APistolWeapon) */
	PistolCharge,
	/** Shotgun shells (AShotgunWeapon) */
	Shell,
	/** Blunderbuss charges of shot (ABlunderbussWeapon) */
	Shot,
	/** Rifle cartridges (BP_Rifle, APirateMusketWeapon with DA_RifleRules) */
	RifleCartridge,
	/** Grenades (AGrenadeWeapon) */
	Grenade,
	/** Rounds of the crank machine gun, fed by the belt (AMachineGunWeapon, T-273) */
	MachineGunBelt,
	/** Pistol cartridges of the submachine gun, fed by its drum (BP_Mitraillette, USubmachineGunRules, T-335) */
	SmgCartridge
};

/** A quantity of one kind of ammunition in a pickup (a crate holds several lots) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FAmmoLot
{
	GENERATED_BODY()

	/** What it is */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo")
	EAmmoKind Kind = EAmmoKind::MusketBall;

	/** How many rounds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1", ClampMax="999"))
	int32 Count = 1;
};

/** Pure rules of the ammunition pickups, tested without a world (monke_island.Weapons.Ammo) */
namespace AmmoMath
{
	/** What a pickup of one lot does for a carrier */
	enum class EPickupResult : uint8
	{
		/** No weapon of that kind: the lot is refused */
		NoWeapon,
		/** Weapons of that kind, all full: nothing taken */
		Full,
		/** Part of the lot went in, the rest stays in the pickup */
		PartlyTaken,
		/** The whole lot went in */
		Taken
	};

	/** Rounds a weapon can still take: its capacity (loaded included) less what it holds */
	inline int32 Room(int32 MaxRounds, int32 Loaded, int32 Reserve)
	{
		return FMath::Max(MaxRounds - FMath::Max(Loaded, 0) - FMath::Max(Reserve, 0), 0);
	}

	/** Shares Count rounds between weapons in order (the one in hand first): OutGiven, same order as Rooms; the total given */
	inline int32 Distribute(const TArray<int32>& Rooms, int32 Count, TArray<int32>& OutGiven)
	{
		OutGiven.Init(0, Rooms.Num());
		int32 Left = FMath::Max(Count, 0);
		for (int32 Index = 0; Index < Rooms.Num() && Left > 0; ++Index)
		{
			const int32 Given = FMath::Clamp(Rooms[Index], 0, Left);
			OutGiven[Index] = Given;
			Left -= Given;
		}
		return FMath::Max(Count, 0) - Left;
	}

	/** Outcome of a lot for Weapons weapons of its kind whose rooms add up to Room */
	inline EPickupResult Judge(int32 Weapons, int32 Room, int32 Count)
	{
		if (Weapons <= 0)
		{
			return EPickupResult::NoWeapon;
		}
		if (Room <= 0)
		{
			return EPickupResult::Full;
		}
		return Room >= Count ? EPickupResult::Taken : EPickupResult::PartlyTaken;
	}
}
