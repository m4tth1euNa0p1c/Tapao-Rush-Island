// monke_island

#include "Items/Bandage/BandageItem.h"
#include "Items/Bandage/BandageMath.h"
#include "Character/Bean/BeanAnimDriverComponent.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/MusketMath.h"
#include "monke_island.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** The owner frees its movement keys this long after an application should have ended, if the server's end never
	 *  came (s) */
	constexpr float MoveHoldMarginSeconds = 0.5f;

	/** Shortest application and shortest delay of its timers (s) */
	constexpr float MinApplySeconds = 0.2f;
	constexpr float MinTimerSeconds = 0.01f;
}

ABandageItem::ABandageItem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// after the puppet built its pose: the roll follows the right hand
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	// nothing changes every frame: a low rate, each change forces an update
	SetNetUpdateFrequency(10.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	const auto MakeMesh = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Mesh->SetupAttachment(RootComponent);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetHiddenInGame(true);
		return Mesh;
	};
	RollMesh = MakeMesh(TEXT("RollMesh"));
	WrappedMesh = MakeMesh(TEXT("WrappedMesh"));
	FirstPersonRoll = MakeMesh(TEXT("FirstPersonRoll"));
	FirstPersonRoll->SetOnlyOwnerSee(true);
	FirstPersonRoll->SetCastShadow(false);
}

void ABandageItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABandageItem, ApplyState);
}

void ABandageItem::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(IsEquipped());
}

void ABandageItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndApplicationBookkeeping();
	SetMoveHold(false);
	if (PlayingClip.IsValid())
	{
		const AIslanderCharacter* Carrier = GetCarrier();
		if (UBeanAnimDriverComponent* Driver = Carrier ? Carrier->FindComponentByClass<UBeanAnimDriverComponent>() : nullptr)
		{
			Driver->StopFullBody(PlayingClip.Get());
		}
		PlayingClip.Reset();
	}
	if (bArmsDriven && Arms.IsValid())
	{
		Arms->ClearPoseOverride();
	}
	bArmsDriven = false;
	Super::EndPlay(EndPlayReason);
}

void ABandageItem::OnAttachedToCarrier()
{
	AIslanderCharacter* Carrier = GetCarrier();
	Puppet = Carrier ? Carrier->FindComponentByClass<UBeanPuppetComponent>() : nullptr;
	Arms = Carrier ? Carrier->FindComponentByClass<UBeanFirstPersonArmsComponent>() : nullptr;
	if (UBeanPuppetComponent* Body = Puppet.Get())
	{
		// ride on the puppet component (scale 1), placed every frame from their bones
		RollMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetIncludingScale);
		WrappedMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetIncludingScale);
		AddTickPrerequisiteComponent(Body);
	}
	if (UCameraComponent* Camera = Carrier ? Carrier->GetFirstPersonCamera() : nullptr)
	{
		FirstPersonRoll->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

void ABandageItem::OnEquippedChanged()
{
	SetActorTickEnabled(true);
	if (!IsEquipped())
	{
		RollMesh->SetHiddenInGame(true);
		FirstPersonRoll->SetHiddenInGame(true);
		if (bArmsDriven && Arms.IsValid())
		{
			Arms->ClearPoseOverride();
		}
		bArmsDriven = false;
		// put away: the keys go back to the interaction and the punch
		if (IsCarrierLocallyControlled())
		{
			SetInputContextActive(UseContext, 0, false);
		}
		// put away, dropped or the last bandage used: the application ends (a healed one only finishes)
		if (HasAuthority() && ApplyState.bApplying)
		{
			if (ApplyState.bHealed)
			{
				FinishApplying();
			}
			else
			{
				Interrupt(EBandageInterrupt::PutAway);
			}
		}
	}
}

void ABandageItem::OnInputBindingsRemoved()
{
	Super::OnInputBindingsRemoved();
	bInputBound = false;
}

// ---------------------------------------------------------------- state

double ABandageItem::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

float ABandageItem::GetApplyElapsed() const
{
	return ApplyState.bApplying ? FMath::Max(0.0f, static_cast<float>(GetServerNow() - ApplyState.StartServerTime)) : 0.0f;
}

float ABandageItem::GetApplyProgress() const
{
	return ApplyState.bApplying ? BandageMath::Progress(GetServerNow(), ApplyState.StartServerTime, ApplyState.Seconds) : 0.0f;
}

BandageMath::FCarrierFacts ABandageItem::GatherFacts() const
{
	BandageMath::FCarrierFacts Facts;
	Facts.bEquipped = IsEquipped();
	Facts.bBusy = ApplyState.bApplying;
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier)
	{
		Facts.bCarried = false;
		Facts.bAlive = false;
		return Facts;
	}
	const UIslanderInventoryComponent* Inventory = Carrier->FindComponentByClass<UIslanderInventoryComponent>();
	Facts.bCarried = Inventory && Inventory->FindSlotOfItem(this) != INDEX_NONE;
	if (const UIslanderHealthComponent* Health = Carrier->GetHealthComponent())
	{
		Facts.bAlive = !Health->IsDead();
		Facts.Health = Health->GetHealth();
		Facts.MaxHealth = Health->MaxHealth;
	}
	else
	{
		Facts.bAlive = false;
	}
	Facts.bRagdoll = Carrier->IsRagdoll();
	if (const UIslanderMovementComponent* Move = Carrier->GetIslanderMovement())
	{
		Facts.bOnGround = Move->IsMovingOnGround();
		Facts.bSwimming = Move->IsSwimming();
		Facts.WaterDepthCm = Move->GetWaterDepthAtFeet();
		Facts.bSprinting = Move->IsSprinting();
		Facts.Speed2D = static_cast<float>(Move->Velocity.Size2D());
	}
	else
	{
		Facts.bOnGround = false;
	}
	return Facts;
}

EBandageRefusal ABandageItem::JudgeStart() const
{
	return Rules ? BandageMath::JudgeStart(GatherFacts(), *Rules) : EBandageRefusal::NoRules;
}

int32 ABandageItem::GetStackCount() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const UIslanderInventoryComponent* Inventory = Carrier ? Carrier->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	return Inventory ? Inventory->GetCount(Inventory->FindSlotOfItem(this)) : 0;
}

// ---------------------------------------------------------------- request (owner) and server

void ABandageItem::RequestApply()
{
	if (HasAuthority())
	{
		ServerApply_Implementation();
		return;
	}
	// keys of a former carrier never drive the item someone else now holds (T-288, network review R-37)
	if (IsCarrierLocallyControlled())
	{
		ServerApply();
	}
}

void ABandageItem::ServerApply_Implementation()
{
	if (!Rules || !ConsumeRequest())
	{
		return;
	}
	// the server's own check of its carrier (the client only asked)
	const EBandageRefusal Refusal = JudgeStart();
	if (Refusal != EBandageRefusal::None)
	{
		// a refusal of the rules (security.md §2): logged, nothing happens, no disconnection
		UE_LOG(Logmonke_island, Log, TEXT("%s: bandage refused (%s)"), *GetNameSafe(GetCarrier()), *UEnum::GetValueAsString(Refusal));
		NotifyCarrier(Rules->GetRefusalText(Refusal), Refusal != EBandageRefusal::FullHealth);
		return;
	}
	StartApplying();
}

bool ABandageItem::ConsumeRequest()
{
	const UWorld* World = GetWorld();
	if (!World || !Rules)
	{
		return false;
	}
	if (RequestLimiter.Consume(World->GetTimeSeconds(), Rules->RequestBurst, Rules->RequestsPerSecond))
	{
		return true;
	}
	UE_LOG(Logmonke_island, Verbose, TEXT("%s: bandage request refused, too many at once (%d refused so far)"), *GetNameSafe(GetCarrier()),
		RequestLimiter.Refused);
	return false;
}

void ABandageItem::StartApplying()
{
	if (!HasAuthority() || !Rules)
	{
		return;
	}
	EndApplicationBookkeeping();
	const FBandageApplyRep Previous = ApplyState;
	ApplyState.bApplying = true;
	ApplyState.bHealed = false;
	ApplyState.Interrupt = EBandageInterrupt::None;
	ApplyState.StartServerTime = GetServerNow();
	ApplyState.Seconds = FMath::Max(Rules->ApplySeconds, MinApplySeconds);
	++ApplyState.Serial;
	ForceNetUpdate();
	SetActorTickEnabled(true);

	// a blow on the carrier cuts it
	if (AIslanderCharacter* Carrier = GetCarrier())
	{
		if (UIslanderHealthComponent* Health = Carrier->GetHealthComponent())
		{
			Health->OnHealthChanged.AddUniqueDynamic(this, &ABandageItem::HandleCarrierHealthChanged);
			BoundHealth = Health;
		}
	}
	FTimerManager& Timers = GetWorldTimerManager();
	const float HealAt = FMath::Clamp(Rules->ToApplyTime(Rules->HealEventSeconds), MinTimerSeconds, ApplyState.Seconds);
	Timers.SetTimer(HealTimer, this, &ABandageItem::LandHeal, HealAt, false);
	Timers.SetTimer(FinishTimer, this, &ABandageItem::FinishApplying, ApplyState.Seconds, false);
	UE_LOG(Logmonke_island, Log, TEXT("%s: applies a bandage (%.1f s, heal at %.2f s)"), *GetNameSafe(GetCarrier()), ApplyState.Seconds, HealAt);
	PlayLocally(Previous);
}

void ABandageItem::HandleCarrierHealthChanged(float NewHealth, float Delta)
{
	if (!HasAuthority() || !Rules || !ApplyState.bApplying || ApplyState.bHealed || Delta >= 0.0f)
	{
		return;
	}
	const EBandageInterrupt Reason = BandageMath::CheckInterrupt(GatherFacts(), *Rules, -Delta);
	if (Reason != EBandageInterrupt::None)
	{
		Interrupt(Reason);
	}
}

void ABandageItem::LandHeal()
{
	if (!HasAuthority() || !Rules || !ApplyState.bApplying || ApplyState.bHealed)
	{
		return;
	}
	// the facts of this very moment (between two ticks)
	const EBandageInterrupt Reason = BandageMath::CheckInterrupt(GatherFacts(), *Rules);
	AIslanderCharacter* Carrier = GetCarrier();
	UIslanderHealthComponent* Health = Carrier ? Carrier->GetHealthComponent() : nullptr;
	if (Reason != EBandageInterrupt::None || !Health)
	{
		Interrupt(Reason != EBandageInterrupt::None ? Reason : EBandageInterrupt::Down);
		return;
	}
	const float Healed = Health->ApplyHeal(Rules->ComputeHeal(Health->GetHealth(), Health->MaxHealth), this);
	const FBandageApplyRep Previous = ApplyState;
	ApplyState.bHealed = true;
	++ApplyState.Serial;
	ForceNetUpdate();
	PlayLocally(Previous);
	UE_LOG(Logmonke_island, Log, TEXT("%s: bandage tied, %.0f health back (%.0f / %.0f)"), *GetNameSafe(Carrier), Healed, Health->GetHealth(), Health->MaxHealth);

	// one bandage of the stack is used now; last: the last one destroys this actor (its gesture ends with it)
	UIslanderInventoryComponent* Inventory = Carrier->FindComponentByClass<UIslanderInventoryComponent>();
	if (!Inventory || !Inventory->RemoveItem(this))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: the bandage healed but was in no inventory, it goes"), *GetName());
		Destroy();
	}
}

void ABandageItem::FinishApplying()
{
	EndApplicationBookkeeping();
	if (!HasAuthority() || !ApplyState.bApplying)
	{
		return;
	}
	const FBandageApplyRep Previous = ApplyState;
	ApplyState.bApplying = false;
	++ApplyState.Serial;
	ForceNetUpdate();
	PlayLocally(Previous);
}

void ABandageItem::Interrupt(EBandageInterrupt Reason)
{
	if (!HasAuthority() || !ApplyState.bApplying)
	{
		return;
	}
	// already tied: the bandage is used, the gesture only stops
	if (ApplyState.bHealed)
	{
		FinishApplying();
		return;
	}
	EndApplicationBookkeeping();
	const FBandageApplyRep Previous = ApplyState;
	ApplyState.bApplying = false;
	ApplyState.Interrupt = Reason == EBandageInterrupt::None ? EBandageInterrupt::Down : Reason;
	++ApplyState.Serial;
	ForceNetUpdate();
	UE_LOG(Logmonke_island, Log, TEXT("%s: bandage cut (%s), nothing used"), *GetNameSafe(GetCarrier()), *UEnum::GetValueAsString(ApplyState.Interrupt));
	PlayLocally(Previous);
}

void ABandageItem::EndApplicationBookkeeping()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealTimer);
		World->GetTimerManager().ClearTimer(FinishTimer);
	}
	if (UIslanderHealthComponent* Health = BoundHealth.Get())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &ABandageItem::HandleCarrierHealthChanged);
	}
	BoundHealth.Reset();
}

void ABandageItem::NotifyCarrier(const FText& Text, bool bWarning)
{
	if (!HasAuthority() || Text.IsEmpty())
	{
		return;
	}
	if (IsCarrierLocallyControlled())
	{
		// listen host: no RPC to itself
		ClientNotice_Implementation(Text, bWarning);
		return;
	}
	ClientNotice(Text, bWarning);
}

void ABandageItem::ClientNotice_Implementation(const FText& Text, bool bWarning)
{
	const UWorld* World = GetWorld();
	if (!World || !Rules)
	{
		return;
	}
	NoticeText = Text;
	bNoticeWarning = bWarning;
	NoticeUntil = World->GetTimeSeconds() + Rules->NoticeSeconds;
	SetActorTickEnabled(true);
}

// ---------------------------------------------------------------- every machine

void ABandageItem::OnRep_ApplyState(const FBandageApplyRep& Previous)
{
	PlayLocally(Previous);
}

void ABandageItem::PlayLocally(const FBandageApplyRep& Previous)
{
	if (!Rules)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	AIslanderCharacter* Carrier = GetCarrier();
	UBeanAnimDriverComponent* Driver = Carrier ? Carrier->FindComponentByClass<UBeanAnimDriverComponent>() : nullptr;
	SetActorTickEnabled(true);

	// the clip: started with a new application (as far in as the server is), stopped at its end
	const bool bNewStart = ApplyState.bApplying && (!Previous.bApplying || Previous.StartServerTime != ApplyState.StartServerTime);
	if (!ApplyState.bApplying || bNewStart)
	{
		if (Driver && PlayingClip.IsValid())
		{
			Driver->StopFullBody(PlayingClip.Get());
		}
		PlayingClip.Reset();
	}
	if (bNewStart && Driver && Rules->ApplyClip)
	{
		const float Late = GetApplyElapsed();
		if (Late < ApplyState.Seconds)
		{
			const float Rate = BandageMath::PlayRate(Rules->ApplyClip->GetPlayLength(), ApplyState.Seconds);
			Driver->PlayFullBody(Rules->ApplyClip, Rate, Late * Rate);
			PlayingClip = Rules->ApplyClip;
		}
	}
	SetMoveHold(ApplyState.bApplying && Rules->bHoldStillWhileApplying);

	const bool bOwner = IsCarrierLocallyControlled();
	// tied: the wrapped bandage stays on the forearm a while, the sound where the carrier stands
	if (ApplyState.bHealed && !Previous.bHealed)
	{
		const float Left = ApplyState.bApplying ? FMath::Max(0.0f, ApplyState.Seconds - GetApplyElapsed()) : 0.0f;
		WrappedUntil = Now + Left + Rules->WrappedLingerSeconds;
		if (Rules->DoneSound && Carrier && World && World->GetNetMode() != NM_DedicatedServer)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Rules->DoneSound, Carrier->GetActorLocation());
		}
		if (bOwner)
		{
			NoticeText = Rules->HealedText;
			bNoticeWarning = false;
			NoticeUntil = Now + Left + Rules->NoticeSeconds;
		}
	}
	// cut: the owner reads why
	if (bOwner && Previous.bApplying && !ApplyState.bApplying && ApplyState.Interrupt != EBandageInterrupt::None && !ApplyState.bHealed)
	{
		NoticeText = Rules->GetInterruptText(ApplyState.Interrupt);
		bNoticeWarning = true;
		NoticeUntil = Now + Rules->NoticeSeconds;
	}
}

void ABandageItem::SetMoveHold(bool bHold)
{
	if (bHold == bMoveHold)
	{
		return;
	}
	if (bHold)
	{
		const AIslanderCharacter* Carrier = GetCarrier();
		APlayerController* Controller = Carrier && Carrier->IsLocallyControlled() ? Cast<APlayerController>(Carrier->GetController()) : nullptr;
		if (!Controller)
		{
			return;
		}
		// the owner's controller ignores the movement keys (a stack: one hold, one release, on the same controller)
		Controller->SetIgnoreMoveInput(true);
		HoldController = Controller;
		bMoveHold = true;
		return;
	}
	if (APlayerController* Controller = HoldController.Get())
	{
		Controller->SetIgnoreMoveInput(false);
	}
	HoldController.Reset();
	bMoveHold = false;
}

void ABandageItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules)
	{
		// told once it is used, not at spawn: the tests spawn the bare class and give it its rules before it ticks
		if (!bWarnedNoRules)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_BandageRules, the bandage does nothing (set Rules in its Blueprint)"), *GetName());
			bWarnedNoRules = true;
		}
		return;
	}
	if (!bMeshesSet)
	{
		RollMesh->SetStaticMesh(Rules->RollMesh);
		FirstPersonRoll->SetStaticMesh(Rules->RollMesh);
		WrappedMesh->SetStaticMesh(Rules->WrappedMesh);
		bMeshesSet = true;
	}
	// the server watches what cuts the bandage under way (the blows arrive through the health)
	if (HasAuthority() && ApplyState.bApplying && !ApplyState.bHealed)
	{
		const EBandageInterrupt Reason = BandageMath::CheckInterrupt(GatherFacts(), *Rules);
		if (Reason != EBandageInterrupt::None)
		{
			Interrupt(Reason);
		}
	}
	UpdateBodyMeshes();
	UpdateFirstPerson();

	const double Now = GetWorld()->GetTimeSeconds();
	if (IsCarrierLocallyControlled())
	{
		BindInput();
		if (Now >= NextLocalCheck)
		{
			NextLocalCheck = Now + Rules->LocalCheckSeconds;
			LocalRefusal = JudgeStart();
			// the crosshair on something to use (a pickup, the boat): the interaction keeps the key
			const AIslanderCharacter* Carrier = GetCarrier();
			const UIslanderGrabComponent* Grab = Carrier ? Carrier->GetGrabComponent() : nullptr;
			AActor* Interactable = nullptr;
			UPrimitiveComponent* Physics = nullptr;
			FVector Point;
			bInteractableInView = Grab && (Grab->IsHolding() || (Grab->FindInteractionTarget(Interactable, Physics, Point) && Interactable));
		}
		SetInputContextActive(UseContext, Rules->InputPriority, IsEquipped() && WantsUseKey());
	}
	// an application whose end never arrived frees the movement keys anyway
	if (bMoveHold && GetServerNow() - ApplyState.StartServerTime > ApplyState.Seconds + MoveHoldMarginSeconds)
	{
		SetMoveHold(false);
	}
	if (!IsEquipped() && !ApplyState.bApplying && Now >= WrappedUntil && Now >= NoticeUntil)
	{
		WrappedMesh->SetHiddenInGame(true);
		SetActorTickEnabled(false);
	}
}

void ABandageItem::UpdateBodyMeshes()
{
	UBeanPuppetComponent* Body = Puppet.Get();
	const AIslanderCharacter* Carrier = GetCarrier();
	const bool bAble = Body && Carrier && !Carrier->IsRagdoll();
	// in first person its carrier sees the first person roll; the body's meshes stay for the others (and the shadow)
	const bool bOwnerFirstPerson = Carrier && IsCarrierLocallyControlled() && Carrier->IsFirstPerson();

	const auto Place = [Body](UStaticMeshComponent* Mesh, FName Bone, FName Socket, const FTransform& OnBone)
	{
		// component space bones carry the FBX root scale (x100): only their place and turn are used (as the torch)
		const FTransform BoneCS = Body->GetBoneTransformByName(Bone, EBoneSpaces::ComponentSpace);
		FTransform Grip = OnBone;
		// the socket of the mesh meets the bone: the mesh is moved by the inverse of the socket
		if (const UStaticMeshSocket* Found = Socket.IsNone() ? nullptr : Mesh->GetStaticMesh()->FindSocket(Socket))
		{
			Grip = FTransform(Found->RelativeRotation, Found->RelativeLocation).Inverse() * Grip;
		}
		Mesh->SetRelativeTransform(Grip * FTransform(BoneCS.GetRotation(), BoneCS.GetLocation()));
	};

	const bool bShowRoll = bAble && IsEquipped() && RollMesh->GetStaticMesh() && Body->GetBoneIndex(Rules->HeldBone) != INDEX_NONE;
	if (RollMesh->bHiddenInGame == bShowRoll)
	{
		RollMesh->SetHiddenInGame(!bShowRoll);
	}
	if (bShowRoll)
	{
		RollMesh->SetOwnerNoSee(bOwnerFirstPerson);
		Place(RollMesh, Rules->HeldBone, Rules->GripSocket, Rules->RollInHand);
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const bool bWrapping = ApplyState.bApplying && Rules->ToClipTime(GetApplyElapsed()) >= Rules->WrapStartSeconds;
	const bool bShowWrapped = bAble && WrappedMesh->GetStaticMesh() && (bWrapping || Now < WrappedUntil) && Body->GetBoneIndex(Rules->WrappedBone) != INDEX_NONE;
	if (WrappedMesh->bHiddenInGame == bShowWrapped)
	{
		WrappedMesh->SetHiddenInGame(!bShowWrapped);
	}
	if (bShowWrapped)
	{
		WrappedMesh->SetOwnerNoSee(bOwnerFirstPerson);
		Place(WrappedMesh, Rules->WrappedBone, NAME_None, Rules->WrappedOnBone);
	}
}

void ABandageItem::UpdateFirstPerson()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UBeanFirstPersonArmsComponent* FirstPersonArms = Arms.Get();
	const bool bShow = Carrier && IsCarrierLocallyControlled() && Carrier->IsFirstPerson() && !Carrier->IsRagdoll() && IsEquipped()
		&& FirstPersonRoll->GetAttachParent() != RootComponent;
	const bool bShowRoll = bShow && FirstPersonRoll->GetStaticMesh() != nullptr;
	if (FirstPersonRoll->bHiddenInGame == bShowRoll)
	{
		FirstPersonRoll->SetHiddenInGame(!bShowRoll);
	}
	if (!bShow || !FirstPersonArms)
	{
		if (bArmsDriven && FirstPersonArms)
		{
			FirstPersonArms->ClearPoseOverride();
		}
		bArmsDriven = false;
		return;
	}

	const FVector& Shoulder = Rules->FirstPersonRightShoulder;
	const FVector& HandPoint = Rules->FirstPersonArmHandPoint;
	FVector Roll = Rules->FirstPersonRollHeld.GetLocation();
	FBeanArmPose Right;
	FBeanArmPose Left = FirstPersonArms->RestPose;
	if (ApplyState.bApplying)
	{
		const float ClipTime = Rules->ToClipTime(GetApplyElapsed());
		if (BandageMath::SampleArmKeys(Rules->FirstPersonKeys, ClipTime, Right, Left))
		{
			// the keys of the art kit: the roll in the right mitten
			Roll = BandageMath::HandOf(Right, HandPoint);
		}
		else
		{
			// no keys yet: the left forearm comes up, the roll goes round it, the forearm goes down
			Roll = BandageMath::WrapRollLocation(*Rules, ClipTime);
			Right = MusketMath::SolveArm(Shoulder, Roll, HandPoint, Rules->FirstPersonArmMaxStretch);
			const float Start = FMath::Max(Rules->WrapStartSeconds, 0.01f);
			const float Done = FMath::Max(Rules->HealEventSeconds, Start + 0.01f);
			const float Up = FMath::Min(ClipTime / Start, 1.0f - (ClipTime - Done) / FMath::Max(Rules->ClipSeconds - Done, 0.05f));
			Left = BandageMath::LerpArm(FirstPersonArms->RestPose, Rules->FirstPersonLeftForearm, FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Up, 0.0f, 1.0f)));
		}
	}
	else
	{
		Right = MusketMath::SolveArm(Shoulder, Roll, HandPoint, Rules->FirstPersonArmMaxStretch);
	}
	FirstPersonRoll->SetRelativeTransform(FTransform(Rules->FirstPersonRollHeld.GetRotation(), Roll));
	FirstPersonArms->SetPoseOverride(Right, Left);
	bArmsDriven = true;
}

// ---------------------------------------------------------------- input (owner)

bool ABandageItem::WantsUseKey() const
{
	return Rules && IsEquipped() && !ApplyState.bApplying && LocalRefusal == EBandageRefusal::None && !bInteractableInView;
}

void ABandageItem::EnsureInput()
{
	if ((UseAction && UseContext) || !Rules)
	{
		return;
	}
	// the keys consume themselves while the context is on: the interaction (F) and the punch (click) do not see them
	UseAction = NewObject<UInputAction>(this, TEXT("IA_BandageApply"), RF_Transient);
	UseAction->ValueType = EInputActionValueType::Boolean;
	UseAction->bConsumeInput = true;
	UseContext = NewObject<UInputMappingContext>(this, TEXT("IMC_BandageApply"), RF_Transient);
	for (const FKey& Key : Rules->UseKeys)
	{
		if (Key.IsValid())
		{
			UseContext->MapKey(UseAction, Key);
		}
	}
}

void ABandageItem::BindInput()
{
	if (bInputBound || !Rules)
	{
		return;
	}
	const AIslanderCharacter* Carrier = GetCarrier();
	UEnhancedInputComponent* Input = Carrier ? Cast<UEnhancedInputComponent>(Carrier->InputComponent) : nullptr;
	if (!Input)
	{
		return;
	}
	EnsureInput();
	KeepInputBinding(Input, Input->BindAction(UseAction, ETriggerEvent::Started, this, &ABandageItem::HandleUsePressed).GetHandle());
	bInputBound = true;
}

void ABandageItem::HandleUsePressed()
{
	// keys of a former carrier never drive the item someone else now holds (T-288, network review R-37)
	if (!IsCarrierLocallyControlled() || !WantsUseKey())
	{
		return;
	}
	RequestApply();
}

// ---------------------------------------------------------------- HUD

bool ABandageItem::GetPrompt(FText& OutText, float& OutProgress, bool& bOutWarning) const
{
	OutProgress = -1.0f;
	bOutWarning = false;
	const UWorld* World = GetWorld();
	if (!Rules || !World || !IsCarrierLocallyControlled())
	{
		return false;
	}
	if (ApplyState.bApplying)
	{
		OutText = ApplyState.bHealed ? Rules->HealedText : Rules->ApplyingText;
		OutProgress = GetApplyProgress();
		return true;
	}
	if (World->GetTimeSeconds() < NoticeUntil && !NoticeText.IsEmpty())
	{
		OutText = NoticeText;
		bOutWarning = bNoticeWarning;
		return true;
	}
	// the interaction's own prompt (the crosshair on something to use)
	if (!IsEquipped() || bInteractableInView)
	{
		return false;
	}
	if (LocalRefusal == EBandageRefusal::None)
	{
		OutText = FText::Format(Rules->PromptText, Rules->GetUseKeyText(), FText::AsNumber(GetStackCount()));
		return true;
	}
	OutText = Rules->GetRefusalText(LocalRefusal);
	bOutWarning = LocalRefusal != EBandageRefusal::FullHealth;
	return !OutText.IsEmpty();
}

bool ABandageItem::GetHudLine(const APawn* Pawn, FText& OutText, float& OutProgress, bool& bOutWarning)
{
	const UIslanderInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	const ABandageItem* Bandage = Inventory ? Cast<ABandageItem>(Inventory->GetEquippedItem()) : nullptr;
	return Bandage && Bandage->GetPrompt(OutText, OutProgress, bOutWarning);
}

// ---------------------------------------------------------------- development

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorldAndArgs GBandageHurtCommand(
	TEXT("monke.Bandage.Hurt"),
	TEXT("Takes N health (default 50) from every player of this server, god mode of the test map included, to try the bandages (server / host)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("monke.Bandage.Hurt: server only (type it in the host's window)"));
			return;
		}
		const float Amount = Args.IsEmpty() ? 50.0f : FMath::Max(FCString::Atof(*Args[0]), 0.0f);
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
			UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
			if (!Health || Health->IsDead())
			{
				continue;
			}
			// the god mode of the test map stops every damage: lifted for this one, never down to death
			const bool bCouldBeDamaged = Pawn->CanBeDamaged();
			Pawn->SetCanBeDamaged(true);
			Health->ApplyHealthDamage(FMath::Min(Amount, Health->GetHealth() - 1.0f), nullptr);
			Pawn->SetCanBeDamaged(bCouldBeDamaged);
			UE_LOG(Logmonke_island, Log, TEXT("monke.Bandage.Hurt: %s at %.0f / %.0f"), *GetNameSafe(Pawn), Health->GetHealth(), Health->MaxHealth);
		}
	}));
#endif
