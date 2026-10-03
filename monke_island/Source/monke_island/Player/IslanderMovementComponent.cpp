// monke_island

#include "Player/IslanderMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "TimerManager.h"
#include "Water/IslandWaterSubsystem.h"

/** Saved move carrying the predicted sprint, guard and flight requests */
class FSavedMove_Islander : public FSavedMove_Character
{
public:

	typedef FSavedMove_Character Super;

	virtual void Clear() override
	{
		Super::Clear();
		bSavedWantsToSprint = false;
		bSavedWantsToGuard = false;
		bSavedWantsToFly = false;
	}

	virtual uint8 GetCompressedFlags() const override
	{
		uint8 Flags = Super::GetCompressedFlags();
		if (bSavedWantsToSprint)
		{
			Flags |= FLAG_Custom_0;
		}
		if (bSavedWantsToGuard)
		{
			Flags |= FLAG_Custom_1;
		}
		if (bSavedWantsToFly)
		{
			Flags |= FLAG_Custom_2;
		}
		return Flags;
	}

	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
	{
		const FSavedMove_Islander* Other = static_cast<const FSavedMove_Islander*>(NewMove.Get());
		if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedWantsToGuard != Other->bSavedWantsToGuard
			|| bSavedWantsToFly != Other->bSavedWantsToFly)
		{
			return false;
		}
		return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
	}

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override
	{
		Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
		if (const UIslanderMovementComponent* Move = Cast<UIslanderMovementComponent>(C->GetCharacterMovement()))
		{
			bSavedWantsToSprint = Move->bWantsToSprint;
			bSavedWantsToGuard = Move->bWantsToGuard;
			bSavedWantsToFly = Move->bWantsToFly;
		}
	}

	virtual void PrepMoveFor(ACharacter* C) override
	{
		Super::PrepMoveFor(C);
		if (UIslanderMovementComponent* Move = Cast<UIslanderMovementComponent>(C->GetCharacterMovement()))
		{
			Move->bWantsToSprint = bSavedWantsToSprint;
			Move->bWantsToGuard = bSavedWantsToGuard;
			Move->bWantsToFly = bSavedWantsToFly;
		}
	}

	uint8 bSavedWantsToSprint : 1 = false;
	uint8 bSavedWantsToGuard : 1 = false;
	uint8 bSavedWantsToFly : 1 = false;
};

/** Client prediction data allocating islander saved moves */
class FNetworkPredictionData_Client_Islander : public FNetworkPredictionData_Client_Character
{
public:

	explicit FNetworkPredictionData_Client_Islander(const UCharacterMovementComponent& ClientMovement)
		: FNetworkPredictionData_Client_Character(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_Islander());
	}
};

UIslanderMovementComponent::UIslanderMovementComponent()
{
	// real-world physics: 1 unit = 1 cm, gravity 9.8 m/s^2, an adult of 75 kg
	GravityScale = 1.0f;
	Mass = 75.0f;

	// on foot: jog at ~3.5 m/s, starts and stops like a person rather than a hovercraft
	MaxWalkSpeed = 350.0f;
	MaxWalkSpeedCrouched = 150.0f;

	// crouching (C / Ctrl): a lower, slower and quieter stance, as in Lethal Company
	NavAgentProps.bCanCrouch = true;
	SetCrouchedHalfHeight(60.0f);
	bCanWalkOffLedgesWhenCrouching = true;
	MaxAcceleration = 1200.0f;
	BrakingDecelerationWalking = 1600.0f;
	GroundFriction = 8.0f;
	MaxStepHeight = 40.0f;
	SetWalkableFloorAngle(45.0f);

	// standing jump of ~0.55 m (v^2 / 2g), little steering once airborne
	JumpZVelocity = 330.0f;
	AirControl = 0.2f;

	// third person: the body turns toward where it walks
	bOrientRotationToMovement = true;
	RotationRate = FRotator(0.0f, 540.0f, 0.0f);

	// swimming at ~1.2 m/s, slightly positive buoyancy keeps the head above water
	MaxSwimSpeed = 120.0f;
	BrakingDecelerationSwimming = 300.0f;
	Buoyancy = 1.2f;

	// flight (double tap jump): fast but stops like a body in the air, not a drone
	MaxFlySpeed = 1000.0f;
	BrakingDecelerationFlying = 1500.0f;

	// pushes physics objects with a force proportional to their mass
	bEnablePhysicsInteraction = true;
	bPushForceScaledToMass = true;
}

void UIslanderMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	WaterSubsystem = UIslandWaterSubsystem::Get(this);
	Stamina = MaxStamina;
	DefaultBuoyancy = Buoyancy;
}

FNetworkPredictionData_Client* UIslanderMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		UIslanderMovementComponent* MutableThis = const_cast<UIslanderMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Islander(*this);
	}
	return ClientPredictionData;
}

void UIslanderMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToGuard = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bWantsToFly = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
}

void UIslanderMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// stamina runs on the server and on the owning client (simulated proxies only display)
	if (CharacterOwner && CharacterOwner->GetLocalRole() >= ROLE_AutonomousProxy)
	{
		if (MovementMode != MOVE_None)
		{
			UpdateStamina(DeltaTime, IsSprinting(), IsSwimming(), Velocity.SizeSquared() > 100.0f);
		}
		else if (CharacterOwner->GetAttachParentActor())
		{
			// seated in a boat, rowing included: rests like standing still (ADR-003)
			UpdateStamina(DeltaTime, false, false, false);
		}

		// out of stamina in the water: the body sinks, then drowning hurts (ADR-003 B-04)
		const bool bDrowning = IsDrowning();
		Buoyancy = bDrowning ? DrowningBuoyancy : DefaultBuoyancy;
		const float Damage = UpdateDrowning(DeltaTime, bDrowning);
		if (Damage > 0.0f && CharacterOwner->HasAuthority())
		{
			if (UIslanderHealthComponent* Health = CharacterOwner->FindComponentByClass<UIslanderHealthComponent>())
			{
				Health->ApplyHealthDamage(Damage, nullptr);
			}
		}
	}
}

float UIslanderMovementComponent::UpdateDrowning(float DeltaSeconds, bool bDrowning)
{
	if (!bDrowning)
	{
		DrownTime = 0.0f;
		return 0.0f;
	}
	const float Before = DrownTime;
	DrownTime += DeltaSeconds;
	const float HurtTime = FMath::Max(0.0f, DrownTime - FMath::Max(Before, DrownGraceTime));
	return HurtTime * DrownDamagePerSecond;
}

bool UIslanderMovementComponent::IsSprinting() const
{
	const bool bCanSprint = bWantsToSprint && !bExhausted && Stamina > 0.0f && !IsCrouching() && !bWantsToGuard;
	return bCanSprint && (IsMovingOnGround() || IsSwimming()) && Velocity.SizeSquared() > 100.0f;
}

bool UIslanderMovementComponent::IsGuarding() const
{
	return bWantsToGuard && (IsMovingOnGround() || IsFalling());
}

bool UIslanderMovementComponent::ConsumeStamina(float Amount)
{
	if (Amount <= 0.0f)
	{
		return true;
	}
	if (Stamina < Amount)
	{
		return false;
	}

	Stamina -= Amount;
	TimeSinceStaminaSpent = 0.0f;
	if (Stamina <= KINDA_SMALL_NUMBER)
	{
		Stamina = 0.0f;
		bExhausted = true;
	}
	return true;
}

void UIslanderMovementComponent::UpdateStamina(float DeltaSeconds, bool bSprinting, bool bSwimming, bool bMoving)
{
	const float SwimCost = bMoving ? SwimStaminaCost : SwimIdleStaminaCost;
	float Cost = (bSprinting ? SprintStaminaCost : 0.0f) + (bSwimming ? SwimCost : 0.0f);
	Cost *= 1.0f + CarriedWeight * StaminaCostPerKg;

	if (Cost > 0.0f)
	{
		Stamina = FMath::Max(0.0f, Stamina - Cost * DeltaSeconds);
		TimeSinceStaminaSpent = 0.0f;
		if (Stamina <= 0.0f)
		{
			bExhausted = true;
		}
	}
	else
	{
		TimeSinceStaminaSpent += DeltaSeconds;
		if (TimeSinceStaminaSpent >= StaminaRegenDelay)
		{
			const float Regen = bMoving ? WalkStaminaRegen : IdleStaminaRegen;
			Stamina = FMath::Min(MaxStamina, Stamina + Regen * DeltaSeconds);
		}
	}

	if (bExhausted && Stamina >= MaxStamina * ExhaustedRecoverFraction)
	{
		bExhausted = false;
	}
}

float UIslanderMovementComponent::GetWeightSpeedFactor() const
{
	return FMath::Max(MinWeightSpeedFactor, 1.0f - CarriedWeight * SpeedLossPerKg);
}

bool UIslanderMovementComponent::CanAttemptJump() const
{
	// at the surface a swimmer can leap out of the water, e.g. into a boat (ADR-003)
	if (IsSwimming())
	{
		return IsJumpAllowed() && Stamina >= JumpStaminaCost && ImmersionDepth() <= WaterJumpMaxImmersion;
	}
	// flying uses jump to rise; on foot a jump needs stamina (Lethal Company)
	if (IsMovingOnGround() && Stamina < JumpStaminaCost)
	{
		return false;
	}
	return Super::CanAttemptJump();
}

bool UIslanderMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	if (IsSwimming())
	{
		if (!CharacterOwner || !CharacterOwner->CanJump())
		{
			return false;
		}
		// leap out of the water toward where the swimmer goes
		FVector Direction = Velocity.GetSafeNormal2D();
		if (Direction.IsNearlyZero())
		{
			Direction = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
		}
		Velocity.Z = WaterJumpZVelocity;
		Velocity += Direction * WaterJumpForwardSpeed;
		bWaterJumping = true;
		SetMovementMode(MOVE_Falling);
		if (!bReplayingMoves)
		{
			ConsumeStamina(FMath::Min(JumpStaminaCost, Stamina));
		}
		return true;
	}

	const bool bFromGround = IsMovingOnGround();
	if (!Super::DoJump(bReplayingMoves, DeltaTime))
	{
		return false;
	}
	if (bFromGround && !bReplayingMoves)
	{
		ConsumeStamina(FMath::Min(JumpStaminaCost, Stamina));
	}
	return true;
}

float UIslanderMovementComponent::GetWaterDepthAtFeet() const
{
	if (!WaterSubsystem || !CharacterOwner || !UpdatedComponent)
	{
		return 0.0f;
	}

	FVector Feet = UpdatedComponent->GetComponentLocation();
	Feet.Z -= CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	// flood included: the local water surface can be above the sea level
	return WaterSubsystem->GetWaterDepthAt(Feet);
}

float UIslanderMovementComponent::GetMaxSpeed() const
{
	float Speed = Super::GetMaxSpeed();

	switch (MovementMode)
	{
	case MOVE_Walking:
	case MOVE_NavWalking:
		{
			if (bWantsToSprint && !bExhausted && Stamina > 0.0f && !IsCrouching() && !bWantsToGuard)
			{
				Speed = MaxSprintSpeed;
			}
			if (bExhausted)
			{
				Speed *= ExhaustedSpeedFactor;
			}
			if (bWantsToGuard)
			{
				Speed *= GuardSpeedFactor;
			}
			Speed *= GetWeightSpeedFactor();

			// wading: the water drags the legs, more and more until the character swims
			const float Depth = GetWaterDepthAtFeet();
			if (Depth > 0.0f && SwimStartDepth > 0.0f)
			{
				Speed *= FMath::Lerp(1.0f, WadingMinSpeedFactor, FMath::Clamp(Depth / SwimStartDepth, 0.0f, 1.0f));
			}
			break;
		}

	case MOVE_Swimming:
		if (bExhausted)
		{
			Speed *= ExhaustedSwimSpeedFactor;
		}
		else if (bWantsToSprint && Stamina > 0.0f)
		{
			Speed *= SprintSwimSpeedMultiplier;
		}
		Speed *= GetWeightSpeedFactor();
		break;

	case MOVE_Flying:
		if (bWantsToSprint)
		{
			Speed *= SprintFlySpeedMultiplier;
		}
		break;

	default:
		break;
	}

	return Speed;
}

bool UIslanderMovementComponent::IsInWater() const
{
	if (Super::IsInWater())
	{
		return true;
	}

	// a swimmer only stands up once the water is clearly shallower than the swimming depth
	const float Threshold = IsSwimming() ? SwimStartDepth - SwimExitHysteresis : SwimStartDepth;
	return GetWaterDepthAtFeet() >= Threshold;
}

float UIslanderMovementComponent::ImmersionDepth() const
{
	// regular water volumes keep the engine behavior
	if (Super::IsInWater() || !CharacterOwner)
	{
		return Super::ImmersionDepth();
	}

	// fraction of the body below the sea surface, drives buoyancy and water friction while swimming
	const float BodyHeight = 2.0f * CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return BodyHeight > 0.0f ? FMath::Clamp(GetWaterDepthAtFeet() / BodyHeight, 0.0f, 1.0f) : 0.0f;
}

void UIslanderMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	// predicted flight request (double tap on jump): applied identically on the owning client and the server
	if (CharacterOwner && MovementMode != MOVE_None)
	{
		if (bWantsToFly && IsSwimming())
		{
			bWantsToFly = false;
		}
		else if (bWantsToFly && !IsFlying())
		{
			// hover where the request happened instead of keeping the fall
			Velocity.Z = FMath::Max(Velocity.Z, 0.0);
			SetMovementMode(MOVE_Flying);
		}
		else if (!bWantsToFly && IsFlying())
		{
			SetMovementMode(MOVE_Falling);
		}
	}

	// engine water volumes switch modes themselves through PhysicsVolumeChanged; disabled movement (ragdoll) stays disabled
	if (!CharacterOwner || !CanEverSwim() || Super::IsInWater() || MovementMode == MOVE_None)
	{
		return;
	}

	// a jump out of the water does not fall back into swimming while it rises
	if (bWaterJumping && (!IsFalling() || Velocity.Z <= 0.0))
	{
		bWaterJumping = false;
	}
	const bool bInSea = IsInWater();
	if (bInSea && !IsSwimming() && !bWaterJumping)
	{
		SetMovementMode(MOVE_Swimming);
	}
	else if (!bInSea && IsSwimming())
	{
		// back to shallow water: fall onto the bottom and walk again
		SetMovementMode(MOVE_Falling);
	}
}

void UIslanderMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	// PhysSwimming drops to falling whenever the physics volume is not a water volume:
	// keep swimming while the sea level says the character is still in the sea
	if (PreviousMovementMode == MOVE_Swimming && MovementMode == MOVE_Falling && !bWaterJumping && !Super::IsInWater() && IsInWater())
	{
		SetMovementMode(MOVE_Swimming);
	}
}

void UIslanderMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	// velocity is still the pre-impact one here: measure how hard the body hits the surface
	const float ImpactSpeed = static_cast<float>(-(Velocity | Hit.ImpactNormal));
	const bool bCrash = IsFlying() && FlyingCrashSpeed > 0.0f && ImpactSpeed >= FlyingCrashSpeed;

	// pushes the physics object that was hit
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);

	// the ragdoll starts next tick, outside of the movement step, with the speed of the crash
	AIslanderCharacter* Islander = Cast<AIslanderCharacter>(CharacterOwner);
	if (bCrash && Islander && !bCrashPending)
	{
		bCrashPending = true;
		bWantsToFly = false;
		const FVector CrashVelocity = Velocity;
		DisableMovement();

		TWeakObjectPtr<AIslanderCharacter> WeakIslander(Islander);
		TWeakObjectPtr<UIslanderMovementComponent> WeakThis(this);
		GetWorld()->GetTimerManager().SetTimerForNextTick([WeakIslander, WeakThis, CrashVelocity]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->bCrashPending = false;
			}
			if (WeakIslander.IsValid())
			{
				WeakIslander->StartRagdoll(CrashVelocity);
			}
		});
	}
}
