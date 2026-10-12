// monke_island

#include "Weapons/FirearmWeapon.h"
#include "Weapons/FirearmDamage.h"
#include "Weapons/FirearmMath.h"
#include "monke_island.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Stealth/StealthSubsystem.h"
#include "TimerManager.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "TimerManager.h"

namespace
{
	/** Loops the carrier's movement chooses: idle, walk, run (T-330) */
	bool IsCarryLoop(EFirearmGesture Gesture)
	{
		return Gesture == EFirearmGesture::Idle || Gesture == EFirearmGesture::Walk || Gesture == EFirearmGesture::Sprint;
	}

	double FirearmServerNow(const UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}

	USkeletalMeshComponent* MakeFirearmMesh(AActor* Owner, FName Name)
	{
		USkeletalMeshComponent* Mesh = Owner->CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		// hidden meshes keep their clip time, so a view switch shows them in step with the others
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
		Mesh->SetHiddenInGame(true);
		return Mesh;
	}

	/** One target hit by one or more projectiles of a shot */
	struct FTargetHits
	{
		TWeakObjectPtr<AActor> Target;
		FHitResult FirstHit;
		FVector Direction = FVector::ZeroVector;
		float Damage = 0.0f;
		int32 Hits = 0;
		/** Hits grouped by the bone struck (T-167) */
		TArray<FFirearmBoneHit> Groups;
		/** Entry point of each projectile */
		TArray<FVector> Impacts;
	};
}

AFirearmWeapon::AFirearmWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	// after the puppet copied the walk pose (its bones are where they are drawn)
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FirstPersonRig = CreateDefaultSubobject<USceneComponent>(TEXT("FirstPersonRig"));
	FirstPersonRig->SetupAttachment(RootComponent);

	FirstPersonArms = MakeFirearmMesh(this, TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(FirstPersonRig);
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->SetCastShadow(false);

	FirstPersonWeapon = MakeFirearmMesh(this, TEXT("FirstPersonWeapon"));
	FirstPersonWeapon->SetupAttachment(FirstPersonRig);
	FirstPersonWeapon->SetOnlyOwnerSee(true);
	FirstPersonWeapon->SetCastShadow(false);

	ThirdPersonWeapon = MakeFirearmMesh(this, TEXT("ThirdPersonWeapon"));
	ThirdPersonWeapon->SetupAttachment(RootComponent);
}

void AFirearmWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFirearmWeapon, Action);
	DOREPLIFETIME_CONDITION(AFirearmWeapon, bAiming, COND_SkipOwner);
}

void AFirearmWeapon::BeginPlay()
{
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Error, TEXT("Firearm %s: no rules (DA_PistolRules / DA_ShotgunRules), the weapon is disabled"), *GetName());
		Super::BeginPlay();
		SetActorTickEnabled(false);
		return;
	}
	if (HasAuthority())
	{
		SpreadStream.GenerateNewSeed();
	}
	if (!Rules->WeaponMesh)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Firearm %s: %s has no WeaponMesh (run scripts/character/setup_pirate_weapons.py)"), *GetName(), *Rules->GetName());
	}
	FirstPersonWeapon->SetSkeletalMeshAsset(Rules->WeaponMesh);
	ThirdPersonWeapon->SetSkeletalMeshAsset(Rules->WeaponMesh);
	FirstPersonArms->SetSkeletalMeshAsset(Rules->FirstPersonArmsMesh);

	Super::BeginPlay();
}

void AFirearmWeapon::OnAttachedToCarrier()
{
	AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !Rules)
	{
		return;
	}
	Arms = Carrier->FindComponentByClass<UBeanFirstPersonArmsComponent>();
	Puppet = Carrier->FindComponentByClass<UBeanPuppetComponent>();

	// the pack's FP camera on the player's camera: the bean root (and the weapon root on it) at the inverse of the view
	if (UCameraComponent* Camera = Carrier->GetFirstPersonCamera())
	{
		FirstPersonRig->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		const FTransform ViewInBean(Rules->FirstPersonViewRotation, Rules->FirstPersonViewLocation);
		FirstPersonRig->SetRelativeTransform(ViewInBean.Inverse());
	}

	if (Puppet.IsValid())
	{
		// the weapon rides on the puppet; UpdateThirdPerson puts it where the clip has it relative to the anchor bone
		ThirdPersonWeapon->AttachToComponent(Puppet.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		AddTickPrerequisiteComponent(Puppet.Get());

		if (!BodyAnimator)
		{
			BodyAnimator = NewObject<USkeletalMeshComponent>(this, TEXT("BodyAnimator"));
			BodyAnimator->SetSkeletalMeshAsset(Cast<USkeletalMesh>(Puppet->GetSkinnedAsset()));
			BodyAnimator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BodyAnimator->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			// never drawn, but its pose must keep updating
			BodyAnimator->SetVisibility(false);
			BodyAnimator->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			// not under the puppet (tick cycle, see AMusketWeapon): only its bone pose is read
			BodyAnimator->SetupAttachment(Puppet->GetAttachParent() ? Puppet->GetAttachParent() : Carrier->GetRootComponent());
			BodyAnimator->RegisterComponent();
			BodyAnimator->SetComponentTickEnabled(false);
		}
	}
	else
	{
		// mannequin fallback: plain grip in the hand
		ThirdPersonWeapon->AttachToComponent(Carrier->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Rules->AnchorBone);
		ThirdPersonWeapon->SetRelativeTransform(Rules->FallbackHandOffset);
	}
	// the clip playing on the other machines when this one joins
	if (PlayingGesture != EFirearmGesture::None)
	{
		if (const FFirearmClip* Clip = Rules->FindClip(PlayingGesture))
		{
			ApplyClipToMeshes(*Clip, GetPlayingClipTime());
		}
	}
}

void AFirearmWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (Arms.IsValid())
	{
		Arms->SetHiddenByItem(false);
	}
	if (Puppet.IsValid() && !bPuppetReleased)
	{
		Puppet->SetBoneOverrideSource(nullptr, {}, 0.0f);
	}
	RestoreFieldOfView();
	Super::EndPlay(EndPlayReason);
}

bool AFirearmWeapon::IsUsable() const
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

EFirearmGesture AFirearmWeapon::GetServerGesture() const
{
	if (Action.Gesture == EFirearmGesture::None || !Rules)
	{
		return EFirearmGesture::None;
	}
	if (HasAuthority())
	{
		// the server's timers decide (a chained gesture starts in the end callback of the previous one)
		return IsBusy() ? Action.Gesture : EFirearmGesture::None;
	}
	const FFirearmClip* Clip = Rules->FindClip(Action.Gesture);
	const double Elapsed = FirearmServerNow(GetWorld()) - Action.ServerStartTime;
	return Clip && Action.ClipStart + Elapsed * GetGestureRate(*Clip) < Clip->Duration ? Action.Gesture : EFirearmGesture::None;
}

float AFirearmWeapon::GetServerGestureProgress() const
{
	const EFirearmGesture Gesture = GetServerGesture();
	const FFirearmClip* Clip = Gesture != EFirearmGesture::None && Rules ? Rules->FindClip(Gesture) : nullptr;
	if (!Clip || Clip->Duration <= 0.0f)
	{
		return 0.0f;
	}
	const double Elapsed = FMath::Max(FirearmServerNow(GetWorld()) - Action.ServerStartTime, 0.0);
	return FMath::Clamp((Action.ClipStart + static_cast<float>(Elapsed) * GetGestureRate(*Clip)) / Clip->Duration, 0.0f, 1.0f);
}

float AFirearmWeapon::GetServerGestureSecondsLeft() const
{
	const EFirearmGesture Gesture = GetServerGesture();
	const FFirearmClip* Clip = Gesture != EFirearmGesture::None && Rules ? Rules->FindClip(Gesture) : nullptr;
	if (!Clip || Clip->Duration <= 0.0f)
	{
		return 0.0f;
	}
	const float Rate = GetGestureRate(*Clip);
	const double Elapsed = FMath::Max(FirearmServerNow(GetWorld()) - Action.ServerStartTime, 0.0);
	const float Done = Action.ClipStart + static_cast<float>(Elapsed) * Rate;
	return FMath::Max(0.0f, (Clip->Duration - Done) / Rate);
}

FVector AFirearmWeapon::GetCarrierAim() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier)
	{
		return GetActorForwardVector();
	}
	return (Carrier->GetController() ? Carrier->GetControlRotation() : Carrier->GetBaseAimRotation()).Vector();
}

// ---------------------------------------------------------------- input

void AFirearmWeapon::BindInput()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UEnhancedInputComponent* Input = Carrier ? Cast<UEnhancedInputComponent>(Carrier->InputComponent) : nullptr;
	if (bInputBound || !Input)
	{
		return;
	}
	// the bindings stay; the mapping context decides when the keys reach them
	if (FireAction)
	{
		KeepInputBinding(Input, Input->BindAction(FireAction, ETriggerEvent::Started, this, &AFirearmWeapon::HandleFireInput).GetHandle());
	}
	if (AimAction)
	{
		KeepInputBinding(Input, Input->BindAction(AimAction, ETriggerEvent::Started, this, &AFirearmWeapon::HandleAimStarted).GetHandle());
		KeepInputBinding(Input, Input->BindAction(AimAction, ETriggerEvent::Completed, this, &AFirearmWeapon::HandleAimCompleted).GetHandle());
	}
	if (ReloadAction)
	{
		KeepInputBinding(Input, Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AFirearmWeapon::HandleReloadInput).GetHandle());
	}
	if (InspectAction)
	{
		KeepInputBinding(Input, Input->BindAction(InspectAction, ETriggerEvent::Started, this, &AFirearmWeapon::HandleInspectInput).GetHandle());
	}
	bInputBound = true;
}

void AFirearmWeapon::OnInputBindingsRemoved()
{
	Super::OnInputBindingsRemoved();
	bInputBound = false;
	bAimWanted = false;
}

void AFirearmWeapon::HandleFireInput()
{
	// keys of a former carrier never drive the item someone else now holds (T-288, network review R-37)
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	RequestFire();
}

void AFirearmWeapon::HandleAimStarted()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	SetAiming(true);
}

void AFirearmWeapon::HandleAimCompleted()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	SetAiming(false);
}

void AFirearmWeapon::HandleReloadInput()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	RequestReload();
}

void AFirearmWeapon::HandleInspectInput()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	RequestInspect();
}

// ---------------------------------------------------------------- requests

void AFirearmWeapon::RequestFire()
{
	if (!IsUsable())
	{
		return;
	}
	if (HasAuthority())
	{
		ServerFire_Implementation();
		return;
	}
	// the carrier sees its shot at once; the server decides the hit and the ammunition
	if (PredictFire())
	{
		const EFirearmGesture Shot = bAiming ? EFirearmGesture::AimFire : EFirearmGesture::Fire;
		OnShotPredicted(Shot);
		PredictGesture(Shot, FireClipStart(Shot));
	}
	ServerFire();
}

void AFirearmWeapon::PredictGesture(EFirearmGesture Gesture, float ClipStart)
{
	// ApplyServerAction keeps it when the server's own start of this gesture arrives
	PredictedGesture = Gesture;
	PredictedTime = GetWorld()->GetTimeSeconds();
	PlayGesture(Gesture, ClipStart, true);
}

void AFirearmWeapon::CancelPredictedGesture(EFirearmGesture Gesture)
{
	if (PredictedGesture == Gesture)
	{
		PredictedGesture = EFirearmGesture::None;
	}
	// only the gesture this machine still plays on its own: one the server plays too stays
	if (Gesture == EFirearmGesture::None || PlayingGesture != Gesture || GetServerGesture() == Gesture)
	{
		return;
	}
	PlayingGesture = EFirearmGesture::None;
	bPlayingFromServer = false;
	PendingLoop = EFirearmGesture::None;
}

float AFirearmWeapon::GetShotSpreadDegrees(bool bMoving) const
{
	return Rules ? FirearmMath::SpreadForStance(bAiming, bMoving, Rules->AimSpreadDegrees, Rules->HipSpreadDegrees) : 0.0f;
}

void AFirearmWeapon::ServerFire_Implementation()
{
	// a server body run without authority is a call that missed the network (2026-10-11: a request made outside the client
	// world's tick in a single-process PIE ran here on the client's copy and hit the check of StartAction): nothing happens
	if (!HasAuthority())
	{
		return;
	}
	if (!IsUsable())
	{
		return;
	}
	// a shot cuts the inspection
	if (GetServerGesture() == EFirearmGesture::Inspect)
	{
		StopAction();
	}
	// nothing loaded, nothing in the pouch: the weapon is thrown away (owner 2026-10-09) instead of clicking
	if (FirearmMath::ShouldDiscard(GetLoadedRounds(), GetReserveRounds(), IsBusy(), Rules->bDiscardWhenEmpty))
	{
		StartDiscard();
		return;
	}
	ServerHandleFire();
}

void AFirearmWeapon::StartDiscard()
{
	check(HasAuthority());
	bDiscardPending = false;
	// the aim drops by the rule, on the server and on the carrier alike (IsDiscarding, network review R-38)
	if (StartAction(EFirearmGesture::Discard))
	{
		UE_LOG(Logmonke_island, Log, TEXT("Firearm %s: empty, %s throws it away"), *GetName(), *GetNameSafe(GetCarrier()));
		return;
	}
	// no Discard clip in the pack: the hammer falls on nothing, then the weapon leaves the hand
	if (StartAction(EFirearmGesture::DryFire))
	{
		bDiscardPending = true;
		UE_LOG(Logmonke_island, Log, TEXT("Firearm %s: empty, no Discard clip, thrown away after DryFire"), *GetName());
		return;
	}
	ReleaseWeapon();
}

void AFirearmWeapon::ReleaseWeapon()
{
	check(HasAuthority());
	bDiscardPending = false;
	AIslanderCharacter* Carrier = GetCarrier();
	UIslanderInventoryComponent* Inventory = Carrier ? Carrier->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	const int32 Slot = Inventory ? Inventory->FindSlotOfItem(this) : INDEX_NONE;
	if (Slot == INDEX_NONE || !Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Firearm %s: cannot be thrown away (not in an inventory)"), *GetName());
		return;
	}
	// to the right of the carrier and down: the empty weapon lands at its feet, counters kept, a pickup again
	const FRotationMatrix View(FRotator(0.0f, GetCarrierAim().Rotation().Yaw, 0.0f));
	const FVector Toss = FirearmMath::DiscardToss(View.GetUnitAxis(EAxis::Y), Rules->DiscardTossSpeed, Rules->DiscardTossPitchDegrees);
	UE_LOG(Logmonke_island, Log, TEXT("Firearm %s: thrown away by %s (%d / %d rounds)"), *GetName(), *Carrier->GetName(), GetLoadedRounds(), GetReserveRounds());
	Inventory->DropSlot(Slot, MAX_int32, TOptional<FVector>(Toss));
}

bool AFirearmWeapon::IsDiscarding() const
{
	const EFirearmGesture Gesture = GetServerGesture();
	if (Gesture == EFirearmGesture::Discard)
	{
		return true;
	}
	if (Gesture != EFirearmGesture::DryFire)
	{
		return false;
	}
	// the DryFire standing in for a missing Discard clip: the server knows it; the carrier reads it from the empty weapon
	// and pouch, which is when the server throws the weapon away (the same aim on both, network review R-38)
	return HasAuthority() ? bDiscardPending
		: Rules && FirearmMath::ShouldDiscard(GetLoadedRounds(), GetReserveRounds(), false, Rules->bDiscardWhenEmpty);
}

// ---------------------------------------------------------------- ammunition (owner 2026-10-09)

EAmmoKind AFirearmWeapon::GetAmmoKind() const
{
	return Rules ? Rules->AmmoKind : EAmmoKind::None;
}

int32 AFirearmWeapon::GetAmmo(EAmmoKind Kind) const
{
	return Kind != EAmmoKind::None && Kind == GetAmmoKind() ? GetLoadedRounds() + GetReserveRounds() : 0;
}

int32 AFirearmWeapon::GetAmmoRoom() const
{
	return GetAmmoKind() == EAmmoKind::None ? 0 : AmmoMath::Room(GetMaxRounds(), GetLoadedRounds(), GetReserveRounds());
}

int32 AFirearmWeapon::AddAmmo(EAmmoKind Kind, int32 Count)
{
	if (!HasAuthority() || Kind == EAmmoKind::None || Kind != GetAmmoKind() || Count <= 0)
	{
		return 0;
	}
	const int32 Taken = FMath::Min(Count, GetAmmoRoom());
	if (Taken <= 0)
	{
		return 0;
	}
	SetRounds(GetLoadedRounds(), GetReserveRounds() + Taken);
	UE_LOG(Logmonke_island, Log, TEXT("Firearm %s: +%d round(s), %d loaded, %d in the pouch"), *GetName(), Taken, GetLoadedRounds(), GetReserveRounds());
	return Taken;
}

void AFirearmWeapon::RequestReload()
{
	if (!IsUsable())
	{
		return;
	}
	if (HasAuthority())
	{
		ServerReload_Implementation();
	}
	else
	{
		ServerReload();
	}
}

void AFirearmWeapon::ServerReload_Implementation()
{
	// a server body run without authority is a call that missed the network (2026-10-11: a request made outside the client
	// world's tick in a single-process PIE ran here on the client's copy and hit the check of StartAction): nothing happens
	if (!HasAuthority())
	{
		return;
	}
	if (!IsUsable())
	{
		return;
	}
	if (GetServerGesture() == EFirearmGesture::Inspect)
	{
		StopAction();
	}
	ServerHandleReload();
}

void AFirearmWeapon::RequestInspect()
{
	if (!IsUsable())
	{
		return;
	}
	if (HasAuthority())
	{
		ServerInspect_Implementation();
	}
	else
	{
		ServerInspect();
	}
}

void AFirearmWeapon::ServerInspect_Implementation()
{
	// a server body run without authority is a call that missed the network (2026-10-11: a request made outside the client
	// world's tick in a single-process PIE ran here on the client's copy and hit the check of StartAction): nothing happens
	if (!HasAuthority())
	{
		return;
	}
	// the inspection lowers the aim by the rule, on the server and on the carrier alike (LowersAim)
	if (IsUsable() && !IsBusy())
	{
		StartAction(EFirearmGesture::Inspect);
		RefreshAim();
	}
}

void AFirearmWeapon::SetAiming(bool bInAiming)
{
	if (HasAuthority())
	{
		ServerSetAiming_Implementation(bInAiming);
		return;
	}
	// the carrier keeps its wish and aims at once when it may; the server keeps the same wish and applies the same rule
	// to the same replicated gesture, so a reload or a throw lowers the aim on both and both raise it again while the
	// click is held (network review R-38). No aiming at all when the rules forbid it (Counter-Strike style, owner
	// 2026-10-09): nothing to tell the server then
	const bool bWanted = bInAiming && Rules && Rules->bAllowAiming;
	if (bWanted == bAimWanted)
	{
		return;
	}
	bAimWanted = bWanted;
	RefreshAim();
	ServerSetAiming(bWanted);
}

void AFirearmWeapon::ServerSetAiming_Implementation(bool bInAiming)
{
	// a server body run without authority is a call that missed the network (2026-10-11: a request made outside the client
	// world's tick in a single-process PIE ran here on the client's copy and hit the check of StartAction): nothing happens
	if (!HasAuthority())
	{
		return;
	}
	const bool bWanted = bInAiming && Rules && Rules->bAllowAiming;
	// a new aim cuts the inspection, as a shot does
	if (bWanted && !bAimWanted && GetServerGesture() == EFirearmGesture::Inspect)
	{
		StopAction();
	}
	bAimWanted = bWanted;
	RefreshAim();
}

bool AFirearmWeapon::LowersAim(EFirearmGesture Gesture) const
{
	return Gesture == EFirearmGesture::Inspect || Gesture == EFirearmGesture::Discard;
}

void AFirearmWeapon::RefreshAim()
{
	const bool bMayAimNow = CanAim() && !IsDiscarding() && !LowersAim(GetServerGesture());
	bAiming = FirearmMath::IsAimed(bAimWanted, Rules && Rules->bAllowAiming, IsUsable(), bMayAimNow);
}

void AFirearmWeapon::OnEquippedChanged()
{
	if (HasAuthority())
	{
		if (IsEquipped())
		{
			// the draw: no shot until the weapon is out
			StartAction(EFirearmGesture::Equip);
		}
		else
		{
			StopAction();
		}
	}
	// put away: the aim is let go on every machine, asked again once drawn (the keys went with the weapon)
	if (!IsEquipped())
	{
		bAimWanted = false;
		bAiming = false;
	}
	// put away: one more tick hides the meshes, gives the arms and the body back and removes the keys
	SetActorTickEnabled(Rules != nullptr);
}

// ---------------------------------------------------------------- server gestures

bool AFirearmWeapon::StartAction(EFirearmGesture Gesture, float ClipStart)
{
	check(HasAuthority());
	const FFirearmClip* Clip = Rules ? Rules->FindClip(Gesture) : nullptr;
	if (!Clip)
	{
		return false;
	}
	FTimerManager& Timers = GetWorldTimerManager();
	const EFirearmGesture Previous = GetServerGesture();
	for (FTimerHandle& Handle : ActionTimers)
	{
		Timers.ClearTimer(Handle);
	}
	ActionTimers.Reset();
	if (Previous != EFirearmGesture::None)
	{
		OnActionStopped(Previous);
	}

	Action.Gesture = Gesture;
	++Action.Serial;
	Action.ClipStart = FMath::Clamp(ClipStart, 0.0f, Clip->Duration);
	Action.ServerStartTime = FirearmServerNow(GetWorld());
	const float Rate = GetGestureRate(*Clip);
	ActionEndTime = GetWorld()->GetTimeSeconds() + (Clip->Duration - Action.ClipStart) / Rate;

	// the logic at the events of the manifest (the shot itself is done by the caller: its event is the clip start)
	const uint8 Serial = Action.Serial;
	for (const FFirearmClipEvent& Event : Clip->Events)
	{
		if (Event.Time > Action.ClipStart + UE_KINDA_SMALL_NUMBER)
		{
			FTimerHandle& Handle = ActionTimers.AddDefaulted_GetRef();
			Timers.SetTimer(Handle, FTimerDelegate::CreateUObject(this, &AFirearmWeapon::HandleServerEvent, Serial, Event.Name),
				(Event.Time - Action.ClipStart) / Rate, false);
		}
	}
	FTimerHandle& EndHandle = ActionTimers.AddDefaulted_GetRef();
	Timers.SetTimer(EndHandle, FTimerDelegate::CreateUObject(this, &AFirearmWeapon::HandleServerFinished, Serial),
		FMath::Max((Clip->Duration - Action.ClipStart) / Rate, 0.01f), false);

	// the server does not get OnRep: play it here (listen host, or watching another player)
	ApplyServerAction();
	return true;
}

void AFirearmWeapon::StopAction()
{
	if (!HasAuthority())
	{
		return;
	}
	const EFirearmGesture Previous = GetServerGesture();
	for (FTimerHandle& Handle : ActionTimers)
	{
		GetWorldTimerManager().ClearTimer(Handle);
	}
	ActionTimers.Reset();
	ActionEndTime = 0.0;
	if (Previous == EFirearmGesture::None)
	{
		// nothing running (a finished gesture ends by itself everywhere)
		return;
	}
	if (Previous == EFirearmGesture::Discard || Previous == EFirearmGesture::DryFire)
	{
		// a cut throw keeps the empty weapon in hand
		bDiscardPending = false;
	}
	Action.Gesture = EFirearmGesture::None;
	++Action.Serial;
	Action.ServerStartTime = FirearmServerNow(GetWorld());
	ApplyServerAction();
	if (Previous != EFirearmGesture::None)
	{
		OnActionStopped(Previous);
	}
}

bool AFirearmWeapon::IsBusy() const
{
	return Action.Gesture != EFirearmGesture::None && GetWorld() && GetWorld()->GetTimeSeconds() < ActionEndTime;
}

void AFirearmWeapon::HandleServerEvent(uint8 InSerial, FName Event)
{
	if (InSerial != Action.Serial)
	{
		return;
	}
	if (Action.Gesture == EFirearmGesture::Discard && Rules && Event == Rules->DiscardReleaseEvent)
	{
		// the weapon leaves the hand here (the drop puts it away, which cuts the rest of the clip)
		ReleaseWeapon();
		return;
	}
	OnActionEvent(Action.Gesture, Event);
}

void AFirearmWeapon::HandleServerFinished(uint8 InSerial)
{
	if (InSerial != Action.Serial)
	{
		return;
	}
	const EFirearmGesture Finished = Action.Gesture;
	ActionTimers.Reset();
	ActionEndTime = 0.0;
	if (Finished == EFirearmGesture::Discard || (bDiscardPending && Finished == EFirearmGesture::DryFire))
	{
		// a Discard clip without its Release event, or the DryFire standing in for it: thrown at the end
		ReleaseWeapon();
		return;
	}
	OnActionFinished(Finished);
}

float AFirearmWeapon::FireClipStart(EFirearmGesture Gesture) const
{
	const FFirearmClip* Clip = Rules ? Rules->FindClip(Gesture) : nullptr;
	return Clip ? FMath::Max(Clip->FindEventTime(Rules->FireEvent), 0.0f) : 0.0f;
}

void AFirearmWeapon::FireProjectiles(int32 Count)
{
	AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !Rules || Count <= 0)
	{
		return;
	}
	// from the carrier's eyes along the replicated view: the client only says "fire"
	const UCameraComponent* Camera = Carrier->GetFirstPersonCamera();
	const FVector Origin = Camera ? Camera->GetComponentLocation() : Carrier->GetPawnViewLocation();
	TArray<FVector> Directions;
	UE_LOG(Logmonke_island, Verbose, TEXT("Firearm %s: %s fires %d projectile(s)%s"), *GetName(), *Carrier->GetName(), Count, bAiming ? TEXT(" (aimed)") : TEXT(""));
	// the shot is heard by the mobs and the villagers around (server, T-262)
	UStealthSubsystem::Report(this, Carrier, Origin, EStealthNoise::Gunshot);
	// the spread of the stance: aimed, or steady on its feet, the shot is tight; on the move it opens (CS style)
	const bool bMoving = Carrier->GetVelocity().Size2D() > Rules->SteadySpeedCmps;
	FirearmMath::SpreadDirections(GetCarrierAim(), Count, GetShotSpreadDegrees(bMoving), SpreadStream, Directions);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FirearmShot), true, Carrier);
	Params.AddIgnoredActor(this);
	TArray<FTargetHits, TInlineAllocator<8>> Targets;
	TArray<FVector_NetQuantize> WorldImpacts;
	TArray<FVector_NetQuantizeNormal> WorldNormals;
	TArray<FVector_NetQuantize> TracerEnds;
	// each projectile its own trace, its own distance and its own bone (T-167)
	for (const FVector& Direction : Directions)
	{
		FHitResult Hit;
		const FVector End = Origin + Direction * Rules->RangeCm;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, Rules->TraceChannel, Params);
		// the giant crab's legs and claws do not block the trace channel (the camera's): a second look finds them
		const bool bStruck = FirearmDamage::TraceHitBodies(GetWorld(), Origin, End, Params, Hit, bHit);
		// a tracer for this round: to what it struck, or to the end of its range
		if (Rules->Tracer.bEnabled && FirearmTracerMath::IsTracerRound(RoundsFired, Rules->Tracer.EveryNth))
		{
			TracerEnds.Add(bStruck ? FVector(Hit.ImpactPoint) : End);
		}
		++RoundsFired;
		if (!bStruck || !Hit.GetActor())
		{
			continue;
		}
		ACharacter* HitCharacter = Cast<ACharacter>(Hit.GetActor());
		if (UPrimitiveComponent* Body = Hit.GetComponent(); Body && Body->IsSimulatingPhysics() && !HitCharacter)
		{
			Body->AddImpulseAtLocation(Direction * Rules->PhysicsImpulse, Hit.ImpactPoint);
		}
		if (!HitCharacter)
		{
			WorldImpacts.Add(Hit.ImpactPoint);
			WorldNormals.Add(Hit.ImpactNormal);
		}
		// the bone: the one of the hit (a colliding mesh), else the one the projectile passes nearest to (the capsule
		// took the trace)
		else if (Hit.BoneName.IsNone() || !Cast<USkeletalMeshComponent>(Hit.GetComponent()))
		{
			Hit.BoneName = FirearmDamage::FindStruckBone(HitCharacter->GetMesh(), Origin, End, Rules->BoneHitRadiusCm);
		}
		const float Damage = Rules->DamageFalloff.IsEmpty()
			? FirearmMath::DamageAtDistance(Rules->Damage, Hit.Distance, Rules->FalloffStartCm, Rules->RangeCm, Rules->FalloffMinScale)
			: Rules->Damage * FirearmDamage::ScaleAtDistance(Rules->DamageFalloff, Hit.Distance);
		FTargetHits* Entry = Targets.FindByPredicate([&Hit](const FTargetHits& Item) { return Item.Target.Get() == Hit.GetActor(); });
		if (!Entry)
		{
			Entry = &Targets.AddDefaulted_GetRef();
			Entry->Target = Hit.GetActor();
			Entry->FirstHit = Hit;
			Entry->Direction = Direction;
		}
		Entry->Damage += Damage;
		++Entry->Hits;
		FirearmDamage::AddPellet(Entry->Groups, Hit.BoneName, Damage, Hit.ImpactPoint);
		Entry->Impacts.Add(Hit.ImpactPoint);
	}
	if (!WorldImpacts.IsEmpty())
	{
		MulticastShotImpacts(WorldImpacts, WorldNormals);
	}
	if (!TracerEnds.IsEmpty())
	{
		MulticastShotTracers(TracerEnds);
	}

	// one blow per target: the pellets of a shotgun shot add up (one reaction, one death), their bones carried along
	for (const FTargetHits& Entry : Targets)
	{
		AActor* Target = Entry.Target.Get();
		if (!Target || !Target->CanBeDamaged() || Entry.Damage <= 0.0f)
		{
			continue;
		}
		const APawn* TargetPawn = Cast<APawn>(Target);
		const bool bFriendly = TargetPawn && TargetPawn->IsPlayerControlled();
		if (bFriendly && !Rules->bFriendlyFire)
		{
			continue;
		}
		FFirearmDamageEvent Shot;
		Shot.DamageTypeClass = UDamageType::StaticClass();
		Shot.Damage = Entry.Damage;
		Shot.ShotDirection = Entry.Direction;
		Shot.HitInfo = Entry.FirstHit;
		Shot.BoneHits = Entry.Groups;
		Shot.Impacts = Entry.Impacts;
		Shot.PelletsFired = Count;
		Shot.LimbDamageScale = Rules->LimbDamageScale;
		// the hit of the strongest bone group stands for the shot (BoneName of the HitResult)
		const int32 Strongest = FirearmDamage::StrongestGroup(Entry.Groups);
		if (Entry.Groups.IsValidIndex(Strongest))
		{
			Shot.HitInfo.BoneName = Entry.Groups[Strongest].Bone;
		}
		DescribeShot(Shot, Entry.FirstHit.Distance);
		Target->TakeDamage(Entry.Damage, Shot, Carrier->GetController(), this);
		if (ACharacter* TargetCharacter = Cast<ACharacter>(Target))
		{
			const float Share = static_cast<float>(Entry.Hits) / static_cast<float>(Count);
			TargetCharacter->LaunchCharacter(Entry.Direction.GetSafeNormal2D() * Rules->KnockbackSpeed * Share + FVector(0.0f, 0.0f, 60.0f * Share), true, false);
		}
		UE_LOG(Logmonke_island, Log, TEXT("Firearm %s: %d/%d hits on %s at %.0f cm, %.0f damage, %d bone(s), strongest %s"), *GetName(), Entry.Hits, Count,
			*Target->GetName(), Entry.FirstHit.Distance, Entry.Damage, Entry.Groups.Num(), *Shot.HitInfo.BoneName.ToString());
	}
}

void AFirearmWeapon::MulticastShotImpacts_Implementation(const TArray<FVector_NetQuantize>& Locations, const TArray<FVector_NetQuantizeNormal>& Normals)
{
	if (!Rules || !Rules->ImpactSystem || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	for (int32 Index = 0; Index < Locations.Num(); ++Index)
	{
		const FVector Location = Locations[Index];
		if (Camera && FVector::DistSquared(Camera->GetCameraLocation(), Location) > FMath::Square(Rules->EffectsCullDistance))
		{
			continue;
		}
		// the puff leaves the surface along its normal (the system sprays along its local +X)
		const FVector Normal = Normals.IsValidIndex(Index) ? FVector(Normals[Index]) : FVector::UpVector;
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Rules->ImpactSystem, Location, Normal.Rotation(), FVector::OneVector, true, true,
			ENCPoolMethod::AutoRelease);
	}
}

void AFirearmWeapon::MulticastShotTracers_Implementation(const TArray<FVector_NetQuantize>& Ends)
{
	if (!Rules || !Rules->Tracer.bEnabled || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// from the muzzle this machine sees: the first person weapon for its carrier, the third person one elsewhere
	const USkeletalMeshComponent* Mesh = GetVisibleWeaponMesh();
	const FName MuzzleSocket = GetShotMuzzleSocket();
	const FVector Muzzle = Mesh && !MuzzleSocket.IsNone() && Mesh->DoesSocketExist(MuzzleSocket) ? Mesh->GetSocketLocation(MuzzleSocket)
		: (Mesh ? Mesh->GetComponentLocation() : GetActorLocation());
	for (const FVector_NetQuantize& End : Ends)
	{
		AFirearmTracer::Launch(GetWorld(), Rules->Tracer, Muzzle, End);
	}
}

// ---------------------------------------------------------------- gestures on this machine

void AFirearmWeapon::OnRep_Action()
{
	ApplyServerAction();
}

void AFirearmWeapon::ApplyServerAction()
{
	PlayedSerial = Action.Serial;
	if (!Rules)
	{
		return;
	}
	if (Action.Gesture == EFirearmGesture::None)
	{
		// cut by the server: back to the carry loops
		if (bPlayingFromServer)
		{
			PlayingGesture = EFirearmGesture::None;
			bPlayingFromServer = false;
		}
		return;
	}
	const FFirearmClip* Clip = Rules->FindClip(Action.Gesture);
	if (!Clip)
	{
		return;
	}
	const float Elapsed = FMath::Max(static_cast<float>(FirearmServerNow(GetWorld()) - Action.ServerStartTime), 0.0f);
	const float ClipTime = Action.ClipStart + Elapsed * GetGestureRate(*Clip);
	if (ClipTime >= Clip->Duration)
	{
		// already over (late joiner)
		return;
	}
	// the carrier already shows the shot it asked for
	const bool bPredicted = PredictedGesture == Action.Gesture && GetWorld()->GetTimeSeconds() - PredictedTime < 1.0;
	PredictedGesture = EFirearmGesture::None;
	if (bPredicted && PlayingGesture == Action.Gesture)
	{
		bPlayingFromServer = true;
		return;
	}
	PlayGesture(Action.Gesture, ClipTime, true);
}

void AFirearmWeapon::PlayGesture(EFirearmGesture Gesture, float ClipStart, bool bFromServer)
{
	const FFirearmClip* Clip = Rules ? Rules->FindClip(Gesture) : nullptr;
	if (!Clip)
	{
		return;
	}
	PlayingGesture = Gesture;
	PlayingClipStart = ClipStart;
	PlayingStartTime = GetWorld()->GetTimeSeconds();
	PlayingRate = GetGestureRate(*Clip);
	PlayingDuration = Clip->Duration;
	bPlayingLoop = Clip->bLoop;
	bPlayingFromServer = bFromServer;
	// the events at the start time play too (the shot starts on its Fire event)
	LastCosmeticTime = ClipStart - UE_KINDA_SMALL_NUMBER;
	ApplyClipToMeshes(*Clip, ClipStart);
}

void AFirearmWeapon::ApplyClipToMeshes(const FFirearmClip& Clip, float Time)
{
	const float Rate = GetGestureRate(Clip);
	auto Play = [&Clip, Time, Rate](USkeletalMeshComponent* Mesh, UAnimSequence* Animation)
	{
		if (!Mesh || !Animation || !Mesh->GetSkeletalMeshAsset())
		{
			return;
		}
		Mesh->PlayAnimation(Animation, Clip.bLoop);
		Mesh->SetPlayRate(Rate);
		const float Length = Animation->GetPlayLength();
		Mesh->SetPosition(Clip.bLoop && Length > 0.0f ? FMath::Fmod(Time, Length) : FMath::Min(Time, Length), false);
	};
	// the four clips of the gesture together: body and weapon from the same time
	Play(BodyAnimator, Clip.BeanThirdPerson);
	Play(ThirdPersonWeapon, Clip.WeaponThirdPerson);
	if (IsCarrierLocallyControlled())
	{
		Play(FirstPersonArms, Clip.BeanFirstPerson);
		Play(FirstPersonWeapon, Clip.WeaponFirstPerson);
	}
}

float AFirearmWeapon::GetPlayingClipTime() const
{
	const float Time = PlayingClipStart + static_cast<float>(GetWorld()->GetTimeSeconds() - PlayingStartTime) * PlayingRate;
	return bPlayingLoop && PlayingDuration > 0.0f ? FMath::Fmod(Time, PlayingDuration) : Time;
}

EFirearmGesture AFirearmWeapon::DesiredLoop() const
{
	if (bAiming)
	{
		return EFirearmGesture::AimIdle;
	}
	const AIslanderCharacter* Carrier = GetCarrier();
	const UIslanderMovementComponent* Move = Carrier ? Cast<UIslanderMovementComponent>(Carrier->GetCharacterMovement()) : nullptr;
	// the smoothed speed, with a lower threshold to stop walking than to start (T-330)
	const bool bMovingLoopNow = PlayingGesture == EFirearmGesture::Walk || PlayingGesture == EFirearmGesture::Sprint;
	if (!Carrier || !Rules || !FirearmMath::IsMovingForLoop(SmoothedCarrierSpeed, Rules->WalkSpeedThreshold, Rules->WalkExitSpeedFactor, bMovingLoopNow))
	{
		return EFirearmGesture::Idle;
	}
	return Move && Move->IsSprinting() ? EFirearmGesture::Sprint : EFirearmGesture::Walk;
}

void AFirearmWeapon::UpdateGestures()
{
	if (PlayedSerial != Action.Serial)
	{
		ApplyServerAction();
	}

	// one-shot gesture: its events, then its end
	if (PlayingGesture != EFirearmGesture::None && !bPlayingLoop)
	{
		const float Time = GetPlayingClipTime();
		if (const FFirearmClip* Clip = Rules->FindClip(PlayingGesture))
		{
			const EFirearmGesture Gesture = PlayingGesture;
			for (const FFirearmClipEvent& Event : Clip->Events)
			{
				if (Event.Time > LastCosmeticTime && Event.Time <= Time)
				{
					OnCosmeticEvent(Gesture, Event.Name);
				}
			}
		}
		LastCosmeticTime = Time;
		if (Time >= PlayingDuration)
		{
			PlayingGesture = EFirearmGesture::None;
			bPlayingFromServer = false;
		}
	}

	// aim in / out between the loops
	const bool bAimNow = bAiming;
	if (bAimNow != bLastAiming)
	{
		bLastAiming = bAimNow;
		if (PlayingGesture == EFirearmGesture::None || bPlayingLoop)
		{
			PlayGesture(bAimNow ? EFirearmGesture::AimIn : EFirearmGesture::AimOut, 0.0f, false);
		}
	}

	// carrying: the loop of the carrier's state. Between two carry loops (idle, walk, run) the new one must be wanted
	// for LoopConfirmSeconds and the one playing must have played LoopMinSeconds: a speed at the threshold or a run
	// that hesitates for a frame (a seam of the ground, a server correction) restarted the arms and the weapon, which
	// shook the view (owner report 2026-10-11, T-330). Aiming and the machine gun's crank still answer at once.
	const EFirearmGesture Loop = DesiredLoop();
	if (PlayingGesture == EFirearmGesture::None)
	{
		PlayGesture(Loop, 0.0f, false);
		PendingLoop = EFirearmGesture::None;
	}
	else if (bPlayingLoop && PlayingGesture != Loop)
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (!IsCarryLoop(PlayingGesture) || !IsCarryLoop(Loop))
		{
			PlayGesture(Loop, 0.0f, false);
			PendingLoop = EFirearmGesture::None;
			return;
		}
		if (PendingLoop != Loop)
		{
			PendingLoop = Loop;
			PendingLoopSince = Now;
		}
		if (FirearmMath::ShouldSwitchCarryLoop(Now, PendingLoopSince, PlayingStartTime, Rules->LoopConfirmSeconds, Rules->LoopMinSeconds))
		{
			// walk and run keep the phase of the step: the arms do not jump between two similar loops
			float Start = 0.0f;
			const FFirearmClip* Next = Rules->FindClip(Loop);
			if (Next && PlayingGesture != EFirearmGesture::Idle && Loop != EFirearmGesture::Idle)
			{
				Start = FirearmMath::SamePhaseTime(GetPlayingClipTime(), PlayingDuration, Next->Duration);
			}
			PlayGesture(Loop, Start, false);
			PendingLoop = EFirearmGesture::None;
		}
	}
	else
	{
		PendingLoop = EFirearmGesture::None;
	}
}

// ---------------------------------------------------------------- effects

void AFirearmWeapon::OnCosmeticEvent(EFirearmGesture Gesture, FName Event)
{
	if (!Rules)
	{
		return;
	}
	if (Event == Rules->FireEvent && (Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire))
	{
		PlayShotEffects();
	}
	for (const FFirearmEventSound& Cue : Rules->EventSounds)
	{
		if (Cue.Event == Event && Cue.Sound)
		{
			UGameplayStatics::SpawnSoundAttached(Cue.Sound, RootComponent, NAME_None, FVector::ZeroVector,
				EAttachLocation::KeepRelativeOffset, true, Cue.Volume, 1.0f, 0.0f, Rules->EventSoundAttenuation);
		}
	}
}

USkeletalMeshComponent* AFirearmWeapon::GetVisibleWeaponMesh() const
{
	return FirstPersonWeapon->bHiddenInGame ? ThirdPersonWeapon.Get() : FirstPersonWeapon.Get();
}

void AFirearmWeapon::SetWeaponMeshVisible(bool bVisible)
{
	// SetVisibility is apart from the hidden-in-game flag the views toggle: both must agree for the mesh to draw
	FirstPersonWeapon->SetVisibility(bVisible, true);
	ThirdPersonWeapon->SetVisibility(bVisible, true);
}

void AFirearmWeapon::SetWeaponBonesHidden(const TArray<FName>& Bones, bool bHideThem)
{
	for (USkeletalMeshComponent* Mesh : { FirstPersonWeapon.Get(), ThirdPersonWeapon.Get() })
	{
		if (!Mesh || !Mesh->GetSkeletalMeshAsset())
		{
			continue;
		}
		for (const FName& Bone : Bones)
		{
			if (Mesh->GetBoneIndex(Bone) == INDEX_NONE)
			{
				continue;
			}
			if (bHideThem)
			{
				Mesh->HideBoneByName(Bone, PBO_None);
			}
			else
			{
				Mesh->UnHideBoneByName(Bone);
			}
		}
	}
}

FName AFirearmWeapon::GetShotMuzzleSocket() const
{
	return Rules ? Rules->MuzzleSocket : NAME_None;
}

void AFirearmWeapon::PlayShotEffects()
{
	USkeletalMeshComponent* Mesh = GetVisibleWeaponMesh();
	const FName MuzzleSocket = GetShotMuzzleSocket();
	const bool bHasMuzzle = !MuzzleSocket.IsNone() && Mesh->DoesSocketExist(MuzzleSocket);
	const FVector MuzzleLocation = bHasMuzzle ? Mesh->GetSocketLocation(MuzzleSocket) : Mesh->GetComponentLocation();
	// the weapon bones are baked in the bean's space: the barrel points where the carrier aims
	const FRotator Aim = GetCarrierAim().Rotation();

	if (Rules->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Rules->FireSound, MuzzleLocation);
	}
	// the blast of the muzzle (T-167): flame, smoke and sparks thrown along the barrel, left in the world; not in front of
	// the eye of a carrier looking through a scope (a narrow view: it would fill it), the others see it
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	const bool bNear = !Camera || FVector::DistSquared(Camera->GetCameraLocation(), MuzzleLocation) <= FMath::Square(Rules->EffectsCullDistance);
	for (UNiagaraSystem* Blast : Rules->MuzzleBlastSystems)
	{
		if (Blast && bNear && !bScopeViewShown)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Blast, MuzzleLocation, FRotator(Aim.Pitch, Aim.Yaw, 0.0f), FVector::OneVector, true, true,
				ENCPoolMethod::AutoRelease);
		}
	}
	if (Rules->FlashMesh && !bScopeViewShown)
	{
		if (!FlashComponent)
		{
			FlashComponent = NewObject<UStaticMeshComponent>(this, TEXT("MuzzleFlash"));
			FlashComponent->SetStaticMesh(Rules->FlashMesh);
			if (Rules->FlashMaterial)
			{
				FlashComponent->SetMaterial(0, Rules->FlashMaterial);
			}
			FlashComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			FlashComponent->SetCastShadow(false);
			FlashComponent->RegisterComponent();
		}
		FlashComponent->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, bHasMuzzle ? MuzzleSocket : NAME_None);
		FlashComponent->SetWorldRotation(FRotator(Aim.Pitch, Aim.Yaw, FMath::FRandRange(0.0f, 360.0f)));
		FlashComponent->SetWorldScale3D(Rules->FlashScale * FMath::FRandRange(0.85f, 1.15f));
		FlashComponent->SetVisibility(true);
	}
	if (Rules->FlashLightIntensity > 0.0f)
	{
		if (!FlashLight)
		{
			FlashLight = NewObject<UPointLightComponent>(this, TEXT("MuzzleLight"));
			FlashLight->SetCastShadows(false);
			FlashLight->RegisterComponent();
		}
		FlashLight->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, bHasMuzzle ? MuzzleSocket : NAME_None);
		FlashLight->SetWorldScale3D(FVector::OneVector);
		FlashLight->SetLightColor(Rules->FlashLightColor);
		FlashLight->SetIntensity(Rules->FlashLightIntensity);
		FlashLight->SetAttenuationRadius(Rules->FlashLightRadius);
		FlashLight->SetVisibility(true);
	}
	FlashEndTime = GetWorld()->GetTimeSeconds() + Rules->FlashSeconds;

	if (IsCarrierLocallyControlled())
	{
		PendingViewKick += FVector2D(Rules->ViewKickPitch, FMath::FRandRange(-Rules->ViewKickYaw, Rules->ViewKickYaw));
	}
}

// ---------------------------------------------------------------- per frame

void AFirearmWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !Rules)
	{
		return;
	}
	const bool bUsable = IsUsable();
	if (HasAuthority() && !bUsable && IsBusy())
	{
		// swimming, carrying an object, knocked out: the gesture (a reload) is lost
		StopAction();
	}

	if (IsCarrierLocallyControlled())
	{
		BindInput();
		SetInputContextActive(MappingContext, MappingPriority, bUsable);
		if (!bUsable && bAimWanted)
		{
			// the keys went with the context: the aim is asked again once the weapon is usable (the server is told)
			SetAiming(false);
		}
	}
	// the server and the carrier apply the same rule to the same replicated gesture (network review R-38)
	if (HasAuthority() || IsCarrierLocallyControlled())
	{
		RefreshAim();
	}

	// the carrier's speed, smoothed for the choice of the carry loop (T-330)
	SmoothedCarrierSpeed = FirearmMath::SmoothSpeed(SmoothedCarrierSpeed, static_cast<float>(Carrier->GetVelocity().Size2D()), DeltaSeconds,
		Rules->LoopSpeedSmoothingSeconds);
	UpdateGestures();

	const bool bFirstPersonView = IsCarrierLocallyControlled() && Carrier->IsFirstPerson() && !Carrier->IsRagdoll();
	UpdateFirstPerson(DeltaSeconds, bUsable && bFirstPersonView);
	UpdateThirdPerson(DeltaSeconds, bUsable && !bFirstPersonView);

	// flame out, view kick
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= FlashEndTime)
	{
		if (FlashComponent && FlashComponent->IsVisible())
		{
			FlashComponent->SetVisibility(false);
		}
		if (FlashLight && FlashLight->IsVisible())
		{
			FlashLight->SetVisibility(false);
		}
	}
	if (!PendingViewKick.IsNearlyZero(0.01f))
	{
		AController* Controller = Carrier->GetController();
		const FVector2D Applied = PendingViewKick * FMath::Min(1.0f, DeltaSeconds * 25.0f);
		PendingViewKick -= Applied;
		if (Controller && IsCarrierLocallyControlled())
		{
			FRotator View = Controller->GetControlRotation();
			View.Pitch += Applied.X;
			View.Yaw += Applied.Y;
			Controller->SetControlRotation(View);
		}
	}
	TickCosmetics(DeltaSeconds);

	// put away: everything hidden and given back, stop ticking until equipped again
	if (!IsEquipped() && UpperBodyAlpha <= 0.0f && Now >= FlashEndTime && PendingViewKick.IsNearlyZero(0.01f) && !HasCosmeticsRunning())
	{
		SetActorTickEnabled(false);
	}
}

void AFirearmWeapon::UpdateFirstPerson(float DeltaSeconds, bool bShow)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UCameraComponent* Camera = Carrier ? Carrier->GetFirstPersonCamera() : nullptr;
	const bool bWasShown = !FirstPersonWeapon->bHiddenInGame;
	if (bShow != bWasShown)
	{
		FirstPersonWeapon->SetHiddenInGame(!bShow);
		FirstPersonArms->SetHiddenInGame(!bShow || !FirstPersonArms->GetSkeletalMeshAsset());
		if (Arms.IsValid())
		{
			// the pack brings its own arms
			Arms->SetHiddenByItem(bShow && FirstPersonArms->GetSkeletalMeshAsset() != nullptr);
		}
		if (bShow && PlayingGesture != EFirearmGesture::None)
		{
			if (const FFirearmClip* Clip = Rules->FindClip(PlayingGesture))
			{
				ApplyClipToMeshes(*Clip, GetPlayingClipTime());
			}
		}
	}
	if (!bShow)
	{
		SetScopeView(false);
		RestoreFieldOfView();
		return;
	}

	// framing: the weapon held low on the right, or its barrel on the view axis when aiming (values in the rules)
	if (!bPosesMeasured)
	{
		MeasureFirstPersonPoses();
	}
	const float TargetAlpha = bAiming ? 1.0f : 0.0f;
	AimAlpha = bWasShown ? FMath::FInterpTo(AimAlpha, TargetAlpha, DeltaSeconds, Rules->AimFramingSpeed) : TargetAlpha;
	FirstPersonRig->SetRelativeTransform(ComputeRigTransform(AimAlpha));
	// a scope fills the view once the weapon is up to the eye (the bolt rifle, T-335)
	SetScopeView(Rules->bHideFirstPersonWhenAimed && bAiming && AimAlpha >= Rules->AimHideAlpha);

	if (Camera)
	{
		if (DefaultFieldOfView <= 0.0f)
		{
			DefaultFieldOfView = Camera->FieldOfView;
		}
		// the pack's lens, so the weapon frames as authored; narrower while aiming
		const float Held = Rules->FirstPersonFieldOfView > 0.0f ? Rules->FirstPersonFieldOfView : DefaultFieldOfView;
		const float Target = bAiming ? Rules->AimFieldOfView : Held;
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, Target, DeltaSeconds, 10.0f));
	}
}

void AFirearmWeapon::MeasureFirstPersonPoses()
{
	bPosesMeasured = true;
	const USkeletalMesh* Asset = FirstPersonWeapon->GetSkeletalMeshAsset();
	if (!Rules || !Asset || !Rules->bAlignFirstPersonToView)
	{
		return;
	}
	const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
	const int32 Grip = RefSkeleton.FindBoneIndex(Rules->GripSocket);
	const int32 Muzzle = RefSkeleton.FindBoneIndex(Rules->MuzzleSocket);
	GripInWeapon = Grip != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Grip).GetLocation() : FVector::ZeroVector;
	MuzzleInWeapon = Muzzle != INDEX_NONE ? FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Muzzle).GetLocation() : FVector::ZeroVector;

	// the point of the weapon put on the view axis when aimed: a scope's eyepiece (a bone of the pack, or a mesh socket),
	// else the muzzle as before
	AimPointInWeapon = MuzzleInWeapon;
	if (!Rules->AimSightSocket.IsNone())
	{
		const int32 Sight = RefSkeleton.FindBoneIndex(Rules->AimSightSocket);
		const USkeletalMeshSocket* Socket = Sight == INDEX_NONE ? Asset->FindSocket(Rules->AimSightSocket) : nullptr;
		const int32 SocketBone = Socket ? RefSkeleton.FindBoneIndex(Socket->BoneName) : INDEX_NONE;
		if (Sight != INDEX_NONE)
		{
			AimPointInWeapon = FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, Sight).GetLocation();
		}
		else if (SocketBone != INDEX_NONE)
		{
			AimPointInWeapon = (Socket->GetSocketLocalTransform() * FAnimationRuntime::GetComponentSpaceTransformRefPose(RefSkeleton, SocketBone)).GetLocation();
		}
		else
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Firearm %s: no %s on %s, aimed with the muzzle on the view axis"), *GetName(),
				*Rules->AimSightSocket.ToString(), *Asset->GetName());
		}
	}

	bHipPoseValid = MeasureWeaponPose(EFirearmGesture::Idle, HipWeaponPose);
	bAimPoseValid = bHipPoseValid && MeasureWeaponPose(EFirearmGesture::AimIdle, AimWeaponPose);
	if (!bHipPoseValid)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Firearm %s: no %s bone or Idle FP clip, first person framed by the pack's camera"),
			*GetName(), *Rules->FirstPersonAlignBone.ToString());
	}
	UE_LOG(Logmonke_island, Log, TEXT("Firearm %s: FP framing grip %s muzzle %s, hip pose %s, aim pose %s"), *GetName(), *GripInWeapon.ToCompactString(),
		*MuzzleInWeapon.ToCompactString(), *HipWeaponPose.Rotator().ToCompactString(), *AimWeaponPose.Rotator().ToCompactString());

	// the clip playing now comes back on the weapon
	if (const FFirearmClip* Clip = PlayingGesture != EFirearmGesture::None ? Rules->FindClip(PlayingGesture) : nullptr)
	{
		ApplyClipToMeshes(*Clip, GetPlayingClipTime());
	}
}

bool AFirearmWeapon::MeasureWeaponPose(EFirearmGesture Gesture, FTransform& OutPose)
{
	const FFirearmClip* Clip = Rules ? Rules->FindClip(Gesture) : nullptr;
	const USkeletalMesh* Asset = FirstPersonWeapon->GetSkeletalMeshAsset();
	const int32 Bone = Asset ? FirstPersonWeapon->GetBoneIndex(Rules->FirstPersonAlignBone) : INDEX_NONE;
	if (!Clip || !Clip->WeaponFirstPerson || Bone == INDEX_NONE)
	{
		return false;
	}
	// the first frame, evaluated now on the (hidden) first person weapon
	FirstPersonWeapon->PlayAnimation(Clip->WeaponFirstPerson, false);
	FirstPersonWeapon->SetPosition(0.0f, false);
	FirstPersonWeapon->TickAnimation(0.0f, false);
	FirstPersonWeapon->RefreshBoneTransforms();
	const FTransform Posed = FirstPersonWeapon->GetBoneTransform(Bone, FTransform::Identity);
	const FTransform Rest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Asset->GetRefSkeleton(), Bone);
	// rest space -> rig: Posed = Rest * Pose
	OutPose = Rest.Inverse() * Posed;
	OutPose.SetScale3D(FVector::OneVector);
	return !OutPose.ContainsNaN();
}

FTransform AFirearmWeapon::ComputeRigTransform(float InAimAlpha) const
{
	// the pack's FP camera on the player's camera (the bean root at the inverse of the view)
	const FTransform PackRig = FTransform(Rules->FirstPersonViewRotation, Rules->FirstPersonViewLocation).Inverse();
	if (!Rules->bAlignFirstPersonToView || !bHipPoseValid)
	{
		return PackRig;
	}
	// the weapon's rest space placed in the view, then the rig such that the clip's weapon lands there
	const FQuat HipQuat = Rules->HipRotation.Quaternion();
	const FTransform HipInView(HipQuat, Rules->HipGripLocation - HipQuat.RotateVector(GripInWeapon));
	const FTransform HipRig = HipWeaponPose.Inverse() * HipInView;
	if (!bAimPoseValid || InAimAlpha <= 0.0f)
	{
		return HipRig;
	}
	const FQuat AimQuat = Rules->AimRotation.Quaternion();
	const FTransform AimInView(AimQuat, Rules->AimMuzzleLocation - AimQuat.RotateVector(AimPointInWeapon));
	const FTransform AimRig = AimWeaponPose.Inverse() * AimInView;
	FTransform Rig;
	Rig.Blend(HipRig, AimRig, FMath::Clamp(InAimAlpha, 0.0f, 1.0f));
	return Rig;
}

void AFirearmWeapon::SetScopeView(bool bInScopeView)
{
	if (bScopeViewShown == bInScopeView)
	{
		return;
	}
	bScopeViewShown = bInScopeView;
	// the visibility, apart from the hidden-in-game flag the views toggle: both must agree for a mesh to draw
	FirstPersonWeapon->SetVisibility(!bInScopeView);
	FirstPersonArms->SetVisibility(!bInScopeView);
}

void AFirearmWeapon::RestoreFieldOfView()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	if (UCameraComponent* Camera = Carrier ? Carrier->GetFirstPersonCamera() : nullptr; Camera && DefaultFieldOfView > 0.0f)
	{
		Camera->SetFieldOfView(DefaultFieldOfView);
	}
	DefaultFieldOfView = 0.0f;
}

void AFirearmWeapon::UpdateThirdPerson(float DeltaSeconds, bool bShow)
{
	if (ThirdPersonWeapon->bHiddenInGame == bShow)
	{
		ThirdPersonWeapon->SetHiddenInGame(!bShow);
	}
	if (!Puppet.IsValid() || !BodyAnimator || !Rules)
	{
		return;
	}

	// upper body only, so the legs keep walking; eased in and out (equip, swim, carry an object, first person)
	UpperBodyAlpha = FMath::FInterpTo(UpperBodyAlpha, bShow ? 1.0f : 0.0f, DeltaSeconds, 10.0f);
	if (!bShow && UpperBodyAlpha < 0.01f)
	{
		UpperBodyAlpha = 0.0f;
	}
	const bool bEvaluate = UpperBodyAlpha > 0.0f;
	if (BodyAnimator->IsComponentTickEnabled() != bEvaluate)
	{
		// the hidden body costs a full bean evaluation: only while it drives the puppet
		BodyAnimator->SetComponentTickEnabled(bEvaluate);
		if (bEvaluate && PlayingGesture != EFirearmGesture::None)
		{
			if (const FFirearmClip* Clip = Rules->FindClip(PlayingGesture))
			{
				ApplyClipToMeshes(*Clip, GetPlayingClipTime());
			}
		}
	}
	if (bEvaluate)
	{
		Puppet->SetBoneOverrideSource(BodyAnimator.Get(), Rules->UpperBodyBones, UpperBodyAlpha);
		bPuppetReleased = false;
	}
	else if (!bPuppetReleased)
	{
		// once: another item (the musket) may drive the puppet now
		Puppet->SetBoneOverrideSource(nullptr, {}, 0.0f);
		bPuppetReleased = true;
	}

	// the weapon keeps, relative to the puppet's anchor bone, the place it has relative to that bone in the clip
	// (both roots share the bean's space in the clip; the bone scales of 100 cancel out)
	if (bShow && BodyAnimator->GetSkeletalMeshAsset() && BodyAnimator->GetBoneIndex(Rules->AnchorBone) != INDEX_NONE
		&& Puppet->GetBoneIndex(Rules->AnchorBone) != INDEX_NONE)
	{
		const FTransform ClipBone = BodyAnimator->GetSocketTransform(Rules->AnchorBone, RTS_Component);
		const FTransform RealBone = Puppet->GetSocketTransform(Rules->AnchorBone, RTS_World);
		if (ClipBone.GetScale3D().GetAbsMin() > UE_KINDA_SMALL_NUMBER && RealBone.GetScale3D().GetAbsMin() > UE_KINDA_SMALL_NUMBER)
		{
			ThirdPersonWeapon->SetWorldTransform(ClipBone.Inverse() * RealBone);
		}
	}
}
