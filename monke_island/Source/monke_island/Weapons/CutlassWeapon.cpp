// monke_island

#include "Weapons/CutlassWeapon.h"
#include "Weapons/CutlassMath.h"

#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/MusketMath.h"
#include "monke_island.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Character/Bean/BeanAnimDriverComponent.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "EnhancedInputComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
	UStaticMeshComponent* MakeCutlassMesh(AActor* Owner, FName Name)
	{
		UStaticMeshComponent* Mesh = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetHiddenInGame(true);
		return Mesh;
	}

	/** Server tolerance on the cancel window of a move: the carrier's clock runs a little ahead of the server's (s) */
	constexpr double MoveEndTolerance = 0.12;

	/** Server tolerance on the finisher charge (s) */
	constexpr double ChargeTolerance = 0.15;

	/** Life of a damage number (s) */
	constexpr double HitMarkerLife = 1.2;

	/** Enemies farther above or below the bean's centre are out of the arc and of the soft aim (cm) */
	constexpr float ArcHeightTolerance = 160.0f;

	/** First person soft aim: the camera is released once this close to the enemy (deg) */
	constexpr float AimAssistDoneDegrees = 0.5f;

	/** Root motion source of the lunge */
	const FName LungeSourceName(TEXT("CutlassLunge"));
}

ACutlassWeapon::ACutlassWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	// after the puppet copied the walk pose, before the first person arms read their override
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FirstPersonMesh = MakeCutlassMesh(this, TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCastShadow(false);

	ThirdPersonMesh = MakeCutlassMesh(this, TEXT("ThirdPersonMesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);

	ScabbardMesh = MakeCutlassMesh(this, TEXT("ScabbardMesh"));
	ScabbardMesh->SetupAttachment(RootComponent);
}

void ACutlassWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACutlassWeapon, MoveState, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ACutlassWeapon, bParrying, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ACutlassWeapon, Sharpness, COND_OwnerOnly);
}

void ACutlassWeapon::BeginPlay()
{
	SpawnTime = GetWorld()->GetTimeSeconds();
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Error, TEXT("Cutlass %s: no DA_CutlassRules, the cutlass is disabled"), *GetName());
		Super::BeginPlay();
		SetActorTickEnabled(false);
		return;
	}
	FirstPersonMesh->SetStaticMesh(Rules->CutlassMesh);
	ThirdPersonMesh->SetStaticMesh(Rules->CutlassMesh);
	ScabbardMesh->SetStaticMesh(Rules->ScabbardMesh);
	CurrentPose = Rules->SheathedKey;

	Super::BeginPlay();
}

void ACutlassWeapon::OnAttachedToCarrier()
{
	AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !Rules)
	{
		return;
	}

	Arms = Carrier->FindComponentByClass<UBeanFirstPersonArmsComponent>();
	Puppet = Carrier->FindComponentByClass<UBeanPuppetComponent>();
	AnimDriver = Carrier->FindComponentByClass<UBeanAnimDriverComponent>();

	if (UCameraComponent* Camera = Carrier->GetFirstPersonCamera())
	{
		FirstPersonMesh->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	// keys are in the bean mesh space: blade and scabbard ride on the puppet component, following its pelvis
	USceneComponent* Body = Puppet.IsValid() ? static_cast<USceneComponent*>(Puppet.Get()) : static_cast<USceneComponent*>(Carrier->GetMesh());
	ThirdPersonMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetIncludingScale);
	ScabbardMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetIncludingScale);
	ThirdPersonMesh->SetHiddenInGame(false);
	ScabbardMesh->SetHiddenInGame(false);

	if (Puppet.IsValid())
	{
		AddTickPrerequisiteComponent(Puppet.Get());
		if (const USkinnedAsset* Asset = Puppet->GetSkinnedAsset())
		{
			const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
			const int32 PelvisIndex = RefSkeleton.FindBoneIndex(Puppet->PelvisBone);
			if (PelvisIndex != INDEX_NONE)
			{
				PelvisReference = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, PelvisIndex);
				bHasPelvisReference = true;
			}
		}
	}
	if (Arms.IsValid())
	{
		Arms->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}
}

void ACutlassWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	ReleaseHitStop();
	SetAttackSlowdown(false);
	if (Arms.IsValid() && bArmsDriven)
	{
		Arms->ClearPoseOverride();
	}
	Super::EndPlay(EndPlayReason);
}

bool ACutlassWeapon::IsUsable() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!IsEquipped() || !Rules || !Carrier || Carrier->IsRagdoll())
	{
		return false;
	}
	if (const UIslanderGrabComponent* Grab = Carrier->GetGrabComponent(); Grab && Grab->IsHolding())
	{
		return false;
	}
	if (const UIslanderHealthComponent* Health = Carrier->GetHealthComponent(); Health && Health->IsDead())
	{
		return false;
	}
	const UCharacterMovementComponent* Move = Carrier->GetCharacterMovement();
	return !(Move && Move->IsSwimming());
}

bool ACutlassWeapon::UsesFirstPersonViewModel() const
{
	return Rules && Rules->bFirstPersonViewModel;
}

CutlassMath::FMoveWindows ACutlassWeapon::GetLocalWindows() const
{
	return Rules ? CutlassMath::GetMoveWindows(*Rules, LocalMove) : CutlassMath::FMoveWindows();
}

float ACutlassWeapon::GetLocalPlayed() const
{
	return LocalMove == ECutlassMove::None ? 0.0f : static_cast<float>(GetWorld()->GetTimeSeconds() - LocalMoveStart);
}

float ACutlassWeapon::GetLocalMoveElapsed() const
{
	if (LocalMove == ECutlassMove::None || !Rules)
	{
		return 0.0f;
	}
	// an attack's recovery plays faster than its track: the track time keeps the body clip on the blade
	const CutlassMath::FMoveWindows Windows = GetLocalWindows();
	const float Played = GetLocalPlayed();
	return Windows.bAttack ? Windows.TrackTime(Played) : Played;
}

float ACutlassWeapon::GetLocalMoveDuration() const
{
	return LocalMove == ECutlassMove::None || !Rules ? 0.0f : Rules->GetMoveDuration(LocalMove);
}

UIslanderMovementComponent* ACutlassWeapon::GetCarrierMovement() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	return Carrier ? Carrier->GetIslanderMovement() : nullptr;
}

float ACutlassWeapon::GetChargeProgress() const
{
	if (LocalChargeStart < 0.0 || bChargeSpent || !Rules || Rules->FinisherChargeSeconds <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - LocalChargeStart) / Rules->FinisherChargeSeconds, 0.0f, 1.0f);
}

// ---------------------------------------------------------------- input

void ACutlassWeapon::BindInput()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UEnhancedInputComponent* Input = Carrier ? Cast<UEnhancedInputComponent>(Carrier->InputComponent) : nullptr;
	if (bInputBound || !Input)
	{
		return;
	}
	// the bindings stay; the mapping context decides when the keys reach them
	if (AttackAction)
	{
		KeepInputBinding(Input, Input->BindAction(AttackAction, ETriggerEvent::Started, this, &ACutlassWeapon::HandleAttackStarted).GetHandle());
		KeepInputBinding(Input, Input->BindAction(AttackAction, ETriggerEvent::Completed, this, &ACutlassWeapon::HandleAttackCompleted).GetHandle());
	}
	if (ParryAction)
	{
		KeepInputBinding(Input, Input->BindAction(ParryAction, ETriggerEvent::Started, this, &ACutlassWeapon::HandleParryStarted).GetHandle());
		KeepInputBinding(Input, Input->BindAction(ParryAction, ETriggerEvent::Completed, this, &ACutlassWeapon::HandleParryCompleted).GetHandle());
	}
	bInputBound = true;
}

void ACutlassWeapon::OnInputBindingsRemoved()
{
	Super::OnInputBindingsRemoved();
	bInputBound = false;
}

void ACutlassWeapon::HandleAttackStarted()
{
	// keys of a former carrier never drive the item someone else now holds (T-288, network review R-37)
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	if (!IsUsable() || bParrying || bParryWanted)
	{
		return;
	}
	LocalChargeStart = GetWorld()->GetTimeSeconds();
	bChargeSpent = false;
	if (!HasAuthority())
	{
		ServerBeginCharge();
	}
	else
	{
		ServerBeginCharge_Implementation();
	}
}

void ACutlassWeapon::HandleAttackCompleted()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	// released before the charge: a combo blow (the finisher already left from Tick otherwise)
	if (LocalChargeStart >= 0.0 && !bChargeSpent)
	{
		RequestAttack(false);
	}
	LocalChargeStart = -1.0;
	bChargeSpent = false;
}

void ACutlassWeapon::HandleParryStarted()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	SetParrying(true);
}

void ACutlassWeapon::HandleParryCompleted()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	SetParrying(false);
}

// ---------------------------------------------------------------- actions

void ACutlassWeapon::RequestAttack(bool bFinisher)
{
	if (!IsUsable() || !Rules)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < LocalAttackOpen)
	{
		// buffered: played as soon as the running move can be cancelled (Tick)
		BufferedPressTime = Now;
		bBufferedFinisher = bFinisher;
		return;
	}
	BufferedPressTime = -1.0;
	const UIslanderMovementComponent* Move = GetCarrierMovement();
	if (!Move || !CutlassMath::HasStaminaFor(Move->GetStamina(), Move->MaxStamina, Rules->StaminaCostFraction))
	{
		return;
	}

	const ECutlassMove Blow = bFinisher ? ECutlassMove::Finisher
		: CutlassMath::NextComboMove(LocalLastCombo, Now - LocalLastComboEnd, Rules->ComboWindowSeconds);
	if (HasAuthority())
	{
		ServerAttack_Implementation(Blow);
		return;
	}
	// the carrier sees its blow at once; the server decides the hit
	PlayMove(Blow);
	if (UIslanderMovementComponent* CarrierMove = GetCarrierMovement())
	{
		CarrierMove->ConsumeStamina(CutlassMath::StaminaCost(CarrierMove->MaxStamina, Rules->StaminaCostFraction));
	}
	ServerAttack(Blow);
}

void ACutlassWeapon::ServerBeginCharge_Implementation()
{
	ServerChargeStart = GetWorld()->GetTimeSeconds();
}

bool ACutlassWeapon::CanAttackNow() const
{
	const UIslanderMovementComponent* Move = GetCarrierMovement();
	return IsUsable() && !bParrying && Move
		&& GetWorld()->GetTimeSeconds() >= ServerAttackOpen - MoveEndTolerance
		&& CutlassMath::HasStaminaFor(Move->GetStamina(), Move->MaxStamina, Rules->StaminaCostFraction);
}

void ACutlassWeapon::ServerAttack_Implementation(ECutlassMove Move)
{
	const double Now = GetWorld()->GetTimeSeconds();
	const FCutlassAttack* Attack = Rules ? Rules->GetAttack(Move) : nullptr;
	bool bValid = Attack && CanAttackNow();
	if (bValid && Move == ECutlassMove::Finisher)
	{
		// the left click must have been held long enough
		bValid = ServerChargeStart >= 0.0 && Now - ServerChargeStart >= Rules->FinisherChargeSeconds - ChargeTolerance;
	}
	else if (bValid)
	{
		// a combo blow: the next one (with some slack for the latency), or a new combo
		const ECutlassMove Expected = CutlassMath::NextComboMove(LastComboMove, Now - LastComboEnd, Rules->ComboWindowSeconds + 0.25f);
		bValid = Move == Expected || Move == ECutlassMove::SlashRight;
	}
	ServerChargeStart = -1.0;
	if (!bValid)
	{
		if (!IsCarrierLocallyControlled())
		{
			ClientRejectAttack();
		}
		return;
	}

	// the blow being cancelled finishes its sweep first, while it is still the move the creatures read
	UpdateServerBlade(Now);

	UIslanderMovementComponent* CarrierMove = GetCarrierMovement();
	CarrierMove->ConsumeStamina(CutlassMath::StaminaCost(CarrierMove->MaxStamina, Rules->StaminaCostFraction));

	const CutlassMath::FMoveWindows Windows = CutlassMath::GetMoveWindows(*Rules, Move);
	ServerMove = Move;
	ServerMoveStart = Now;
	ServerAttackOpen = Now + Windows.AttackCancel;
	ServerDefensiveOpen = Now + Windows.DefensiveCancel;
	if (Move != ECutlassMove::Finisher)
	{
		LastComboMove = Move;
		LastComboEnd = Now + Windows.Duration;
	}
	ServerBlowTargets.Reset();
	bServerBladeStarted = false;
	bServerBladeDone = false;
	bServerArcDone = false;

	MoveState.Move = Move;
	++MoveState.Counter;
	// the server is the carrier (host) or watches another player: OnRep does not run here
	PlayMove(Move);
}

void ACutlassWeapon::ClientRejectAttack_Implementation()
{
	// back to the guard; the stamina and the position come back with the server's corrections
	StopLocalMove();
	BufferedPressTime = -1.0;
	LocalAttackOpen = 0.0;
	LocalDefensiveOpen = 0.0;
	SetAttackSlowdown(false);
	if (const AIslanderCharacter* Carrier = GetCarrier())
	{
		if (UCharacterMovementComponent* Movement = Carrier->GetCharacterMovement())
		{
			Movement->RemoveRootMotionSource(LungeSourceName);
		}
	}
}

// ---------------------------------------------------------------- blade (server)

FTransform ACutlassWeapon::GetBladeFrame() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier)
	{
		return GetActorTransform();
	}
	const USceneComponent* Body = Puppet.IsValid() ? static_cast<const USceneComponent*>(Puppet.Get()) : static_cast<const USceneComponent*>(Carrier->GetMesh());
	return Body ? Body->GetComponentTransform() : Carrier->GetActorTransform();
}

void ACutlassWeapon::UpdateServerBlade(double Now)
{
	if (bServerBladeDone || !HasAuthority())
	{
		return;
	}
	const FCutlassAttack* Attack = Rules ? Rules->GetAttack(ServerMove) : nullptr;
	if (!Attack || !GetCarrier() || !IsUsable())
	{
		bServerBladeDone = true;
		return;
	}
	const CutlassMath::FMoveWindows Windows = CutlassMath::GetMoveWindows(*Rules, ServerMove);
	const float Played = static_cast<float>(Now - ServerMoveStart);
	if (Played < Windows.ActiveStart)
	{
		return;
	}

	// played time is the track time up to the last active frame
	const float To = FMath::Min(Played, Windows.ActiveEnd);
	const FTransform Frame = GetBladeFrame();
	if (!bServerBladeStarted)
	{
		bServerBladeStarted = true;
		ServerBladeTime = Windows.ActiveStart;
		ServerBladeFrame = Frame;
	}

	FHitBatch Batch;
	SweepBlade(*Attack, ServerBladeTime, To, ServerBladeFrame, Frame, Batch);
	if (!bServerArcDone && To >= Windows.HitTime)
	{
		bServerArcDone = true;
		CheckArc(*Attack, Batch);
	}
	ServerBladeTime = To;
	ServerBladeFrame = Frame;
	if (Played >= Windows.ActiveEnd)
	{
		bServerBladeDone = true;
	}
	SendHits(Batch);
}

void ACutlassWeapon::SweepBlade(const FCutlassAttack& Attack, float From, float To, const FTransform& FrameFrom, const FTransform& FrameTo, FHitBatch& Batch)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& Type : Rules->HitObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(Type));
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CutlassBlade), false, Carrier);
	Params.AddIgnoredActor(this);
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(Rules->BladeSweepRadiusCm);

	// sub-steps along the track: a slow server frame still follows the curve of the cut
	const int32 Steps = CutlassMath::SubstepCount(From, To, Rules->BladeSubstepSeconds);
	TArray<FVector> Previous;
	TArray<FVector> Current;
	CutlassMath::BladePoints(CutlassMath::SampleKeys(Attack.Keys, From, Rules->GuardKey), Rules->BladeStartCm, Rules->BladeTipCm,
		Rules->BladeSamples, FrameFrom, Previous);
	TArray<FHitResult> Hits;
	for (int32 Step = 1; Step <= Steps; ++Step)
	{
		const float Alpha = static_cast<float>(Step) / static_cast<float>(Steps);
		FTransform Frame;
		Frame.Blend(FrameFrom, FrameTo, Alpha);
		CutlassMath::BladePoints(CutlassMath::SampleKeys(Attack.Keys, FMath::Lerp(From, To, Alpha), Rules->GuardKey), Rules->BladeStartCm,
			Rules->BladeTipCm, Rules->BladeSamples, Frame, Current);
		for (int32 Index = 0; Index < Current.Num() && Index < Previous.Num(); ++Index)
		{
			Hits.Reset();
			GetWorld()->SweepMultiByObjectType(Hits, Previous[Index], Current[Index], FQuat::Identity, ObjectParams, Sphere, Params);
			for (const FHitResult& Hit : Hits)
			{
				AActor* Target = Hit.GetActor();
				if (!Target || ServerBlowTargets.Contains(Target))
				{
					continue;
				}
				const FVector Impact = Hit.bStartPenetrating ? Current[Index] : FVector(Hit.ImpactPoint);
				StrikeTarget(Target, Hit.GetComponent(), Impact, Attack, Batch);
			}
		}
		Swap(Previous, Current);
	}
}

void ACutlassWeapon::CheckArc(const FCutlassAttack& Attack, FHitBatch& Batch)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const FVector Origin = Carrier->GetActorLocation();
	// the bean faces its blow (turned toward the aim or the soft aim target)
	const FVector Forward = Carrier->GetActorForwardVector();
	const float SearchRadius = Rules->ReachCm + Rules->TargetRadiusAllowance + 100.0f;

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& Type : Rules->HitObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(Type));
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CutlassSweep), false, Carrier);
	Params.AddIgnoredActor(this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(SearchRadius), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || ServerBlowTargets.Contains(Target))
		{
			continue;
		}
		const FVector Offset = Target->GetActorLocation() - Origin;
		const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
		const float TargetRadius = TargetCharacter && TargetCharacter->GetCapsuleComponent()
			? FMath::Min(TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius(), Rules->TargetRadiusAllowance)
			: Rules->TargetRadiusAllowance * 0.5f;
		if (FMath::Abs(Offset.Z) > ArcHeightTolerance || !CutlassMath::IsInSweep(Forward, Offset, Rules->ReachCm + TargetRadius, Attack.ArcDegrees))
		{
			continue;
		}
		UPrimitiveComponent* Component = Overlap.GetComponent();
		StrikeTarget(Target, Component, Component ? Component->GetComponentLocation() : Target->GetActorLocation(), Attack, Batch);
	}
}

void ACutlassWeapon::StrikeTarget(AActor* Target, UPrimitiveComponent* Component, const FVector& ImpactPoint, const FCutlassAttack& Attack, FHitBatch& Batch)
{
	// each target once per blow, whichever check found it first
	ServerBlowTargets.Add(Target);
	AIslanderCharacter* Carrier = GetCarrier();
	const APawn* TargetPawn = Cast<APawn>(Target);
	if (TargetPawn && TargetPawn->IsPlayerControlled() && !Rules->bFriendlyFire)
	{
		return;
	}

	FVector Direction = (Target->GetActorLocation() - Carrier->GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Carrier->GetActorForwardVector();
	}
	// cuts carry their own damage type: a creature killed by one, or a corpse hit by one, may fall in two halves
	const TSubclassOf<UDamageType> DamageType = UDamageType::StaticClass();
	const float Damage = CutlassMath::BlowDamage(Attack.Damage, Sharpness, Rules->DullThreshold, Rules->DullDamageMultiplier);
	if (TargetPawn)
	{
		const UIslanderHealthComponent* Health = Target->FindComponentByClass<UIslanderHealthComponent>();
		const bool bWasAlive = !Health || !Health->IsDead();
		FHitResult Hit(Target, Component, ImpactPoint, -Direction);
		UGameplayStatics::ApplyPointDamage(Target, Damage, Direction, Hit, Carrier->GetController(), this, DamageType);
		const bool bKill = bWasAlive && Health && Health->IsDead();
		if (ACharacter* HitCharacter = Cast<ACharacter>(Target); HitCharacter && !bKill)
		{
			HitCharacter->LaunchCharacter(Direction * Rules->KnockbackSpeed + FVector(0.0f, 0.0f, 60.0f), true, false);
		}
		Sharpness = FMath::Max(0.0f, Sharpness - Rules->SharpnessLossPerHit);
		Batch.Locations.Add(Target->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f));
		Batch.Damages.Add(Damage);
		Batch.Kills.Add(CutlassMath::IsKillBlow(bKill, Rules->bEdged, Attack.bCut));
		Batch.Struck.Add(Target);
		Batch.bAnyKill |= bKill;
		UE_LOG(Logmonke_island, Log, TEXT("Cutlass: %s hits %s for %.0f%s"), *Carrier->GetName(), *Target->GetName(), Damage, bKill ? TEXT(" (kill)") : TEXT(""));
	}
	else if (Component && Component->IsSimulatingPhysics())
	{
		// loose bodies (a corpse may split in two on a cut) take the blow, then the push
		FHitResult Hit(Target, Component, ImpactPoint, -Direction);
		UGameplayStatics::ApplyPointDamage(Target, Damage, Direction, Hit, Carrier->GetController(), this, DamageType);
		if (IsValid(Component) && Component->IsSimulatingPhysics())
		{
			Component->AddImpulseAtLocation(Direction * Rules->PhysicsImpulse, ImpactPoint);
		}
		Batch.Struck.Add(Target);
		if (!Batch.bStruckBody)
		{
			Batch.bStruckBody = true;
			Batch.BodyLocation = ImpactPoint;
		}
	}
}

void ACutlassWeapon::SendHits(FHitBatch& Batch)
{
	if (Batch.Locations.Num() == 0 && Batch.bStruckBody)
	{
		MulticastBladeSound(ECutlassSound::Hit, Batch.BodyLocation);
	}
	if (Batch.Locations.Num() > 0)
	{
		MulticastBladeSound(Batch.Kills.Contains(true) ? ECutlassSound::Kill : ECutlassSound::Hit, Batch.Locations[0]);
	}
	if (Batch.Struck.Num() == 0)
	{
		return;
	}
	if (IsCarrierLocallyControlled())
	{
		ClientShowHits_Implementation(Batch.Locations, Batch.Damages, Batch.Kills, Batch.Struck, Batch.bAnyKill);
	}
	else
	{
		ClientShowHits(Batch.Locations, Batch.Damages, Batch.Kills, Batch.Struck, Batch.bAnyKill);
	}
}

void ACutlassWeapon::ClientShowHits_Implementation(const TArray<FVector_NetQuantize>& Locations, const TArray<float>& Damages, const TArray<bool>& Kills,
	const TArray<AActor*>& Struck, bool bAnyKill)
{
	const double Now = GetWorld()->GetTimeSeconds();
	HitMarkers.RemoveAll([Now](const FCutlassHitMarker& Marker) { return Now - Marker.Time > HitMarkerLife; });
	for (int32 Index = 0; Index < Locations.Num() && Index < Damages.Num(); ++Index)
	{
		FCutlassHitMarker& Marker = HitMarkers.AddDefaulted_GetRef();
		Marker.Location = Locations[Index];
		Marker.Damage = Damages[Index];
		Marker.Time = Now;
		Marker.bKill = Kills.IsValidIndex(Index) && Kills[Index];
	}
	if (IsCarrierLocallyControlled())
	{
		PlayHitFeel(Struck, bAnyKill || Kills.Contains(true));
	}
}

// ---------------------------------------------------------------- feel (carrier)

void ACutlassWeapon::PlayHitFeel(const TArray<AActor*>& Struck, bool bKill)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Rules || !Carrier)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();

	// hit-stop on this machine only: the blade holds still (Tick) and the struck enemies slow down, per actor (never
	// the world: other players keep playing). The carrier's pawn is left alone, its movement is predicted.
	const float Stop = bKill ? Rules->KillHitStopSeconds : Rules->HitStopSeconds;
	if (Stop > 0.0f)
	{
		LocalHitStopUntil = FMath::Max(LocalHitStopUntil, Now + Stop);
		for (AActor* Actor : Struck)
		{
			if (IsValid(Actor) && Actor != Carrier)
			{
				Actor->CustomTimeDilation = Rules->HitStopTimeDilation;
				HitStopActors.AddUnique(Actor);
			}
		}
		HitStopActorsUntil = FMath::Max(HitStopActorsUntil, Now + Stop);
	}

	if (Rules->HitCameraShake)
	{
		if (const APlayerController* Controller = Cast<APlayerController>(Carrier->GetController()); Controller && Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraShake(Rules->HitCameraShake, bKill ? Rules->KillShakeScale : Rules->HitShakeScale);
		}
	}
}

void ACutlassWeapon::ReleaseHitStop()
{
	for (const TWeakObjectPtr<AActor>& Actor : HitStopActors)
	{
		if (Actor.IsValid())
		{
			Actor->CustomTimeDilation = 1.0f;
		}
	}
	HitStopActors.Reset();
	HitStopActorsUntil = 0.0;
}

// ---------------------------------------------------------------- strike direction, lunge, slowdown (owner and server)

void ACutlassWeapon::GatherEnemies(float Radius, TArray<AActor*>& OutEnemies) const
{
	OutEnemies.Reset();
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || Radius <= 0.0f)
	{
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CutlassAim), false, Carrier);
	Params.AddIgnoredActor(this);
	TArray<FOverlapResult> Overlaps;
	const FVector Origin = Carrier->GetActorLocation();
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		APawn* Pawn = Cast<APawn>(Overlap.GetActor());
		// never toward a friend; the dead are no longer enemies
		if (!Pawn || Pawn->IsPlayerControlled() || OutEnemies.Contains(Pawn)
			|| FMath::Abs(Pawn->GetActorLocation().Z - Origin.Z) > ArcHeightTolerance)
		{
			continue;
		}
		if (const UIslanderHealthComponent* Health = Pawn->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
		{
			continue;
		}
		OutEnemies.Add(Pawn);
	}
}

void ACutlassWeapon::BeginStrike(ECutlassMove Move)
{
	AIslanderCharacter* Carrier = GetCarrier();
	const FCutlassAttack* Attack = Rules ? Rules->GetAttack(Move) : nullptr;
	if (!Carrier || !Attack)
	{
		return;
	}
	const FVector Origin = Carrier->GetActorLocation();
	StrikeYaw = Rules->bFaceAimOnAttack ? Carrier->GetBaseAimRotation().Yaw : Carrier->GetActorRotation().Yaw;
	bStrikeAssisted = false;
	bStrikeTurning = true;

	TArray<AActor*> Enemies;
	GatherEnemies(FMath::Max(Rules->bAimAssist ? Rules->AimAssistRangeCm : 0.0f, Rules->LungeBlockDistanceCm + Attack->LungeDistanceCm), Enemies);
	TArray<FVector, TInlineAllocator<8>> Points;
	for (const AActor* Enemy : Enemies)
	{
		Points.Add(Enemy->GetActorLocation());
	}

	// soft aim: toward the nearest enemy in the cone around the aim
	if (Rules->bAimAssist)
	{
		const int32 Target = CutlassMath::PickAimTarget(Origin, StrikeYaw, Points, Rules->AimAssistConeDegrees, Rules->AimAssistRangeCm);
		if (Target != INDEX_NONE)
		{
			const FVector ToTarget = (Points[Target] - Origin).GetSafeNormal2D();
			if (!ToTarget.IsNearlyZero())
			{
				StrikeYaw = ToTarget.Rotation().Yaw;
				bStrikeAssisted = true;
			}
		}
	}

	// lunge: a short step toward the blow, none against an enemy already at the blade
	UCharacterMovementComponent* Movement = Carrier->GetCharacterMovement();
	if (Attack->LungeDistanceCm <= 0.0f || !Movement || !Movement->IsMovingOnGround())
	{
		return;
	}
	const FVector Forward = FRotator(0.0f, StrikeYaw, 0.0f).Vector();
	float NearestAhead = -1.0f;
	for (const FVector& Point : Points)
	{
		const FVector Flat(Point.X - Origin.X, Point.Y - Origin.Y, 0.0f);
		const float Distance = Flat.Size();
		if (FVector::DotProduct(Flat, Forward) > 0.0f || Distance < 1.0f)
		{
			NearestAhead = NearestAhead < 0.0f ? Distance : FMath::Min(NearestAhead, Distance);
		}
	}
	const float Distance = CutlassMath::LungeDistance(Attack->LungeDistanceCm, NearestAhead, Rules->LungeBlockDistanceCm);
	if (Distance <= 1.0f)
	{
		return;
	}
	Movement->RemoveRootMotionSource(LungeSourceName);
	TSharedPtr<FRootMotionSource_ConstantForce> Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->InstanceName = LungeSourceName;
	Source->AccumulateMode = ERootMotionAccumulateMode::Additive;
	Source->Priority = 5;
	Source->Force = Forward * (Distance / FMath::Max(Rules->LungeSeconds, 0.01f));
	Source->Duration = Rules->LungeSeconds;
	Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
	Movement->ApplyRootMotionSource(Source);
}

void ACutlassWeapon::UpdateStrikeTurn(float DeltaSeconds)
{
	AIslanderCharacter* Carrier = GetCarrier();
	const CutlassMath::FMoveWindows Windows = GetLocalWindows();
	if (!bStrikeTurning || !Carrier || !Windows.bAttack || GetLocalPlayed() >= Windows.ActiveEnd)
	{
		bStrikeTurning = false;
		return;
	}
	if (!Carrier->bUseControllerRotationYaw)
	{
		// third person: the body faces the blow until its last active frame (it would turn to its movement otherwise)
		FRotator Rotation = Carrier->GetActorRotation();
		Rotation.Yaw = CutlassMath::StepYawToward(Rotation.Yaw, StrikeYaw, Rules->BodyTurnRateDegPerSec * DeltaSeconds);
		Carrier->SetActorRotation(Rotation);
		return;
	}
	// first person: the body follows the camera; only the soft aim turns the camera, then lets go of it
	AController* Controller = Carrier->GetController();
	if (!bStrikeAssisted || !IsCarrierLocallyControlled() || !Controller)
	{
		bStrikeTurning = false;
		return;
	}
	FRotator Rotation = Controller->GetControlRotation();
	Rotation.Yaw = CutlassMath::StepYawToward(Rotation.Yaw, StrikeYaw, Rules->AimAssistTurnRateDegPerSec * DeltaSeconds);
	Controller->SetControlRotation(Rotation);
	if (FMath::Abs(FRotator::NormalizeAxis(StrikeYaw - Rotation.Yaw)) < AimAssistDoneDegrees)
	{
		bStrikeTurning = false;
	}
}

void ACutlassWeapon::SetAttackSlowdown(bool bSlow)
{
	if (bSlow && SlowedBaseWalkSpeed < 0.0f && Rules)
	{
		const AIslanderCharacter* Carrier = GetCarrier();
		UCharacterMovementComponent* Movement = Carrier ? Carrier->GetCharacterMovement() : nullptr;
		if (Movement)
		{
			SlowedMovement = Movement;
			SlowedBaseWalkSpeed = Movement->MaxWalkSpeed;
			Movement->MaxWalkSpeed = SlowedBaseWalkSpeed * Rules->AttackMoveSpeedMultiplier;
		}
	}
	else if (!bSlow && SlowedBaseWalkSpeed >= 0.0f)
	{
		if (SlowedMovement.IsValid())
		{
			SlowedMovement->MaxWalkSpeed = SlowedBaseWalkSpeed;
		}
		SlowedMovement.Reset();
		SlowedBaseWalkSpeed = -1.0f;
	}
}

bool ACutlassWeapon::IsCarrierTryingToMove() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const UCharacterMovementComponent* Movement = Carrier ? Carrier->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return false;
	}
	if (Movement->IsFalling())
	{
		return true;
	}
	// the carrier and the server know the input; the other players only see the speed
	if (HasAuthority() || IsCarrierLocallyControlled())
	{
		return !Movement->GetCurrentAcceleration().IsNearlyZero(1.0);
	}
	return Movement->Velocity.SizeSquared2D() > FMath::Square(Rules->MovementCancelSpeed);
}

// ---------------------------------------------------------------- parry

void ACutlassWeapon::SetParrying(bool bInParrying)
{
	// the carrier raises the blade as soon as its move allows it; the server keeps it for the damage and the others
	bParryWanted = bInParrying;
	UpdateParry(GetWorld()->GetTimeSeconds());
	if (!HasAuthority())
	{
		ServerSetParrying(bInParrying);
	}
}

void ACutlassWeapon::ServerSetParrying_Implementation(bool bInParrying)
{
	bParryWanted = bInParrying;
	UpdateParry(GetWorld()->GetTimeSeconds());
}

void ACutlassWeapon::UpdateParry(double Now)
{
	const double Open = HasAuthority() ? ServerDefensiveOpen : LocalDefensiveOpen;
	const bool bWasParrying = bParrying;
	bParrying = bParryWanted && IsUsable() && Now >= Open;
	if (bParrying && !bWasParrying && GetLocalWindows().bAttack)
	{
		// the parry cancels the end of the blow
		StopLocalMove();
	}
}

float ACutlassWeapon::ModifyIncomingDamage(float Damage, const AActor* DamageCauser)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UIslanderMovementComponent* Move = GetCarrierMovement();
	if (!bParrying || !Rules || !Carrier || !Move || !DamageCauser || !IsUsable())
	{
		return Damage;
	}
	const FVector ToCauser = (DamageCauser->GetActorLocation() - Carrier->GetActorLocation()).GetSafeNormal2D();
	const float FacingDot = FVector::DotProduct(Carrier->GetActorForwardVector().GetSafeNormal2D(), ToCauser);
	const bool bHasStamina = CutlassMath::HasStaminaFor(Move->GetStamina(), Move->MaxStamina, Rules->ParryStaminaCostFraction);
	const float Result = CutlassMath::ParriedDamage(Damage, true, FacingDot, Rules->ParryFrontDot, Rules->ParryReduction, bHasStamina);
	if (Result < Damage)
	{
		Move->ConsumeStamina(CutlassMath::StaminaCost(Move->MaxStamina, Rules->ParryStaminaCostFraction));
		MulticastBladeSound(ECutlassSound::Parry, Carrier->GetActorLocation() + Carrier->GetActorForwardVector() * 40.0f + FVector(0.0f, 0.0f, 30.0f));
		// the carrier feels the block: the blade recoils in its view, the camera jolts
		if (IsCarrierLocallyControlled())
		{
			PlayBlockFeel();
		}
		else
		{
			ClientParryBlocked();
		}
	}
	return Result;
}

void ACutlassWeapon::ClientParryBlocked_Implementation()
{
	PlayBlockFeel();
}

void ACutlassWeapon::PlayBlockFeel()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Rules || !Carrier || !IsCarrierLocallyControlled())
	{
		return;
	}
	BlockRecoilStart = GetWorld()->GetTimeSeconds();
	if (Rules->BlockCameraShake)
	{
		if (const APlayerController* Controller = Cast<APlayerController>(Carrier->GetController()); Controller && Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->StartCameraShake(Rules->BlockCameraShake, Rules->BlockShakeScale);
		}
	}
}

// ---------------------------------------------------------------- moves

void ACutlassWeapon::OnEquippedChanged()
{
	if (!Rules)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bArrival = Now - SpawnTime < 0.2;
	if (IsEquipped())
	{
		// key 2: out of the scabbard (an item equipped on arrival is already in hand)
		if (bArrival)
		{
			CurrentPose = Rules->GuardKey;
		}
		else
		{
			PlayMove(ECutlassMove::Draw);
			PlayBladeSound(ECutlassSound::Draw, GetActorLocation());
		}
		if (HasAuthority())
		{
			ServerMove = ECutlassMove::Draw;
			ServerMoveStart = Now;
			ServerAttackOpen = bArrival ? Now : Now + Rules->DrawSeconds;
			ServerDefensiveOpen = ServerAttackOpen;
			bServerBladeDone = true;
		}
	}
	else
	{
		PlayMove(ECutlassMove::Sheathe);
		if (!bArrival)
		{
			PlayBladeSound(ECutlassSound::Sheathe, GetActorLocation());
		}
		bParrying = false;
		bParryWanted = false;
		BufferedPressTime = -1.0;
		LocalChargeStart = -1.0;
		bStrikeTurning = false;
		SetAttackSlowdown(false);
		if (HasAuthority())
		{
			ServerMove = ECutlassMove::None;
			ServerAttackOpen = 0.0;
			ServerDefensiveOpen = 0.0;
			bServerBladeDone = true;
		}
	}
}

void ACutlassWeapon::OnRep_MoveState()
{
	PlayMove(MoveState.Move);
}

void ACutlassWeapon::PlayMove(ECutlassMove Move)
{
	const double Now = GetWorld()->GetTimeSeconds();
	LocalMove = Move;
	LocalMoveStart = Now;
	++LocalMoveSerial;
	const CutlassMath::FMoveWindows Windows = GetLocalWindows();
	LocalAttackOpen = Now + Windows.AttackCancel;
	LocalDefensiveOpen = Now + Windows.DefensiveCancel;
	if (Rules && Rules->GetAttack(Move))
	{
		if (Move != ECutlassMove::Finisher)
		{
			LocalLastCombo = Move;
			LocalLastComboEnd = Now + Windows.Duration;
		}
		PlayBladeSound(Move == ECutlassMove::Finisher ? ECutlassSound::FinisherSwing : ECutlassSound::Swing, GetActorLocation());
		if (HasAuthority() || IsCarrierLocallyControlled())
		{
			BeginStrike(Move);
		}
	}
}

void ACutlassWeapon::StopLocalMove()
{
	LocalMove = ECutlassMove::None;
	bStrikeTurning = false;
}

void ACutlassWeapon::MulticastBladeSound_Implementation(ECutlassSound Sound, FVector_NetQuantize Location)
{
	PlayBladeSound(Sound, Location);
}

void ACutlassWeapon::PlayBladeSound(ECutlassSound Sound, const FVector& Location) const
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USoundBase* Asset = nullptr;
	switch (Sound)
	{
	case ECutlassSound::Swing:			Asset = Rules->SwingSound; break;
	case ECutlassSound::FinisherSwing:	Asset = Rules->FinisherSwingSound ? Rules->FinisherSwingSound : Rules->SwingSound; break;
	case ECutlassSound::Hit:			Asset = Rules->HitSound; break;
	case ECutlassSound::Kill:			Asset = Rules->KillSound ? Rules->KillSound : Rules->HitSound; break;
	case ECutlassSound::Parry:			Asset = Rules->ParrySound; break;
	case ECutlassSound::Draw:			Asset = Rules->DrawSound; break;
	case ECutlassSound::Sheathe:		Asset = Rules->SheatheSound; break;
	}
	if (Asset)
	{
		const float Pitch = 1.0f + FMath::FRandRange(-Rules->SoundPitchJitter, Rules->SoundPitchJitter);
		UGameplayStatics::PlaySoundAtLocation(this, Asset, Location, 1.0f, Pitch, 0.0f, Rules->SoundAttenuation);
	}
}

// ---------------------------------------------------------------- per frame

FCutlassKey ACutlassWeapon::ComputeTargetPose() const
{
	if (LocalMove != ECutlassMove::None)
	{
		const TArray<FCutlassKey>* Keys = Rules->GetKeys(LocalMove);
		const CutlassMath::FMoveWindows Windows = GetLocalWindows();
		const float Played = GetLocalPlayed();
		if (Keys && Keys->Num() > 0 && Played < Windows.Duration)
		{
			// the recovery plays faster than the track; draw and sheathe are stretched to their tunable time
			return CutlassMath::SampleKeys(*Keys, Windows.TrackTime(Played), Rules->GuardKey);
		}
	}
	if (!IsUsable())
	{
		return Rules->SheathedKey;
	}
	if (bParrying)
	{
		return Rules->ParryKey;
	}
	// charging the finisher: the blade goes up above the head
	if (const float Charge = GetChargeProgress(); Charge > 0.25f && Rules->Finisher.Keys.Num() > 1)
	{
		return CutlassMath::BlendKeys(Rules->GuardKey, Rules->Finisher.Keys[1], FMath::SmoothStep(0.25f, 1.0f, Charge));
	}
	return Rules->GuardKey;
}

FTransform ACutlassWeapon::FollowPelvis(const FTransform& MeshSpace) const
{
	if (!bHasPelvisReference || !Puppet.IsValid())
	{
		return MeshSpace;
	}
	const FTransform PelvisNow = Puppet->GetBoneTransformByName(Puppet->PelvisBone, EBoneSpaces::ComponentSpace);
	return MeshSpace * PelvisReference.Inverse() * PelvisNow;
}

void ACutlassWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !Rules)
	{
		return;
	}
	const bool bUsable = IsUsable();
	const double Now = GetWorld()->GetTimeSeconds();

	// first, while the running blow is still the move the creatures read: its blade sweep
	if (HasAuthority())
	{
		UpdateServerBlade(Now);
	}

	if (IsCarrierLocallyControlled())
	{
		BindInput();
		SetInputContextActive(CutlassMappingContext, CutlassMappingPriority, bUsable);
		if (!bUsable)
		{
			bParryWanted = false;
			LocalChargeStart = -1.0;
			BufferedPressTime = -1.0;
		}
		// hit-stop: the blade (and the body clip that follows it) holds still a moment
		if (Now < LocalHitStopUntil && GetLocalWindows().bAttack)
		{
			LocalMoveStart += DeltaSeconds;
			LocalAttackOpen += DeltaSeconds;
			LocalDefensiveOpen += DeltaSeconds;
			LocalLastComboEnd += DeltaSeconds;
		}
		// held long enough: the finisher leaves without waiting for the release
		if (LocalChargeStart >= 0.0 && !bChargeSpent && Now - LocalChargeStart >= Rules->FinisherChargeSeconds)
		{
			bChargeSpent = true;
			RequestAttack(true);
		}
		// a click during the last blow: played as soon as the blow can be cancelled
		if (BufferedPressTime >= 0.0)
		{
			switch (CutlassMath::EvaluateBufferedInput(Now, BufferedPressTime, Rules->InputBufferSeconds, LocalAttackOpen))
			{
			case CutlassMath::EBufferedInput::Fire:
				{
					const bool bFinisher = bBufferedFinisher;
					BufferedPressTime = -1.0;
					RequestAttack(bFinisher);
					break;
				}
			case CutlassMath::EBufferedInput::Expired:
				BufferedPressTime = -1.0;
				break;
			default:
				break;
			}
		}
	}

	if (HitStopActors.Num() > 0 && Now >= HitStopActorsUntil)
	{
		ReleaseHitStop();
	}

	const bool bDrivesCarrier = HasAuthority() || IsCarrierLocallyControlled();
	if (bDrivesCarrier)
	{
		UpdateParry(Now);
	}

	// end of the move: played out, or its recovery cancelled by a step or a jump
	if (LocalMove != ECutlassMove::None)
	{
		const CutlassMath::FMoveWindows Windows = GetLocalWindows();
		const float Played = GetLocalPlayed();
		if (Played >= Windows.Duration || (Windows.bAttack && Played >= Windows.DefensiveCancel && IsCarrierTryingToMove()))
		{
			StopLocalMove();
		}
	}

	if (bDrivesCarrier)
	{
		UpdateStrikeTurn(DeltaSeconds);
		const CutlassMath::FMoveWindows Windows = GetLocalWindows();
		SetAttackSlowdown(bUsable && Windows.bAttack && GetLocalPlayed() < Windows.DefensiveCancel);
	}

	// inside a move the track is followed exactly; outside, the pose blends to the guard, the parry or the scabbard
	const FCutlassKey Target = ComputeTargetPose();
	if (LocalMove != ECutlassMove::None)
	{
		CurrentPose = Target;
	}
	else
	{
		CurrentPose = CutlassMath::BlendKeys(CurrentPose, Target, 1.0f - FMath::Exp(-Rules->PoseBlendSpeed * DeltaSeconds));
	}

	// another item in hand (the musket): the cutlass stays sheathed and leaves the arms alone
	const UIslanderInventoryComponent* Inventory = Carrier->FindComponentByClass<UIslanderInventoryComponent>();
	const AInventoryItemActor* InHand = Inventory ? Inventory->GetEquippedItem() : nullptr;
	const bool bOtherItem = InHand && InHand != this;
	if (bOtherItem && LocalMove == ECutlassMove::Sheathe)
	{
		LocalMove = ECutlassMove::None;
		CurrentPose = Rules->SheathedKey;
	}
	const bool bDrawn = bUsable || LocalMove == ECutlassMove::Sheathe;

	// first person with the library: the owner sees the body's own arms, the blade stays in the body's hand; the view model
	// of T-239 needs UBeanAnimDriverComponent to leave the arm meshes to the cutlass (UsesFirstPersonViewModel)
	const bool bBodyArms = AnimDriver.IsValid() && AnimDriver->ShowsBodyArmsInFirstPerson();
	const bool bFirstPersonView = IsCarrierLocallyControlled() && Carrier->IsFirstPerson() && !Carrier->IsRagdoll() && !bBodyArms;
	if (bBodyArms && !bWarnedBodyArms && UsesFirstPersonViewModel() && IsCarrierLocallyControlled() && Carrier->IsFirstPerson())
	{
		bWarnedBodyArms = true;
		UE_LOG(Logmonke_island, Log, TEXT("Cutlass %s: first person view model not shown, the animation library shows the body arms (BeanAnimDriver bBodyArmsInFirstPerson)"), *GetName());
	}
	// the sheathe plays in first person too (the blade goes down to the hip, out of view)
	const bool bShowFirstPerson = bFirstPersonView && (bUsable || LocalMove == ECutlassMove::Sheathe);
	UpdateFirstPerson(bShowFirstPerson, DeltaSeconds);
	// put away, a weapon without a scabbard (a mob's spear or club, T-161) is hidden instead of hanging at the hip
	UpdateThirdPerson(CutlassMath::ShowsThirdPersonMesh(bShowFirstPerson, bDrawn, Rules->bShowWhenPutAway),
		bDrawn && !bFirstPersonView && !bOtherItem && !Carrier->IsRagdoll());
}

FCutlassViewKey ACutlassWeapon::ComputeViewPose() const
{
	if (LocalMove != ECutlassMove::None)
	{
		const TArray<FCutlassViewKey>* Keys = Rules->GetViewKeys(LocalMove);
		const CutlassMath::FMoveWindows Windows = GetLocalWindows();
		const float Played = GetLocalPlayed();
		if (Keys && Keys->Num() > 0 && Played < Windows.Duration)
		{
			// same clock as the body: the recovery plays faster, the draw and the sheathe are stretched, the hit-stop holds
			return CutlassMath::SampleViewKeys(*Keys, Windows.TrackTime(Played), Rules->FirstPersonGuardKey);
		}
	}
	if (bParrying)
	{
		return Rules->FirstPersonParryKey;
	}
	// charging the long blow: the blade goes up to the start of its track, raised above the view
	if (const float Charge = GetChargeProgress(); Charge > 0.25f && Rules->Finisher.ViewKeys.Num() > 0)
	{
		return CutlassMath::BlendViewKeys(Rules->FirstPersonGuardKey, Rules->Finisher.ViewKeys[0], FMath::SmoothStep(0.25f, 1.0f, Charge));
	}
	return Rules->FirstPersonGuardKey;
}

CutlassMath::FViewSway ACutlassWeapon::UpdateViewSway(float DeltaSeconds)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const UCharacterMovementComponent* Movement = Carrier ? Carrier->GetCharacterMovement() : nullptr;

	// breath: a slow sine, always on (too small to read during a blow)
	BreathPhase = FMath::Fmod(BreathPhase + DeltaSeconds * Rules->BreathFrequencyHz, 1.0f);
	CutlassMath::FViewSway Sway = CutlassMath::ViewBreath(BreathPhase, Rules->BreathAmplitudeCm, Rules->BreathPitchDegrees);

	// run bob: the stride phase advances with the ground speed; the amplitude follows it with a lag (none at a stop, in the
	// air or in the water) and drops during a blow
	const float Speed = Movement ? static_cast<float>(Movement->Velocity.Size2D()) : 0.0f;
	const float SpeedWeight = CutlassMath::BobTargetWeight(Speed, Rules->BobReferenceSpeed, Movement && Movement->IsMovingOnGround());
	BobPhase = FMath::Fmod(BobPhase + DeltaSeconds * Rules->BobFrequencyHz * SpeedWeight, 1.0f);
	const float TargetWeight = GetLocalWindows().bAttack ? SpeedWeight * Rules->BobAttackScale : SpeedWeight;
	BobWeight = FMath::FInterpTo(BobWeight, TargetWeight, DeltaSeconds, Rules->BobDampingSpeed);
	Sway += CutlassMath::ViewBob(BobPhase, BobWeight, Rules->BobVerticalCm, Rules->BobLateralCm, Rules->BobRollDegrees);

	// a blocked hit pushes the set back toward the face, out and back
	if (BlockRecoilStart >= 0.0)
	{
		const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - BlockRecoilStart);
		if (Elapsed >= Rules->BlockRecoilSeconds)
		{
			BlockRecoilStart = -1.0;
		}
		else
		{
			const float Envelope = CutlassMath::BlockRecoilEnvelope(Elapsed, Rules->BlockRecoilSeconds);
			Sway.Location += Rules->BlockRecoilLocation * Envelope;
			Sway.Rotation += Rules->BlockRecoilRotation * Envelope;
		}
	}
	return Sway;
}

void ACutlassWeapon::UpdateFirstPerson(bool bShow, float DeltaSeconds)
{
	const bool bWasShown = !FirstPersonMesh->bHiddenInGame;
	if (!bShow)
	{
		if (bWasShown)
		{
			FirstPersonMesh->SetHiddenInGame(true);
		}
		if (bArmsDriven && Arms.IsValid())
		{
			Arms->ClearPoseOverride();
		}
		bArmsDriven = false;
		BobWeight = 0.0f;
		LeftHandWeight = 0.0f;
		BlockRecoilStart = -1.0;
		return;
	}

	// the move's own first person track is followed exactly, blended in from the pose shown when it starts (the long blow
	// from the charge, a buffered blow from the recovery of the last one); the parry, the charge and the guard are blended into
	const FCutlassViewKey Target = ComputeViewPose();
	if (!bWasShown)
	{
		FirstPersonMesh->SetHiddenInGame(false);
		ViewPose = Target;
		ViewMoveSerial = LocalMoveSerial;
		BreathPhase = 0.0f;
	}
	else if (LocalMove != ECutlassMove::None)
	{
		if (ViewMoveSerial != LocalMoveSerial)
		{
			ViewMoveSerial = LocalMoveSerial;
			ViewMoveStartPose = ViewPose;
		}
		const float BlendIn = Rules->ViewMoveBlendInSeconds > KINDA_SMALL_NUMBER ? FMath::SmoothStep(0.0f, Rules->ViewMoveBlendInSeconds, GetLocalPlayed()) : 1.0f;
		ViewPose = BlendIn >= 1.0f ? Target : CutlassMath::BlendViewKeys(ViewMoveStartPose, Target, BlendIn);
	}
	else
	{
		ViewPose = CutlassMath::BlendViewKeys(ViewPose, Target, 1.0f - FMath::Exp(-Rules->PoseBlendSpeed * DeltaSeconds));
	}

	// breath, run bob and the recoil of a blocked hit move the whole set about the camera; never through the near clip plane
	const FTransform Swayed = FTransform(ViewPose.Rotation, ViewPose.Location) * UpdateViewSway(DeltaSeconds).ToTransform();
	FCutlassViewKey Shown = ViewPose;
	Shown.Location = Swayed.GetLocation();
	Shown.Rotation = Swayed.Rotator();
	Shown = CutlassMath::KeepAheadOfNearPlane(Shown, Rules->FirstPersonHiltCm, Rules->BladeTipCm, Rules->FirstPersonNearClipCm);
	FirstPersonMesh->SetRelativeTransform(CutlassMath::MeshOnGrip(FTransform(Shown.Rotation, Shown.Location), Rules->MeshInGrip));

	if (!Arms.IsValid())
	{
		return;
	}
	// right mitten on the grip (rubber arm from its shoulder, as the musket); the left one comes up on the flat of the
	// blade while the parry is held, and goes back out of view
	const FBeanArmPose Right = MusketMath::SolveArm(Rules->FirstPersonShoulder, Shown.Location, Rules->FirstPersonArmHandPoint, Rules->FirstPersonArmMaxStretch);
	const float LeftTarget = bParrying && Rules->bParryLeftHand ? 1.0f : 0.0f;
	LeftHandWeight = bWasShown ? FMath::FInterpTo(LeftHandWeight, LeftTarget, DeltaSeconds, Rules->LeftHandBlendSpeed) : LeftTarget;
	FBeanArmPose Left = Arms->RestPose;
	if (LeftHandWeight > KINDA_SMALL_NUMBER)
	{
		const FVector Support = Shown.Location + Shown.Rotation.RotateVector(FVector(Rules->ParryLeftHandAlongBladeCm, 0.0, 0.0)) + Rules->ParryLeftHandOffset;
		const FBeanArmPose Hold = MusketMath::MirrorArm(MusketMath::SolveArm(Rules->FirstPersonLeftShoulder, Support, Rules->FirstPersonArmHandPoint, Rules->FirstPersonArmMaxStretch));
		Left = CutlassMath::BlendArmPoses(Left, Hold, LeftHandWeight);
	}
	Arms->SetPoseOverride(Right, Left);
	bArmsDriven = true;
}

void ACutlassWeapon::UpdateThirdPerson(bool bShowBlade, bool bDriveArm)
{
	const float ScabbardAlpha = FMath::Clamp(CurrentPose.ScabbardAlpha, 0.0f, 1.0f);
	FTransform Scabbard;
	Scabbard.Blend(Rules->ScabbardRest, Rules->ScabbardDrawn, ScabbardAlpha);
	ScabbardMesh->SetRelativeTransform(FollowPelvis(Scabbard));

	// sheathed, the blade sits in the scabbard; drawn, in the hand
	const bool bDrawnPose = bDriveArm;
	const FCutlassKey& Shown = bDrawnPose ? CurrentPose : Rules->SheathedKey;
	FTransform Blade(Shown.GripRotation, Shown.GripLocation);
	if (!bDrawnPose)
	{
		Blade = FTransform(Rules->SheathedKey.GripRotation, Rules->SheathedKey.GripLocation)
			* FTransform(Rules->ScabbardRest).Inverse() * Scabbard;
	}
	FTransform BladeOnBody = FollowPelvis(Blade);
	if (bDriveArm && Puppet.IsValid() && IsLibraryAnimated() && Puppet->GetBoneIndex(Rules->GripBone) != INDEX_NONE)
	{
		// the library animates the arm: the blade is welded to the hand (bone scale of the FBX left out)
		const FTransform Hand = Puppet->GetBoneTransformByName(Rules->GripBone, EBoneSpaces::ComponentSpace);
		BladeOnBody = Rules->LibraryGripInHand * FTransform(Hand.GetRotation(), Hand.GetLocation());
	}
	else if (bDriveArm && Puppet.IsValid())
	{
		// drawn: the arm reaches for the key, then the blade goes where the mitten really is (the bean's arm is short)
		BladeOnBody.SetLocation(ReachArm(BladeOnBody.GetLocation()));
	}
	ThirdPersonMesh->SetRelativeTransform(CutlassMath::MeshOnGrip(BladeOnBody, Rules->MeshInGrip));
	if (ThirdPersonMesh->bHiddenInGame == bShowBlade)
	{
		ThirdPersonMesh->SetHiddenInGame(!bShowBlade);
	}
}

bool ACutlassWeapon::IsLibraryAnimated() const
{
	return AnimDriver.IsValid() && AnimDriver->IsDrivingBody();
}

FVector ACutlassWeapon::ReachArm(const FVector& Target)
{
	UBeanPuppetComponent* Body = Puppet.Get();
	if (!Body || Body->GetBoneIndex(Rules->UpperArmBone) == INDEX_NONE || Body->GetBoneIndex(Rules->LowerArmBone) == INDEX_NONE
		|| Body->GetBoneIndex(Rules->GripBone) == INDEX_NONE)
	{
		if (!bWarnedMissingBones)
		{
			bWarnedMissingBones = true;
			UE_LOG(Logmonke_island, Warning, TEXT("Cutlass %s: arm bones %s / %s / %s not found on the puppet, the blade is not held"),
				*GetName(), *Rules->UpperArmBone.ToString(), *Rules->LowerArmBone.ToString(), *Rules->GripBone.ToString());
		}
		return Target;
	}
	// component space bones carry the FBX root scale (x100): only their locations and rotations are used
	const FTransform Upper = Body->GetBoneTransformByName(Rules->UpperArmBone, EBoneSpaces::ComponentSpace);
	const FTransform Lower = Body->GetBoneTransformByName(Rules->LowerArmBone, EBoneSpaces::ComponentSpace);
	const FVector Shoulder = Upper.GetLocation();
	const FVector Elbow = Lower.GetLocation();
	const FVector Hand = Body->GetBoneTransformByName(Rules->GripBone, EBoneSpaces::ComponentSpace).GetLocation();
	const float UpperLength = FVector::Dist(Shoulder, Elbow);
	// forearm + the grip inside the mitten
	const float LowerLength = FVector::Dist(Elbow, Hand) + Rules->GripReachCm;
	if (UpperLength < 0.1f || LowerLength < 0.1f)
	{
		return Target;
	}

	const FVector NewElbow = CutlassMath::SolveElbow(Shoulder, Target, UpperLength, LowerLength, Rules->ElbowPole);
	const FQuat UpperDelta = FQuat::FindBetweenNormals((Elbow - Shoulder).GetSafeNormal(), (NewElbow - Shoulder).GetSafeNormal());
	Body->SetBoneRotationByName(Rules->UpperArmBone, (UpperDelta * Upper.GetRotation()).Rotator(), EBoneSpaces::ComponentSpace);

	// the forearm turned with the upper arm; turn it again so the mitten points at the target
	const FVector LowerNow = UpperDelta.RotateVector(Hand - Elbow).GetSafeNormal();
	const FVector LowerWanted = (Target - NewElbow).GetSafeNormal();
	const FQuat LowerDelta = FQuat::FindBetweenNormals(LowerNow, LowerWanted);
	Body->SetBoneRotationByName(Rules->LowerArmBone, (LowerDelta * UpperDelta * Lower.GetRotation()).Rotator(), EBoneSpaces::ComponentSpace);

	// where the grip really is now
	const FVector ElbowNow = Body->GetBoneTransformByName(Rules->LowerArmBone, EBoneSpaces::ComponentSpace).GetLocation();
	const FVector HandNow = Body->GetBoneTransformByName(Rules->GripBone, EBoneSpaces::ComponentSpace).GetLocation();
	return HandNow + (HandNow - ElbowNow).GetSafeNormal() * Rules->GripReachCm;
}
