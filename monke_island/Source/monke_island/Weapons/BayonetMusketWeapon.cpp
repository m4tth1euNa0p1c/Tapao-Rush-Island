// monke_island

#include "Weapons/BayonetMusketWeapon.h"
#include "Weapons/BayonetMath.h"
#include "Weapons/BayonetMusketRules.h"
#include "Weapons/FirearmDamage.h"
#include "monke_island.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Sound/SoundBase.h"

namespace
{
	/** Root motion source of the lunge */
	const FName BayonetLungeName(TEXT("BayonetLunge"));

	bool IsThrustGesture(EFirearmGesture Gesture)
	{
		return Gesture == EFirearmGesture::BayonetStab || Gesture == EFirearmGesture::BayonetCharge;
	}

	/** One target the point may strike */
	struct FThrustCandidate
	{
		TWeakObjectPtr<AActor> Target;
		TWeakObjectPtr<UPrimitiveComponent> Component;
		FVector Point = FVector::ZeroVector;
		float Score = 0.0f;
		/** A living creature (a corpse or a loose body comes after the living ones) */
		bool bAlive = false;
	};
}

ABayonetMusketWeapon::ABayonetMusketWeapon()
{
}

const UBayonetMusketRules* ABayonetMusketWeapon::GetBayonetRules() const
{
	return Cast<UBayonetMusketRules>(Rules);
}

void ABayonetMusketWeapon::BeginPlay()
{
	if (Rules && !GetBayonetRules())
	{
		UE_LOG(Logmonke_island, Error, TEXT("Bayonet musket %s: %s is not a UBayonetMusketRules, the weapon is disabled"), *GetName(), *Rules->GetName());
		Rules = nullptr;
	}
	Super::BeginPlay();
}

bool ABayonetMusketWeapon::IsThrusting() const
{
	return IsThrustGesture(GetServerGesture());
}

bool ABayonetMusketWeapon::LowersAim(EFirearmGesture Gesture) const
{
	return IsThrustGesture(Gesture) || Super::LowersAim(Gesture);
}

bool ABayonetMusketWeapon::PredictFire() const
{
	return !IsThrustGesture(GetLocalGesture()) && Super::PredictFire();
}

// ---------------------------------------------------------------- input (carrier)

void ABayonetMusketWeapon::Tick(float DeltaSeconds)
{
	if (IsCarrierLocallyControlled())
	{
		BindThrust();
	}
	Super::Tick(DeltaSeconds);
}

void ABayonetMusketWeapon::BindThrust()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UEnhancedInputComponent* Input = Carrier ? Cast<UEnhancedInputComponent>(Carrier->InputComponent) : nullptr;
	// the right click when no key of its own: the musket is fired from the hip (bAllowAiming off), its aim key is free
	const UInputAction* ThrustKey = ThrustAction ? ThrustAction.Get() : AimAction.Get();
	if (!Input || !ThrustKey || ThrustInput.Get() == Input)
	{
		return;
	}
	KeepInputBinding(Input, Input->BindAction(ThrustKey, ETriggerEvent::Started, this, &ABayonetMusketWeapon::HandleThrustInput).GetHandle());
	ThrustInput = Input;
}

void ABayonetMusketWeapon::OnInputBindingsRemoved()
{
	Super::OnInputBindingsRemoved();
	// the binding went with the others: bound again for the next carrier
	ThrustInput.Reset();
}

void ABayonetMusketWeapon::HandleThrustInput()
{
	// keys of a former carrier never drive the item someone else now holds (T-288, network review R-37)
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	RequestThrust();
}

bool ABayonetMusketWeapon::WantsCharge(float SpeedShare) const
{
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	const AIslanderCharacter* Carrier = GetCarrier();
	const UIslanderMovementComponent* Move = Carrier ? Carrier->GetIslanderMovement() : nullptr;
	if (!Bayonet || !Move)
	{
		return false;
	}
	return BayonetMath::IsCharge(Move->IsSprinting(), static_cast<float>(Carrier->GetVelocity().Size2D()),
		Bayonet->ChargeMinSpeedCmps * FMath::Clamp(SpeedShare, 0.0f, 1.0f));
}

bool ABayonetMusketWeapon::HasStaminaFor(const FBayonetThrust& Thrust) const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const UIslanderMovementComponent* Move = Carrier ? Carrier->GetIslanderMovement() : nullptr;
	return Move && BayonetMath::HasStaminaFor(Move->GetStamina(), Move->MaxStamina, Thrust.StaminaCostFraction);
}

void ABayonetMusketWeapon::RequestThrust()
{
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	AIslanderCharacter* Carrier = GetCarrier();
	// the fire key and the right click are shared by the firearms (IMC_Musket): only the musket in hand listens
	if (!Bayonet || !Carrier || !IsUsable())
	{
		return;
	}
	const bool bCharge = WantsCharge(1.0f);
	if (HasAuthority())
	{
		ServerThrust_Implementation(bCharge);
		return;
	}
	// the carrier shows its thrust at once when the server will take it: the same checks on the replicated gesture, and
	// no second thrust over the one it shows already
	const FBayonetThrust& Thrust = Bayonet->GetThrust(bCharge);
	if (IsThrustGesture(GetLocalGesture()) || !BayonetMath::MayThrust(GetServerGesture(), Bayonet->ReloadGesture, Bayonet->bThrustCutsReload)
		|| !HasStaminaFor(Thrust))
	{
		return;
	}
	const EFirearmGesture Gesture = bCharge ? EFirearmGesture::BayonetCharge : EFirearmGesture::BayonetStab;
	if (Bayonet->FindClip(Gesture))
	{
		PredictGesture(Gesture, 0.0f);
	}
	// the stamina is simulated by the server and by the carrier: both spend it (as the cutlass's blows)
	if (UIslanderMovementComponent* Move = Carrier->GetIslanderMovement())
	{
		Move->ConsumeStamina(BayonetMath::StaminaCost(Move->MaxStamina, Thrust.StaminaCostFraction));
	}
	FaceThrust();
	BeginLunge(Thrust);
	ServerThrust(bCharge);
}

// ---------------------------------------------------------------- server

void ABayonetMusketWeapon::ServerThrust_Implementation(bool bCharge)
{
	// a server body run without authority is a call that missed the network (see AFirearmWeapon::ServerFire_Implementation)
	if (!HasAuthority())
	{
		return;
	}
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	AIslanderCharacter* Carrier = GetCarrier();
	UIslanderMovementComponent* Move = Carrier ? Carrier->GetIslanderMovement() : nullptr;
	// the charge only for a carrier that runs (slack: its speed changed a little since the press), else the thrust
	const bool bChargeNow = bCharge && Bayonet && WantsCharge(Bayonet->ChargeSpeedTolerance);
	const FBayonetThrust* Thrust = Bayonet ? &Bayonet->GetThrust(bChargeNow) : nullptr;
	const EFirearmGesture Playing = GetServerGesture();
	// one thrust per gesture: a burst of requests is refused while one plays (the stamina bounds them too)
	if (!Thrust || !Move || !IsUsable() || !BayonetMath::MayThrust(Playing, Bayonet->ReloadGesture, Bayonet->bThrustCutsReload)
		|| !HasStaminaFor(*Thrust))
	{
		if (!IsCarrierLocallyControlled())
		{
			ClientRejectThrust(bCharge);
		}
		return;
	}
	// the inspection or the reload is cut (a cut reload keeps the ball in the pouch)
	if (Playing != EFirearmGesture::None)
	{
		StopAction();
	}
	Move->ConsumeStamina(BayonetMath::StaminaCost(Move->MaxStamina, Thrust->StaminaCostFraction));
	bServerCharge = bChargeNow;
	bStruck = false;
	FaceThrust();
	BeginLunge(*Thrust);
	const EFirearmGesture Gesture = bChargeNow ? EFirearmGesture::BayonetCharge : EFirearmGesture::BayonetStab;
	UE_LOG(Logmonke_island, Log, TEXT("Bayonet musket %s: %s %s (stamina %.0f)"), *GetName(), *Carrier->GetName(),
		bChargeNow ? TEXT("charges") : TEXT("thrusts"), Move->GetStamina());
	if (!StartAction(Gesture))
	{
		// no clip for the gesture in the rules: the point strikes at once rather than never
		Strike();
	}
}

void ABayonetMusketWeapon::OnActionEvent(EFirearmGesture Gesture, FName Event)
{
	// the musket's own events (its ball leaves at Shot)
	Super::OnActionEvent(Gesture, Event);
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	if (Bayonet && IsThrustGesture(Gesture) && Event == Bayonet->GetThrust(Gesture == EFirearmGesture::BayonetCharge).HitEvent)
	{
		Strike();
	}
}

void ABayonetMusketWeapon::OnActionFinished(EFirearmGesture Gesture)
{
	Super::OnActionFinished(Gesture);
	if (IsThrustGesture(Gesture))
	{
		// a clip without its hit event in the DA still strikes, at its end
		Strike();
	}
}

void ABayonetMusketWeapon::OnActionStopped(EFirearmGesture Gesture)
{
	Super::OnActionStopped(Gesture);
	if (IsThrustGesture(Gesture))
	{
		// cut before the point arrived (put away, swim, knocked out): it strikes nothing
		bStruck = true;
	}
}

void ABayonetMusketWeapon::Strike()
{
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	AIslanderCharacter* Carrier = GetCarrier();
	UWorld* World = GetWorld();
	if (bStruck || !HasAuthority() || !Bayonet || !Carrier || !World)
	{
		return;
	}
	bStruck = true;
	const FBayonetThrust& Thrust = Bayonet->GetThrust(bServerCharge);
	const FVector Origin = Carrier->GetActorLocation();
	FVector Forward = GetCarrierAim().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = Carrier->GetActorForwardVector();
	}
	const UCameraComponent* Camera = Carrier->GetFirstPersonCamera();
	const FVector Eyes = Camera ? Camera->GetComponentLocation() : Carrier->GetPawnViewLocation();

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& Type : Bayonet->HitObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(Type));
	}
	if (!ObjectParams.IsValid())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Bayonet musket %s: %s has no HitObjectTypes, the point strikes nothing"), *GetName(), *Bayonet->GetName());
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BayonetThrust), false, Carrier);
	Params.AddIgnoredActor(this);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Thrust.ReachCm + Bayonet->HeightToleranceCm),
		Params);

	// the bodies in the short cone in front of the carrier, by the nearest point of each (a big crab's leg is within reach
	// while its centre is not), the one right in front of the point first
	TArray<FThrustCandidate, TInlineAllocator<8>> Candidates;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Target || !Component || Target == Carrier || Target == this)
		{
			continue;
		}
		FVector Point;
		if (Component->GetClosestPointOnCollision(Origin, Point) < 0.0f)
		{
			Point = Component->GetComponentLocation();
		}
		const float Score = BayonetMath::ThrustScore(Forward, Point - Origin, Thrust.ReachCm, Thrust.HalfAngleDegrees, Bayonet->HeightToleranceCm);
		if (Score < 0.0f)
		{
			continue;
		}
		if (FThrustCandidate* Same = Candidates.FindByPredicate([Target](const FThrustCandidate& Item) { return Item.Target.Get() == Target; }))
		{
			if (Score < Same->Score)
			{
				Same->Component = Component;
				Same->Point = Point;
				Same->Score = Score;
			}
			continue;
		}
		const UIslanderHealthComponent* Health = Target->FindComponentByClass<UIslanderHealthComponent>();
		FThrustCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Target = Target;
		Candidate.Component = Component;
		Candidate.Point = Point;
		Candidate.Score = Score;
		Candidate.bAlive = Health && !Health->IsDead();
	}
	// the living first (a corpse in front does not take the point from the enemy behind it), the nearest to the axis first
	Candidates.Sort([](const FThrustCandidate& A, const FThrustCandidate& B)
	{
		return A.bAlive != B.bAlive ? A.bAlive : A.Score < B.Score;
	});

	int32 Struck = 0;
	bool bAnyKill = false;
	FVector FirstPoint = FVector::ZeroVector;
	for (const FThrustCandidate& Candidate : Candidates)
	{
		AActor* Target = Candidate.Target.Get();
		UPrimitiveComponent* Component = Candidate.Component.Get();
		if (Struck >= Thrust.MaxTargets)
		{
			break;
		}
		if (!Target || !Component)
		{
			continue;
		}
		const APawn* TargetPawn = Cast<APawn>(Target);
		if (TargetPawn && TargetPawn->IsPlayerControlled() && !Bayonet->bFriendlyFire)
		{
			continue;
		}
		// no thrust through a wall: a free line from the eyes to the point
		FCollisionQueryParams SightParams(SCENE_QUERY_STAT(BayonetSight), false, Carrier);
		SightParams.AddIgnoredActor(this);
		SightParams.AddIgnoredActor(Target);
		if (World->LineTraceTestByChannel(Eyes, Candidate.Point, Bayonet->SightChannel, SightParams))
		{
			continue;
		}

		// the blow, as the cutlass deals it (point damage, this weapon the causer): the struck bone is the one the line of
		// the point passes nearest to, from the eyes through where it enters
		FVector Direction = (Candidate.Point - Origin).GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = Forward;
		}
		const FVector LineEnd = Candidate.Point + (Candidate.Point - Eyes).GetSafeNormal() * Bayonet->ThrustBoneRadiusCm;
		FHitResult Hit(Target, Component, Candidate.Point, -Direction);
		Hit.TraceStart = Eyes;
		Hit.TraceEnd = LineEnd;
		Hit.Distance = static_cast<float>(FVector::Dist(Eyes, Candidate.Point));
		Hit.bBlockingHit = true;
		ACharacter* HitCharacter = Cast<ACharacter>(Target);
		if (HitCharacter && HitCharacter->GetMesh())
		{
			const FName Bone = FirearmDamage::FindStruckBone(HitCharacter->GetMesh(), Eyes, LineEnd, Bayonet->ThrustBoneRadiusCm);
			if (!Bone.IsNone())
			{
				Hit.Component = HitCharacter->GetMesh();
				Hit.BoneName = Bone;
			}
		}
		const UIslanderHealthComponent* Health = Target->FindComponentByClass<UIslanderHealthComponent>();
		const bool bWasAlive = !Health || !Health->IsDead();
		UGameplayStatics::ApplyPointDamage(Target, Thrust.Damage, Direction, Hit, Carrier->GetController(), this, UDamageType::StaticClass());
		const bool bKill = bWasAlive && Health && Health->IsDead();
		if (HitCharacter && !bKill)
		{
			HitCharacter->LaunchCharacter(Direction * Thrust.KnockbackSpeed + FVector(0.0f, 0.0f, Thrust.KnockbackUpSpeed), true, false);
		}
		else if (IsValid(Component) && Component->IsSimulatingPhysics())
		{
			Component->AddImpulseAtLocation(Direction * Thrust.PhysicsImpulse, Candidate.Point);
		}
		if (Struck == 0)
		{
			FirstPoint = Candidate.Point;
		}
		++Struck;
		bAnyKill |= bKill;
		UE_LOG(Logmonke_island, Log, TEXT("Bayonet musket %s: %s %s %s for %.0f (bone %s)%s"), *GetName(), *Carrier->GetName(),
			bServerCharge ? TEXT("charges into") : TEXT("stabs"), *Target->GetName(), Thrust.Damage, *Hit.BoneName.ToString(), bKill ? TEXT(", kill") : TEXT(""));
	}
	if (Struck == 0)
	{
		return;
	}
	MulticastThrustSound(FirstPoint, bAnyKill);
	if (IsCarrierLocallyControlled())
	{
		PlayStruckFeel(bServerCharge, bAnyKill);
	}
	else
	{
		ClientThrustStruck(bServerCharge, bAnyKill);
	}
}

// ---------------------------------------------------------------- the body (server and carrier)

void ABayonetMusketWeapon::FaceThrust()
{
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	AIslanderCharacter* Carrier = GetCarrier();
	if (!Bayonet || !Bayonet->bFaceThrust || !Carrier || Carrier->bUseControllerRotationYaw)
	{
		// first person: the body follows the view already
		return;
	}
	FRotator Rotation = Carrier->GetActorRotation();
	Rotation.Yaw = GetCarrierAim().Rotation().Yaw;
	Carrier->SetActorRotation(Rotation);
}

float ABayonetMusketWeapon::NearestEnemyAhead(const FVector& Forward, float Range) const
{
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	const AIslanderCharacter* Carrier = GetCarrier();
	const UWorld* World = GetWorld();
	if (!Bayonet || !Carrier || !World || Range <= 0.0f)
	{
		return -1.0f;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BayonetLunge), false, Carrier);
	Params.AddIgnoredActor(this);
	TArray<FOverlapResult> Overlaps;
	const FVector Origin = Carrier->GetActorLocation();
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Range), Params);
	float Nearest = -1.0f;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const APawn* Pawn = Cast<APawn>(Overlap.GetActor());
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		// never toward a friend; the dead are no longer in the way
		if (!Pawn || !Component || Pawn->IsPlayerControlled())
		{
			continue;
		}
		if (const UIslanderHealthComponent* Health = Pawn->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
		{
			continue;
		}
		FVector Point;
		if (Component->GetClosestPointOnCollision(Origin, Point) < 0.0f)
		{
			Point = Component->GetComponentLocation();
		}
		const FVector Offset = Point - Origin;
		const FVector Flat(Offset.X, Offset.Y, 0.0f);
		if (FMath::Abs(Offset.Z) > Bayonet->HeightToleranceCm || FVector::DotProduct(Flat, Forward) <= 0.0f)
		{
			continue;
		}
		const float Distance = static_cast<float>(Flat.Size());
		Nearest = Nearest < 0.0f ? Distance : FMath::Min(Nearest, Distance);
	}
	return Nearest;
}

void ABayonetMusketWeapon::BeginLunge(const FBayonetThrust& Thrust)
{
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	const AIslanderCharacter* Carrier = GetCarrier();
	UCharacterMovementComponent* Movement = Carrier ? Carrier->GetCharacterMovement() : nullptr;
	if (!Bayonet || !Movement || !Movement->IsMovingOnGround() || Thrust.LungeDistanceCm <= 1.0f)
	{
		return;
	}
	FVector Forward = GetCarrierAim().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = Carrier->GetActorForwardVector();
	}
	// a step toward the point, none into an enemy already at it
	const float Distance = BayonetMath::LungeDistance(Thrust.LungeDistanceCm,
		NearestEnemyAhead(Forward, Thrust.LungeDistanceCm + Bayonet->LungeBlockDistanceCm), Bayonet->LungeBlockDistanceCm);
	if (Distance <= 1.0f)
	{
		return;
	}
	const float Seconds = FMath::Max(Thrust.LungeSeconds, 0.01f);
	Movement->RemoveRootMotionSource(BayonetLungeName);
	TSharedPtr<FRootMotionSource_ConstantForce> Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->InstanceName = BayonetLungeName;
	Source->AccumulateMode = ERootMotionAccumulateMode::Additive;
	Source->Priority = 5;
	Source->Force = Forward * (Distance / Seconds);
	Source->Duration = Seconds;
	// a charge does not fling the carrier on at the end of its lunge
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Source->FinishVelocityParams.ClampVelocity = Bayonet->LungeExitSpeedCmps;
	Movement->ApplyRootMotionSource(Source);
}

// ---------------------------------------------------------------- carrier

void ABayonetMusketWeapon::ClientRejectThrust_Implementation(bool bCharge)
{
	// back to the carry; the stamina and the position come back with the server's corrections
	CancelPredictedGesture(bCharge ? EFirearmGesture::BayonetCharge : EFirearmGesture::BayonetStab);
	if (const AIslanderCharacter* Carrier = GetCarrier())
	{
		if (UCharacterMovementComponent* Movement = Carrier->GetCharacterMovement())
		{
			Movement->RemoveRootMotionSource(BayonetLungeName);
		}
	}
}

void ABayonetMusketWeapon::ClientThrustStruck_Implementation(bool bCharge, bool bKill)
{
	PlayStruckFeel(bCharge, bKill);
}

void ABayonetMusketWeapon::PlayStruckFeel(bool bCharge, bool bKill)
{
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Bayonet || !Carrier || !Bayonet->HitCameraShake || !IsCarrierLocallyControlled())
	{
		return;
	}
	const APlayerController* Controller = Cast<APlayerController>(Carrier->GetController());
	if (Controller && Controller->PlayerCameraManager)
	{
		const float Scale = Bayonet->GetThrust(bCharge).HitShakeScale * (bKill ? Bayonet->KillShakeMultiplier : 1.0f);
		Controller->PlayerCameraManager->StartCameraShake(Bayonet->HitCameraShake, Scale);
	}
}

void ABayonetMusketWeapon::MulticastThrustSound_Implementation(FVector_NetQuantize Location, bool bKill)
{
	const UBayonetMusketRules* Bayonet = GetBayonetRules();
	if (!Bayonet || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USoundBase* Sound = bKill && Bayonet->ThrustKillSound ? Bayonet->ThrustKillSound.Get() : Bayonet->ThrustHitSound.Get();
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, 1.0f, FMath::FRandRange(0.94f, 1.06f), 0.0f, Bayonet->EventSoundAttenuation);
	}
}
