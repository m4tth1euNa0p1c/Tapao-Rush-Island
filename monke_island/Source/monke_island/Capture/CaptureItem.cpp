// monke_island

#include "Capture/CaptureItem.h"
#include "Capture/CaptureMath.h"
#include "Capture/CaptureRules.h"
#include "Capture/CaptureSubsystem.h"
#include "Character/Bean/BeanAnimDriverComponent.h"
#include "Character/Bean/BeanAnimLibrary.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** The owner frees its movement keys this long after a gesture should have ended, if the server's end never came (s) */
	constexpr float GestureHoldMarginSeconds = 0.5f;
}

ACaptureItem::ACaptureItem()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// after the puppet built its pose: the item follows the right hand
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	// nothing changes every frame: a low rate, each change forces an update
	SetNetUpdateFrequency(10.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	HeldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldMesh"));
	HeldMesh->SetupAttachment(RootComponent);
	HeldMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldMesh->SetCanEverAffectNavigation(false);
	HeldMesh->SetGenerateOverlapEvents(false);
	HeldMesh->SetHiddenInGame(true);

	// in first person its carrier sees the body's own arms holding it, and the gestures they make
	Tags.Add(UBeanAnimDriverComponent::BodyArmsInFirstPersonTag);
}

void ACaptureItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACaptureItem, Gesture);
}

void ACaptureItem::BeginPlay()
{
	if (!Rules)
	{
		UE_LOG(LogMonkeCapture, Error, TEXT("%s: no DA_CaptureRules, the item does nothing (set Rules in its Blueprint)"), *GetName());
	}
	Super::BeginPlay();
	SetActorTickEnabled(IsEquipped());
}

void ACaptureItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	SetGestureHold(false);
	if (PlayingClip.IsValid())
	{
		const AIslanderCharacter* Carrier = GetCarrier();
		if (UBeanAnimDriverComponent* Driver = Carrier ? Carrier->FindComponentByClass<UBeanAnimDriverComponent>() : nullptr)
		{
			Driver->StopFullBody(PlayingClip.Get());
		}
		PlayingClip.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ACaptureItem::OnAttachedToCarrier()
{
	AIslanderCharacter* Carrier = GetCarrier();
	Puppet = Carrier ? Carrier->FindComponentByClass<UBeanPuppetComponent>() : nullptr;
	if (UBeanPuppetComponent* Body = Puppet.Get())
	{
		// rides on the puppet component (scale 1), placed every frame from the hand bone
		HeldMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetIncludingScale);
		AddTickPrerequisiteComponent(Body);
	}
}

void ACaptureItem::OnEquippedChanged()
{
	SetActorTickEnabled(IsEquipped() || Gesture.Gesture != ECaptureGesture::None);
	if (!IsEquipped())
	{
		HeldMesh->SetHiddenInGame(true);
		// put away: the key goes back to the interaction
		if (IsCarrierLocallyControlled())
		{
			SetInputContextActive(UseContext, 0, false);
		}
		bUseKeyDown = false;
	}
}

void ACaptureItem::OnInputBindingsRemoved()
{
	Super::OnInputBindingsRemoved();
	bInputBound = false;
	bUseKeyDown = false;
}

bool ACaptureItem::IsUsable() const
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
	return Move && Move->IsMovingOnGround() && !Move->IsSwimming();
}

double ACaptureItem::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

float ACaptureItem::GetGestureElapsed() const
{
	return Gesture.Gesture == ECaptureGesture::None ? 0.0f : FMath::Max(0.0f, static_cast<float>(GetServerNow() - Gesture.StartServerTime));
}

float ACaptureItem::GetGestureProgress() const
{
	return Gesture.Gesture == ECaptureGesture::None ? 0.0f : CaptureMath::Progress(GetServerNow(), Gesture.StartServerTime, Gesture.Seconds);
}

// ---------------------------------------------------------------- per frame

void ACaptureItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules)
	{
		return;
	}
	UpdateHeldMesh();
	if (IsCarrierLocallyControlled())
	{
		BindInput();
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextLocalCheck)
		{
			NextLocalCheck = Now + Rules->LocalCheckSeconds;
			RefreshLocal();
		}
		SetInputContextActive(UseContext, Rules->InputPriority, IsEquipped() && WantsUseKey());
	}
	// a gesture whose end never arrived frees the movement keys anyway
	if (bGestureHold && GetServerNow() - Gesture.StartServerTime > Gesture.Seconds + GestureHoldMarginSeconds)
	{
		SetGestureHold(false);
	}
	if (!IsEquipped() && Gesture.Gesture == ECaptureGesture::None)
	{
		SetActorTickEnabled(false);
	}
}

void ACaptureItem::UpdateHeldMesh()
{
	UBeanPuppetComponent* Body = Puppet.Get();
	UStaticMesh* Mesh = GetHeldMesh();
	const AIslanderCharacter* Carrier = GetCarrier();
	const bool bShow = Body && Mesh && ShouldShowHeldMesh() && !(Carrier && Carrier->IsRagdoll()) && Body->GetBoneIndex(Rules->HeldBone) != INDEX_NONE;
	if (Mesh && HeldMesh->GetStaticMesh() != Mesh)
	{
		HeldMesh->SetStaticMesh(Mesh);
	}
	if (HeldMesh->bHiddenInGame == bShow)
	{
		HeldMesh->SetHiddenInGame(!bShow);
	}
	if (!bShow)
	{
		return;
	}
	// set down during a gesture (the folded cage on the sand before the cage opens)
	FTransform Override;
	if (GetHeldOverride(Override))
	{
		HeldMesh->SetWorldTransform(Override);
		return;
	}
	// component space bones carry the FBX root scale (x100): only their place and turn are used (as the torch)
	const FTransform BoneCS = Body->GetBoneTransformByName(Rules->HeldBone, EBoneSpaces::ComponentSpace);
	FTransform Grip = GetHeldGrip();
	// the grip socket of the mesh meets the hand: the mesh is moved by the inverse of the socket
	if (const UStaticMeshSocket* Socket = Mesh->FindSocket(Rules->HeldGripSocket))
	{
		Grip = FTransform(Socket->RelativeRotation, Socket->RelativeLocation).Inverse() * Grip;
	}
	HeldMesh->SetRelativeTransform(Grip * FTransform(BoneCS.GetRotation(), BoneCS.GetLocation()));
}

// ---------------------------------------------------------------- input (owner)

void ACaptureItem::EnsureInput()
{
	if ((UseAction && UseContext) || !Rules)
	{
		return;
	}
	// the key consumes itself while the context is on: the interaction (F) does not see it then
	UseAction = NewObject<UInputAction>(this, TEXT("IA_CaptureUse"), RF_Transient);
	UseAction->ValueType = EInputActionValueType::Boolean;
	UseAction->bConsumeInput = true;
	UseContext = NewObject<UInputMappingContext>(this, TEXT("IMC_CaptureUse"), RF_Transient);
	for (const FKey& Key : Rules->UseKeys)
	{
		if (Key.IsValid())
		{
			UseContext->MapKey(UseAction, Key);
		}
	}
}

void ACaptureItem::BindInput()
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
	KeepInputBinding(Input, Input->BindAction(UseAction, ETriggerEvent::Started, this, &ACaptureItem::HandleUsePressed).GetHandle());
	KeepInputBinding(Input, Input->BindAction(UseAction, ETriggerEvent::Completed, this, &ACaptureItem::HandleUseReleased).GetHandle());
	KeepInputBinding(Input, Input->BindAction(UseAction, ETriggerEvent::Canceled, this, &ACaptureItem::HandleUseReleased).GetHandle());
	bInputBound = true;
}

void ACaptureItem::HandleUsePressed()
{
	// keys of a former carrier never drive the item someone else now holds (T-288, network review R-37)
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	bUseKeyDown = true;
	if (IsUsable())
	{
		OnUsePressed();
	}
}

void ACaptureItem::HandleUseReleased()
{
	if (!IsCarrierLocallyControlled() || !bUseKeyDown)
	{
		return;
	}
	bUseKeyDown = false;
	OnUseReleased();
}

// ---------------------------------------------------------------- gestures

UAnimSequence* ACaptureItem::GetGestureClip(ECaptureGesture InGesture) const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const UBeanAnimDriverComponent* Driver = Carrier ? Carrier->FindComponentByClass<UBeanAnimDriverComponent>() : nullptr;
	const UBeanAnimLibrary* Library = Driver ? Driver->Library.Get() : nullptr;
	if (!Library)
	{
		return nullptr;
	}
	switch (InGesture)
	{
	case ECaptureGesture::PlaceCage:	return Library->CagePlace;
	case ECaptureGesture::Cuff:			return Library->Cuff;
	default:							return nullptr;
	}
}

void ACaptureItem::StartGesture(ECaptureGesture InGesture, float Seconds)
{
	if (!HasAuthority())
	{
		return;
	}
	Gesture.Gesture = InGesture;
	++Gesture.Serial;
	Gesture.StartServerTime = GetServerNow();
	Gesture.Seconds = FMath::Max(Seconds, 0.05f);
	ForceNetUpdate();
	SetActorTickEnabled(true);
	PlayGestureLocally(Gesture);
}

void ACaptureItem::StopGesture()
{
	if (!HasAuthority() || Gesture.Gesture == ECaptureGesture::None)
	{
		return;
	}
	Gesture.Gesture = ECaptureGesture::None;
	++Gesture.Serial;
	ForceNetUpdate();
	PlayGestureLocally(Gesture);
}

void ACaptureItem::OnRep_Gesture(const FCaptureGestureRep& Previous)
{
	PlayGestureLocally(Gesture);
}

void ACaptureItem::PlayGestureLocally(const FCaptureGestureRep& Rep)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UBeanAnimDriverComponent* Driver = Carrier ? Carrier->FindComponentByClass<UBeanAnimDriverComponent>() : nullptr;
	// the clip of the gesture before stops (cut short, or replaced)
	if (Driver && PlayingClip.IsValid())
	{
		Driver->StopFullBody(PlayingClip.Get());
	}
	PlayingClip.Reset();
	const bool bActive = Rep.Gesture != ECaptureGesture::None;
	SetGestureHold(bActive && Rules && Rules->bHoldStillDuringGestures);
	if (!bActive || !Rules)
	{
		return;
	}
	SetActorTickEnabled(true);
	UAnimSequence* Clip = GetGestureClip(Rep.Gesture);
	// a gesture received late starts as far in as the server is; one long over is not played
	const float Late = FMath::Max(0.0f, static_cast<float>(GetServerNow() - Rep.StartServerTime));
	if (!Driver || !Clip || Late >= Rep.Seconds)
	{
		return;
	}
	const bool bStretch = Rep.Gesture != ECaptureGesture::Cuff || Rules->bStretchCuffClipToHold;
	const float Rate = bStretch ? CaptureMath::GesturePlayRate(Clip->GetPlayLength(), Rep.Seconds) : 1.0f;
	Driver->PlayFullBody(Clip, Rate, Late * Rate);
	PlayingClip = Clip;
}

void ACaptureItem::SetGestureHold(bool bHold)
{
	if (bHold == bGestureHold)
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
		bGestureHold = true;
		return;
	}
	if (APlayerController* Controller = HoldController.Get())
	{
		Controller->SetIgnoreMoveInput(false);
	}
	HoldController.Reset();
	bGestureHold = false;
}

// ---------------------------------------------------------------- server helpers

bool ACaptureItem::ConsumeRequest(const TCHAR* What)
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
	UE_LOG(LogMonkeCapture, Verbose, TEXT("%s: %s refused, too many requests at once (%d refused so far)"), *GetNameSafe(GetCarrier()), What,
		RequestLimiter.Refused);
	return false;
}

void ACaptureItem::Refuse(ECaptureRefusal Reason, float HealthFraction)
{
	if (!Rules || Reason == ECaptureRefusal::None)
	{
		return;
	}
	// a refusal of the rules (security.md §2): logged, nothing happens, no disconnection
	UE_LOG(LogMonkeCapture, Warning, TEXT("%s: %s refused (%s)"), *GetNameSafe(GetCarrier()), *GetName(), *UEnum::GetValueAsString(Reason));
	NotifyCarrier(Rules->GetRefusalText(Reason, HealthFraction), true);
}

void ACaptureItem::NotifyCarrier(const FText& Text, bool bWarning)
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

void ACaptureItem::ClientNotice_Implementation(const FText& Text, bool bWarning)
{
	if (UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this))
	{
		Capture->PostNotice(Text, UCaptureSubsystem::NoticeSeconds, bWarning);
	}
}
