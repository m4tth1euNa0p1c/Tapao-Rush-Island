// monke_island

#include "Weapons/TorchItem.h"
#include "Weapons/CutlassMath.h"
#include "Weapons/MusketMath.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** Server world time on every machine (the clock the fuel burns by) */
	double TorchServerNow(const UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}
}

ATorchItem::ATorchItem()
{
	PrimaryActorTick.bCanEverTick = true;
	// after the puppet built its pose: the torch follows the left hand
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	// nothing changes every frame any more (the fuel is computed, network review R-40): a low rate, each change forces
	// an update (Net Update Frequency stays tunable in the class defaults of the Blueprint)
	SetNetUpdateFrequency(10.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	TorchMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorchMesh"));
	TorchMesh->SetupAttachment(RootComponent);
	TorchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TorchMesh->SetCanEverAffectNavigation(false);

	FlameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlameMesh"));
	FlameMesh->SetupAttachment(TorchMesh);
	FlameMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlameMesh->SetCastShadow(false);
	FlameMesh->SetVisibility(false);

	// the carrier's own view: a torch in the left first person arm (T-330), never seen by the others
	FirstPersonTorch = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonTorch"));
	FirstPersonTorch->SetupAttachment(RootComponent);
	FirstPersonTorch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonTorch->SetCanEverAffectNavigation(false);
	FirstPersonTorch->SetOnlyOwnerSee(true);
	FirstPersonTorch->SetCastShadow(false);
	FirstPersonTorch->SetHiddenInGame(true);

	FirstPersonFlame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonFlame"));
	FirstPersonFlame->SetupAttachment(FirstPersonTorch);
	FirstPersonFlame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonFlame->SetOnlyOwnerSee(true);
	FirstPersonFlame->SetCastShadow(false);
	FirstPersonFlame->SetVisibility(false);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(TorchMesh);
	Light->SetIntensityUnits(ELightUnits::Candelas);
	Light->SetUseTemperature(true);
	Light->SetSourceRadius(4.0f);
	Light->SetSoftSourceRadius(12.0f);
	Light->SetVisibility(false);
}

void ATorchItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATorchItem, MoveState);
	DOREPLIFETIME(ATorchItem, bLit);
	DOREPLIFETIME_CONDITION(ATorchItem, bRaised, COND_SkipOwner);
	DOREPLIFETIME(ATorchItem, FuelAtChange);
	DOREPLIFETIME(ATorchItem, LitServerTime);
}

void ATorchItem::BeginPlay()
{
	SpawnTime = GetWorld()->GetTimeSeconds();
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Error, TEXT("Torch %s: no DA_TorchRules, the torch is disabled"), *GetName());
		Super::BeginPlay();
		SetActorTickEnabled(false);
		return;
	}
	TorchMesh->SetStaticMesh(Rules->TorchMesh);
	FlameMesh->SetStaticMesh(Rules->FlameMesh);
	if (Rules->FlameMaterial)
	{
		FlameMesh->SetMaterial(0, Rules->FlameMaterial);
	}
	static const FName FlameSocket(TEXT("flame"));
	static const FName LightSocket(TEXT("light"));
	FlameMesh->AttachToComponent(TorchMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TorchMesh->DoesSocketExist(FlameSocket) ? FlameSocket : NAME_None);
	Light->AttachToComponent(TorchMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, TorchMesh->DoesSocketExist(LightSocket) ? LightSocket : NAME_None);
	FirstPersonTorch->SetStaticMesh(Rules->TorchMesh);
	FirstPersonFlame->SetStaticMesh(Rules->FlameMesh);
	if (Rules->FlameMaterial)
	{
		FirstPersonFlame->SetMaterial(0, Rules->FlameMaterial);
	}
	FirstPersonFlame->AttachToComponent(FirstPersonTorch, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		FirstPersonTorch->DoesSocketExist(FlameSocket) ? FlameSocket : NAME_None);
	// the FBX sockets come in with a scale of 100 (memory: ue-mcp-import-pitfalls)
	FlameMesh->SetWorldScale3D(FVector::OneVector);
	FirstPersonFlame->SetWorldScale3D(FVector::OneVector);
	Light->SetWorldScale3D(FVector::OneVector);
	Light->SetTemperature(Rules->LightTemperature);
	Light->SetCastShadows(Rules->bLightCastsShadows);
	if (HasAuthority())
	{
		FuelAtChange = Rules->BurnTime;
	}
	Super::BeginPlay();
}

void ATorchItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (bArmsDriven && Arms.IsValid())
	{
		Arms->ClearPoseOverride();
	}
	bArmsDriven = false;
	Super::EndPlay(EndPlayReason);
}

void ATorchItem::OnAttachedToCarrier()
{
	AIslanderCharacter* Carrier = GetCarrier();
	Puppet = Carrier ? Carrier->FindComponentByClass<UBeanPuppetComponent>() : nullptr;
	if (Puppet.IsValid())
	{
		// rides on the puppet component (scale 1), placed every frame from the hand or the hip
		TorchMesh->AttachToComponent(Puppet.Get(), FAttachmentTransformRules::SnapToTargetIncludingScale);
		AddTickPrerequisiteComponent(Puppet.Get());
	}
	// the first person torch rides on the carrier's first person camera; the left first person arm holds it (T-330)
	Arms = Carrier ? Carrier->FindComponentByClass<UBeanFirstPersonArmsComponent>() : nullptr;
	if (UCameraComponent* Camera = Carrier ? Carrier->GetFirstPersonCamera() : nullptr)
	{
		FirstPersonTorch->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

bool ATorchItem::IsUsable() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!IsEquipped() || !Rules || !Carrier || Carrier->IsRagdoll())
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

float ATorchItem::GetLocalMoveElapsed() const
{
	return LocalMove == ETorchMove::None ? 0.0f : static_cast<float>(GetWorld()->GetTimeSeconds() - LocalMoveStart);
}

float ATorchItem::ComputeFuel(float InFuelAtChange, double LitTime, double Now, bool bInLit)
{
	const float Fuel = FMath::IsFinite(InFuelAtChange) ? FMath::Max(InFuelAtChange, 0.0f) : 0.0f;
	if (!bInLit)
	{
		return Fuel;
	}
	// a clock that went back (a new world) burns nothing
	return FMath::Max(0.0f, Fuel - static_cast<float>(FMath::Max(Now - LitTime, 0.0)));
}

float ATorchItem::GetFuel() const
{
	return ComputeFuel(FuelAtChange, LitServerTime, bLit ? TorchServerNow(GetWorld()) : 0.0, bLit);
}

// ---------------------------------------------------------------- equip, light

void ATorchItem::OnEquippedChanged()
{
	if (!Rules)
	{
		return;
	}
	if (!HasAuthority())
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(IgniteTimer);
	GetWorldTimerManager().ClearTimer(FlameTimer);
	if (IsEquipped())
	{
		const bool bArrival = GetWorld()->GetTimeSeconds() - SpawnTime < 0.2;
		if (!bArrival)
		{
			StartMove(ETorchMove::Equip);
		}
		// unlit: the flint and steel once it is in hand (no fuel left: it stays out); weak: never on a destroyed torch
		if (!bLit && GetFuel() > 0.0f)
		{
			GetWorldTimerManager().SetTimer(IgniteTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (IsEquipped())
				{
					StartMove(ETorchMove::Ignite);
					GetWorldTimerManager().SetTimer(FlameTimer, this, &ATorchItem::FlameOn, Rules->FlameOnTime, false);
				}
			}), bArrival ? 0.1f : Rules->EquipSeconds, false);
		}
	}
	else
	{
		// put out and hung back on the hip
		StartMove(ETorchMove::Unequip);
		PutOut();
		bRaised = false;
	}
}

void ATorchItem::FlameOn()
{
	const float Fuel = GetFuel();
	if (bLit || !IsEquipped() || Fuel <= 0.0f)
	{
		return;
	}
	// the fuel left and the time it starts to burn: every machine computes the rest (network review R-40)
	FuelAtChange = Fuel;
	LitServerTime = TorchServerNow(GetWorld());
	bLit = true;
	ForceNetUpdate();
}

void ATorchItem::PutOut()
{
	if (!bLit)
	{
		return;
	}
	FuelAtChange = GetFuel();
	bLit = false;
	ForceNetUpdate();
}

void ATorchItem::StartMove(ETorchMove Move)
{
	MoveState.Move = Move;
	++MoveState.Counter;
	ForceNetUpdate();
	PlayMove(Move);
	if (Move == ETorchMove::Swing)
	{
		GetWorldTimerManager().SetTimer(HitTimer, this, &ATorchItem::ApplySwingHit, Rules->SwingHitTime, false);
	}
}

void ATorchItem::OnRep_MoveState()
{
	PlayMove(MoveState.Move);
}

void ATorchItem::PlayMove(ETorchMove Move)
{
	LocalMove = Move;
	LocalMoveStart = GetWorld()->GetTimeSeconds();
}

// ---------------------------------------------------------------- input

void ATorchItem::BindInput()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UEnhancedInputComponent* Input = Carrier ? Cast<UEnhancedInputComponent>(Carrier->InputComponent) : nullptr;
	if (bInputBound || !Input || !Rules)
	{
		return;
	}
	if (Rules->SwingAction)
	{
		KeepInputBinding(Input, Input->BindAction(Rules->SwingAction, ETriggerEvent::Started, this, &ATorchItem::HandleSwing).GetHandle());
	}
	if (Rules->RaiseAction)
	{
		KeepInputBinding(Input, Input->BindAction(Rules->RaiseAction, ETriggerEvent::Started, this, &ATorchItem::HandleRaiseStarted).GetHandle());
		KeepInputBinding(Input, Input->BindAction(Rules->RaiseAction, ETriggerEvent::Completed, this, &ATorchItem::HandleRaiseCompleted).GetHandle());
	}
	bInputBound = true;
}

void ATorchItem::OnInputBindingsRemoved()
{
	Super::OnInputBindingsRemoved();
	bInputBound = false;
}

void ATorchItem::HandleSwing()
{
	// keys of a former carrier never drive the item someone else now holds (T-288, network review R-37)
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	if (IsUsable())
	{
		ServerSwing();
	}
}

void ATorchItem::HandleRaiseStarted()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	if (IsUsable())
	{
		bRaised = true;
		ServerSetRaised(true);
	}
}

void ATorchItem::HandleRaiseCompleted()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	bRaised = false;
	ServerSetRaised(false);
}

void ATorchItem::ServerSetRaised_Implementation(bool bInRaised)
{
	const bool bWasRaised = bRaised;
	bRaised = bInRaised && IsUsable();
	if (bRaised != bWasRaised)
	{
		ForceNetUpdate();
	}
}

void ATorchItem::ServerSwing_Implementation()
{
	if (!IsUsable() || (LocalMove != ETorchMove::None && GetLocalMoveElapsed() < GetLocalMoveDuration()))
	{
		return;
	}
	const AIslanderCharacter* Carrier = GetCarrier();
	UIslanderMovementComponent* Move = Carrier ? Carrier->GetIslanderMovement() : nullptr;
	if (!Move || !CutlassMath::HasStaminaFor(Move->GetStamina(), Move->MaxStamina, Rules->StaminaCostFraction))
	{
		return;
	}
	Move->ConsumeStamina(CutlassMath::StaminaCost(Move->MaxStamina, Rules->StaminaCostFraction));
	StartMove(ETorchMove::Swing);
}

// ---------------------------------------------------------------- blow

void ATorchItem::ApplySwingHit()
{
	AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !IsUsable())
	{
		return;
	}
	const FVector Origin = Carrier->GetActorLocation();
	const FVector Forward = FRotator(0.0f, Carrier->GetBaseAimRotation().Yaw, 0.0f).Vector();
	FCollisionObjectQueryParams Objects(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TorchSwing), false, Carrier);
	Params.AddIgnoredActor(this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Rules->ReachCm + 80.0f), Params);
	TSet<AActor*> Done;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		const APawn* Pawn = Cast<APawn>(Target);
		if (!Pawn || Pawn->IsPlayerControlled() || Done.Contains(Target))
		{
			continue;
		}
		const FVector Offset = Target->GetActorLocation() - Origin;
		if (FMath::Abs(Offset.Z) > 160.0f || !CutlassMath::IsInSweep(Forward, Offset, Rules->ReachCm + 40.0f, Rules->ArcDegrees))
		{
			continue;
		}
		Done.Add(Target);
		const FVector Direction = FVector(Offset.X, Offset.Y, 0.0f).GetSafeNormal();
		FHitResult Hit(Target, Overlap.GetComponent(), Target->GetActorLocation(), -Direction);
		UGameplayStatics::ApplyPointDamage(Target, Rules->SwingDamage, Direction, Hit, Carrier->GetController(), this, UDamageType::StaticClass());
		if (bLit && Rules->BurnPerSecond > 0.0f)
		{
			Burn(Target, FMath::RoundToInt(Rules->BurnSeconds));
		}
	}
}

void ATorchItem::Burn(TWeakObjectPtr<AActor> Target, int32 TicksLeft)
{
	if (TicksLeft <= 0)
	{
		return;
	}
	FTimerHandle Handle;
	// weak: a torch destroyed meanwhile (its carrier left) burns no more (network review R-47)
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Target, TicksLeft]()
	{
		if (AActor* Alive = Target.Get())
		{
			const AIslanderCharacter* Carrier = GetCarrier();
			UGameplayStatics::ApplyDamage(Alive, Rules->BurnPerSecond, Carrier ? Carrier->GetController() : nullptr, this, UDamageType::StaticClass());
			Burn(Target, TicksLeft - 1);
		}
	}), 1.0f, false);
}

// ---------------------------------------------------------------- per frame

void ATorchItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules || !GetCarrier())
	{
		return;
	}
	const bool bUsable = IsUsable();
	if (IsCarrierLocallyControlled())
	{
		BindInput();
		SetInputContextActive(Rules->MappingContext, Rules->MappingPriority, bUsable);
		if (!bUsable)
		{
			bRaised = false;
		}
	}
	if (LocalMove != ETorchMove::None && GetLocalMoveElapsed() >= GetLocalMoveDuration())
	{
		LocalMove = ETorchMove::None;
	}

	if (HasAuthority())
	{
		if (!bUsable && bRaised)
		{
			bRaised = false;
			ForceNetUpdate();
		}
		if (bLit)
		{
			// out of fuel, or the head under the water
			const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
			const FVector Head = FlameMesh->GetComponentLocation();
			if (GetFuel() <= 0.0f || (Water && Head.Z < Water->GetWaterSurfaceAt(Head)) || !IsEquipped())
			{
				PutOut();
			}
		}
	}
	UpdateAttachment();
	UpdateFirstPerson(DeltaSeconds);
	UpdateFlame(DeltaSeconds);
}

void ATorchItem::UpdateAttachment()
{
	UBeanPuppetComponent* Body = Puppet.Get();
	if (!Body)
	{
		return;
	}
	// in the hand from the grab of the equip until the hook of the unequip; on the hip otherwise
	bool bInHand = IsEquipped();
	if (LocalMove == ETorchMove::Equip)
	{
		bInHand = GetLocalMoveElapsed() >= Rules->AttachToHandTime;
	}
	else if (LocalMove == ETorchMove::Unequip)
	{
		bInHand = GetLocalMoveElapsed() < Rules->AttachToHipTime;
	}
	const FName Bone = bInHand ? Rules->GripBone : Rules->HipBone;
	if (Body->GetBoneIndex(Bone) == INDEX_NONE)
	{
		return;
	}
	// component space bones carry the FBX root scale (x100): only location and rotation are used
	const FTransform BoneCS = Body->GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace);
	const FTransform Relative = (bInHand ? Rules->GripInHand : Rules->OnHip) * FTransform(BoneCS.GetRotation(), BoneCS.GetLocation());
	TorchMesh->SetRelativeTransform(Relative);
	const AIslanderCharacter* Carrier = GetCarrier();
	// only seen while in use: the hip keeps no torch on the skin (owner request 2026-10-04)
	const bool bShow = bInHand && !(Carrier && Carrier->IsRagdoll());
	if (TorchMesh->bHiddenInGame == bShow)
	{
		TorchMesh->SetHiddenInGame(!bShow, true);
	}
}

FTransform ATorchItem::ComputeFirstPersonPose() const
{
	// held, raised by the raise key
	FTransform Pose;
	Pose.Blend(Rules->FirstPersonHeld, Rules->FirstPersonRaised, FMath::Clamp(RaiseAlpha, 0.0f, 1.0f));

	const auto Ease = [](const FTransform& From, const FTransform& To, float Alpha)
	{
		FTransform Out;
		Out.Blend(From, To, FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f)));
		return Out;
	};
	const float Duration = GetLocalMoveDuration();
	const float Elapsed = GetLocalMoveElapsed();
	switch (LocalMove)
	{
	case ETorchMove::Equip:
		// comes up from below the view once the hand has taken it
		return Ease(Rules->FirstPersonLowered, Pose, (Elapsed - Rules->AttachToHandTime) / FMath::Max(Duration - Rules->AttachToHandTime, 0.05f));
	case ETorchMove::Unequip:
		return Ease(Pose, Rules->FirstPersonLowered, Elapsed / FMath::Max(Rules->AttachToHipTime, 0.05f));
	case ETorchMove::Swing:
		{
			// drawn back on the left, across the view at the hit, back in the hand
			const float Hit = FMath::Clamp(Rules->SwingHitTime, 0.05f, FMath::Max(Duration, 0.05f));
			const float Back = Hit * 0.45f;
			if (Elapsed < Back)
			{
				return Ease(Pose, Rules->FirstPersonSwingFrom, Elapsed / Back);
			}
			if (Elapsed < Hit)
			{
				return Ease(Rules->FirstPersonSwingFrom, Rules->FirstPersonSwingTo, (Elapsed - Back) / (Hit - Back));
			}
			return Ease(Rules->FirstPersonSwingTo, Pose, (Elapsed - Hit) / FMath::Max(Duration - Hit, 0.05f));
		}
	default:
		return Pose;
	}
}

void ATorchItem::UpdateFirstPerson(float DeltaSeconds)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const bool bInHand = IsEquipped() || LocalMove == ETorchMove::Unequip;
	const bool bShow = Rules->bFirstPersonViewModel && Carrier && IsCarrierLocallyControlled() && Carrier->IsFirstPerson()
		&& !Carrier->IsRagdoll() && bInHand && FirstPersonTorch->GetAttachParent() != RootComponent;
	const bool bWasShown = !FirstPersonTorch->bHiddenInGame;
	if (bShow != bWasShown)
	{
		FirstPersonTorch->SetHiddenInGame(!bShow);
		FirstPersonFlame->SetHiddenInGame(!bShow);
		// the body's torch stays for the others, its shadow too; its carrier sees the first person one
		TorchMesh->SetOwnerNoSee(bShow);
		FlameMesh->SetOwnerNoSee(bShow);
		TorchMesh->SetCastHiddenShadow(bShow);
	}
	// the light shines from the flame the carrier sees (the body's torch is beside its head, out of its view)
	if (bShow != bLightOnFirstPerson)
	{
		static const FName LightSocket(TEXT("light"));
		UStaticMeshComponent* Holder = bShow ? FirstPersonTorch.Get() : TorchMesh.Get();
		Light->AttachToComponent(Holder, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Holder->DoesSocketExist(LightSocket) ? LightSocket : NAME_None);
		Light->SetWorldScale3D(FVector::OneVector);
		bLightOnFirstPerson = bShow;
	}
	if (!bShow)
	{
		if (bArmsDriven && Arms.IsValid())
		{
			Arms->ClearPoseOverride();
		}
		bArmsDriven = false;
		BobWeight = 0.0f;
		return;
	}

	// the pose of the move or of the raise, and a small bob at the pace of the steps
	FTransform Shown = ComputeFirstPersonPose();
	const UCharacterMovementComponent* Movement = Carrier->GetCharacterMovement();
	const float Speed = Movement ? static_cast<float>(Movement->Velocity.Size2D()) : 0.0f;
	const float SpeedWeight = CutlassMath::BobTargetWeight(Speed, Rules->FirstPersonBobReferenceSpeed, Movement && Movement->IsMovingOnGround());
	BobPhase = FMath::Fmod(BobPhase + DeltaSeconds * Rules->FirstPersonBobFrequencyHz * SpeedWeight, 1.0f);
	BobWeight = FMath::FInterpTo(BobWeight, SpeedWeight, DeltaSeconds, Rules->FirstPersonPoseBlendSpeed);
	Shown.AddToTranslation(CutlassMath::ViewBob(BobPhase, BobWeight, Rules->FirstPersonBobVerticalCm, Rules->FirstPersonBobLateralCm, 0.0f).Location);
	FirstPersonPose = Shown;
	FirstPersonTorch->SetRelativeTransform(Shown);

	// the left first person arm on the grip (rubber arm from its shoulder, as the sabre); the right one rests out of view
	if (Arms.IsValid())
	{
		const FBeanArmPose Left = MusketMath::MirrorArm(MusketMath::SolveArm(Rules->FirstPersonLeftShoulder, Shown.GetLocation(),
			Rules->FirstPersonArmHandPoint, Rules->FirstPersonArmMaxStretch));
		Arms->SetPoseOverride(Arms->RestPose, Left);
		bArmsDriven = true;
	}
}

void ATorchItem::UpdateFlame(float DeltaSeconds)
{
	RaiseAlpha = FMath::FInterpConstantTo(RaiseAlpha, bRaised ? 1.0f : 0.0f, DeltaSeconds, 1.0f / Rules->RaiseBlendTime);
	if (FlameMesh->IsVisible() != bLit)
	{
		FlameMesh->SetVisibility(bLit);
		Light->SetVisibility(bLit);
	}
	if (FirstPersonFlame->IsVisible() != bLit)
	{
		FirstPersonFlame->SetVisibility(bLit);
	}
	if (!bLit)
	{
		return;
	}
	// flicker: a slow swell, a fast noise; the flame weakens at the end of the fuel
	const double T = GetWorld()->GetTimeSeconds();
	const float Noise = FMath::PerlinNoise1D(static_cast<float>(T * Rules->FlickerNoiseHz));
	float Flicker = Rules->Flicker.X + Rules->Flicker.Y * FMath::Sin(UE_TWO_PI * Rules->Flicker.Z * T) + Rules->Flicker.W * Noise;
	const float Fuel = GetFuel();
	if (Rules->WarningTime > 0.0f && Fuel < Rules->WarningTime)
	{
		Flicker *= FMath::Lerp(0.45f, 1.0f, Fuel / Rules->WarningTime);
	}
	Light->SetIntensity(FMath::Lerp(Rules->LightIntensity, Rules->RaisedLightIntensity, RaiseAlpha) * Flicker);
	Light->SetAttenuationRadius(FMath::Lerp(Rules->LightRadius, Rules->RaisedLightRadius, RaiseAlpha));
	Light->SetRelativeLocation(FVector(FMath::PerlinNoise1D(T * 6.0 + 3.1), FMath::PerlinNoise1D(T * 6.0 + 7.7), 0.0));
}
