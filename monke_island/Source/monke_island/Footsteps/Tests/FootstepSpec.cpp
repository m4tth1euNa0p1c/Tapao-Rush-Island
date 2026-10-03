// monke_island

#include "Misc/AutomationTest.h"
#include "Footsteps/FootstepMath.h"
#include "Footsteps/FootstepRules.h"
#include "Hazards/CoconutFallRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Footsteps
BEGIN_DEFINE_SPEC(FFootstepSpec, "monke_island.Footsteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UFootstepRules* Rules = nullptr;

	EFootstepSurface Classify(std::initializer_list<FString> InNames)
	{
		const TArray<FString> Names(InNames);
		EFootstepSurface Surface = Rules->DefaultSurface;
		FootstepMath::SurfaceFromNames(Rules->Keywords, Names, Surface);
		return Surface;
	}
END_DEFINE_SPEC(FFootstepSpec)

void FFootstepSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UFootstepRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Surface", [this]()
	{
		It("hears planks on the boat and the village buildings", [this]()
		{
			TestEqual(TEXT("boat hull"), Classify({ TEXT("SM_Barque"), TEXT("Hull"), TEXT("BP_Barque_C_0") }), EFootstepSurface::Wood);
			TestEqual(TEXT("minka"), Classify({ TEXT("SM_Minka") }), EFootstepSurface::Wood);
		});

		It("hears stone, coral and litter by their names, case-insensitive", [this]()
		{
			TestEqual(TEXT("stone lantern"), Classify({ TEXT("SM_Village_StoneLantern") }), EFootstepSurface::Stone);
			TestEqual(TEXT("coral rubble"), Classify({ TEXT("sm_coral_rubble_02") }), EFootstepSurface::Coral);
			TestEqual(TEXT("frond"), Classify({ TEXT("SM_Palm_Frond_Dead") }), EFootstepSurface::Litter);
		});

		It("falls back to the default surface for an unknown floor", [this]()
		{
			EFootstepSurface Surface = EFootstepSurface::Water;
			const TArray<FString> Names = { TEXT("SM_Something") };
			TestFalse(TEXT("no keyword"), FootstepMath::SurfaceFromNames(Rules->Keywords, Names, Surface));
		});

		It("keeps the terrain out of the keywords (its surface comes from the height)", [this]()
		{
			EFootstepSurface Surface;
			const TArray<FString> Names = { TEXT("SM_Raraka_Terrain_1_2"), TEXT("StaticMeshComponent0"), TEXT("StaticMeshActor_12") };
			TestFalse(TEXT("terrain names match no keyword"), FootstepMath::SurfaceFromNames(Rules->Keywords, Names, Surface));
		});

		It("makes the terrain wet sand near the water and dry sand above", [this]()
		{
			TestEqual(TEXT("at the water"), FootstepMath::TerrainSurface(10.0f, Rules->WetSandBandCm), EFootstepSurface::SandWet);
			TestEqual(TEXT("on the beach crest"), FootstepMath::TerrainSurface(200.0f, Rules->WetSandBandCm), EFootstepSurface::SandDry);
		});
	});

	Describe("Cadence", [this]()
	{
		It("is silent standing still, faster sprinting than walking", [this]()
		{
			const auto Rate = [this](float Speed)
			{
				return FootstepMath::StepRate(Speed, Rules->MinSpeed, Rules->SlowStepRate, Rules->WalkCadence, Rules->SprintCadence);
			};
			TestEqual(TEXT("still"), Rate(0.0f), 0.0f);
			TestEqual(TEXT("walking"), Rate(Rules->WalkCadence.X), static_cast<float>(Rules->WalkCadence.Y), 0.001f);
			TestEqual(TEXT("sprinting"), Rate(Rules->SprintCadence.X), static_cast<float>(Rules->SprintCadence.Y), 0.001f);
			TestTrue(TEXT("a tiki walking slowly steps less often"), Rate(120.0f) < Rate(Rules->WalkCadence.X) && Rate(120.0f) > 0.0f);
		});

		It("plays a landing only after a real fall", [this]()
		{
			TestEqual(TEXT("a step down"), FootstepMath::LandingVolume(100.0f, Rules->LandingMinSpeed, Rules->LandingVolume), 0.0f);
			TestTrue(TEXT("a jump"), FootstepMath::LandingVolume(Rules->LandingMinSpeed * 1.5f, Rules->LandingMinSpeed, Rules->LandingVolume) > 0.0f);
		});
	});

	Describe("Coconut thud", [this]()
	{
		It("is silent for a roll, louder for a fall from the crown", [this]()
		{
			UCoconutFallRules* Coconut = NewObject<UCoconutFallRules>();
			TestEqual(TEXT("rolling"), Coconut->LandVolumeFor(50.0f), 0.0f);
			const float Bounce = Coconut->LandVolumeFor(300.0f);
			const float Fall = Coconut->LandVolumeFor(1600.0f);
			TestTrue(TEXT("a bounce is heard"), Bounce > 0.0f);
			TestTrue(TEXT("a fall is louder than a bounce"), Fall > Bounce);
			TestEqual(TEXT("a fall from the crown is the loudest"), Fall, static_cast<float>(Coconut->LandSoundVolumeRange.Y), 0.001f);
		});
	});
}

#endif
