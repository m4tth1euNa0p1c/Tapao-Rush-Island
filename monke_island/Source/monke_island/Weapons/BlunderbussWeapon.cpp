// monke_island

#include "Weapons/BlunderbussWeapon.h"
#include "Weapons/BlunderbussRules.h"
#include "Weapons/FirearmDamage.h"
#include "monke_island.h"

ABlunderbussWeapon::ABlunderbussWeapon()
{
}

const UBlunderbussRules* ABlunderbussWeapon::GetBlunderbussRules() const
{
	return Cast<UBlunderbussRules>(Rules);
}

float ABlunderbussWeapon::GetCloseBurstDistance() const
{
	const UBlunderbussRules* Blunderbuss = GetBlunderbussRules();
	return Blunderbuss ? Blunderbuss->CloseBurstDistanceCm : 0.0f;
}

void ABlunderbussWeapon::BeginPlay()
{
	if (Rules && !GetBlunderbussRules())
	{
		UE_LOG(Logmonke_island, Error, TEXT("Blunderbuss %s: %s is not a UBlunderbussRules, the weapon is disabled"), *GetName(), *Rules->GetName());
		Rules = nullptr;
	}
	Super::BeginPlay();
}

int32 ABlunderbussWeapon::GetProjectilesPerShot() const
{
	const UBlunderbussRules* Blunderbuss = GetBlunderbussRules();
	return Blunderbuss ? FMath::Max(Blunderbuss->PelletCount, 1) : 1;
}

void ABlunderbussWeapon::DescribeShot(FFirearmDamageEvent& Shot, float Distance)
{
	const UBlunderbussRules* Blunderbuss = GetBlunderbussRules();
	if (!Blunderbuss)
	{
		return;
	}
	// point blank: the pellets shred the body if they kill (T-167, as the shotgun)
	Shot.CloseBurstDistance = Blunderbuss->CloseBurstDistanceCm;
	if (Blunderbuss->CloseBurstDistanceCm > 0.0f && Distance <= Blunderbuss->CloseBurstDistanceCm)
	{
		Shot.ShredLimbs = SpreadStream.RandRange(Blunderbuss->CloseShredLimbsMin, FMath::Max(Blunderbuss->CloseShredLimbsMin, Blunderbuss->CloseShredLimbsMax));
	}
}
