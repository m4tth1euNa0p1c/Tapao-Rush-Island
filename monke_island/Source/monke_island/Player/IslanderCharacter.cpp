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
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCombatComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "TimerManager.h"
#include "monke_island.h"

AIslanderCharacter::AIslanderCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UIslanderMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = false;

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
	ApplyViewMode(bFirstPerson, bFirstPerson);
}

void AIslanderCharacter::ToggleView()
{
	bFirstPerson = !bFirstPerson;
	// knocked out: the view stays third person until the body stands up
	ApplyViewMode(bFirstPerson, bFirstPerson && !bIsRagdoll);
	if (!HasAuthority())
	{
		ServerSetFirstPerson(bFirstPerson);
	}
}

void AIslanderCharacter::ServerSetFirstPerson_Implementation(bool bInFirstPerson)
{
	bFirstPerson = bInFirstPerson;
	ApplyViewMode(bFirstPerson, bFirstPerson && !bIsRagdoll);
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
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started, this, &AIslanderCharacter::CrouchPressed);
			EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &AIslanderCharacter::CrouchHeld);
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
		if (ToggleViewAction)
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

	// sprinting stands the character up (Lethal Company)
	if (bIsCrouched)
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

void AIslanderCharacter::CrouchPressed()
{
	if (!GetCharacterMovement()->IsSwimming())
	{
		ToggleCrouch();
	}
}

void AIslanderCharacter::CrouchHeld()
{
	// in the water the crouch key dives
	if (GetCharacterMovement()->IsSwimming() && !bIsRagdoll)
	{
		AddMovementInput(FVector::DownVector, 1.0f);
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
	// interaction key (F): lift / let go the physics object in view, or use the interactable in view
	if (!bIsRagdoll && GrabComponent && !(CombatComponent && CombatComponent->IsGuarding()))
	{
		GrabComponent->RequestInteract();
	}
}

void AIslanderCharacter::GrabReleased()
{
	// press to lift, press again to let go: nothing on release
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
	MulticastTumble(GetVelocity() + GetActorForwardVector() * TumbleForwardSpeed + FVector(0.0f, 0.0f, 150.0f));
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
	MulticastTumble(Velocity);
}

void AIslanderCharacter::MulticastTumble_Implementation(FVector_NetQuantize10 Velocity)
{
	StartRagdoll(Velocity);
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
	StartRagdoll(Velocity);
	// a dead body does not stand up
	GetWorldTimerManager().ClearTimer(RagdollTimer);
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

	// a second press shortly after the first toggles flight
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastJumpPressTime <= FlyDoubleTapTime)
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
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (bIsRagdoll || !MeshComp)
	{
		return;
	}

	if (!MeshComp->GetPhysicsAsset())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("'%s' cannot ragdoll: its mesh has no physics asset"), *GetNameSafe(this));
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
		return;
	}

	bIsRagdoll = true;
	RagdollStartTime = GetWorld()->GetTimeSeconds();
	MeshRelativeTransform = MeshComp->GetRelativeTransform();
	MeshCollisionProfile = MeshComp->GetCollisionProfileName();
	CapsuleCollision = GetCapsuleComponent()->GetCollisionEnabled();

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// the capsule stays behind without colliding; the body is now driven by physics
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCollisionProfileName(FName(TEXT("Ragdoll")));
	MeshComp->SetAllBodiesSimulatePhysics(true);
	MeshComp->SetSimulatePhysics(true);
	MeshComp->WakeAllRigidBodies();
	MeshComp->SetAllPhysicsLinearVelocity(InitialVelocity);

	// the camera follows the body, in third person while knocked out
	CameraBoom->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, RagdollPelvisBone);
	ApplyViewMode(bFirstPerson, false);

	GetWorldTimerManager().SetTimer(RagdollTimer, this, &AIslanderCharacter::TryRecoverFromRagdoll, FMath::Max(RagdollMinDuration, 0.01f), false);
}

void AIslanderCharacter::TryRecoverFromRagdoll()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!bIsRagdoll || !MeshComp)
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
	const FVector Pelvis = MeshComp->GetSocketLocation(RagdollPelvisBone);
	FVector Location = Pelvis + FVector(0.0f, 0.0f, HalfHeight);

	FHitResult Hit;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(IslanderRagdollRecover), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Pelvis + FVector(0.0f, 0.0f, HalfHeight), Pelvis - FVector(0.0f, 0.0f, 2.0f * HalfHeight), ECC_Visibility, Params))
	{
		Location = Hit.Location + FVector(0.0f, 0.0f, HalfHeight + 2.0f);
	}

	MeshComp->SetSimulatePhysics(false);
	MeshComp->SetAllBodiesSimulatePhysics(false);
	MeshComp->SetCollisionProfileName(MeshCollisionProfile);
	MeshComp->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	MeshComp->SetRelativeTransform(MeshRelativeTransform);

	SetActorLocationAndRotation(Location, FRotator(0.0f, GetActorRotation().Yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	GetCapsuleComponent()->SetCollisionEnabled(CapsuleCollision);
	CameraBoom->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	bIsRagdoll = false;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	ApplyViewMode(bFirstPerson, bFirstPerson);
}
