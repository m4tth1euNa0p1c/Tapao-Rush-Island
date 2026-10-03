// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki
BEGIN_DEFINE_SPEC(FTikiSpec, "monke_island.Tiki.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiSpec)

void FTikiSpec::Define()
{
	Describe("Locomotion", [this]()
	{
		It("idles when nearly still, walks at walking speed, runs above", [this]()
		{
			TestTrue(TEXT("idle"), TikiMath::ChooseLocomotion(5.0f, 120.0f) == ETikiLocomotion::Idle);
			TestTrue(TEXT("walk"), TikiMath::ChooseLocomotion(120.0f, 120.0f) == ETikiLocomotion::Walk);
			TestTrue(TEXT("run"), TikiMath::ChooseLocomotion(400.0f, 120.0f) == ETikiLocomotion::Run);
		});
	});

	Describe("Attack", [this]()
	{
		It("starts only in range and after the cooldown", [this]()
		{
			TestTrue(TEXT("in range, cooled down"), TikiMath::CanStartAttack(100.0f, 160.0f, 10.0, 8.0, 1.6f));
			TestFalse(TEXT("out of range"), TikiMath::CanStartAttack(200.0f, 160.0f, 10.0, 8.0, 1.6f));
			TestFalse(TEXT("cooldown running"), TikiMath::CanStartAttack(100.0f, 160.0f, 10.0, 9.0, 1.6f));
		});
	});

	Describe("Bushes", [this]()
	{
		It("recognises the shrub meshes of Raraka and ignores the palms and the grass", [this]()
		{
			const UTikiRules* Rules = GetDefault<UTikiRules>();
			TestTrue(TEXT("miki-miki"), TikiMath::IsBushMeshName(TEXT("SM_MikiMiki_B"), Rules->BushMeshKeywords));
			TestTrue(TEXT("pandanus"), TikiMath::IsBushMeshName(TEXT("SM_Pandanus_Young"), Rules->BushMeshKeywords));
			TestFalse(TEXT("palm"), TikiMath::IsBushMeshName(TEXT("SM_CocoPalm_A"), Rules->BushMeshKeywords));
			TestFalse(TEXT("grass"), TikiMath::IsBushMeshName(TEXT("SM_Grass_Tuft"), Rules->BushMeshKeywords));
		});

		It("picks the nearest bush within the radius, none beyond", [this]()
		{
			const TArray<FVector> Bushes = { FVector(1000, 0, 0), FVector(300, 0, 0), FVector(0, 2000, 0) };
			TestEqual(TEXT("nearest"), TikiMath::NearestIndex(Bushes, FVector::ZeroVector, 3000.0f), 1);
			TestEqual(TEXT("none within 200 cm"), TikiMath::NearestIndex(Bushes, FVector::ZeroVector, 200.0f), INDEX_NONE);
		});
	});

	Describe("Night spawn ring", [this]()
	{
		It("stays between the min and max distance", [this]()
		{
			for (int32 Step = 0; Step <= 10; ++Step)
			{
				const FVector Point = TikiMath::PointOnRing(FVector(5, 5, 7), Step * 0.6f, Step / 10.0f, 1500.0f, 3500.0f);
				const double Distance = FVector::Dist2D(Point, FVector(5, 5, 7));
				TestTrue(TEXT("on the ring"), Distance >= 1499.0 && Distance <= 3501.0);
				TestEqual(TEXT("Z kept"), Point.Z, 7.0);
			}
		});
	});
}

#endif
