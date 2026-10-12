// monke_island

#include "Misc/AutomationTest.h"
#include "Weapons/CutlassMath.h"
#include "Weapons/CutlassRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons
BEGIN_DEFINE_SPEC(FCutlassSpec, "monke_island.Weapons.Cutlass",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UCutlassRules* Rules = nullptr;
END_DEFINE_SPEC(FCutlassSpec)

void FCutlassSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UCutlassRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	It("costs 8 % of the stamina per blow, 11 % per parried hit, and refuses a blow below that", [this]()
	{
		TestEqual(TEXT("cost"), CutlassMath::StaminaCost(100.0f, Rules->StaminaCostFraction), 8.0f, 0.001f);
		TestEqual(TEXT("parry cost"), CutlassMath::StaminaCost(100.0f, Rules->ParryStaminaCostFraction), 11.0f, 0.001f);
		TestTrue(TEXT("8 left"), CutlassMath::HasStaminaFor(8.0f, 100.0f, Rules->StaminaCostFraction));
		TestFalse(TEXT("7 left"), CutlassMath::HasStaminaFor(7.0f, 100.0f, Rules->StaminaCostFraction));
		// a full bar gives 12 blows (9 before T-152)
		float Stamina = 100.0f;
		int32 Blows = 0;
		while (CutlassMath::HasStaminaFor(Stamina, 100.0f, Rules->StaminaCostFraction))
		{
			Stamina -= CutlassMath::StaminaCost(100.0f, Rules->StaminaCostFraction);
			++Blows;
		}
		TestEqual(TEXT("blows per bar"), Blows, 12);
	});

	It("buffers a click during a blow and plays it when the blow can be cancelled", [this]()
	{
		const float Buffer = Rules->InputBufferSeconds;
		TestEqual(TEXT("buffer"), Buffer, 0.25f, 0.001f);
		// window opens at 1.0: a click at 0.85 waits, then fires at the opening
		TestTrue(TEXT("waits"), CutlassMath::EvaluateBufferedInput(0.9, 0.85, Buffer, 1.0) == CutlassMath::EBufferedInput::Wait);
		TestTrue(TEXT("fires at the opening"), CutlassMath::EvaluateBufferedInput(1.0, 0.85, Buffer, 1.0) == CutlassMath::EBufferedInput::Fire);
		TestTrue(TEXT("fires after"), CutlassMath::EvaluateBufferedInput(1.02, 0.85, Buffer, 1.0) == CutlassMath::EBufferedInput::Fire);
		// a click 0.4 s before the opening is forgotten (no blow out of nowhere)
		TestTrue(TEXT("too early"), CutlassMath::EvaluateBufferedInput(0.7, 0.6, Buffer, 1.0) == CutlassMath::EBufferedInput::Expired);
	});

	It("opens the cancel windows after the active frames and shortens the recovery by 30 %", [this]()
	{
		for (const ECutlassMove Move : { ECutlassMove::SlashRight, ECutlassMove::SlashLeft, ECutlassMove::Thrust, ECutlassMove::Finisher })
		{
			const FCutlassAttack* Attack = Rules->GetAttack(Move);
			const CutlassMath::FMoveWindows W = CutlassMath::GetMoveWindows(*Rules, Move);
			TestTrue(TEXT("attack"), W.bAttack);
			TestTrue(TEXT("hit inside the active frames"), W.ActiveStart <= Attack->HitTime && Attack->HitTime <= W.ActiveEnd);
			TestTrue(TEXT("defensive cancel after the active frames"), W.DefensiveCancel >= W.ActiveEnd);
			TestTrue(TEXT("next blow after the defensive cancel"), W.AttackCancel >= W.DefensiveCancel);
			TestTrue(TEXT("next blow before the old end of the move"), W.AttackCancel < W.TrackLength);
			TestEqual(TEXT("recovery 30 % shorter"), W.Duration - W.ActiveEnd, 0.7f * (W.TrackLength - W.ActiveEnd), 0.001f);
			TestEqual(TEXT("track followed exactly before the end of the active frames"), W.TrackTime(W.HitTime), W.HitTime, 0.0001f);
			TestEqual(TEXT("track ends with the move"), W.TrackTime(W.Duration), W.TrackLength, 0.001f);
		}
		const CutlassMath::FMoveWindows Right = CutlassMath::GetMoveWindows(*Rules, ECutlassMove::SlashRight);
		TestEqual(TEXT("right cut: next blow at 0.40 s (0.54 before)"), Right.AttackCancel, 0.40f, 0.001f);
		TestEqual(TEXT("right cut: over at 0.474 s"), Right.Duration, 0.474f, 0.001f);
		// the draw cannot be cancelled
		const CutlassMath::FMoveWindows Draw = CutlassMath::GetMoveWindows(*Rules, ECutlassMove::Draw);
		TestFalse(TEXT("draw is no attack"), Draw.bAttack);
		TestEqual(TEXT("draw busy to its end"), Draw.AttackCancel, Rules->DrawSeconds, 0.001f);
		TestEqual(TEXT("draw stretched"), Draw.TrackTime(Rules->DrawSeconds), Draw.TrackLength, 0.001f);
	});

	It("aims softly at the nearest enemy in the 60 degree cone within 2.5 m", [this]()
	{
		TestEqual(TEXT("cone"), Rules->AimAssistConeDegrees, 60.0f);
		TestEqual(TEXT("range"), Rules->AimAssistRangeCm, 250.0f);
		const FVector Origin(0.0, 0.0, 0.0);
		const TArray<FVector> Candidates = {
			FVector(200.0, 0.0, 0.0),	// ahead, far
			FVector(120.0, 50.0, 0.0),	// 22.6 degrees right, nearer
			FVector(50.0, 100.0, 0.0),	// 63 degrees: out of the cone
			FVector(-80.0, 0.0, 0.0),	// behind
			FVector(300.0, 0.0, 0.0)	// out of range
		};
		const float Cone = Rules->AimAssistConeDegrees;
		const float Range = Rules->AimAssistRangeCm;
		TestEqual(TEXT("nearest in the cone"), CutlassMath::PickAimTarget(Origin, 0.0f, Candidates, Cone, Range), 1);
		TestEqual(TEXT("aiming left: nobody"), CutlassMath::PickAimTarget(Origin, -90.0f, Candidates, Cone, Range), INDEX_NONE);
		TestEqual(TEXT("aiming back"), CutlassMath::PickAimTarget(Origin, 180.0f, Candidates, Cone, Range), 3);
		TestEqual(TEXT("empty"), CutlassMath::PickAimTarget(Origin, 0.0f, TArray<FVector>(), Cone, Range), INDEX_NONE);
		// the turn is bounded and takes the short way
		TestEqual(TEXT("bounded"), CutlassMath::StepYawToward(0.0f, 30.0f, 6.0f), 6.0f, 0.001f);
		TestEqual(TEXT("reached"), CutlassMath::StepYawToward(28.0f, 30.0f, 6.0f), 30.0f, 0.001f);
		TestEqual(TEXT("short way across 180"), CutlassMath::StepYawToward(170.0f, -170.0f, 6.0f), 176.0f, 0.001f);
	});

	It("steps in on the cuts, not against an enemy already at the blade", [this]()
	{
		TestTrue(TEXT("right cut lunges"), Rules->SlashRight.LungeDistanceCm > 0.0f);
		TestTrue(TEXT("left cut lunges"), Rules->SlashLeft.LungeDistanceCm > 0.0f);
		TestEqual(TEXT("thrust does not"), Rules->Thrust.LungeDistanceCm, 0.0f);
		const float Block = Rules->LungeBlockDistanceCm;
		TestEqual(TEXT("nobody ahead"), CutlassMath::LungeDistance(30.0f, -1.0f, Block), 30.0f, 0.001f);
		TestEqual(TEXT("enemy far"), CutlassMath::LungeDistance(30.0f, Block + 100.0f, Block), 30.0f, 0.001f);
		TestEqual(TEXT("stops short of the enemy"), CutlassMath::LungeDistance(30.0f, Block + 10.0f, Block), 10.0f, 0.001f);
		TestEqual(TEXT("enemy in contact"), CutlassMath::LungeDistance(30.0f, Block - 20.0f, Block), 0.0f, 0.001f);
	});

	It("sweeps the blade along its track through the active frames", [this]()
	{
		// a bean whose mesh faces +X (mesh +Y forward, -X right), its feet 52 cm under the actor's centre
		const FTransform MeshToActor(FRotator(0.0f, -90.0f, 0.0f), FVector(0.0, 0.0, -52.0));
		const float Radius = Rules->BladeSweepRadiusCm;
		auto Closest = [this, &MeshToActor](ECutlassMove Move, const FVector& Point)
		{
			const FCutlassAttack* Attack = Rules->GetAttack(Move);
			return CutlassMath::MinDistanceToBladeSweep(Attack->Keys, Rules->GuardKey, Attack->ActiveStartTime, Attack->ActiveEndTime,
				Rules->BladeSubstepSeconds, Rules->BladeStartCm, Rules->BladeTipCm, MeshToActor, Point);
		};
		TestTrue(TEXT("right cut crosses the front right"), Closest(ECutlassMove::SlashRight, FVector(65.0, 25.0, 5.0)) < Radius);
		TestTrue(TEXT("left cut crosses the front"), Closest(ECutlassMove::SlashLeft, FVector(70.0, -5.0, 5.0)) < Radius);
		TestTrue(TEXT("thrust reaches forward"), Closest(ECutlassMove::Thrust, FVector(65.0, 25.0, 5.0)) < Radius);
		TestTrue(TEXT("finisher comes down in front"), Closest(ECutlassMove::Finisher, FVector(68.0, -8.0, 20.0)) < Radius);
		TestTrue(TEXT("nothing behind"), Closest(ECutlassMove::SlashRight, FVector(-60.0, 0.0, 0.0)) > 50.0f);
		TestTrue(TEXT("nothing far ahead"), Closest(ECutlassMove::Thrust, FVector(160.0, 0.0, 0.0)) > 50.0f);
		// the sweep is cut in steps of 1/60 s at most, whatever the server frame
		TestEqual(TEXT("one step"), CutlassMath::SubstepCount(0.2f, 0.2f, Rules->BladeSubstepSeconds), 1);
		TestEqual(TEXT("a slow frame"), CutlassMath::SubstepCount(0.17f, 0.30f, Rules->BladeSubstepSeconds), 8);
		// points spread from the guard to the tip
		TArray<FVector> Points;
		FCutlassKey Key;
		CutlassMath::BladePoints(Key, Rules->BladeStartCm, Rules->BladeTipCm, Rules->BladeSamples, FTransform::Identity, Points);
		if (TestEqual(TEXT("samples"), Points.Num(), 3))
		{
			TestEqual(TEXT("guard"), Points[0].X, static_cast<double>(Rules->BladeStartCm), 0.01);
			TestEqual(TEXT("tip"), Points.Last().X, static_cast<double>(Rules->BladeTipCm), 0.01);
		}
	});

	It("chains right cut, backhand cut and thrust, and restarts after the window", [this]()
	{
		const float Window = Rules->ComboWindowSeconds;
		TestEqual(TEXT("first"), CutlassMath::NextComboMove(ECutlassMove::None, 0.0, Window), ECutlassMove::SlashRight);
		TestEqual(TEXT("second"), CutlassMath::NextComboMove(ECutlassMove::SlashRight, 0.1, Window), ECutlassMove::SlashLeft);
		TestEqual(TEXT("third"), CutlassMath::NextComboMove(ECutlassMove::SlashLeft, 0.1, Window), ECutlassMove::Thrust);
		TestEqual(TEXT("again"), CutlassMath::NextComboMove(ECutlassMove::Thrust, 0.1, Window), ECutlassMove::SlashRight);
		TestEqual(TEXT("late"), CutlassMath::NextComboMove(ECutlassMove::SlashRight, Window + 0.1, Window), ECutlassMove::SlashRight);
	});

	It("uses the damage of the maquette and kills a Coco Warrior (80) with a combo", [this]()
	{
		TestEqual(TEXT("right"), Rules->SlashRight.Damage, 35.0f);
		TestEqual(TEXT("left"), Rules->SlashLeft.Damage, 35.0f);
		TestEqual(TEXT("thrust"), Rules->Thrust.Damage, 45.0f);
		TestEqual(TEXT("finisher"), Rules->Finisher.Damage, 70.0f);
		TestFalse(TEXT("the thrust does not cut in two"), Rules->Thrust.bCut);
		TestTrue(TEXT("two cuts and a thrust"), Rules->SlashRight.Damage + Rules->SlashLeft.Damage + Rules->Thrust.Damage >= 80.0f);
		TestTrue(TEXT("a cut and the finisher"), Rules->SlashRight.Damage + Rules->Finisher.Damage >= 80.0f);
	});

	It("dulls under half sharpness", [this]()
	{
		TestEqual(TEXT("sharp"), CutlassMath::BlowDamage(35.0f, 1.0f, Rules->DullThreshold, Rules->DullDamageMultiplier), 35.0f, 0.001f);
		TestEqual(TEXT("dull"), CutlassMath::BlowDamage(35.0f, 0.4f, Rules->DullThreshold, Rules->DullDamageMultiplier), 28.0f, 0.001f);
	});

	It("parries 80 % of a frontal hit while there is stamina", [this]()
	{
		TestEqual(TEXT("front"), CutlassMath::ParriedDamage(20.0f, true, 1.0f, Rules->ParryFrontDot, Rules->ParryReduction, true), 4.0f, 0.001f);
		TestEqual(TEXT("behind"), CutlassMath::ParriedDamage(20.0f, true, -1.0f, Rules->ParryFrontDot, Rules->ParryReduction, true), 20.0f, 0.001f);
		TestEqual(TEXT("exhausted"), CutlassMath::ParriedDamage(20.0f, true, 1.0f, Rules->ParryFrontDot, Rules->ParryReduction, false), 20.0f, 0.001f);
		TestEqual(TEXT("lowered"), CutlassMath::ParriedDamage(20.0f, false, 1.0f, Rules->ParryFrontDot, Rules->ParryReduction, true), 20.0f, 0.001f);
	});

	It("sweeps the arc in front within the reach only", [this]()
	{
		const FVector Forward(1.0, 0.0, 0.0);
		TestTrue(TEXT("in front"), CutlassMath::IsInSweep(Forward, FVector(60.0, 0.0, 0.0), 85.0f, 110.0f));
		TestTrue(TEXT("at 50 degrees"), CutlassMath::IsInSweep(Forward, FVector(50.0, 59.0, 0.0), 85.0f, 110.0f));
		TestFalse(TEXT("at 70 degrees"), CutlassMath::IsInSweep(Forward, FVector(25.0, 70.0, 0.0), 85.0f, 110.0f));
		TestFalse(TEXT("behind"), CutlassMath::IsInSweep(Forward, FVector(-60.0, 0.0, 0.0), 85.0f, 110.0f));
		TestFalse(TEXT("too far"), CutlassMath::IsInSweep(Forward, FVector(120.0, 0.0, 0.0), 85.0f, 110.0f));
		// T-152: the safety net of the blade sweep reaches 1 m
		TestTrue(TEXT("generous reach"), CutlassMath::IsInSweep(Forward, FVector(95.0, 0.0, 0.0), Rules->ReachCm, 110.0f));
	});

	It("plays the second PC's tracks in order, the hit inside each blow", [this]()
	{
		for (const ECutlassMove Move : { ECutlassMove::SlashRight, ECutlassMove::SlashLeft, ECutlassMove::Thrust, ECutlassMove::Finisher })
		{
			const FCutlassAttack* Attack = Rules->GetAttack(Move);
			if (!TestNotNull(TEXT("attack"), Attack) || !TestTrue(TEXT("keys"), Attack->Keys.Num() >= 3))
			{
				return;
			}
			for (int32 Index = 1; Index < Attack->Keys.Num(); ++Index)
			{
				TestTrue(TEXT("sorted"), Attack->Keys[Index].Time >= Attack->Keys[Index - 1].Time);
			}
			TestTrue(TEXT("hit inside the blow"), Attack->HitTime > 0.0f && Attack->HitTime < Attack->Keys.Last().Time);
		}
		TestTrue(TEXT("draw track"), Rules->DrawKeys.Num() >= 3);
		TestTrue(TEXT("sheathe track"), Rules->SheatheKeys.Num() >= 3);
		TestEqual(TEXT("draw time"), Rules->GetMoveDuration(ECutlassMove::Draw), 0.5f, 0.001f);
		TestEqual(TEXT("sheathe time"), Rules->GetMoveDuration(ECutlassMove::Sheathe), 0.6f, 0.001f);
	});

	It("keeps the blade in front of the bean and the scabbard on its left hip", [this]()
	{
		// mesh space: +Y forward, -X right
		TestTrue(TEXT("guard in front"), Rules->GuardKey.GripLocation.Y > 0.0);
		TestTrue(TEXT("thrust points forward"), FRotationMatrix(Rules->Thrust.Keys[2].GripRotation).GetUnitAxis(EAxis::X).Y > 0.9);
		TestTrue(TEXT("scabbard on the left"), Rules->ScabbardRest.GetLocation().X > 0.0);
	});

	It("sweeps the first person view with each blow, the sabre never through the near clip plane", [this]()
	{
		// T-239: the first person tracks are sorted, and from the pommel to the tip the sabre stays ahead of the near plane
		// (5 cm in the project) all along, so the owner never sees it cut open
		const float Hilt = Rules->FirstPersonHiltCm;
		const float Tip = Rules->BladeTipCm;
		const float Limit = Rules->FirstPersonNearClipCm;
		TestTrue(TEXT("limit beyond the near plane"), Limit > 5.0f);
		auto CheckTrack = [this, Hilt, Tip, Limit](const TArray<FCutlassViewKey>& Keys, const TCHAR* Name)
		{
			if (!TestTrue(FString::Printf(TEXT("%s: keys"), Name), Keys.Num() >= 3))
			{
				return;
			}
			for (int32 Index = 1; Index < Keys.Num(); ++Index)
			{
				TestTrue(FString::Printf(TEXT("%s: sorted"), Name), Keys[Index].Time >= Keys[Index - 1].Time);
			}
			float Worst = TNumericLimits<float>::Max();
			for (float Time = 0.0f; Time <= Keys.Last().Time + KINDA_SMALL_NUMBER; Time += 1.0f / 120.0f)
			{
				Worst = FMath::Min(Worst, CutlassMath::ViewBladeMinDepth(CutlassMath::SampleViewKeys(Keys, Time, Rules->FirstPersonGuardKey), Hilt, Tip));
			}
			TestTrue(FString::Printf(TEXT("%s: ahead of the near plane (%.1f cm)"), Name, Worst), Worst >= Limit);
		};
		CheckTrack(Rules->SlashRight.ViewKeys, TEXT("right cut"));
		CheckTrack(Rules->SlashLeft.ViewKeys, TEXT("backhand cut"));
		CheckTrack(Rules->Thrust.ViewKeys, TEXT("thrust"));
		CheckTrack(Rules->Finisher.ViewKeys, TEXT("long blow"));
		CheckTrack(Rules->FirstPersonDrawKeys, TEXT("draw"));
		CheckTrack(Rules->FirstPersonSheatheKeys, TEXT("sheathe"));
		TestTrue(TEXT("guard ahead"), CutlassMath::ViewBladeMinDepth(Rules->FirstPersonGuardKey, Hilt, Tip) >= Limit);
		TestTrue(TEXT("parry ahead"), CutlassMath::ViewBladeMinDepth(Rules->FirstPersonParryKey, Hilt, Tip) >= Limit);

		// the guard sits low at the right of the view, the parry lays the blade across it (tip up and to the left)
		TestTrue(TEXT("guard low right"), Rules->FirstPersonGuardKey.Location.Y > 0.0 && Rules->FirstPersonGuardKey.Location.Z < 0.0);
		const FVector ParryTip = Rules->FirstPersonParryKey.Location + Rules->FirstPersonParryKey.Rotation.RotateVector(FVector(Tip, 0.0, 0.0));
		TestTrue(TEXT("parry across"), ParryTip.Y < 0.0 && ParryTip.Z > Rules->FirstPersonParryKey.Location.Z);

		// each blow crosses the middle of the view at its hit: the tip of the blade within 20 cm of the view axis
		auto TipAt = [this, Tip](const FCutlassAttack& Attack, float Time)
		{
			const FCutlassViewKey Key = CutlassMath::SampleViewKeys(Attack.ViewKeys, Time, Rules->FirstPersonGuardKey);
			return Key.Location + Key.Rotation.RotateVector(FVector(Tip, 0.0, 0.0));
		};
		for (const ECutlassMove Move : { ECutlassMove::SlashRight, ECutlassMove::SlashLeft, ECutlassMove::Thrust, ECutlassMove::Finisher })
		{
			const FCutlassAttack* Attack = Rules->GetAttack(Move);
			const FVector AtHit = TipAt(*Attack, Attack->HitTime);
			TestTrue(TEXT("tip ahead at the hit"), AtHit.X > 40.0);
			TestTrue(TEXT("tip near the axis at the hit"), FMath::Sqrt(AtHit.Y * AtHit.Y + AtHit.Z * AtHit.Z) < 20.0);
		}
		// the right cut comes from the right, the backhand from the left, the long blow falls from above the view
		TestTrue(TEXT("right cut right to left"), TipAt(Rules->SlashRight, Rules->SlashRight.HitTime - 0.08f).Y > TipAt(Rules->SlashRight, Rules->SlashRight.HitTime + 0.08f).Y);
		TestTrue(TEXT("backhand left to right"), TipAt(Rules->SlashLeft, Rules->SlashLeft.HitTime - 0.08f).Y < TipAt(Rules->SlashLeft, Rules->SlashLeft.HitTime + 0.08f).Y);
		TestTrue(TEXT("long blow top to bottom"), TipAt(Rules->Finisher, Rules->Finisher.ActiveStartTime).Z > 30.0 && TipAt(Rules->Finisher, Rules->Finisher.ActiveEndTime).Z < -30.0);
		// the long blow's track starts raised: the charge of the held click brings the blade there, the blow falls from it
		TestTrue(TEXT("long blow starts raised"), Rules->Finisher.ViewKeys[0].Location.Z > Rules->FirstPersonGuardKey.Location.Z + 20.0
			&& Rules->Finisher.ViewKeys[0].Rotation.Pitch > 45.0);
		TestTrue(TEXT("thrust reaches"), TipAt(Rules->Thrust, Rules->Thrust.HitTime).X > TipAt(Rules->Thrust, 0.0f).X + 20.0);

		// the safety net pushes a key that is too close straight ahead, to the limit exactly
		FCutlassViewKey Close;
		Close.Location = FVector(4.0, 0.0, 0.0);
		const FCutlassViewKey Pushed = CutlassMath::KeepAheadOfNearPlane(Close, Hilt, Tip, Limit);
		TestEqual(TEXT("pushed to the limit"), CutlassMath::ViewBladeMinDepth(Pushed, Hilt, Tip), Limit, 0.001f);
		TestEqual(TEXT("only ahead"), Pushed.Location.Y, 0.0, 0.001);
		TestTrue(TEXT("left alone when ahead"), CutlassMath::KeepAheadOfNearPlane(Rules->FirstPersonGuardKey, Hilt, Tip, Limit).Location.Equals(Rules->FirstPersonGuardKey.Location, 0.001f));
	});

	It("bobs with the run, stays still at a stop, breathes and recoils in a loop", [this]()
	{
		const float V = Rules->BobVerticalCm;
		const float L = Rules->BobLateralCm;
		const float R = Rules->BobRollDegrees;
		TestTrue(TEXT("bob values"), V > 0.0f && L > 0.0f && R > 0.0f);
		// no bob at speed 0, whatever the phase
		const CutlassMath::FViewSway Still = CutlassMath::ViewBob(0.37f, 0.0f, V, L, R);
		TestTrue(TEXT("still"), Still.Location.IsNearlyZero(0.0001) && Still.Rotation.IsNearlyZero(0.0001f));
		// periodic over one stride, two dips a stride (one per step), the roll swapping sides
		const CutlassMath::FViewSway A = CutlassMath::ViewBob(0.37f, 1.0f, V, L, R);
		const CutlassMath::FViewSway B = CutlassMath::ViewBob(1.37f, 1.0f, V, L, R);
		TestTrue(TEXT("periodic"), A.Location.Equals(B.Location, 0.001f) && A.Rotation.Equals(B.Rotation, 0.001f));
		TestEqual(TEXT("first step down"), CutlassMath::ViewBob(0.25f, 1.0f, V, L, R).Location.Z, static_cast<double>(-V), 0.001);
		TestEqual(TEXT("second step down"), CutlassMath::ViewBob(0.75f, 1.0f, V, L, R).Location.Z, static_cast<double>(-V), 0.001);
		TestEqual(TEXT("between the steps"), CutlassMath::ViewBob(0.5f, 1.0f, V, L, R).Location.Z, 0.0, 0.001);
		TestEqual(TEXT("roll swaps sides"), CutlassMath::ViewBob(0.25f, 1.0f, V, L, R).Rotation.Roll, -CutlassMath::ViewBob(0.75f, 1.0f, V, L, R).Rotation.Roll, 0.001);
		TestEqual(TEXT("half weight"), CutlassMath::ViewBob(0.25f, 0.5f, V, L, R).Location.Z, static_cast<double>(-0.5f * V), 0.001);
		// the weight follows the ground speed up to the reference speed, none in the air
		TestEqual(TEXT("half speed"), CutlassMath::BobTargetWeight(300.0f, 600.0f, true), 0.5f, 0.001f);
		TestEqual(TEXT("capped"), CutlassMath::BobTargetWeight(900.0f, 600.0f, true), 1.0f, 0.001f);
		TestEqual(TEXT("in the air"), CutlassMath::BobTargetWeight(300.0f, 600.0f, false), 0.0f, 0.001f);
		TestEqual(TEXT("stopped"), CutlassMath::BobTargetWeight(0.0f, 600.0f, true), 0.0f, 0.001f);
		// the breath is centred at its start and loops
		const CutlassMath::FViewSway Breath = CutlassMath::ViewBreath(0.0f, Rules->BreathAmplitudeCm, Rules->BreathPitchDegrees);
		TestTrue(TEXT("breath centred"), Breath.Location.IsNearlyZero(0.0001) && Breath.Rotation.IsNearlyZero(0.0001f));
		TestEqual(TEXT("breath in"), CutlassMath::ViewBreath(0.25f, Rules->BreathAmplitudeCm, Rules->BreathPitchDegrees).Location.Z, static_cast<double>(Rules->BreathAmplitudeCm), 0.001);
		TestTrue(TEXT("breath loops"), CutlassMath::ViewBreath(0.2f, 1.0f, 1.0f).Location.Equals(CutlassMath::ViewBreath(1.2f, 1.0f, 1.0f).Location, 0.001f));
		// the recoil of a blocked hit is silent before and after, full in the middle
		const float Recoil = Rules->BlockRecoilSeconds;
		TestEqual(TEXT("before"), CutlassMath::BlockRecoilEnvelope(-0.01f, Recoil), 0.0f, 0.001f);
		TestEqual(TEXT("at the block"), CutlassMath::BlockRecoilEnvelope(0.0f, Recoil), 0.0f, 0.001f);
		TestEqual(TEXT("peak"), CutlassMath::BlockRecoilEnvelope(Recoil * 0.5f, Recoil), 1.0f, 0.001f);
		TestEqual(TEXT("over"), CutlassMath::BlockRecoilEnvelope(Recoil, Recoil), 0.0f, 0.001f);
		// two sways add up
		CutlassMath::FViewSway Sum = A;
		Sum += Breath;
		TestTrue(TEXT("sum"), Sum.Location.Equals(A.Location + Breath.Location, 0.001f));
	});

	It("reaches a target with a two-bone arm", [this]()
	{
		const FVector Shoulder(0.0, 0.0, 0.0);
		const FVector Target(30.0, 0.0, 0.0);
		const FVector Elbow = CutlassMath::SolveElbow(Shoulder, Target, 20.0f, 20.0f, FVector(0.0, 0.0, -1.0));
		TestEqual(TEXT("upper length"), FVector::Dist(Shoulder, Elbow), 20.0, 0.01);
		TestEqual(TEXT("lower length"), FVector::Dist(Elbow, Target), 20.0, 0.01);
		TestTrue(TEXT("bends toward the pole"), Elbow.Z < 0.0);
		const FVector Far = CutlassMath::SolveElbow(Shoulder, FVector(100.0, 0.0, 0.0), 20.0f, 20.0f, FVector(0.0, 0.0, -1.0));
		TestEqual(TEXT("out of reach: straight"), Far.X, 20.0, 0.1);
	});
}

#endif
