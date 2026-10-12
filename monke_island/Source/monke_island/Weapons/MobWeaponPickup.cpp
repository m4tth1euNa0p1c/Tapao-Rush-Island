// monke_island

#include "Weapons/MobWeaponPickup.h"
#include "Items/ValuableComponent.h"

AMobWeaponPickup::AMobWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	Valuable = CreateDefaultSubobject<UValuableComponent>(TEXT("Valuable"));
	// a weapon is a tool: thrown around it loses its value, it never vanishes
	Valuable->bBreakAtZero = false;
}
