// monke_island

// Boss moves of the giant crab (T-261): phases and roar, charge, flank stomps, belly slam, the moments of its actions,
// the damage it takes, its boss bar. The hunter (targets, approach, blows, clips) is in GiantCrab.cpp.

#include "Creatures/GiantCrab.h"
#include "Arena/ArenaZone.h"
#include "Creatures/GiantCrabBody.h"
#include "Creatures/GiantCrabFeedback.h"
#include "Creatures/GiantCrabInternal.h"
#include "Creatures/GiantCrabLag.h"
#include "Creatures/GiantCrabMath.h"
#include "Creatures/GiantCrabRules.h"
#include "Player/IslanderHealthComponent.h"
#include "Weapons/FirearmDamage.h"
#include "monke_island.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

// ---------------------------------------------------------------- flank stomps, belly slam, charge (server)

void AGiantCrab::StompImpact(int8 Side)
{
	const FGiantCrabAttackRules& Stomp = Rules->Stomp;
	const FVector Center = GetActorLocation() + GetActorRightVector() * (static_cast<float>(Side) * Rules->StompSideOffsetCm);
	Shockwave(Center, Stomp.ShockwaveRadiusCm, Stomp.ShockwaveDamage, Stomp.KnockbackSpeed, Stomp.KnockbackUp);
}

void AGiantCrab::BellySlamImpact()
{
	const FGiantCrabBellySlamRules& Slam = Rules->BellySlam;
	const FVector Ground = GetGroundPoint();
	const float Yaw = static_cast<float>(GetActorRotation().Yaw);
	const FVector Center = Ground + GetActorForwardVector() * Slam.FootprintCenterCm.X + GetActorRightVector() * Slam.FootprintCenterCm.Y;
	FPreyList Prey;
	GatherPrey(Prey);
	int32 Crushed = 0;
	for (ACharacter* Victim : Prey)
	{
		const FVector There = Victim->GetActorLocation();
		const FVector Feet = There - FVector(0.0f, 0.0f, GiantCrabInternal::PreyHalfHeight(Victim));
		// the edge of the shell crushes too: the footprint grows by the body of the one under it
		const float Body = GiantCrabInternal::PreyRadius(Victim);
		if (GiantCrabMath::IsUnderShell(Ground, Yaw, Feet, Slam.FootprintCenterCm, Slam.FootprintRadiiCm + FVector2D(Body, Body), Slam.MaxHeightCm))
		{
			Fling(Victim, GiantCrabMath::Knockback(Center, There, Slam.CrushOutSpeed, Slam.CrushUpSpeed), true);
			HurtPrey(Victim, Slam.Damage);
			++Crushed;
		}
		else if (FVector::Dist2D(There, Center) <= Slam.WaveRadiusCm && FMath::Abs(Feet.Z - Ground.Z) <= GiantCrabInternal::MaxReachHeight)
		{
			Fling(Victim, GiantCrabMath::Knockback(Center, There, Slam.WaveKnockbackSpeed, Slam.WaveKnockbackUp), false);
			HurtPrey(Victim, Slam.WaveDamage);
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: belly slam, %d crushed under it"), *GetName(), Crushed);
}

void AGiantCrab::ChargeStrikes()
{
	const FGiantCrabChargeRules& Charge = Rules->Charge;
	const FVector Me = GetActorLocation();
	const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	FPreyList Prey;
	GatherPrey(Prey);
	for (ACharacter* Victim : Prey)
	{
		const FVector There = Victim->GetActorLocation();
		if (Struck.Contains(Victim) || FMath::Abs(There.Z - Me.Z) > GiantCrabInternal::MaxReachHeight
			|| GiantCrabMath::EdgeDistance(Me, Radius, There, GiantCrabInternal::PreyRadius(Victim)) > Charge.HitReachCm
			|| !GiantCrabMath::IsInFront(ChargeDirection, There - Me, Charge.HitConeDegrees))
		{
			continue;
		}
		Struck.Add(Victim);
		// tossed ahead and to the side it stood on
		const float SideSign = GiantCrabMath::SignedAngle(ChargeDirection, There - Me) >= 0.0f ? 1.0f : -1.0f;
		const FVector Aside = FVector(-ChargeDirection.Y, ChargeDirection.X, 0.0f) * SideSign;
		const FVector Velocity = (ChargeDirection * 0.75f + Aside * 0.65f).GetSafeNormal() * Charge.KnockbackSpeed + FVector(0.0f, 0.0f, Charge.KnockbackUp);
		Fling(Victim, Velocity, Charge.bKnockoutPlayers);
		HurtPrey(Victim, Charge.Damage);
		MulticastCue(EGiantCrabCue::Hit, There);
		UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: charges through %s"), *GetName(), *Victim->GetName());
	}
}

// ---------------------------------------------------------------- boss moves (server)

void AGiantCrab::UpdatePhase()
{
	TArray<float, TInlineAllocator<4>> Thresholds;
	for (const FGiantCrabPhaseRules& PhaseRules : Rules->Phases)
	{
		Thresholds.Add(PhaseRules.HealthFraction);
	}
	const int32 Wanted = GiantCrabMath::NextPhase(Phase, GiantCrabMath::PhaseForHealth(Health->GetHealthFraction(), Thresholds));
	if (Wanted == Phase)
	{
		return;
	}
	Phase = static_cast<uint8>(Wanted);
	OnRep_Phase();
	const FGiantCrabPhaseRules& PhaseRules = CurrentPhase();
	UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: phase %d at %.0f%% health%s%s"), *GetName(), Phase + 1, Health->GetHealthFraction() * 100.0f,
		PhaseRules.bRoar ? TEXT(", roars") : TEXT(""), PhaseRules.bFrenzy ? TEXT(", frenzy") : TEXT(""));
	if (!PhaseRules.bRoar)
	{
		return;
	}
	Planned = EGiantCrabAttack::None;
	WeakHits.Reset();
	// on its back it roars once it stands again; otherwise at once, whatever it was doing
	const EGiantCrabState State = Action.State;
	if (State == EGiantCrabState::Stagger || State == EGiantCrabState::Flipped || State == EGiantCrabState::Recover)
	{
		bRoarPending = true;
		return;
	}
	SetState(EGiantCrabState::Roar);
}

void AGiantCrab::UpdateRoar()
{
	if (StateTime() >= FMath::Max(Rules->Roar.MinSeconds, ClipLength(GetStateClip(EGiantCrabState::Roar, EGiantCrabAttack::None))))
	{
		SetState(Target.IsValid() ? EGiantCrabState::Approach : EGiantCrabState::Idle);
	}
}

void AGiantCrab::UpdateChargeWindup(float DeltaSeconds)
{
	const FGiantCrabChargeRules& Charge = Rules->Charge;
	AActor* Prey = Target.Get();
	// it turns a flank to its prey while it paws the sand: a crab charges sideways
	if (Prey)
	{
		const float Turn = GiantCrabMath::SideTurn(GetActorForwardVector(), Prey->GetActorLocation() - GetActorLocation());
		const float MaxStep = Rules->TurnRateDegrees * CurrentPhase().TurnRateScale * 2.0f * DeltaSeconds;
		SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + FMath::Clamp(Turn, -MaxStep, MaxStep), 0.0f));
	}
	if (StateTime() < Charge.WindupSeconds / FMath::Max(0.05f, Action.PlayRate))
	{
		return;
	}
	if (!Prey)
	{
		SetState(EGiantCrabState::Idle);
		return;
	}
	// straight through where its prey stands, past it, held inside its ground
	const FVector Me = GetActorLocation();
	ChargeDirection = (Prey->GetActorLocation() - Me).GetSafeNormal2D();
	if (ChargeDirection.IsNearlyZero())
	{
		ChargeDirection = GetActorForwardVector().GetSafeNormal2D();
	}
	ChargeEnd = GiantCrabMath::ChargeEnd(Me, Prey->GetActorLocation(), Charge.OvershootCm, Charge.MaxDistanceCm + Charge.OvershootCm);
	const float Body = GetCapsuleComponent()->GetScaledCapsuleRadius() + Rules->EdgeMarginCm;
	if (const AArenaZone* Zone = Arena.Get())
	{
		ArenaZoneMath::ClampIntoCircle(Zone->GetActorLocation(), FMath::Max(0.0f, Zone->Radius - Body), ChargeEnd);
	}
	else
	{
		ArenaZoneMath::ClampIntoCircle(Home, FMath::Max(0.0f, Rules->LeashRadiusCm - Body), ChargeEnd);
	}
	// exactly side-on: the run is to its left or to its right
	SetActorRotation(FRotator(0.0f, GetActorRotation().Yaw + GiantCrabMath::SideTurn(GetActorForwardVector(), ChargeDirection), 0.0f));
	NextCharge = GetWorld()->GetTimeSeconds() + Charge.Cooldown;
	SetState(EGiantCrabState::Charge);
	UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: charges at %s (%.0f m)"), *GetName(), *Prey->GetName(), FVector::Dist2D(Me, ChargeEnd) / 100.0f);
}

void AGiantCrab::UpdateCharge(float DeltaSeconds)
{
	const FGiantCrabChargeRules& Charge = Rules->Charge;
	const FVector Me = GetActorLocation();
	const float Speed = Charge.SpeedCmps * CurrentPhase().MoveSpeedScale;
	const bool bArrived = FVector::Dist2D(Me, ChargeEnd) <= GiantCrabInternal::ChargeArrivalCm;
	const bool bStalled = StateTime() >= GiantCrabInternal::ChargeStallSeconds && GetVelocity().Size2D() < Speed * GiantCrabInternal::ChargeStallShare;
	const bool bEdge = !IsInTerritory(Me + ChargeDirection * (GetCapsuleComponent()->GetScaledCapsuleRadius() + Rules->EdgeMarginCm));
	if (bArrived || bStalled || bEdge || StateTime() >= Charge.MaxSeconds)
	{
		// it stops dead and stands out of breath: the opening
		GetCharacterMovement()->StopMovementImmediately();
		const double Now = GetWorld()->GetTimeSeconds();
		StunnedUntil = Now + Charge.RecoverySeconds;
		NextAttack = StunnedUntil;
		Planned = EGiantCrabAttack::None;
		SetState(EGiantCrabState::Idle);
		return;
	}
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	AddMovementInput(ChargeDirection, 1.0f, true);
	ChargeStrikes();
}

bool AGiantCrab::CheckUnderShell(float DeltaSeconds)
{
	UnderShellClock += DeltaSeconds;
	if (UnderShellClock < GiantCrabInternal::UnderShellInterval)
	{
		return false;
	}
	const float Step = UnderShellClock;
	UnderShellClock = 0.0f;
	const FGiantCrabBellySlamRules& Slam = Rules->BellySlam;
	const FVector Ground = GetGroundPoint();
	const float Yaw = static_cast<float>(GetActorRotation().Yaw);
	FPreyList Prey;
	GatherPrey(Prey);
	bool bLongEnough = false;
	for (ACharacter* Victim : Prey)
	{
		const FVector Feet = Victim->GetActorLocation() - FVector(0.0f, 0.0f, GiantCrabInternal::PreyHalfHeight(Victim));
		const bool bUnder = GiantCrabMath::IsUnderShell(Ground, Yaw, Feet, Slam.FootprintCenterCm, Slam.FootprintRadiiCm, Slam.MaxHeightCm);
		if (!bUnder)
		{
			UnderShell.Remove(Victim);
			continue;
		}
		float& Time = UnderShell.FindOrAdd(Victim);
		Time = GiantCrabMath::UnderShellTime(Time, true, Step);
		bLongEnough |= Time >= Slam.TriggerSeconds;
	}
	for (auto It = UnderShell.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
	// it drops whatever it is doing on its feet, but not in the middle of a blow, a charge or a roar
	const EGiantCrabState State = Action.State;
	const bool bFree = State == EGiantCrabState::Idle || State == EGiantCrabState::Approach || State == EGiantCrabState::Taunt
		|| State == EGiantCrabState::Block || State == EGiantCrabState::HitReact;
	if (!bLongEnough || !bFree || GetWorld()->GetTimeSeconds() < NextBellySlam)
	{
		return false;
	}
	GetCharacterMovement()->StopMovementImmediately();
	Planned = EGiantCrabAttack::None;
	SetState(EGiantCrabState::BellySlam);
	UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: someone under its shell, it drops"), *GetName());
	return true;
}

void AGiantCrab::UpdateBellySlam()
{
	const float Length = ClipLength(GetStateClip(EGiantCrabState::BellySlam, EGiantCrabAttack::None)) / FMath::Max(0.05f, Action.PlayRate);
	const float LastMoment = ServerMoments.IsEmpty() ? 0.0f : ServerMoments.Last().Time;
	if (StateTime() >= FMath::Max(Length, LastMoment + 0.3f))
	{
		NextBellySlam = GetWorld()->GetTimeSeconds() + CurrentPhase().BellySlamCooldown;
		UnderShell.Reset();
		SetState(Target.IsValid() ? EGiantCrabState::Approach : EGiantCrabState::Idle);
	}
}

void AGiantCrab::FireServerMoments()
{
	const float Time = StateTime();
	while (ServerMomentsDone < ServerMoments.Num() && ServerMoments[ServerMomentsDone].Time <= Time)
	{
		const FGiantCrabMoment Moment = ServerMoments[ServerMomentsDone++];
		switch (Moment.Cue)
		{
		case EGiantCrabCue::Stomp:
			StompImpact(Moment.Side);
			break;
		case EGiantCrabCue::BellySlam:
			BellySlamImpact();
			break;
		case EGiantCrabCue::RoarWave:
		{
			const FGiantCrabRoarRules& Roar = Rules->Roar;
			Shockwave(GetActorLocation(), Roar.ShockwaveRadiusCm, Roar.ShockwaveDamage, Roar.KnockbackSpeed, Roar.KnockbackUp, Roar.KnockoutRadiusCm);
			break;
		}
		default:
			// sound and shake only, on every machine
			break;
		}
	}
}

void AGiantCrab::GetActionMoments(const FGiantCrabActionRep& InAction, TArray<FGiantCrabMoment>& OutMoments) const
{
	OutMoments.Reset();
	if (!Rules)
	{
		return;
	}
	const float Rate = FMath::Max(0.05f, InAction.PlayRate);
	auto Add = [&OutMoments, Rate](float ClipSeconds, EGiantCrabCue Cue, int8 Side)
	{
		FGiantCrabMoment& Moment = OutMoments.AddDefaulted_GetRef();
		Moment.Time = FMath::Max(0.0f, ClipSeconds) / Rate;
		Moment.Cue = Cue;
		Moment.Side = Side;
	};
	switch (InAction.State)
	{
	case EGiantCrabState::Attack:
		if (InAction.Attack == EGiantCrabAttack::Slam)
		{
			Add(Rules->Slam.HitStart, EGiantCrabCue::Slam, 0);
		}
		else if (InAction.Attack == EGiantCrabAttack::Stomp)
		{
			// the notifies of A_GiantCrab_LegStomp, else the times of the rules, flanks in turn from the left
			const UAnimSequenceBase* Clip = GetAttackClip(EGiantCrabAttack::Stomp);
			const float Left = FindNotifyTime(Clip, Rules->StompNotifyLeft, -1.0f);
			const float Right = FindNotifyTime(Clip, Rules->StompNotifyRight, -1.0f);
			if (Left >= 0.0f || Right >= 0.0f)
			{
				if (Left >= 0.0f)
				{
					Add(Left, EGiantCrabCue::Stomp, -1);
				}
				if (Right >= 0.0f)
				{
					Add(Right, EGiantCrabCue::Stomp, 1);
				}
			}
			else
			{
				for (int32 Index = 0; Index < Rules->StompImpactTimes.Num(); ++Index)
				{
					Add(Rules->StompImpactTimes[Index], EGiantCrabCue::Stomp, Index % 2 == 0 ? -1 : 1);
				}
			}
		}
		break;
	case EGiantCrabState::BellySlam:
		Add(FindNotifyTime(GetStateClip(EGiantCrabState::BellySlam, EGiantCrabAttack::None), Rules->BellySlam.ImpactNotify,
			Rules->BellySlam.FallbackImpactSeconds), EGiantCrabCue::BellySlam, 0);
		break;
	case EGiantCrabState::Roar:
		Add(0.0f, EGiantCrabCue::Roar, 0);
		Add(FindNotifyTime(GetStateClip(EGiantCrabState::Roar, EGiantCrabAttack::None), Rules->Roar.ImpactNotify, Rules->Roar.FallbackImpactSeconds),
			EGiantCrabCue::RoarWave, 0);
		break;
	case EGiantCrabState::ChargeWindup:
		Add(0.0f, EGiantCrabCue::Charge, 0);
		break;
	case EGiantCrabState::Taunt:
		// the claw snaps of the clip clack by themselves (NotifyCues); a clip without them clacks once
		if (FindNotifyTime(Rules->Taunt, GiantCrabInternal::ClawSnapNotify, -1.0f) < 0.0f)
		{
			Add(0.0f, EGiantCrabCue::Clack, 0);
		}
		break;
	case EGiantCrabState::Dead:
		Add(0.0f, EGiantCrabCue::Death, 0);
		break;
	default:
		break;
	}
	OutMoments.Sort([](const FGiantCrabMoment& A, const FGiantCrabMoment& B) { return A.Time < B.Time; });
}

FVector AGiantCrab::GetMomentLocation(const FGiantCrabMoment& Moment) const
{
	const FVector Ground = GetGroundPoint();
	if (!Rules)
	{
		return Ground;
	}
	switch (Moment.Cue)
	{
	case EGiantCrabCue::Slam:
		return Ground + GetActorForwardVector() * (GetCapsuleComponent()->GetScaledCapsuleRadius() + Rules->Slam.ReachCm * 0.6f);
	case EGiantCrabCue::Stomp:
		return Ground + GetActorRightVector() * (static_cast<float>(Moment.Side) * Rules->StompSideOffsetCm);
	case EGiantCrabCue::BellySlam:
		return Ground + GetActorForwardVector() * Rules->BellySlam.FootprintCenterCm.X + GetActorRightVector() * Rules->BellySlam.FootprintCenterCm.Y;
	case EGiantCrabCue::Roar:
	case EGiantCrabCue::Charge:
	case EGiantCrabCue::Death:
		return GetMesh()->DoesSocketExist(GiantCrabInternal::MouthSocket) ? GetMesh()->GetSocketLocation(GiantCrabInternal::MouthSocket) : GetActorLocation();
	default:
		return Ground;
	}
}

// ---------------------------------------------------------------- damage

float AGiantCrab::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !Rules || IsDead())
	{
		return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// where it struck: the point, bone and body of a shot or a cut, else the striker
	APawn* Attacker = EventInstigator ? EventInstigator->GetPawn() : nullptr;
	if (!Attacker && DamageCauser)
	{
		Attacker = Cast<APawn>(DamageCauser) ? Cast<APawn>(DamageCauser) : DamageCauser->GetInstigator();
	}
	FVector Point = Attacker ? Attacker->GetActorLocation() : GetActorLocation();
	FName Bone = NAME_None;
	const UPrimitiveComponent* StruckComponent = nullptr;
	const FHitResult* HitInfo = nullptr;
	const bool bPoint = DamageEvent.IsOfType(FPointDamageEvent::ClassID);
	if (bPoint)
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		HitInfo = &PointEvent.HitInfo;
		Point = PointEvent.HitInfo.ImpactPoint;
		Bone = PointEvent.HitInfo.BoneName;
		StruckComponent = PointEvent.HitInfo.GetComponent();
	}
	// a distant player struck the crab as its machine showed it, a moment ago (T-305, R-25): 0 for the host and the mobs
	const float Rewind = GetInstigatorRewind(EventInstigator);
	const bool bShot = DamageEvent.IsOfType(FFirearmDamageEvent::ClassID);
	const FVector From = Attacker ? Attacker->GetActorLocation() : Point;
	const bool bFront = GiantCrabMath::IsInFront(GetActorForwardVector(), From - GetActorLocation(), Rules->FrontConeDegrees);
	const bool bOnBack = Action.State == EGiantCrabState::Stagger || Action.State == EGiantCrabState::Flipped;

	// whoever hurts it inside its ground becomes its prey when it hunts nothing; it fights (the boss bar)
	LastHurtTime = Now;
	if (!Target.IsValid() && Attacker && IsPrey(Attacker) && IsInTerritory(Attacker->GetActorLocation()))
	{
		Target = Attacker;
		LastSensedTarget = Now;
	}
	// the roar of a new phase, or a crab made invulnerable for a test: nothing gets through
	if (Rules->bInvulnerable || (Action.State == EGiantCrabState::Roar && Rules->Roar.bInvulnerable))
	{
		return 0.0f;
	}

	// the part struck (its hit body, its bone, else the body nearest the point): raised claws, a weak point, or its share
	FName ZoneBone;
	const EGiantCrabZone Zone = Bodies ? Bodies->ResolveZone(StruckComponent, Bone, Point, bPoint, ZoneBone)
		: GiantCrabMath::ZoneOfBone(Bone, Rules->LegBoneToken);
	if (ZoneBone.IsNone())
	{
		ZoneBone = Bone;
	}
	float Scale = 1.0f;
	bool bTurnOver = false;
	bool bLagged = false;
	const FGiantCrabWeakPoint* Weak = nullptr;
	if (Action.State == EGiantCrabState::Block && bFront)
	{
		// raised claws take most of a blow from the front
		Scale = Rules->BlockedDamageScale;
	}
	else
	{
		for (const FGiantCrabWeakPoint& Candidate : Rules->WeakPoints)
		{
			const bool bByBone = !ZoneBone.IsNone() && Candidate.Bones.Contains(ZoneBone);
			const bool bByPoint = bPoint && GetMesh()->DoesSocketExist(Candidate.Socket)
				&& FVector::Dist(GetMesh()->GetSocketLocation(Candidate.Socket), Point) <= Candidate.RadiusCm;
			if (bByBone || bByPoint)
			{
				Weak = &Candidate;
				break;
			}
		}
		if (!Weak && HitInfo && Rewind > 0.0f)
		{
			// missed here, but aimed where the player saw a weak point (bounded by the rules)
			Weak = FindLaggedWeakPoint(*HitInfo, Point, Rewind);
			bLagged = Weak != nullptr;
		}
		if (Weak)
		{
			Scale *= Weak->DamageScale;
			// the elbow of a claw stuck in the sand after a slam: one blow turns it over (a distant player's blow counts
			// as late after the window as the player saw it late)
			const bool bStuckClaw = Weak->Socket == Rules->SlamStuckWeakPoint
				&& GiantCrabLag::IsInLateWindow(Now, StuckClawFrom, StuckClawTo, Rewind, Rules->LagWindowSeconds);
			bTurnOver = !bOnBack && (bStuckClaw || (bShot && Weak->bShotFlips)
				|| GiantCrabMath::RecordWeakHit(WeakHits, Now, Rules->FlipWindow, Rules->FlipHits));
		}
		else if (!bOnBack)
		{
			// on its feet, the shell, a leg or a claw takes the rest
			Scale *= Zone == EGiantCrabZone::Leg ? Rules->LegDamageScale : Zone == EGiantCrabZone::Claw ? Rules->ClawDamageScale
				: Zone == EGiantCrabZone::Shell ? Rules->ShellDamageScale : 1.0f;
		}
		if (bOnBack)
		{
			// on its back the soft underside is offered
			Scale *= Rules->FlippedDamageScale;
		}
	}

	// the blow counts for at most the cap of the phase, then the share of the part, then the resistance of the phase
	const FGiantCrabPhaseRules& PhaseRules = CurrentPhase();
	const float Cap = PhaseRules.DamageCap > 0.0f ? PhaseRules.DamageCap : Rules->IncomingDamageCap;
	const float Dealt = Super::TakeDamage(GiantCrabMath::IncomingDamage(DamageAmount, Cap, Scale, PhaseRules.DamageTakenScale, false), DamageEvent,
		EventInstigator, DamageCauser);
	UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: %s on the %s (%s)%s x%.2f -> %.1f, %.0f left%s"), *GetName(), bShot ? TEXT("shot") : TEXT("blow"),
		*UEnum::GetDisplayValueAsText(Zone).ToString(), *ZoneBone.ToString(),
		Weak ? *FString::Printf(TEXT(", weak point %s%s"), *Weak->Socket.ToString(), bLagged ? *FString::Printf(TEXT(" as seen %.0f ms ago"), Rewind * 1000.0f) : TEXT("")) : TEXT(""),
		Scale, Dealt, Health->GetHealth(), bTurnOver ? TEXT(", turned over") : TEXT(""));
	if (IsDead())
	{
		return Dealt;
	}
	// a new phase: its roar comes first
	UpdatePhase();
	if (Action.State == EGiantCrabState::Roar)
	{
		return Dealt;
	}
	if (bTurnOver)
	{
		WeakHits.Reset();
		SetState(EGiantCrabState::Stagger);
	}
	else if (Action.State == EGiantCrabState::Idle || Action.State == EGiantCrabState::Approach || Action.State == EGiantCrabState::Taunt)
	{
		const float BlockChance = PhaseRules.BlockChance >= 0.0f ? PhaseRules.BlockChance : Rules->BlockChance;
		if (bFront && FMath::FRand() < BlockChance)
		{
			SetState(EGiantCrabState::Block);
		}
		else if (Dealt >= Rules->HitReactDamage && Now >= NextHitReact)
		{
			NextHitReact = Now + Rules->HitReactCooldown;
			SetState(EGiantCrabState::HitReact);
		}
	}
	return Dealt;
}

void AGiantCrab::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
{
	const float Scale = Rules ? Rules->KnockbackTakenScale : 1.0f;
	if (Scale > 0.0f)
	{
		Super::LaunchCharacter(LaunchVelocity * Scale, bXYOverride, bZOverride);
	}
}

void AGiantCrab::HandleDeath(AActor* Killer)
{
	LocalDeathTime = GetWorld()->GetTimeSeconds();
	// the eyes cloud over at the notify of the death clip (every machine)
	if (Feedback && Rules)
	{
		const UAnimSequenceBase* DeathClip = Action.bOnBack && Rules->DeathFlipped ? Rules->DeathFlipped.Get() : Rules->Death.Get();
		Feedback->OnDeath(FindNotifyTime(DeathClip, Rules->DeadEyesNotify, Rules->DeadEyesFallbackSeconds));
	}
	if (HasAuthority())
	{
		Target = nullptr;
		SetState(EGiantCrabState::Dead);
		GetCharacterMovement()->DisableMovement();
		UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: dead%s in phase %d, killed by %s"), *GetName(), Action.bOnBack ? TEXT(" on its back") : TEXT(""),
			Phase + 1, Killer ? *Killer->GetName() : TEXT("?"));
	}
	// shots pass through the shell left on the sand; the players still walk around it
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	if (Bodies)
	{
		Bodies->StopBlockingShots();
	}
}

// ---------------------------------------------------------------- boss bar and cues (every machine)

bool AGiantCrab::GetBossBarView(const APawn* Viewer, FBossBarView& OutView) const
{
	if (!Rules || !Rules->bBossBar || !Viewer || !Health)
	{
		return false;
	}
	// while it fights, and a little after its death
	if (IsDead() ? LocalDeathTime < 0.0 || GetWorld()->GetTimeSeconds() - LocalDeathTime > Rules->BossBarLingerSeconds : !bEngaged)
	{
		return false;
	}
	// the players inside its arena, or near it without one
	const FVector Where = Viewer->GetActorLocation();
	const AArenaZone* Zone = Arena.Get();
	if (!(Zone && Zone->IsInside(Where)) && FVector::Dist2D(Where, GetActorLocation()) > Rules->BossBarRadiusCm)
	{
		return false;
	}
	OutView.Name = Rules->BossName;
	OutView.HealthFraction = Health->GetHealthFraction();
	OutView.PhaseMarks.Reset();
	for (int32 Index = 1; Index < Rules->Phases.Num(); ++Index)
	{
		OutView.PhaseMarks.Add(Rules->Phases[Index].HealthFraction);
	}
	OutView.Phase = Phase;
	const FGiantCrabPhaseRules* PhaseRules = GetPhaseRules();
	OutView.bFrenzy = PhaseRules && PhaseRules->bFrenzy;
	OutView.bDead = IsDead();
	return true;
}

void AGiantCrab::MulticastCue_Implementation(EGiantCrabCue Cue, FVector_NetQuantize Location)
{
	if (Feedback && GetNetMode() != NM_DedicatedServer)
	{
		Feedback->PlayCue(Cue, Location);
	}
}
