// monke_island

#include "Weapons/CutlassWeapon.h"
#include "Weapons/CutlassMath.h"

#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"
#include "AnimationRuntime.h"
#include "Camera/CameraComponent.h"
#include "Character/Bean/BeanAnimDriverComponent.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
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

	/** Server tolerance on the end of a move: the carrier's clock runs a little ahead of the server's (s) */
	constexpr double MoveEndTolerance = 0.12;

	/** Server tolerance on the finisher charge (s) */
	constexpr double ChargeTolerance = 0.15;

	/** Life of a damage number (s) */
	constexpr double HitMarkerLife = 1.2;
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

float ACutlassWeapon::GetLocalMoveElapsed() const
{
	return LocalMove == ECutlassMove::None ? 0.0f : static_cast<float>(GetWorld()->GetTimeSeconds() - LocalMoveStart);
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
		Input->BindAction(AttackAction, ETriggerEvent::Started, this, &ACutlassWeapon::HandleAttackStarted);
		Input->BindAction(AttackAction, ETriggerEvent::Completed, this, &ACutlassWeapon::HandleAttackCompleted);
	}
	if (ParryAction)
	{
		Input->BindAction(ParryAction, ETriggerEvent::Started, this, &ACutlassWeapon::HandleParryStarted);
		Input->BindAction(ParryAction, ETriggerEvent::Completed, this, &ACutlassWeapon::HandleParryCompleted);
	}
	bInputBound = true;
}

void ACutlassWeapon::HandleAttackStarted()
{
	if (!IsUsable() || bParrying)
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
	SetParrying(true);
}

void ACutlassWeapon::HandleParryCompleted()
{
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
	const bool bBusy = LocalMove != ECutlassMove::None && Now < LocalMoveStart + Rules->GetMoveDuration(LocalMove);
	if (bBusy)
	{
		// one click is kept and played when the blow ends
		bQueuedAttack = !bFinisher;
		return;
	}
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
		&& GetWorld()->GetTimeSeconds() >= ServerMoveEnd - MoveEndTolerance
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

	UIslanderMovementComponent* CarrierMove = GetCarrierMovement();
	CarrierMove->ConsumeStamina(CutlassMath::StaminaCost(CarrierMove->MaxStamina, Rules->StaminaCostFraction));

	ServerMove = Move;
	ServerMoveEnd = Now + Rules->GetMoveDuration(Move);
	if (Move != ECutlassMove::Finisher)
	{
		LastComboMove = Move;
		LastComboEnd = ServerMoveEnd;
	}
	MoveState.Move = Move;
	++MoveState.Counter;
	// the server is the carrier (host) or watches another player: OnRep does not run here
	PlayMove(Move);
	GetWorldTimerManager().SetTimer(HitTimer, this, &ACutlassWeapon::ApplyHit, FMath::Max(Attack->HitTime, 0.01f), false);
}

void ACutlassWeapon::ClientRejectAttack_Implementation()
{
	// back to the guard; the stamina comes back with the server's correction
	LocalMove = ECutlassMove::None;
	bQueuedAttack = false;
}

void ACutlassWeapon::ApplyHit()
{
	AIslanderCharacter* Carrier = GetCarrier();
	const FCutlassAttack* Attack = Rules ? Rules->GetAttack(ServerMove) : nullptr;
	if (!Carrier || !Attack || !IsUsable())
	{
		return;
	}

	const FVector Origin = Carrier->GetActorLocation();
	const FVector Forward = FRotator(0.0f, Carrier->GetBaseAimRotation().Yaw, 0.0f).Vector();
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

	TArray<FVector_NetQuantize> Locations;
	TArray<float> Damages;
	TArray<bool> Kills;
	TSet<const AActor*> Done;
	// cuts carry their own damage type: a creature killed by one, or a corpse hit by one, may fall in two halves
	const TSubclassOf<UDamageType> DamageType = UDamageType::StaticClass();
	FVector StruckBodyLocation = FVector::ZeroVector;
	bool bStruckBody = false;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Done.Contains(Target))
		{
			continue;
		}
		const FVector Offset = Target->GetActorLocation() - Origin;
		const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
		const float TargetRadius = TargetCharacter && TargetCharacter->GetCapsuleComponent()
			? FMath::Min(TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius(), Rules->TargetRadiusAllowance)
			: Rules->TargetRadiusAllowance * 0.5f;
		if (FMath::Abs(Offset.Z) > 160.0f || !CutlassMath::IsInSweep(Forward, Offset, Rules->ReachCm + TargetRadius, Attack->ArcDegrees))
		{
			continue;
		}
		Done.Add(Target);

		const APawn* TargetPawn = Cast<APawn>(Target);
		const bool bFriendly = TargetPawn && TargetPawn->IsPlayerControlled();
		if (bFriendly && !Rules->bFriendlyFire)
		{
			continue;
		}

		const FVector Direction = FVector(Offset.X, Offset.Y, 0.0f).GetSafeNormal();
		if (TargetPawn)
		{
			const UIslanderHealthComponent* Health = Target->FindComponentByClass<UIslanderHealthComponent>();
			const bool bWasAlive = !Health || !Health->IsDead();
			const float Damage = CutlassMath::BlowDamage(Attack->Damage, Sharpness, Rules->DullThreshold, Rules->DullDamageMultiplier);
			FHitResult Hit(Target, Overlap.GetComponent(), Target->GetActorLocation(), -Direction);
			UGameplayStatics::ApplyPointDamage(Target, Damage, Direction, Hit, Carrier->GetController(), this, DamageType);
			const bool bKill = bWasAlive && Health && Health->IsDead();
			if (ACharacter* HitCharacter = Cast<ACharacter>(Target); HitCharacter && !bKill)
			{
				HitCharacter->LaunchCharacter(Direction * Rules->KnockbackSpeed + FVector(0.0f, 0.0f, 60.0f), true, false);
			}
			Sharpness = FMath::Max(0.0f, Sharpness - Rules->SharpnessLossPerHit);
			Locations.Add(Target->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f));
			Damages.Add(Damage);
			Kills.Add(bKill && Attack->bCut);
			UE_LOG(Logmonke_island, Log, TEXT("Cutlass: %s hits %s for %.0f%s"), *Carrier->GetName(), *Target->GetName(), Damage, bKill ? TEXT(" (kill)") : TEXT(""));
		}
		else if (UPrimitiveComponent* Body = Overlap.GetComponent(); Body && Body->IsSimulatingPhysics())
		{
			// loose bodies (a corpse may split in two on a cut) take the blow, then the push
			const float Damage = CutlassMath::BlowDamage(Attack->Damage, Sharpness, Rules->DullThreshold, Rules->DullDamageMultiplier);
			FHitResult Hit(Target, Body, Body->GetComponentLocation(), -Direction);
			UGameplayStatics::ApplyPointDamage(Target, Damage, Direction, Hit, Carrier->GetController(), this, DamageType);
			if (IsValid(Body) && Body->IsSimulatingPhysics())
			{
				Body->AddImpulseAtLocation(Direction * Rules->PhysicsImpulse, Body->GetComponentLocation());
			}
			if (!bStruckBody)
			{
				bStruckBody = true;
				StruckBodyLocation = Hit.ImpactPoint;
			}
		}
	}

	if (Locations.Num() == 0 && bStruckBody)
	{
		MulticastBladeSound(ECutlassSound::Hit, StruckBodyLocation);
	}
	if (Locations.Num() > 0)
	{
		MulticastBladeSound(Kills.Contains(true) ? ECutlassSound::Kill : ECutlassSound::Hit, Locations[0]);
		if (IsCarrierLocallyControlled())
		{
			ClientShowHits_Implementation(Locations, Damages, Kills);
		}
		else
		{
			ClientShowHits(Locations, Damages, Kills);
		}
	}
}

void ACutlassWeapon::ClientShowHits_Implementation(const TArray<FVector_NetQuantize>& Locations, const TArray<float>& Damages, const TArray<bool>& Kills)
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
}

void ACutlassWeapon::SetParrying(bool bInParrying)
{
	// the carrier raises the blade at once; the server keeps it for the damage and the other players
	bParrying = bInParrying && IsUsable();
	if (!HasAuthority())
	{
		ServerSetParrying(bParrying);
	}
}

void ACutlassWeapon::ServerSetParrying_Implementation(bool bInParrying)
{
	bParrying = bInParrying && IsUsable();
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
	const bool bHasStamina = CutlassMath::HasStaminaFor(Move->GetStamina(), Move->MaxStamina, Rules->StaminaCostFraction);
	const float Result = CutlassMath::ParriedDamage(Damage, true, FacingDot, Rules->ParryFrontDot, Rules->ParryReduction, bHasStamina);
	if (Result < Damage)
	{
		Move->ConsumeStamina(CutlassMath::StaminaCost(Move->MaxStamina, Rules->StaminaCostFraction));
		MulticastBladeSound(ECutlassSound::Parry, Carrier->GetActorLocation() + Carrier->GetActorForwardVector() * 40.0f + FVector(0.0f, 0.0f, 30.0f));
	}
	return Result;
}

void ACutlassWeapon::OnEquippedChanged()
{
	if (!Rules)
	{
		return;
	}
	const bool bArrival = GetWorld()->GetTimeSeconds() - SpawnTime < 0.2;
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
			ServerMoveEnd = GetWorld()->GetTimeSeconds() + Rules->DrawSeconds;
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
		bQueuedAttack = false;
		LocalChargeStart = -1.0;
		if (HasAuthority())
		{
			GetWorldTimerManager().ClearTimer(HitTimer);
			ServerMove = ECutlassMove::None;
			ServerMoveEnd = 0.0;
		}
	}
}

void ACutlassWeapon::OnRep_MoveState()
{
	PlayMove(MoveState.Move);
}

void ACutlassWeapon::PlayMove(ECutlassMove Move)
{
	LocalMove = Move;
	LocalMoveStart = GetWorld()->GetTimeSeconds();
	if (Rules && Rules->GetAttack(Move))
	{
		if (Move != ECutlassMove::Finisher)
		{
			LocalLastCombo = Move;
			LocalLastComboEnd = LocalMoveStart + Rules->GetMoveDuration(Move);
		}
		PlayBladeSound(Move == ECutlassMove::Finisher ? ECutlassSound::FinisherSwing : ECutlassSound::Swing, GetActorLocation());
	}
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
	const double Now = GetWorld()->GetTimeSeconds();
	if (LocalMove != ECutlassMove::None)
	{
		const TArray<FCutlassKey>* Keys = Rules->GetKeys(LocalMove);
		const float Duration = Rules->GetMoveDuration(LocalMove);
		const float Elapsed = static_cast<float>(Now - LocalMoveStart);
		if (Keys && Keys->Num() > 0 && Elapsed < Duration)
		{
			// draw and sheathe are stretched to their tunable time
			const float TrackLength = FMath::Max(Keys->Last().Time, 0.01f);
			return CutlassMath::SampleKeys(*Keys, Elapsed * TrackLength / FMath::Max(Duration, 0.01f), Rules->GuardKey);
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

	if (HasAuthority() && !bUsable)
	{
		bParrying = false;
	}

	if (IsCarrierLocallyControlled())
	{
		BindInput();
		SetInputContextActive(CutlassMappingContext, CutlassMappingPriority, bUsable);
		if (!bUsable)
		{
			bParrying = false;
			LocalChargeStart = -1.0;
		}
		// held long enough: the finisher leaves without waiting for the release
		if (LocalChargeStart >= 0.0 && !bChargeSpent && Now - LocalChargeStart >= Rules->FinisherChargeSeconds)
		{
			bChargeSpent = true;
			RequestAttack(true);
		}
		// a click during the last blow
		if (bQueuedAttack && (LocalMove == ECutlassMove::None || Now >= LocalMoveStart + Rules->GetMoveDuration(LocalMove)))
		{
			bQueuedAttack = false;
			RequestAttack(false);
		}
	}

	if (LocalMove != ECutlassMove::None && Now >= LocalMoveStart + Rules->GetMoveDuration(LocalMove))
	{
		LocalMove = ECutlassMove::None;
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

	// first person with the library: the owner sees the body's own arms, the blade stays in the body's hand
	const bool bBodyArms = AnimDriver.IsValid() && AnimDriver->ShowsBodyArmsInFirstPerson();
	const bool bFirstPersonView = IsCarrierLocallyControlled() && Carrier->IsFirstPerson() && !Carrier->IsRagdoll() && !bBodyArms;
	UpdateFirstPerson(bDrawn && bUsable && bFirstPersonView);
	UpdateThirdPerson(!bFirstPersonView || !bUsable, bDrawn && !bFirstPersonView && !bOtherItem && !Carrier->IsRagdoll());
}

void ACutlassWeapon::UpdateFirstPerson(bool bShow)
{
	if (!bShow)
	{
		if (!FirstPersonMesh->bHiddenInGame)
		{
			FirstPersonMesh->SetHiddenInGame(true);
		}
		if (bArmsDriven && Arms.IsValid())
		{
			Arms->ClearPoseOverride();
		}
		bArmsDriven = false;
		return;
	}
	FirstPersonMesh->SetHiddenInGame(false);

	// bean mesh space -> first person camera: the camera sits at the eyes, looking along the mesh +Y
	const FVector Eye = Puppet.IsValid() ? Puppet->FirstPersonEyeLocation : FVector(0.0, 27.0, 82.0);
	const FTransform EyeInMesh(FRotator(0.0f, 90.0f, 0.0f), Eye);
	const FTransform KeyInMesh(CurrentPose.GripRotation, CurrentPose.GripLocation);
	FTransform InCamera = KeyInMesh.GetRelativeTransform(EyeInMesh);
	InCamera.AddToTranslation(Rules->FirstPersonOffset);
	FirstPersonMesh->SetRelativeTransform(InCamera);

	if (Arms.IsValid())
	{
		// right arm from its shoulder to the grip; left arm out of view
		const FVector Grip = InCamera.GetLocation();
		const FVector Direction = (Grip - Rules->FirstPersonShoulder).GetSafeNormal();
		const FBeanArmPose Right(Grip - Direction * Rules->FirstPersonArmLength, Direction.Rotation());
		Arms->SetPoseOverride(Right, Arms->RestPose);
		bArmsDriven = true;
	}
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
	ThirdPersonMesh->SetRelativeTransform(BladeOnBody);
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
