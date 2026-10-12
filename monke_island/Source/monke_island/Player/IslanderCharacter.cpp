// monke_island

#include "Player/IslanderCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Emotes/TrumpetEmoteComponent.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCombatComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderKnockoutMath.h"
#include "Player/IslanderMovementComponent.h"
#include "TimerManager.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"

AIslanderCharacter::AIslanderCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UIslanderMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// ticks only while ragdolling (buoyancy in the water)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// real-world proportions: 1.80 m tall, ~0.68 m across the shoulders
	GetCapsuleComponent()->InitCapsuleSize(34.0f, 90.0f);

	// the mannequin is authored facing +Y with its origin between the feet
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -90.0f), FRotator(0.0f, -90.0f, 0.0f));

	// the body turns toward its movement, the camera follows the controller
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 35.0f, 60.0f);
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetFieldOfView(80.0f);

	// eyes of a 1.80 m adult; a skin (bean) moves it to its own eyes
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(RootComponent);
	FirstPersonCamera->SetRelativeLocation(FVector(15.0f, 0.0f, 70.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(90.0f);
	FirstPersonCamera->SetAutoActivate(false);
	FirstPersonCamera->ComponentTags.Add(TEXT("FirstPerson"));

	HealthComponent = CreateDefaultSubobject<UIslanderHealthComponent>(TEXT("Health"));
	CombatComponent = CreateDefaultSubobject<UIslanderCombatComponent>(TEXT("Combat"));
	GrabComponent = CreateDefaultSubobject<UIslanderGrabComponent>(TEXT("Grab"));
}

void AIslanderCharacter::BeginPlay()
{
	Super::BeginPlay();

	bFirstPerson = bStartInFirstPerson;
	ApplyViewMode(IsFirstPerson(), IsFirstPerson());

	// in a network session the giant crab's bodies follow the animation of each machine: the moves predicted here would
	// meet them elsewhere than the server does, its fixed shapes stop the islander instead (T-305). Alone, nothing is
	// predicted: its bodies keep stopping the islander as before
	if (UCapsuleComponent* Capsule = GetCapsuleComponent(); Capsule && GetNetMode() != NM_Standalone)
	{
		Capsule->SetMoveIgnoreMask(AnimatedHitBodyMaskFilter);
	}
}

void AIslanderCharacter::ToggleView()
{
	bFirstPerson = !bFirstPerson;
	// knocked out: the view stays third person until the body stands up
	ApplyViewMode(IsFirstPerson(), IsFirstPerson() && !bIsRagdoll);
	if (!HasAuthority())
	{
		ServerSetFirstPerson(bFirstPerson);
	}
}

void AIslanderCharacter::ServerSetFirstPerson_Implementation(bool bInFirstPerson)
{
	bFirstPerson = bInFirstPerson;
	ApplyViewMode(IsFirstPerson(), IsFirstPerson() && !bIsRagdoll);
}

void AIslanderCharacter::SetForceThirdPerson(bool bForce)
{
	if (bForceThirdPerson == bForce)
	{
		return;
	}
	bForceThirdPerson = bForce;
	ApplyViewMode(IsFirstPerson(), IsFirstPerson() && !bIsRagdoll);
}

void AIslanderCharacter::ApplyViewMode(bool bInFirstPerson, bool bFirstPersonCamera)
{
	FollowCamera->SetActive(!bFirstPersonCamera);
	FirstPersonCamera->SetActive(bFirstPersonCamera);

	// first person (Lethal Company): the body faces where the player looks; third person: it turns toward its movement
	bUseControllerRotationYaw = bInFirstPerson;
	GetCharacterMovement()->bOrientRotationToMovement = !bInFirstPerson;
}

void AIslanderCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddUniqueDynamic(this, &AIslanderCharacter::HandleDeath);
	}
}

void AIslanderCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AIslanderCharacter, KnockoutRep);
}

UIslanderMovementComponent* AIslanderCharacter::GetIslanderMovement() const
{
	return Cast<UIslanderMovementComponent>(GetCharacterMovement());
}

void AIslanderCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// add the character specific mappings for the local player
	if (const APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			for (UInputMappingContext* Context : CharacterMappingContexts)
			{
				if (Context)
				{
					Subsystem->AddMappingContext(Context, 1);
				}
			}
		}
	}
}

void AIslanderCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AIslanderCharacter::JumpPressed);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &AIslanderCharacter::JumpHeld);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AIslanderCharacter::JumpReleased);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AIslanderCharacter::MoveInput);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AIslanderCharacter::LookInput);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AIslanderCharacter::LookInput);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AIslanderCharacter::SprintStart);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AIslanderCharacter::SprintEnd);
		if (CrouchAction)
		{
			// held, as in Counter-Strike (T-262): down on the press, up on the release
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AIslanderCharacter::CrouchPressed);
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &AIslanderCharacter::CrouchHeld);
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AIslanderCharacter::CrouchReleased);
		}
		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AIslanderCharacter::AttackPressed);
		}
		if (GuardAction)
		{
			EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Started, this, &AIslanderCharacter::GuardPressed);
			EnhancedInputComponent->BindAction(GuardAction, ETriggerEvent::Completed, this, &AIslanderCharacter::GuardReleased);
		}
		if (ToggleViewAction && bAllowViewToggle)
		{
			EnhancedInputComponent->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &AIslanderCharacter::ToggleView);
		}
		if (GrabAction)
		{
			EnhancedInputComponent->BindAction(GrabAction, ETriggerEvent::Started, this, &AIslanderCharacter::GrabPressed);
			EnhancedInputComponent->BindAction(GrabAction, ETriggerEvent::Completed, this, &AIslanderCharacter::GrabReleased);
		}
		if (DropAction)
		{
			EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Started, this, &AIslanderCharacter::DropPressed);
		}
		if (HoldDistanceAction)
		{
			EnhancedInputComponent->BindAction(HoldDistanceAction, ETriggerEvent::Triggered, this, &AIslanderCharacter::HoldDistanceInput);
		}
		if (TumbleAction)
		{
			EnhancedInputComponent->BindAction(TumbleAction, ETriggerEvent::Started, this, &AIslanderCharacter::TumblePressed);
		}
	}
	else
	{
		UE_LOG(Logmonke_island, Error, TEXT("'%s' requires an Enhanced Input Component"), *GetNameSafe(this));
	}
}

void AIslanderCharacter::MoveInput(const FInputActionValue& Value)
{
	if (!Controller || bIsRagdoll)
	{
		return;
	}

	const FVector2D Movement = Value.Get<FVector2D>();
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	// on foot only the camera yaw counts; in the water and in the air the view pitch steers up and down
	const UCharacterMovementComponent* CharMovement = GetCharacterMovement();
	const FRotator ForwardRotation = CharMovement->IsSwimming() || CharMovement->IsFlying() ? ControlRotation : YawRotation;

	AddMovementInput(FRotationMatrix(ForwardRotation).GetUnitAxis(EAxis::X), Movement.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Movement.X);
}

void AIslanderCharacter::LookInput(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();

	AddControllerYawInput(Look.X);
	AddControllerPitchInput(Look.Y);
}

void AIslanderCharacter::SprintStart()
{
	if (UIslanderMovementComponent* IslanderMovement = GetIslanderMovement())
	{
		IslanderMovement->SetWantsToSprint(true);
	}

	// sprinting stands the character up (Lethal Company), unless the crouch key is held: crouched, no sprint (T-262)
	if (bIsCrouched && !bCrouchKeyHeld)
	{
		UnCrouch();
	}
}

void AIslanderCharacter::ToggleCrouch()
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else if (!bIsRagdoll && Move->IsMovingOnGround())
	{
		Crouch();
	}
}

bool AIslanderCharacter::IsRitualLocked() const
{
	const UTrumpetEmoteComponent* Trumpet = FindComponentByClass<UTrumpetEmoteComponent>();
	return Trumpet && Trumpet->IsRitualLocked();
}

void AIslanderCharacter::CrouchFromKey()
{
	// held by the flag ceremony: the bean stays as it is; in the water the key dives instead
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!bIsCrouched && !Move->bWantsToCrouch && !bIsRagdoll && Move->IsMovingOnGround() && !IsRitualLocked())
	{
		Crouch();
	}
}

void AIslanderCharacter::CrouchPressed()
{
	// crouched while the key is held, as in Counter-Strike (T-262): unseen by the mobs and villagers but at contact
	bCrouchKeyHeld = true;
	CrouchFromKey();
}

void AIslanderCharacter::CrouchHeld()
{
	// in the water the crouch key dives
	if (GetCharacterMovement()->IsSwimming())
	{
		if (!bIsRagdoll)
		{
			AddMovementInput(FVector::DownVector, 1.0f);
		}
		return;
	}
	// still held once it lands, leaves the water or stands up from a tumble: it crouches as soon as it can
	CrouchFromKey();
}

void AIslanderCharacter::CrouchReleased()
{
	bCrouchKeyHeld = false;
	if ((bIsCrouched || GetCharacterMovement()->bWantsToCrouch) && !IsRitualLocked())
	{
		UnCrouch();
	}
}

void AIslanderCharacter::AttackPressed()
{
	if (bIsRagdoll)
	{
		return;
	}
	// holding an object: left click throws it (cahier §5: carrying prevents attacking)
	if (GrabComponent && GrabComponent->IsHolding())
	{
		GrabComponent->RequestThrow();
	}
	else if (CombatComponent)
	{
		CombatComponent->RequestPunch();
	}
}

void AIslanderCharacter::GrabPressed()
{
	// interaction key (F, T-166): a tap uses the interactable in view (or lifts a plain prop), holding it lifts the
	// physics object in view, pressing it while holding lets go
	if (!bIsRagdoll && GrabComponent && !(CombatComponent && CombatComponent->IsGuarding()))
	{
		GrabComponent->PressInteractKey();
	}
}

void AIslanderCharacter::GrabReleased()
{
	if (GrabComponent)
	{
		GrabComponent->ReleaseInteractKey();
	}
}

void AIslanderCharacter::DropPressed()
{
	if (GrabComponent)
	{
		GrabComponent->RequestRelease();
	}
}

void AIslanderCharacter::HoldDistanceInput(const FInputActionValue& Value)
{
	if (GrabComponent)
	{
		GrabComponent->AdjustHoldDistance(Value.Get<float>());
	}
}

void AIslanderCharacter::TumblePressed()
{
	if (bIsRagdoll || (HealthComponent && HealthComponent->IsDead()))
	{
		return;
	}
	if (const UTrumpetEmoteComponent* Trumpet = FindComponentByClass<UTrumpetEmoteComponent>(); Trumpet && Trumpet->IsRitualLocked())
	{
		return;
	}
	if (HasAuthority())
	{
		ServerTumble_Implementation();
	}
	else
	{
		ServerTumble();
	}
}

void AIslanderCharacter::ServerTumble_Implementation()
{
	if (bIsRagdoll || (HealthComponent && HealthComponent->IsDead()) || GetCharacterMovement()->IsSwimming())
	{
		return;
	}
	if (GrabComponent)
	{
		GrabComponent->RequestRelease();
	}
	BeginServerKnockout(GetVelocity() + GetActorForwardVector() * TumbleForwardSpeed + FVector(0.0f, 0.0f, 150.0f));
}

void AIslanderCharacter::Knockout(FVector Velocity)
{
	if (!HasAuthority() || bIsRagdoll || (HealthComponent && HealthComponent->IsDead()))
	{
		return;
	}
	if (GrabComponent)
	{
		GrabComponent->RequestRelease();
	}
	BeginServerKnockout(Velocity);
}

void AIslanderCharacter::GuardPressed()
{
	if (!bIsRagdoll && CombatComponent)
	{
		CombatComponent->SetGuard(true);
	}
}

void AIslanderCharacter::GuardReleased()
{
	if (CombatComponent)
	{
		CombatComponent->SetGuard(false);
	}
}

void AIslanderCharacter::Landed(const FHitResult& Hit)
{
	// the velocity is still the falling one here
	const float LandingSpeed = static_cast<float>(-GetVelocity().Z);
	Super::Landed(Hit);

	if (HasAuthority() && HealthComponent)
	{
		HealthComponent->ApplyFallDamage(LandingSpeed);
	}
}

void AIslanderCharacter::HandleDeath(AActor* Killer)
{
	if (!HasAuthority())
	{
		return;
	}

	// cahier §5: a dead holder releases its object once
	if (GrabComponent)
	{
		GrabComponent->RequestRelease();
	}
	MulticastDeathRagdoll(GetVelocity());
	if (RespawnDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &AIslanderCharacter::Respawn, RespawnDelay, false);
	}
}

void AIslanderCharacter::MulticastDeathRagdoll_Implementation(FVector_NetQuantize10 Velocity)
{
	if (CombatComponent)
	{
		CombatComponent->SetGuard(false);
	}
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		DisableInput(PlayerController);
	}
	// a dead body does not stand up (already knocked out: it stays down)
	bDeathRagdoll = true;
	const FVector Start = GetActorLocation();
	StartLocalRagdoll(Velocity);
	GetWorldTimerManager().ClearTimer(RagdollTimer);
	if (!HasAuthority())
	{
		return;
	}
	// the players who join or see the body later find it down too; its capsule keeps following it
	if (!KnockoutRep.bActive)
	{
		KnockoutRep.Serial = IslanderKnockoutMath::NextSerial(KnockoutRep.Serial);
		KnockoutRep.StartServerTime = static_cast<float>(GetServerNow());
		KnockoutRep.StartLocation = Start;
		KnockoutRep.StartVelocity = Velocity;
	}
	KnockoutRep.bActive = bIsRagdoll;
	KnockoutRep.bDead = true;
	if (bIsRagdoll && RagdollFollowInterval > 0.0f && !GetWorldTimerManager().IsTimerActive(RagdollFollowTimer))
	{
		GetWorldTimerManager().SetTimer(RagdollFollowTimer, this, &AIslanderCharacter::FollowRagdoll, RagdollFollowInterval, true);
	}
	ForceNetUpdate();
}

void AIslanderCharacter::Respawn()
{
	AController* OldController = Controller;
	AGameModeBase* GameMode = GetWorld()->GetAuthGameMode();
	if (!OldController || !GameMode)
	{
		return;
	}

	// provisional: the cahier's spectator mode and end-of-expedition rules replace this (L5)
	OldController->UnPossess();
	GameMode->RestartPlayer(OldController);
	SetLifeSpan(10.0f);
}

void AIslanderCharacter::SprintEnd()
{
	if (UIslanderMovementComponent* IslanderMovement = GetIslanderMovement())
	{
		IslanderMovement->SetWantsToSprint(false);
	}
}

void AIslanderCharacter::JumpPressed()
{
	if (bIsRagdoll)
	{
		return;
	}
	if (const UTrumpetEmoteComponent* Trumpet = FindComponentByClass<UTrumpetEmoteComponent>(); Trumpet && Trumpet->IsRitualLocked())
	{
		return;
	}

	// a second press shortly after the first toggles flight
	const double Now = GetWorld()->GetTimeSeconds();
	// the double tap flight is a development cheat, off in a packaged build unless monke.AllowFlight (T-291)
	if (UIslanderMovementComponent::IsFlightAllowed() && Now - LastJumpPressTime <= FlyDoubleTapTime)
	{
		LastJumpPressTime = -1.0e6;
		StopJumping();
		ToggleFlying();
		return;
	}
	LastJumpPressTime = Now;

	if (!IsFlying())
	{
		Jump();
	}
}

void AIslanderCharacter::JumpHeld()
{
	// holding jump climbs while flying and swims up toward the surface
	if ((IsFlying() || GetCharacterMovement()->IsSwimming()) && !bIsRagdoll)
	{
		AddMovementInput(FVector::UpVector, 1.0f);
	}
}

void AIslanderCharacter::JumpReleased()
{
	StopJumping();
}

bool AIslanderCharacter::IsFlying() const
{
	return GetCharacterMovement()->IsFlying();
}

void AIslanderCharacter::ToggleFlying()
{
	UIslanderMovementComponent* Move = GetIslanderMovement();
	if (bIsRagdoll || !Move || Move->IsSwimming())
	{
		return;
	}

	// the movement component applies the request on the owning client and the server alike (predicted flag)
	if (bIsCrouched)
	{
		UnCrouch();
	}
	Move->SetWantsToFly(!Move->IsFlying());
}

void AIslanderCharacter::StartRagdoll(FVector InitialVelocity)
{
	// the server decides every knockout (T-287): the crash or the tumble of a client comes back from the server's state
	if (!HasAuthority())
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("'%s': ragdoll asked on a client, the server's knockout plays it here"), *GetNameSafe(this));
		return;
	}
	if (bIsRagdoll || (HealthComponent && HealthComponent->IsDead()))
	{
		return;
	}
	BeginServerKnockout(InitialVelocity);
}

void AIslanderCharacter::BeginServerKnockout(const FVector& Velocity)
{
	const FVector Start = GetActorLocation();
	if (!StartLocalRagdoll(Velocity))
	{
		return;
	}
	KnockoutRep.bActive = true;
	KnockoutRep.bDead = false;
	KnockoutRep.Serial = IslanderKnockoutMath::NextSerial(KnockoutRep.Serial);
	KnockoutRep.StartServerTime = static_cast<float>(GetServerNow());
	KnockoutRep.StartLocation = Start;
	KnockoutRep.StartVelocity = Velocity;

	// the server alone stands the body up; meanwhile its capsule follows the pelvis, where the hits and the mobs look
	GetWorldTimerManager().SetTimer(RagdollTimer, this, &AIslanderCharacter::TryRecoverFromRagdoll, FMath::Max(RagdollMinDuration, 0.01f), false);
	if (RagdollFollowInterval > 0.0f)
	{
		GetWorldTimerManager().SetTimer(RagdollFollowTimer, this, &AIslanderCharacter::FollowRagdoll, RagdollFollowInterval, true);
	}
	ForceNetUpdate();
	UE_LOG(Logmonke_island, Log, TEXT("'%s' knocked out at %s, thrown at %.0f cm/s (knockout %d)"), *GetNameSafe(this), *Start.ToCompactString(),
		Velocity.Size(), KnockoutRep.Serial);
}

void AIslanderCharacter::OnRep_Knockout()
{
	switch (IslanderKnockoutMath::Resolve(KnockoutRep.bActive, KnockoutRep.bDead, bIsRagdoll, bDeathRagdoll))
	{
	case IslanderKnockoutMath::EAction::Start:
		StartKnockoutFromServer();
		break;
	case IslanderKnockoutMath::EAction::Stand:
		// on the server's word only, where the server's body stood up (the local ragdoll landed a little elsewhere)
		StopLocalRagdoll(KnockoutRep.RecoverLocation, KnockoutRep.RecoverYaw);
		break;
	default:
		break;
	}
	if (KnockoutRep.bDead)
	{
		bDeathRagdoll = true;
	}
}

void AIslanderCharacter::StartKnockoutFromServer()
{
	const float Age = IslanderKnockoutMath::CatchUpAge(GetServerNow(), KnockoutRep.StartServerTime, RagdollCatchUpMaxSeconds);
	FVector Velocity = FVector::ZeroVector;
	if (Age >= 0.0f)
	{
		// fresh: from where the server's body started, as far into its flight as the knockout is old, rather than from
		// where this machine had the capsule (a client held by the crab, or ahead of the server by its prediction)
		const float GravityZ = GetWorld()->GetGravityZ();
		const FVector Start = KnockoutRep.StartLocation;
		FVector Location = IslanderKnockoutMath::BallisticLocation(Start, KnockoutRep.StartVelocity, GravityZ, Age);
		Velocity = IslanderKnockoutMath::BallisticVelocity(KnockoutRep.StartVelocity, GravityZ, Age);
		FHitResult Hit;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderKnockoutCatchUp), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Location, ECC_Visibility, Params))
		{
			// something stopped the server's body on the way: the flight starts again from its start
			Location = Start;
			Velocity = KnockoutRep.StartVelocity;
		}
		SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
	}
	// older (joined late, or the body came back into view): it drops where the capsule is, which follows the server's
	StartLocalRagdoll(Velocity);
}

bool AIslanderCharacter::StartLocalRagdoll(const FVector& InitialVelocity)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (bIsRagdoll || !MeshComp)
	{
		return false;
	}

	if (!MeshComp->GetPhysicsAsset())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("'%s' cannot ragdoll: its mesh has no physics asset"), *GetNameSafe(this));
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		return false;
	}

	bIsRagdoll = true;
	RagdollStartTime = GetWorld()->GetTimeSeconds();
	MeshRelativeTransform = MeshComp->GetRelativeTransform();
	MeshCollisionProfile = MeshComp->GetCollisionProfileName();
	CapsuleCollision = GetCapsuleComponent()->GetCollisionEnabled();

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// the capsule stays without colliding; the body is now driven by physics. The mesh leaves the capsule (the engine
	// keeps a simulated skeletal mesh attached): the server's capsule follows the pelvis, the movement corrections and
	// the replicated movement move the capsule, and no move of the capsule may drag the simulated bodies (T-287)
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	MeshComp->SetCollisionProfileName(FName(TEXT("Ragdoll")));
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->SetSimulatePhysics(true);
	for (FBodyInstance* Body : MeshComp->Bodies)
	{
		if (Body)
		{
			Body->SetMaxDepenetrationVelocity(RagdollMaxDepenetrationVelocity);
		}
	}
	MeshComp->WakeAllRigidBodies();
	MeshComp->SetAllPhysicsLinearVelocity(InitialVelocity);
	SetActorTickEnabled(true);

	// the camera follows the body, in third person while knocked out
	CameraBoom->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, RagdollPelvisBone);
	ApplyViewMode(IsFirstPerson(), false);
	return true;
}

void AIslanderCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsRagdoll)
	{
		ApplyRagdollBuoyancy();
	}
	else
	{
		SetActorTickEnabled(false);
	}
}

void AIslanderCharacter::ApplyRagdollBuoyancy()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!MeshComp || !Water)
	{
		return;
	}
	for (FBodyInstance* Body : MeshComp->Bodies)
	{
		// every body simulates while ragdolling (AddForce does nothing on one that does not); no
		// IsInstanceSimulatingPhysics here: it is inline over PhysicsCore, which the editor DLL does not link
		if (!Body || !Body->IsValidBodyInstance())
		{
			continue;
		}
		const FVector Center = Body->GetCOMPosition();
		const float Depth = Water->GetWaterSurfaceAt(Center) - static_cast<float>(Center.Z);
		// 0 at RagdollFloatDepth above the surface, 1 at RagdollFloatDepth below
		const float Immersion = FMath::Clamp((Depth + RagdollFloatDepth) / (2.0f * RagdollFloatDepth), 0.0f, 1.0f);
		if (Immersion <= 0.0f)
		{
			continue;
		}
		const float Mass = Body->GetBodyMass();
		const FVector Lift(0.0f, 0.0f, Mass * -GetWorld()->GetGravityZ() * RagdollBuoyancy * Immersion);
		const FVector Drag = -Body->GetUnrealWorldVelocity() * (Mass * RagdollWaterDrag * Immersion);
		Body->AddForce(Lift + Drag, false, false);
	}
}

void AIslanderCharacter::TryRecoverFromRagdoll()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	// the server alone stands a body up (T-287): the clients follow KnockoutRep
	if (!HasAuthority() || !bIsRagdoll || bDeathRagdoll || !MeshComp)
	{
		return;
	}

	// wait until the body has settled, unless it keeps moving for too long (sinking, sliding)
	const bool bTimedOut = GetWorld()->GetTimeSeconds() - RagdollStartTime >= RagdollMaxDuration;
	if (!bTimedOut && MeshComp->GetPhysicsLinearVelocity(RagdollPelvisBone).Size() > RagdollSettledSpeed)
	{
		GetWorldTimerManager().SetTimer(RagdollTimer, this, &AIslanderCharacter::TryRecoverFromRagdoll, 0.25f, false);
		return;
	}

	// stand up on the ground under the pelvis
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Pelvis = GetRagdollPelvisLocation();
	FVector Location = Pelvis + FVector(0.0f, 0.0f, HalfHeight);

	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderRagdollRecover), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pelvis + FVector(0.0f, 0.0f, HalfHeight), Pelvis - FVector(0.0f, 0.0f, 2.0f * HalfHeight), ECC_Visibility, Params))
	{
		Location = Hit.Location + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
	}

	// every machine stands up there, facing the same way
	const float Yaw = static_cast<float>(GetActorRotation().Yaw);
	KnockoutRep.bActive = false;
	KnockoutRep.RecoverLocation = Location;
	KnockoutRep.RecoverYaw = Yaw;
	StopLocalRagdoll(Location, Yaw);
	ForceNetUpdate();
	UE_LOG(Logmonke_island, Log, TEXT("'%s' stands up at %s after %.1f s (knockout %d)"), *GetNameSafe(this), *Location.ToCompactString(),
		GetWorld()->GetTimeSeconds() - RagdollStartTime, KnockoutRep.Serial);
}

void AIslanderCharacter::StopLocalRagdoll(const FVector& Location, float Yaw)
{
	GetWorldTimerManager().ClearTimer(RagdollTimer);
	GetWorldTimerManager().ClearTimer(RagdollFollowTimer);
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bIsRagdoll || !MeshComp)
	{
		return;
	}

	MeshComp->SetSimulatePhysics(false);
	MeshComp->SetAllBodiesSimulatePhysics(false);
	MeshComp->SetCollisionProfileName(MeshCollisionProfile);
	MeshComp->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	MeshComp->SetRelativeTransform(MeshRelativeTransform);

	SetActorLocationAndRotation(Location, FRotator(0.0f, Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCapsuleComponent()->SetCollisionEnabled(CapsuleCollision);
	CameraBoom->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	bIsRagdoll = false;
	SetActorTickEnabled(false);
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	ApplyViewMode(IsFirstPerson(), IsFirstPerson());
}

void AIslanderCharacter::FollowRagdoll()
{
	if (!HasAuthority() || !bIsRagdoll)
	{
		GetWorldTimerManager().ClearTimer(RagdollFollowTimer);
		return;
	}
	// no sweep and no teleport of the children: the capsule collides with nothing meanwhile, and the ragdoll is no
	// longer attached to it; the owner's machine gets the place by the movement corrections, the others by the
	// replicated movement
	const FVector Target = IslanderKnockoutMath::FollowLocation(GetRagdollPelvisLocation(), GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	if (IslanderKnockoutMath::ShouldFollow(GetActorLocation(), Target, RagdollFollowMinStepCm))
	{
		SetActorLocation(Target, false, nullptr, ETeleportType::None);
	}
}

FVector AIslanderCharacter::GetRagdollPelvisLocation() const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	if (!MeshComp)
	{
		return GetActorLocation();
	}
	// the physics body itself: the bone transforms of a mesh nobody renders may lag behind its simulation
	if (const FBodyInstance* Body = MeshComp->GetBodyInstance(RagdollPelvisBone); Body && Body->IsValidBodyInstance())
	{
		return Body->GetUnrealWorldTransform().GetLocation();
	}
	return MeshComp->GetSocketLocation(RagdollPelvisBone);
}

double AIslanderCharacter::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
