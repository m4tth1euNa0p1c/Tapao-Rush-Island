// monke_island

#include "Misc/AutomationTest.h"
#include "Character/Bean/BeanAnimLibrary.h"
#include "Character/Bean/BeanSwimAnim.h"
#include "Player/IslanderMovementComponent.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Bean.Swim
BEGIN_DEFINE_SPEC(FBeanSwimSpec, "monke_island.Bean.Swim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UBeanAnimLibrary* Library = nullptr;

	/** Water pose with the defaults of DA_BeanAnimLibrary */
	bool Pose(bool bSwimming, float Depth, bool bWas, float VerticalSpeed = 0.0f, bool bFlying = false) const
	{
		BeanSwimAnim::FWaterInput In;
		In.bSwimming = bSwimming;
		In.bFlying = bFlying;
		In.DepthAtFeet = Depth;
		In.VerticalSpeed = VerticalSpeed;
		return BeanSwimAnim::IsInWaterPose(In, bWas, Library->SwimPoseMinDepth, Library->SwimPoseExitHysteresis,
			Library->SwimPoseMaxRiseSpeed);
	}
END_DEFINE_SPEC(FBeanSwimSpec)

void FBeanSwimSpec::Define()
{
	BeforeEach([this]()
	{
		Library = NewObject<UBeanAnimLibrary>();
		Library->AddToRoot();
	});

	AfterEach([this]()
	{
		Library->RemoveFromRoot();
		Library = nullptr;
	});

	Describe("WaterPose", [this]()
	{
		It("plays the water clips whenever the movement swims", [this]()
		{
			TestTrue(TEXT("swimming at the surface"), Pose(true, 120.0f, false));
			TestTrue(TEXT("diving 4 m deep"), Pose(true, 400.0f, true, -100.0f));
			TestTrue(TEXT("swimming in a shallow spot"), Pose(true, 40.0f, false));
		});

		It("keeps the pose while the short bean capsule bobs between swimming and falling", [this]()
		{
			// 110 cm capsule, swim start depth 120 cm: the mode flips to falling above it and back below it
			bool bWas = false;
			int32 Frames = 0;
			for (int32 Frame = 0; Frame < 240; ++Frame)
			{
				const bool bSwimming = Frame % 6 != 5;
				const float Depth = 120.0f + 2.5f * FMath::Sin(Frame * 0.7f);
				bWas = Pose(bSwimming, Depth, bWas, 9.0f * FMath::Cos(Frame * 0.7f));
				Frames += bWas ? 1 : 0;
			}
			TestEqual(TEXT("water pose on every frame"), Frames, 240);
		});

		It("floats the bean in water deeper than SwimPoseMinDepth even on foot, and leaves it a hysteresis lower", [this]()
		{
			TestFalse(TEXT("wading in 70 cm"), Pose(false, 70.0f, false));
			TestTrue(TEXT("80 cm: afloat"), Pose(false, 80.0f, false));
			TestTrue(TEXT("back to 70 cm: still afloat"), Pose(false, 70.0f, true));
			TestFalse(TEXT("60 cm: on its feet again"), Pose(false, 60.0f, true));
		});

		It("leaves the pose for a leap out of the water and in flight", [this]()
		{
			TestFalse(TEXT("jump out of the water, 620 cm/s"), Pose(false, 110.0f, true, 620.0f));
			TestTrue(TEXT("climbing back to the surface, 120 cm/s"), Pose(true, 150.0f, true, 120.0f));
			TestFalse(TEXT("flying low over the sea"), Pose(false, 300.0f, true, 0.0f, true));
		});
	});

	Describe("Stroke", [this]()
	{
		It("treads water at rest and swims the breaststroke when it moves, cross faded in between", [this]()
		{
			const float Tread = Library->TreadMaxSpeed;
			const float Full = Library->SwimFullSpeed;
			TestEqual(TEXT("at rest"), BeanSwimAnim::StrokeWeight(0.0f, Tread, Full), 0.0f);
			TestEqual(TEXT("bobbing noise"), BeanSwimAnim::StrokeWeight(10.0f, Tread, Full), 0.0f);
			TestEqual(TEXT("half way"), BeanSwimAnim::StrokeWeight((Tread + Full) * 0.5f, Tread, Full), 0.5f, 0.001f);
			TestEqual(TEXT("full stroke"), BeanSwimAnim::StrokeWeight(Full, Tread, Full), 1.0f);
			TestEqual(TEXT("swim speed"), BeanSwimAnim::StrokeWeight(120.0f, Tread, Full), 1.0f);
			TestEqual(TEXT("same thresholds: a switch"), BeanSwimAnim::StrokeWeight(30.0f, 20.0f, 20.0f), 1.0f);
		});

		It("plays the stroke at the pace of the swimming speed, within limits", [this]()
		{
			const float Stroke = Library->SwimStrokeSpeed;
			const float Min = Library->SwimMinPlayRate;
			const float Max = Library->SwimMaxPlayRate;
			TestEqual(TEXT("swim speed: own pace"), BeanSwimAnim::StrokePlayRate(120.0f, Stroke, Min, Max), 1.0f, 0.001f);
			TestEqual(TEXT("sprint x1.5"), BeanSwimAnim::StrokePlayRate(180.0f, Stroke, Min, Max), 1.5f, 0.001f);
			TestEqual(TEXT("exhausted x0.6: slowest"), BeanSwimAnim::StrokePlayRate(72.0f, Stroke, Min, Max), Min, 0.001f);
			TestEqual(TEXT("fastest"), BeanSwimAnim::StrokePlayRate(500.0f, Stroke, Min, Max), Max, 0.001f);
			TestEqual(TEXT("limits upside down"), BeanSwimAnim::StrokePlayRate(500.0f, Stroke, 1.2f, 0.5f), 1.2f, 0.001f);
		});
	});

	Describe("Float", [this]()
	{
		It("floats the root SwimRootDepth under the surface, between the dry placement and SwimMaxLift", [this]()
		{
			const float Depth = Library->SwimRootDepth;
			const float Max = Library->SwimMaxLift;
			// sea at 0: the capsule swims with its feet 120 cm deep, the bean is lifted 60 cm (the former SwimLift)
			TestEqual(TEXT("swimming"), BeanSwimAnim::FloatLift(0.0f, -120.0f, Depth, Max), 60.0f, 0.01f);
			TestEqual(TEXT("wading 80 cm deep"), BeanSwimAnim::FloatLift(0.0f, -80.0f, Depth, Max), 20.0f, 0.01f);
			TestEqual(TEXT("40 cm deep: feet on the bottom"), BeanSwimAnim::FloatLift(0.0f, -40.0f, Depth, Max), 0.0f);
			TestEqual(TEXT("diving 3 m: follows the capsule"), BeanSwimAnim::FloatLift(0.0f, -300.0f, Depth, Max), Max);
			TestEqual(TEXT("a wave crest 15 cm higher"), BeanSwimAnim::FloatLift(15.0f, -120.0f, Depth, Max), 75.0f, 0.01f);
		});

		It("keeps the defaults coherent with the clips and the bean capsule", [this]()
		{
			const UIslanderMovementComponent* Move = GetDefault<UIslanderMovementComponent>();
			// the clips are authored for a 60 cm water line (bean_swim_clips.py WATER_LINE, bean_swim_clips.json)
			TestEqual(TEXT("root depth of the clips"), Library->SwimRootDepth, 60.0f);
			// the bean capsule swims at the movement's start depth: floating there needs this lift
			TestTrue(TEXT("lift reaches the swim depth"), Library->SwimMaxLift >= Move->SwimStartDepth - Library->SwimRootDepth);
			// the floating bean clears the bottom until it leaves the pose
			TestTrue(TEXT("pose leaves above the root depth"),
				Library->SwimPoseMinDepth - Library->SwimPoseExitHysteresis >= Library->SwimRootDepth);
			TestTrue(TEXT("tread below the stroke"), Library->TreadMaxSpeed < Library->SwimFullSpeed);
			TestTrue(TEXT("full stroke before the swim speed"), Library->SwimFullSpeed <= Move->MaxSwimSpeed);
			TestTrue(TEXT("own pace inside the rate limits"), Library->SwimMinPlayRate <= 1.0f && Library->SwimMaxPlayRate >= 1.0f);
			TestTrue(TEXT("a jump out of the water leaves the pose"), Move->WaterJumpZVelocity > Library->SwimPoseMaxRiseSpeed);
		});
	});
}

#endif
