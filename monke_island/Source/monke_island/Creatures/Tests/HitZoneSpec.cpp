// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/HitZoneMath.h"
#include "Fort/FortMoat.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Creatures.HitZone
BEGIN_DEFINE_SPEC(FHitZoneSpec, "monke_island.Creatures.HitZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FHitZoneSpec)

void FHitZoneSpec::Define()
{
	It("finds the zone of the bones of the tiki and bean skeletons", [this]()
	{
		TestEqual(TEXT("head"), HitZones::ZoneOfBone(TEXT("head")), static_cast<uint8>(HitZones::Head));
		TestEqual(TEXT("crest"), HitZones::ZoneOfBone(TEXT("crest")), static_cast<uint8>(HitZones::Head));
		TestEqual(TEXT("hat"), HitZones::ZoneOfBone(TEXT("hat")), static_cast<uint8>(HitZones::Head));
		TestEqual(TEXT("spine"), HitZones::ZoneOfBone(TEXT("spine_02")), static_cast<uint8>(HitZones::Torso));
		TestEqual(TEXT("pelvis"), HitZones::ZoneOfBone(TEXT("pelvis")), static_cast<uint8>(HitZones::Torso));
		TestEqual(TEXT("left upper arm"), HitZones::ZoneOfBone(TEXT("upperarm_l")), static_cast<uint8>(HitZones::ArmL));
		TestEqual(TEXT("right hand"), HitZones::ZoneOfBone(TEXT("hand_r")), static_cast<uint8>(HitZones::ArmR));
		TestEqual(TEXT("spear in the right hand"), HitZones::ZoneOfBone(TEXT("weapon_r")), static_cast<uint8>(HitZones::ArmR));
		TestEqual(TEXT("left thigh"), HitZones::ZoneOfBone(TEXT("thigh_l")), static_cast<uint8>(HitZones::LegL));
		TestEqual(TEXT("right foot"), HitZones::ZoneOfBone(TEXT("foot_r")), static_cast<uint8>(HitZones::LegR));
		TestEqual(TEXT("coat tail"), HitZones::ZoneOfBone(TEXT("coattail_l")), static_cast<uint8>(HitZones::Torso));
		TestEqual(TEXT("no bone"), HitZones::ZoneOfBone(NAME_None), static_cast<uint8>(HitZones::None));
		TestEqual(TEXT("unknown bone"), HitZones::ZoneOfBone(TEXT("tail_tip")), static_cast<uint8>(HitZones::None));
	});

	It("drowns in the water of the moat only, not on the bridge, in the court or on the sand around the fort", [this]()
	{
		// surface at 115, drown 40 under it, the moat 800 wide from the walls
		TestTrue(TEXT("in the moat, deep"), FortMoatMath::IsInMoatWater(false, 300.0, 30.0, 115.0, 800.0f, 40.0f));
		TestFalse(TEXT("on the bridge"), FortMoatMath::IsInMoatWater(false, 300.0, 165.0, 115.0, 800.0f, 40.0f));
		TestFalse(TEXT("wading at the edge"), FortMoatMath::IsInMoatWater(false, 300.0, 90.0, 115.0, 800.0f, 40.0f));
		TestFalse(TEXT("inside the walls (the vault)"), FortMoatMath::IsInMoatWater(true, 0.0, 66.0, 115.0, 800.0f, 40.0f));
		TestFalse(TEXT("on the sand past the moat"), FortMoatMath::IsInMoatWater(false, 1200.0, 20.0, 115.0, 800.0f, 40.0f));
	});
}

#endif
