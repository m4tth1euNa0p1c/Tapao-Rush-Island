// monke_island

#include "Creatures/GiantCrabRules.h"
#include "Creatures/GiantCrabCameraShake.h"

namespace
{
	/** Capsule along its bone (the hint's sphyl and sphere), centre in the mesh's space at rest */
	FGiantCrabHitBody CrabCapsuleBody(const TCHAR* Bone, const FVector& Center, float Radius, float Length, EGiantCrabZone Zone)
	{
		FGiantCrabHitBody Body;
		Body.Bone = Bone;
		Body.CenterCm = Center;
		Body.RadiusCm = Radius;
		Body.LengthCm = Length;
		Body.Zone = Zone;
		return Body;
	}

	/** Box along the mesh's axes at rest (the hint's box) */
	FGiantCrabHitBody CrabBoxBody(const TCHAR* Bone, const FVector& Center, const FVector& Size, EGiantCrabZone Zone)
	{
		FGiantCrabHitBody Body;
		Body.Bone = Bone;
		Body.bBox = true;
		Body.CenterCm = Center;
		Body.BoxSizeCm = Size;
		Body.Zone = Zone;
		return Body;
	}

	/** Upright pillar standing at a point of the mesh's ground plane at rest (T-328) */
	FGiantCrabPawnPillar CrabPillar(const FString& Name, double X, double Y, float Radius)
	{
		FGiantCrabPawnPillar Pillar;
		Pillar.Name = FName(*Name);
		Pillar.CenterCm = FVector2D(X, Y);
		Pillar.RadiusCm = Radius;
		return Pillar;
	}

	FGiantCrabCueRules CrabCue(float Volume, float PitchJitter, TSubclassOf<UCameraShakeBase> Shake = nullptr, float ShakeScale = 1.0f,
		float Inner = 400.0f, float Outer = 2500.0f, float Dust = 0.0f)
	{
		FGiantCrabCueRules Rules;
		Rules.Volume = Volume;
		Rules.PitchJitter = PitchJitter;
		Rules.Shake = Shake;
		Rules.ShakeScale = ShakeScale;
		Rules.ShakeInnerRadiusCm = Inner;
		Rules.ShakeOuterRadiusCm = Outer;
		Rules.DustScale = Dust;
		return Rules;
	}
}

UGiantCrabRules::UGiantCrabRules()
{
	BossName = NSLOCTEXT("GiantCrab", "BossName", "Tupa Nui, Gardien du récif");

	// ---- phases (T-261): > 66 % as before but quicker; <= 66 % roar, combos, stomps, charge; <= 33 % frenzy
	FGiantCrabPhaseRules First;
	First.HealthFraction = 1.0f;
	First.AttackPlayRate = 1.15f;
	First.RecoveryScale = 0.8f;
	Phases.Add(First);

	FGiantCrabPhaseRules Second;
	Second.HealthFraction = 0.66f;
	Second.bRoar = true;
	Second.MoveSpeedScale = 1.15f;
	Second.TurnRateScale = 1.2f;
	Second.AttackPlayRate = 1.3f;
	Second.RecoveryScale = 0.6f;
	Second.DamageScale = 1.2f;
	Second.DamageTakenScale = 0.8f;
	Second.DamageCap = 120.0f;
	Second.BlockChance = 0.35f;
	Second.StompWeight = 1.5f;
	Second.ChargeWeight = 1.0f;
	Second.ComboChance = 0.55f;
	Second.Combos = { { EGiantCrabAttack::Swipe, EGiantCrabAttack::Slam }, { EGiantCrabAttack::Slam, EGiantCrabAttack::Stomp },
		{ EGiantCrabAttack::Stomp, EGiantCrabAttack::Swipe } };
	Second.BellySlamCooldown = 3.0f;
	Phases.Add(Second);

	FGiantCrabPhaseRules Third;
	Third.HealthFraction = 0.33f;
	Third.bRoar = true;
	Third.bFrenzy = true;
	Third.MoveSpeedScale = 1.3f;
	Third.TurnRateScale = 1.4f;
	Third.AttackPlayRate = 1.5f;
	Third.RecoveryScale = 0.35f;
	Third.DamageScale = 1.4f;
	Third.DamageTakenScale = 0.65f;
	Third.DamageCap = 100.0f;
	Third.BlockChance = 0.2f;
	Third.StompWeight = 1.5f;
	Third.ChargeWeight = 1.5f;
	Third.ComboChance = 0.75f;
	Third.Combos = { { EGiantCrabAttack::Swipe, EGiantCrabAttack::Slam }, { EGiantCrabAttack::Slam, EGiantCrabAttack::Grab },
		{ EGiantCrabAttack::Grab, EGiantCrabAttack::Grab }, { EGiantCrabAttack::Stomp, EGiantCrabAttack::Grab } };
	Third.BellySlamCooldown = 2.0f;
	Phases.Add(Third);

	// ---- hit bodies: the kit's PHAT_GiantCrab_hint.json (mesh space at rest, cm; capsules along the bone's Y axis)
	HitBodies.Add(CrabBoxBody(TEXT("body"), FVector(0.0, 25.0, 160.0), FVector(240.0, 190.0, 62.0), EGiantCrabZone::Shell));
	HitBodies.Add(CrabBoxBody(TEXT("abdomen"), FVector(0.0, -25.0, 125.0), FVector(90.0, 110.0, 12.0), EGiantCrabZone::Belly));
	for (const float Side : { 1.0f, -1.0f })
	{
		const TCHAR* S = Side > 0.0f ? TEXT("l") : TEXT("r");
		HitBodies.Add(CrabCapsuleBody(*FString::Printf(TEXT("eye_stalk_%s"), S), FVector(46.5f * Side, 118.3, 184.9), 9.0f, 0.0f, EGiantCrabZone::Eye));
		HitBodies.Add(CrabCapsuleBody(*FString::Printf(TEXT("eye_%s"), S), FVector(53.1f * Side, 127.5, 203.7), 9.0f, 0.0f, EGiantCrabZone::Eye));
	}
	// claws: the right one is the slightly bigger cutter of the kit
	const FVector LeftHand(84.0, 211.8, 112.8);
	const FVector LeftHandSize(30.0, 120.0, 46.0);
	const FVector LeftFinger(38.5, 252.6, 122.3);
	const FVector RightHand(-89.7, 215.8, 105.8);
	const FVector RightHandSize(40.0, 130.0, 62.0);
	const FVector RightFinger(-44.2, 268.9, 116.0);
	HitBodies.Add(CrabCapsuleBody(TEXT("claw_l_arm"), FVector(110.1, 128.0, 144.1), 19.0f, 47.0f, EGiantCrabZone::Claw));
	HitBodies.Add(CrabCapsuleBody(TEXT("claw_l_forearm"), FVector(125.1, 173.0, 133.1), 21.0f, 8.0f, EGiantCrabZone::Joint));
	HitBodies.Add(CrabBoxBody(TEXT("claw_l_hand"), LeftHand, LeftHandSize, EGiantCrabZone::Claw));
	HitBodies.Add(CrabCapsuleBody(TEXT("claw_l_finger"), LeftFinger, 9.0f, 42.0f, EGiantCrabZone::Claw));
	HitBodies.Add(CrabCapsuleBody(TEXT("claw_r_arm"), FVector(-112.2, 125.8, 142.9), 19.0f, 47.0f, EGiantCrabZone::Claw));
	HitBodies.Add(CrabCapsuleBody(TEXT("claw_r_forearm"), FVector(-130.2, 168.8, 129.9), 21.0f, 8.0f, EGiantCrabZone::Joint));
	HitBodies.Add(CrabBoxBody(TEXT("claw_r_hand"), RightHand, RightHandSize, EGiantCrabZone::Claw));
	HitBodies.Add(CrabCapsuleBody(TEXT("claw_r_finger"), RightFinger, 9.0f, 44.0f, EGiantCrabZone::Claw));
	// walking legs, left (+X of the mesh) and mirrored right: merus, propodus, dactyl
	struct FLegSegment
	{
		const TCHAR* Name;
		FVector Center;
		float Radius;
		float Length;
	};
	const FLegSegment Legs[] = {
		{ TEXT("1_merus"), FVector(158.9, 111.5, 148.9), 14.0f, 73.2f }, { TEXT("1_propodus"), FVector(208.5, 164.0, 120.3), 11.0f, 65.4f },
		{ TEXT("1_dactyl"), FVector(230.0, 186.6, 41.4), 8.0f, 68.6f },
		{ TEXT("2_merus"), FVector(188.2, 42.5, 156.5), 14.0f, 82.0f }, { TEXT("2_propodus"), FVector(257.6, 65.4, 132.5), 11.0f, 73.0f },
		{ TEXT("2_dactyl"), FVector(286.9, 75.0, 45.0), 8.0f, 76.0f },
		{ TEXT("3_merus"), FVector(181.5, -50.6, 154.8), 14.0f, 82.0f }, { TEXT("3_propodus"), FVector(252.0, -80.8, 130.8), 11.0f, 73.0f },
		{ TEXT("3_dactyl"), FVector(283.2, -94.2, 45.0), 8.0f, 76.0f },
		{ TEXT("4_merus"), FVector(137.6, -111.7, 144.4), 14.0f, 65.5f }, { TEXT("4_propodus"), FVector(177.7, -162.2, 112.7), 11.0f, 58.8f },
		{ TEXT("4_dactyl"), FVector(193.9, -182.6, 38.2), 8.0f, 62.2f },
	};
	for (const float Side : { 1.0f, -1.0f })
	{
		for (const FLegSegment& Segment : Legs)
		{
			const FString Bone = FString::Printf(TEXT("leg_%s%s"), Side > 0.0f ? TEXT("l") : TEXT("r"), Segment.Name);
			HitBodies.Add(CrabCapsuleBody(*Bone, FVector(Segment.Center.X * Side, Segment.Center.Y, Segment.Center.Z), Segment.Radius, Segment.Length,
				EGiantCrabZone::Leg));
		}
	}

	// ---- upright pillars that stop the characters (T-328): the eight leg tips (a dactyl stands almost upright, from the
	// sand to the knee at 84 cm: its centre is its foot) and the two claws where they hang at rest, two pillars along each
	// hand and one at its pincer. The gaps between legs 1-2, 2-3, 3-4 and behind stay open: a bean still walks under the
	// belly (T-261)
	constexpr float LegPillarRadius = 25.0f;
	constexpr float FingerPillarRadius = 15.0f;
	for (const float Side : { 1.0f, -1.0f })
	{
		for (int32 Leg = 1; Leg <= 4; ++Leg)
		{
			const FLegSegment& Dactyl = Legs[(Leg - 1) * 3 + 2];
			PawnPillars.Add(CrabPillar(FString::Printf(TEXT("leg_%s%d"), Side > 0.0f ? TEXT("l") : TEXT("r"), Leg), Dactyl.Center.X * Side, Dactyl.Center.Y,
				LegPillarRadius));
		}
	}
	for (const bool bLeft : { true, false })
	{
		const FVector& Hand = bLeft ? LeftHand : RightHand;
		const FVector& HandSize = bLeft ? LeftHandSize : RightHandSize;
		const FVector& Finger = bLeft ? LeftFinger : RightFinger;
		const TCHAR* S = bLeft ? TEXT("l") : TEXT("r");
		// along the hand (Y), each a quarter of its length from its centre: each covers its half of the hand, corners
		// included, and a centimetre more
		const double Quarter = HandSize.Y * 0.25;
		const float HandRadius = static_cast<float>(FMath::Sqrt(FMath::Square(HandSize.X * 0.5) + FMath::Square(Quarter)) + 1.0);
		PawnPillars.Add(CrabPillar(FString::Printf(TEXT("claw_%s_hand_back"), S), Hand.X, Hand.Y - Quarter, HandRadius));
		PawnPillars.Add(CrabPillar(FString::Printf(TEXT("claw_%s_hand_front"), S), Hand.X, Hand.Y + Quarter, HandRadius));
		PawnPillars.Add(CrabPillar(FString::Printf(TEXT("claw_%s_finger"), S), Finger.X, Finger.Y, FingerPillarRadius));
	}

	// ---- stomp of the flanks (the hit reaction stands in for A_GiantCrab_LegStomp until T-260 delivers it)
	Stomp.Damage = 0.0f;
	Stomp.ReachCm = 120.0f;
	Stomp.ConeDegrees = 180.0f;
	Stomp.LungeCm = 0.0f;
	Stomp.KnockbackSpeed = 650.0f;
	Stomp.KnockbackUp = 420.0f;
	Stomp.ShockwaveRadiusCm = 230.0f;
	Stomp.ShockwaveDamage = 25.0f;
	Stomp.Recovery = 0.9f;
	Stomp.bNeedsFacing = false;
	StompImpactTimes = { 0.25f, 0.55f };

	// ---- feet: the dactyls, the hind pairs heavier; the tip at the end of the bone (manifest v2 of T-260, leg_tips)
	const float TipLengths[] = { 84.6f, 92.0f, 92.0f, 78.2f };
	for (const TCHAR* Side : { TEXT("l"), TEXT("r") })
	{
		for (int32 Leg = 1; Leg <= 4; ++Leg)
		{
			FGiantCrabFoot Foot;
			Foot.Bone = *FString::Printf(TEXT("leg_%s%d_dactyl"), Side, Leg);
			Foot.bHeavy = Leg >= 3;
			Foot.TipOffsetCm = FVector(0.0f, -TipLengths[Leg - 1], 0.0f);
			Feet.Add(Foot);
		}
	}

	// ---- cues: shakes and sand (the sounds come from scripts/editor/crab_boss_setup_ue.py)
	StepCue = CrabCue(0.55f, 0.12f);
	HeavyStepCue = CrabCue(0.8f, 0.1f, UGiantCrabStepShake::StaticClass(), 0.5f, 250.0f, 1400.0f);
	SlamCue = CrabCue(1.0f, 0.06f, UGiantCrabImpactShake::StaticClass(), 1.0f, 400.0f, 2200.0f, 2.5f);
	StompCue = CrabCue(0.9f, 0.08f, UGiantCrabImpactShake::StaticClass(), 0.7f, 350.0f, 1800.0f, 2.0f);
	BellySlamCue = CrabCue(1.0f, 0.05f, UGiantCrabImpactShake::StaticClass(), 1.6f, 600.0f, 3000.0f, 4.0f);
	RoarCue = CrabCue(1.0f, 0.04f, UGiantCrabRoarShake::StaticClass(), 1.0f, 800.0f, 4500.0f);
	RoarWaveCue = CrabCue(1.0f, 0.05f, UGiantCrabImpactShake::StaticClass(), 1.2f, 900.0f, 3500.0f, 3.0f);
	ChargeCue = CrabCue(1.0f, 0.05f, UGiantCrabRoarShake::StaticClass(), 0.5f, 600.0f, 3000.0f);
	HitCue = CrabCue(0.9f, 0.08f);
	ClackCue = CrabCue(0.8f, 0.1f);
	HissCue = CrabCue(0.7f, 0.1f);
	GrowlCue = CrabCue(0.75f, 0.08f);
	DeathCue = CrabCue(1.0f, 0.03f, UGiantCrabRoarShake::StaticClass(), 0.6f, 600.0f, 3000.0f);
	BodyFallCue = CrabCue(0.9f, 0.06f, UGiantCrabImpactShake::StaticClass(), 0.8f, 400.0f, 2000.0f, 2.0f);
	NotifyCues.Add(TEXT("AN_Claw_Snap"), EGiantCrabCue::Clack);
	NotifyCues.Add(TEXT("AN_BodyFall"), EGiantCrabCue::BodyFall);
	NotifyCues.Add(TEXT("AN_Land"), EGiantCrabCue::BodyFall);

	// ---- frenzy: red eyes (M_GiantCrab_Eyes: EmissiveColor, EmissiveStrength 2 at rest)
	EyeMaterialSlots = { TEXT("MI_GiantCrab_Eye") };
	FrenzyEyeColorParams = { TEXT("EmissiveColor") };
	FrenzyEyeScalarParams.Add(TEXT("EmissiveStrength"), 12.0f);
	EyeSockets = { TEXT("WeakPoint_Eye_L"), TEXT("WeakPoint_Eye_R") };
}

const FGiantCrabPhaseRules* UGiantCrabRules::GetPhase(int32 Index) const
{
	return Phases.IsValidIndex(Index) ? &Phases[Index] : nullptr;
}

void UGiantCrabRules::GetPhaseThresholds(TArray<float>& OutThresholds) const
{
	OutThresholds.Reset(Phases.Num());
	for (const FGiantCrabPhaseRules& Phase : Phases)
	{
		OutThresholds.Add(Phase.HealthFraction);
	}
}

const FGiantCrabCueRules* UGiantCrabRules::GetCue(EGiantCrabCue Cue) const
{
	switch (Cue)
	{
	case EGiantCrabCue::Step:
		return &StepCue;
	case EGiantCrabCue::HeavyStep:
		return &HeavyStepCue;
	case EGiantCrabCue::Slam:
		return &SlamCue;
	case EGiantCrabCue::Stomp:
		return &StompCue;
	case EGiantCrabCue::BellySlam:
		return &BellySlamCue;
	case EGiantCrabCue::Roar:
		return &RoarCue;
	case EGiantCrabCue::RoarWave:
		return &RoarWaveCue;
	case EGiantCrabCue::Charge:
		return &ChargeCue;
	case EGiantCrabCue::Hit:
		return &HitCue;
	case EGiantCrabCue::Clack:
		return &ClackCue;
	case EGiantCrabCue::Hiss:
		return &HissCue;
	case EGiantCrabCue::Growl:
		return &GrowlCue;
	case EGiantCrabCue::Death:
		return &DeathCue;
	case EGiantCrabCue::BodyFall:
		return &BodyFallCue;
	default:
		return nullptr;
	}
}
