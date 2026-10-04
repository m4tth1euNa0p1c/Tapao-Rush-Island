// monke_island

#include "Weapons/MusketWeapon.h"
#include "Weapons/MusketMath.h"
#include "monke_island.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "TimerManager.h"

namespace
{
	double ServerNow(const UWorld* World)
	{
		const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
		return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
	}

	USkeletalMeshComponent* MakeMusketMesh(AActor* Owner, FName Name)
	{
		USkeletalMeshComponent* Mesh = Owner->CreateDefaultSubobject<USkeletalMeshComponent>(Name);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		Mesh->SetHiddenInGame(true);
		return Mesh;
	}
}

AMusketWeapon::AMusketWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	// after the puppet copied the walk pose, before the first person arms read their override
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	FirstPersonMesh = MakeMusketMesh(this, TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(RootComponent);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetCastShadow(false);

	ThirdPersonMesh = MakeMusketMesh(this, TEXT("ThirdPersonMesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);
}

void AMusketWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMusketWeapon, bLoaded);
	DOREPLIFETIME_CONDITION(AMusketWeapon, PouchBalls, COND_OwnerOnly);
	DOREPLIFETIME(AMusketWeapon, bReloading);
	DOREPLIFETIME(AMusketWeapon, ReloadStartTime);
	DOREPLIFETIME(AMusketWeapon, FireCount);
	DOREPLIFETIME_CONDITION(AMusketWeapon, bAiming, COND_SkipOwner);
}

void AMusketWeapon::BeginPlay()
{
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Error, TEXT("Musket %s: no DA_MusketRules, the musket is disabled"), *GetName());
		Super::BeginPlay();
		SetActorTickEnabled(false);
		return;
	}

	if (HasAuthority())
	{
		const MusketMath::FAmmo Ammo = MusketMath::StartingAmmo(Rules->MaxBalls, Rules->bStartLoaded);
		bLoaded = Ammo.bLoaded;
		PouchBalls = Ammo.PouchBalls;
		SpreadStream.GenerateNewSeed();
	}
	FirstPersonMesh->SetSkeletalMeshAsset(Rules->MusketMesh);
	ThirdPersonMesh->SetSkeletalMeshAsset(Rules->MusketMesh);

	Super::BeginPlay();
}

void AMusketWeapon::OnAttachedToCarrier()
{
	AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !Rules)
	{
		return;
	}

	Arms = Carrier->FindComponentByClass<UBeanFirstPersonArmsComponent>();
	Puppet = Carrier->FindComponentByClass<UBeanPuppetComponent>();

	if (UCameraComponent* Camera = Carrier->GetFirstPersonCamera())
	{
		FirstPersonMesh->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	// the grip follows the bean's right hand (or the mannequin's when the bean puppet is missing)
	USceneComponent* Body = Puppet.IsValid() ? static_cast<USceneComponent*>(Puppet.Get()) : static_cast<USceneComponent*>(Carrier->GetMesh());
	ThirdPersonMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Rules->HandBone);
	SetThirdPersonGrip(Rules->HandOffset);

	if (Puppet.IsValid())
	{
		// copy the shot / reload over the pose the puppet just took from the walk animation
		AddTickPrerequisiteComponent(Puppet.Get());

		if (!BodyAnimator && (Rules->BodyHoldAnimation || Rules->BodyReloadAnimation || Rules->BodyFireAnimation))
		{
			BodyAnimator = NewObject<USkeletalMeshComponent>(this, TEXT("BodyAnimator"));
			BodyAnimator->SetSkeletalMeshAsset(Cast<USkeletalMesh>(Puppet->GetSkinnedAsset()));
			BodyAnimator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BodyAnimator->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			// never drawn, but its pose must keep updating
			BodyAnimator->SetVisibility(false);
			BodyAnimator->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			// not under the puppet: an attached component ticks after its parent, and the puppet must tick after
			// this one to copy its pose (cycle). Only its local bone pose is read, so where it hangs does not matter
			BodyAnimator->SetupAttachment(Puppet->GetAttachParent() ? Puppet->GetAttachParent() : Carrier->GetRootComponent());
			BodyAnimator->RegisterComponent();
			// evaluates only while it drives the puppet (see UpdateThirdPerson)
			BodyAnimator->SetComponentTickEnabled(false);
		}
	}
	if (Arms.IsValid())
	{
		Arms->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
	}
}

void AMusketWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (Arms.IsValid())
	{
		Arms->ClearPoseOverride();
	}
	if (Puppet.IsValid())
	{
		Puppet->SetBoneOverrideSource(nullptr, {}, 0.0f);
	}
	const AIslanderCharacter* Carrier = GetCarrier();
	ResetViewRoll();
	if (UCameraComponent* Camera = Carrier ? Carrier->GetFirstPersonCamera() : nullptr; Camera && DefaultFieldOfView > 0.0f)
	{
		Camera->SetFieldOfView(DefaultFieldOfView);
	}
	Super::EndPlay(EndPlayReason);
}

bool AMusketWeapon::IsUsable() const
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

float AMusketWeapon::GetReloadProgress() const
{
	if (!bReloading || !Rules)
	{
		return 0.0f;
	}
	return FMath::Clamp(static_cast<float>(ServerNow(GetWorld()) - ReloadStartTime) / Rules->ReloadSeconds, 0.0f, 1.0f);
}

// ---------------------------------------------------------------- input

void AMusketWeapon::BindInput()
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
		Input->BindAction(FireAction, ETriggerEvent::Started, this, &AMusketWeapon::HandleFire);
	}
	if (AimAction)
	{
		Input->BindAction(AimAction, ETriggerEvent::Started, this, &AMusketWeapon::HandleAimStarted);
		Input->BindAction(AimAction, ETriggerEvent::Completed, this, &AMusketWeapon::HandleAimCompleted);
	}
	if (ReloadAction)
	{
		Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AMusketWeapon::HandleReload);
	}
	bInputBound = true;
}

void AMusketWeapon::HandleFire()
{
	RequestFire();
}

void AMusketWeapon::HandleAimStarted()
{
	SetAiming(true);
}

void AMusketWeapon::HandleAimCompleted()
{
	SetAiming(false);
}

void AMusketWeapon::HandleReload()
{
	RequestReload();
}

// ---------------------------------------------------------------- actions

void AMusketWeapon::RequestFire()
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
	// the carrier sees its shot at once; the server decides the hit
	if (bLoaded && !bReloading)
	{
		PlayFireEffects();
	}
	ServerFire();
}

void AMusketWeapon::ServerFire_Implementation()
{
	MusketMath::FAmmo Ammo{ bLoaded, PouchBalls };
	if (!IsUsable())
	{
		return;
	}
	if (!MusketMath::CanFire(Ammo, bReloading, bRecovering))
	{
		if (!bLoaded && Rules->bAutoReloadOnEmptyFire)
		{
			ServerReload_Implementation();
		}
		return;
	}

	MusketMath::Fire(Ammo);
	bLoaded = Ammo.bLoaded;
	bRecovering = true;
	++FireCount;
	// the server is the carrier (host) or watches another player: OnRep does not run here
	PlayFireEffects();

	GetWorldTimerManager().SetTimer(ShotTimer, this, &AMusketWeapon::TraceShot, FMath::Max(Rules->FireDelaySeconds, 0.001f), false);
	GetWorldTimerManager().SetTimer(RecoverTimer, this, &AMusketWeapon::EndRecover, FMath::Max(Rules->FireDelaySeconds + Rules->RecoverSeconds, 0.01f), false);
}

void AMusketWeapon::EndRecover()
{
	bRecovering = false;
}

void AMusketWeapon::TraceShot()
{
	AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !Rules)
	{
		return;
	}

	// from the carrier's eyes along the replicated view: the client only says "fire"
	const UCameraComponent* Camera = Carrier->GetFirstPersonCamera();
	const FVector Origin = Camera ? Camera->GetComponentLocation() : Carrier->GetPawnViewLocation();
	const FVector Aim = Carrier->GetControlRotation().Vector();
	const FVector Direction = MusketMath::SpreadDirection(Aim, bAiming ? Rules->AimSpreadDegrees : Rules->HipSpreadDegrees, SpreadStream);
	const FVector End = Origin + Direction * Rules->RangeCm;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MusketShot), true, Carrier);
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, Rules->TraceChannel, Params))
	{
		return;
	}

	AActor* Target = Hit.GetActor();
	if (!Target)
	{
		return;
	}
	const APawn* TargetPawn = Cast<APawn>(Target);
	const bool bFriendly = TargetPawn && TargetPawn->IsPlayerControlled();
	if (!bFriendly || Rules->bFriendlyFire)
	{
		UGameplayStatics::ApplyPointDamage(Target, Rules->Damage, Direction, Hit, Carrier->GetController(), this, UDamageType::StaticClass());
	}

	if (ACharacter* TargetCharacter = Cast<ACharacter>(Target))
	{
		if (!bFriendly || Rules->bFriendlyFire)
		{
			TargetCharacter->LaunchCharacter(FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal() * Rules->KnockbackSpeed + FVector(0.0f, 0.0f, 80.0f), true, false);
		}
	}
	else if (UPrimitiveComponent* Body = Hit.GetComponent(); Body && Body->IsSimulatingPhysics())
	{
		Body->AddImpulseAtLocation(Direction * Rules->PhysicsImpulse, Hit.ImpactPoint);
	}
	UE_LOG(Logmonke_island, Verbose, TEXT("Musket: %s hit %s at %.0f cm"), *Carrier->GetName(), *Target->GetName(), Hit.Distance);
}

void AMusketWeapon::RequestReload()
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

void AMusketWeapon::ServerReload_Implementation()
{
	const MusketMath::FAmmo Ammo{ bLoaded, PouchBalls };
	if (!IsUsable() || !MusketMath::CanReload(Ammo, bReloading, bRecovering))
	{
		return;
	}
	bReloading = true;
	bAiming = false;
	ReloadStartTime = ServerNow(GetWorld());
	PlayReloadEffects(true);
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &AMusketWeapon::CompleteReload, Rules->ReloadSeconds, false);
}

void AMusketWeapon::CompleteReload()
{
	MusketMath::FAmmo Ammo{ bLoaded, PouchBalls };
	MusketMath::CompleteReload(Ammo);
	bLoaded = Ammo.bLoaded;
	PouchBalls = Ammo.PouchBalls;
	bReloading = false;
	PlayReloadEffects(false);
}

void AMusketWeapon::CancelReload()
{
	if (!bReloading)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(ReloadTimer);
	bReloading = false;
	PlayReloadEffects(false);
}

void AMusketWeapon::SetAiming(bool bInAiming)
{
	// the carrier shoulders at once; the server keeps it for the spread and the other players
	bAiming = bInAiming && IsUsable() && !bReloading;
	if (!HasAuthority())
	{
		ServerSetAiming(bAiming);
	}
}

void AMusketWeapon::ServerSetAiming_Implementation(bool bInAiming)
{
	bAiming = bInAiming && IsUsable() && !bReloading;
}

void AMusketWeapon::OnEquippedChanged()
{
	if (HasAuthority() && !IsEquipped())
	{
		CancelReload();
		bAiming = false;
	}
	// put away: one more tick hides the meshes, gives the arms and the body back and removes the keys
	SetActorTickEnabled(Rules != nullptr);
}

// ---------------------------------------------------------------- effects

void AMusketWeapon::OnRep_FireCount()
{
	// the carrier already played its own shot
	if (!IsCarrierLocallyControlled())
	{
		PlayFireEffects();
	}
}

void AMusketWeapon::OnRep_Reloading()
{
	PlayReloadEffects(bReloading);
}

void AMusketWeapon::PlayAnimations(UAnimSequence* BodyAnimation, UAnimSequence* MusketAnimation, float Start, float Duration)
{
	const float SafeDuration = FMath::Max(Duration, 0.05f);
	if (MusketAnimation)
	{
		const float Rate = FMath::Max(MusketAnimation->GetPlayLength() - Start, 0.05f) / SafeDuration;
		for (USkeletalMeshComponent* Mesh : { FirstPersonMesh.Get(), ThirdPersonMesh.Get() })
		{
			Mesh->PlayAnimation(MusketAnimation, false);
			Mesh->SetPlayRate(Rate);
			Mesh->SetPosition(Start, false);
		}
	}
	if (BodyAnimation && BodyAnimator)
	{
		const float Rate = FMath::Max(BodyAnimation->GetPlayLength() - Start, 0.05f) / SafeDuration;
		BodyAnimator->PlayAnimation(BodyAnimation, false);
		BodyAnimator->SetPlayRate(Rate);
		BodyAnimator->SetPosition(Start, false);
		BodyAnimationEndTime = GetWorld()->GetTimeSeconds() + SafeDuration;
		bBodyHolding = false;
	}
}

void AMusketWeapon::PlayFireEffects()
{
	if (!Rules)
	{
		return;
	}
	const float Start = Rules->FireAnimationTriggerTime;
	const float Length = Rules->BodyFireAnimation ? Rules->BodyFireAnimation->GetPlayLength() - Start : Rules->RecoverSeconds;
	PlayAnimations(Rules->BodyFireAnimation, Rules->MusketFireAnimation, Start, Length);

	// flintlock: the flash in the pan, then the charge goes off after the lock time
	if (Rules->LockSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Rules->LockSound, GetActorLocation());
	}
	if (Rules->FireDelaySeconds > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(ShotEffectsTimer, this, &AMusketWeapon::SpawnShotEffects, Rules->FireDelaySeconds, false);
	}
	else
	{
		SpawnShotEffects();
	}
}

void AMusketWeapon::SpawnShotEffects()
{
	if (!Rules)
	{
		return;
	}
	// the musket seen on this machine: first person for its carrier, in the bean's hand for everyone else
	USkeletalMeshComponent* Mesh = FirstPersonMesh->bHiddenInGame ? ThirdPersonMesh.Get() : FirstPersonMesh.Get();
	static const FName MuzzleSocket(TEXT("muzzle"));
	const bool bHasMuzzle = Mesh->DoesSocketExist(MuzzleSocket);
	const FVector MuzzleLocation = bHasMuzzle ? Mesh->GetSocketLocation(MuzzleSocket) : Mesh->GetComponentLocation();
	const FVector Forward = Mesh->GetComponentQuat().GetForwardVector();
	Recoil = 1.0f;

	if (Rules->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Rules->FireSound, MuzzleLocation);
	}

	// flame and light, attached to the muzzle so they follow the recoil
	if (Rules->FlashMesh)
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
		FlashComponent->SetWorldRotation(FRotator(Forward.Rotation().Pitch, Forward.Rotation().Yaw, FMath::FRandRange(0.0f, 360.0f)));
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
		FlashLight->SetLightColor(Rules->FlashLightColor);
		FlashLight->SetIntensity(Rules->FlashLightIntensity);
		FlashLight->SetAttenuationRadius(Rules->FlashLightRadius);
		FlashLight->SetVisibility(true);
	}
	FlashEndTime = GetWorld()->GetTimeSeconds() + Rules->FlashSeconds;

	// black powder smoke: a jet from the muzzle, a puff from the pan
	if (Rules->SmokeMesh && Rules->SmokePuffs > 0)
	{
		while (SmokeComponents.Num() < Rules->SmokePuffs)
		{
			UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(this);
			Puff->SetStaticMesh(Rules->SmokeMesh);
			Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Puff->SetCastShadow(false);
			Puff->SetUsingAbsoluteLocation(true);
			Puff->SetUsingAbsoluteRotation(true);
			Puff->SetUsingAbsoluteScale(true);
			Puff->SetVisibility(false);
			Puff->RegisterComponent();
			UMaterialInstanceDynamic* Material = Rules->SmokeMaterial ? Puff->CreateDynamicMaterialInstance(0, Rules->SmokeMaterial) : nullptr;
			SmokeComponents.Add(Puff);
			SmokeMaterials.Add(Material);
			SmokePuffs.AddDefaulted();
		}
		const float MeshDiameter = FMath::Max(Rules->SmokeMesh->GetBounds().BoxExtent.GetMax() * 2.0f, 1.0f);
		static const FName PanSocket(TEXT("pan"));
		const FVector PanLocation = Mesh->DoesSocketExist(PanSocket) ? Mesh->GetSocketLocation(PanSocket) : MuzzleLocation;
		const int32 Count = FMath::Min(Rules->SmokePuffs, SmokeComponents.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const bool bPanPuff = Index == Count - 1 && Count > 1;
			// the jet: fast puffs first, slower ones lagging behind, in a narrow cone
			const float Share = static_cast<float>(Index) / FMath::Max(Count - 1, 1);
			const FVector Direction = FMath::VRandCone(Forward, FMath::DegreesToRadians(bPanPuff ? 60.0f : 12.0f));
			FSmokePuff& Puff = SmokePuffs[Index];
			Puff.Velocity = Direction * Rules->SmokeSpeed * (bPanPuff ? 0.1f : FMath::Lerp(1.0f, 0.25f, Share));
			Puff.Age = 0.0f;
			Puff.Life = Rules->SmokeSeconds * FMath::FRandRange(0.75f, 1.15f);
			Puff.StartSize = Rules->SmokeSize.X * (bPanPuff ? 0.6f : 1.0f) / MeshDiameter;
			Puff.EndSize = Rules->SmokeSize.Y * FMath::FRandRange(0.7f, 1.2f) * (bPanPuff ? 0.5f : 1.0f) / MeshDiameter;
			Puff.bActive = true;
			UStaticMeshComponent* Component = SmokeComponents[Index];
			Component->SetWorldLocation(bPanPuff ? PanLocation : MuzzleLocation + Forward * 3.0f);
			Component->SetWorldRotation(FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), 0.0f));
			Component->SetWorldScale3D(FVector(Puff.StartSize));
			Component->SetVisibility(true);
		}
	}

	// the carrier feels it: the view jumps up and a little sideways
	if (IsCarrierLocallyControlled())
	{
		PendingViewKick += FVector2D(Rules->ViewKickPitch, FMath::FRandRange(-Rules->ViewKickYaw, Rules->ViewKickYaw));
	}
}

bool AMusketWeapon::HasShotEffectsRunning() const
{
	if (FlashEndTime > GetWorld()->GetTimeSeconds() || !PendingViewKick.IsNearlyZero(0.01f) || AppliedViewRoll != 0.0f)
	{
		return true;
	}
	for (const FSmokePuff& Puff : SmokePuffs)
	{
		if (Puff.bActive)
		{
			return true;
		}
	}
	return false;
}

void AMusketWeapon::UpdateShotEffects(float DeltaSeconds)
{
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
	else if (FlashLight && Rules)
	{
		// the flash dies out over its short life
		const float Left = static_cast<float>(FlashEndTime - Now) / FMath::Max(Rules->FlashSeconds, 0.01f);
		FlashLight->SetIntensity(Rules->FlashLightIntensity * FMath::Clamp(Left, 0.0f, 1.0f));
	}

	if (!Rules)
	{
		return;
	}
	const float Drag = FMath::Exp(-4.0f * DeltaSeconds);
	for (int32 Index = 0; Index < SmokePuffs.Num(); ++Index)
	{
		FSmokePuff& Puff = SmokePuffs[Index];
		if (!Puff.bActive)
		{
			continue;
		}
		UStaticMeshComponent* Component = SmokeComponents.IsValidIndex(Index) ? SmokeComponents[Index].Get() : nullptr;
		Puff.Age += DeltaSeconds;
		const float T = Puff.Age / FMath::Max(Puff.Life, 0.01f);
		if (T >= 1.0f || !Component)
		{
			Puff.bActive = false;
			if (Component)
			{
				Component->SetVisibility(false);
			}
			continue;
		}
		// slowed by the air, rising, growing fast then slowly, fading out
		Puff.Velocity = Puff.Velocity * Drag + FVector(0.0f, 0.0f, Rules->SmokeRise * (1.0f - Drag));
		Component->SetWorldLocation(Component->GetComponentLocation() + Puff.Velocity * DeltaSeconds);
		const float Growth = 1.0f - FMath::Square(1.0f - T);
		Component->SetWorldScale3D(FVector(FMath::Lerp(Puff.StartSize, Puff.EndSize, Growth)));
		if (UMaterialInstanceDynamic* Material = SmokeMaterials.IsValidIndex(Index) ? SmokeMaterials[Index].Get() : nullptr)
		{
			Material->SetScalarParameterValue(Rules->SmokeOpacityParameter, Rules->SmokeOpacity * FMath::Square(1.0f - T));
		}
	}
}

void AMusketWeapon::UpdateViewKick(float DeltaSeconds)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	AController* Controller = Carrier ? Carrier->GetController() : nullptr;
	if (!Rules || !Controller || !IsCarrierLocallyControlled())
	{
		return;
	}
	FRotator View = Controller->GetControlRotation();
	const FRotator Before = View;

	// the kick lands within a few frames, then the player brings the aim back down
	const FVector2D Applied = PendingViewKick * FMath::Min(1.0f, DeltaSeconds * 25.0f);
	PendingViewKick -= Applied;
	View.Pitch += Applied.X;
	View.Yaw += Applied.Y;

	// a short roll shake that fades with the recoil, removed once it is over
	const float Roll = Recoil > 0.02f ? Rules->ViewShakeRoll * Recoil * FMath::Sin(static_cast<float>(GetWorld()->GetTimeSeconds()) * 55.0f) : 0.0f;
	View.Roll += Roll - AppliedViewRoll;
	AppliedViewRoll = Roll;
	if (!View.Equals(Before))
	{
		Controller->SetControlRotation(View);
	}
}

void AMusketWeapon::ResetViewRoll()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	AController* Controller = Carrier ? Carrier->GetController() : nullptr;
	if (Controller && Carrier->IsLocallyControlled() && AppliedViewRoll != 0.0f)
	{
		FRotator View = Controller->GetControlRotation();
		View.Roll -= AppliedViewRoll;
		Controller->SetControlRotation(View);
	}
	AppliedViewRoll = 0.0f;
	PendingViewKick = FVector2D::ZeroVector;
}

void AMusketWeapon::PlayReloadEffects(bool bStart)
{
	if (!Rules)
	{
		return;
	}
	if (!bStart)
	{
		// back to the carry pose
		BodyAnimationEndTime = 0.0;
		for (USkeletalMeshComponent* Mesh : { FirstPersonMesh.Get(), ThirdPersonMesh.Get() })
		{
			// a cancelled reload puts the ramrod back (the finished one ends stowed and cocked)
			if (Rules->MusketReloadAnimation && Mesh->GetPosition() < Rules->MusketReloadAnimation->GetPlayLength() * 0.95f)
			{
				Mesh->SetPosition(0.0f, false);
			}
			Mesh->Stop();
		}
		return;
	}
	// sounds from where the reload is (a late joiner does not hear the past ones)
	NextReloadCue = 0;
	UpdateReloadSounds();

	// a late joiner starts the animation where the reload is
	const float Elapsed = FMath::Clamp(static_cast<float>(ServerNow(GetWorld()) - ReloadStartTime), 0.0f, Rules->ReloadSeconds);
	const float Remaining = Rules->ReloadSeconds - Elapsed;
	const UAnimSequence* Reference = Rules->BodyReloadAnimation ? Rules->BodyReloadAnimation.Get() : Rules->MusketReloadAnimation.Get();
	const float Start = Reference ? Reference->GetPlayLength() * Elapsed / Rules->ReloadSeconds : 0.0f;
	PlayAnimations(Rules->BodyReloadAnimation, Rules->MusketReloadAnimation, Start, Remaining);
}

// ---------------------------------------------------------------- per frame

void AMusketWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Carrier || !Rules)
	{
		return;
	}
	const bool bUsable = IsUsable();

	if (HasAuthority() && !bUsable)
	{
		CancelReload();
		bAiming = false;
	}

	// the carrier's keys: only while the musket can be used (otherwise the clicks punch, throw, guard)
	if (IsCarrierLocallyControlled())
	{
		BindInput();
		SetInputContextActive(MusketMappingContext, MusketMappingPriority, bUsable);
		if (!bUsable)
		{
			bAiming = false;
		}
	}

	const bool bFirstPersonView = IsCarrierLocallyControlled() && Carrier->IsFirstPerson() && !Carrier->IsRagdoll();
	UpdateFirstPerson(DeltaSeconds, bUsable && bFirstPersonView);
	UpdateThirdPerson(DeltaSeconds, bUsable && !bFirstPersonView);
	UpdateShotEffects(DeltaSeconds);
	UpdateViewKick(DeltaSeconds);
	UpdateReloadSounds();

	Recoil = FMath::FInterpTo(Recoil, 0.0f, DeltaSeconds, 8.0f);

	// put away: everything is hidden and given back, stop ticking until equipped again
	if (!IsEquipped() && UpperBodyAlpha <= 0.0f && !HasShotEffectsRunning())
	{
		ResetViewRoll();
		SetActorTickEnabled(false);
	}
}

void AMusketWeapon::UpdateFirstPerson(float DeltaSeconds, bool bShow)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UCameraComponent* Camera = Carrier ? Carrier->GetFirstPersonCamera() : nullptr;
	const bool bWasShown = !FirstPersonMesh->bHiddenInGame;

	if (!bShow)
	{
		if (bWasShown)
		{
			FirstPersonMesh->SetHiddenInGame(true);
			if (Arms.IsValid())
			{
				Arms->ClearPoseOverride();
			}
		}
		if (Camera && DefaultFieldOfView > 0.0f)
		{
			Camera->SetFieldOfView(DefaultFieldOfView);
			DefaultFieldOfView = 0.0f;
		}
		return;
	}

	// target: shouldered or held, else the reload track stretched over the reload time
	FMusketFirstPersonKey From = bAiming ? Rules->AimPose : Rules->HoldPose;
	FMusketFirstPersonKey To = From;
	float KeyAlpha = 0.0f;
	if (bReloading && Rules->ReloadKeys.Num() > 0)
	{
		const float TrackLength = FMath::Max(Rules->ReloadKeys.Last().Time, 0.01f);
		const MusketMath::FKeySpan Span = MusketMath::FindKeys(Rules->ReloadKeys, GetReloadProgress() * TrackLength);
		From = Rules->ReloadKeys[Span.A];
		To = Rules->ReloadKeys[Span.B];
		KeyAlpha = Span.Alpha;
	}
	const FMusketFirstPersonKey Target = MusketMath::BlendKeys(From, To, KeyAlpha);
	if (!bWasShown)
	{
		FirstPersonMesh->SetHiddenInGame(false);
		CurrentPose = Target;
		// come up from below the view
		CurrentPose.MusketLocation.Z -= 30.0f;
	}
	CurrentPose = MusketMath::BlendKeys(CurrentPose, Target, 1.0f - FMath::Exp(-Rules->PoseBlendSpeed * DeltaSeconds));

	// recoil: back and muzzle up (the mittens hold the musket, they follow)
	const FVector MusketLocation = CurrentPose.MusketLocation - FVector(Rules->RecoilKick.X * Recoil, 0.0f, 0.0f);
	const FRotator MusketRotation = CurrentPose.MusketRotation + FRotator(Rules->RecoilKick.Y * Recoil, 0.0f, 0.0f);
	FirstPersonMesh->SetRelativeLocationAndRotation(MusketLocation, MusketRotation);
	const FTransform MusketToCamera(MusketRotation, MusketLocation);

	// mittens: the holds of the two keys around now, eased in the frame of the musket so they ride with it
	const FVector RightHold = FMath::Lerp(HandHoldInMusket(From.RightHandSocket, From.RightHandOffset, MusketToCamera),
		HandHoldInMusket(To.RightHandSocket, To.RightHandOffset, MusketToCamera), KeyAlpha);
	const FVector LeftHold = FMath::Lerp(HandHoldInMusket(From.LeftHandSocket, From.LeftHandOffset, MusketToCamera),
		HandHoldInMusket(To.LeftHandSocket, To.LeftHandOffset, MusketToCamera), KeyAlpha);
	const float HandAlpha = bWasShown ? 1.0f - FMath::Exp(-Rules->HandBlendSpeed * DeltaSeconds) : 1.0f;
	RightHandInMusket = FMath::Lerp(RightHandInMusket, RightHold, HandAlpha);
	LeftHandInMusket = FMath::Lerp(LeftHandInMusket, LeftHold, HandAlpha);

	if (Arms.IsValid())
	{
		const FBeanArmPose Right = MusketMath::SolveArm(Rules->RightShoulder, MusketToCamera.TransformPosition(RightHandInMusket),
			Rules->ArmHandPoint, Rules->MaxArmStretch);
		const FBeanArmPose Left = MusketMath::SolveArm(Rules->LeftShoulder, MusketToCamera.TransformPosition(LeftHandInMusket),
			Rules->ArmHandPoint, Rules->MaxArmStretch);
		Arms->SetPoseOverride(Right, MusketMath::MirrorArm(Left));
	}

	if (Camera)
	{
		if (DefaultFieldOfView <= 0.0f)
		{
			DefaultFieldOfView = Camera->FieldOfView;
		}
		const float TargetFieldOfView = (bAiming && !bReloading) ? Rules->AimFieldOfView : DefaultFieldOfView;
		Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFieldOfView, DeltaSeconds, 10.0f));
	}
}

void AMusketWeapon::UpdateReloadSounds()
{
	if (!bReloading || !Rules || Rules->ReloadSoundCues.Num() == 0)
	{
		return;
	}
	// same timeline as the first person keys: A_Musket_Reload time stretched over ReloadSeconds
	const float TrackLength = Rules->ReloadKeys.Num() > 0 ? Rules->ReloadKeys.Last().Time
		: (Rules->MusketReloadAnimation ? Rules->MusketReloadAnimation->GetPlayLength() : Rules->ReloadSeconds);
	const float Time = GetReloadProgress() * TrackLength;
	const bool bCatchUp = NextReloadCue == 0;
	while (NextReloadCue < Rules->ReloadSoundCues.Num() && Rules->ReloadSoundCues[NextReloadCue].Time <= Time)
	{
		const FMusketSoundCue& Cue = Rules->ReloadSoundCues[NextReloadCue++];
		// at the start of a reload seen late, skip what already happened (more than a frame or two ago)
		if (Cue.Sound && !(bCatchUp && Time - Cue.Time > 0.1f))
		{
			UGameplayStatics::SpawnSoundAttached(Cue.Sound, RootComponent, NAME_None, FVector::ZeroVector,
				EAttachLocation::KeepRelativeOffset, true, Cue.Volume, 1.0f, 0.0f, Rules->ReloadSoundAttenuation);
		}
	}
}

FVector AMusketWeapon::HandHoldInMusket(FName Socket, const FVector& Offset, const FTransform& MusketToCamera) const
{
	if (Socket.IsNone())
	{
		return MusketToCamera.InverseTransformPosition(Offset);
	}
	// the sockets are bones of SK_Bean_Musket: the ramrod tip moves with A_Musket_Reload
	return FirstPersonMesh->GetSocketTransform(Socket, RTS_Component).GetLocation() + Offset;
}

void AMusketWeapon::PlayBodyHold()
{
	if (!BodyAnimator || !Rules || !Rules->BodyHoldAnimation)
	{
		return;
	}
	BodyAnimator->PlayAnimation(Rules->BodyHoldAnimation, true);
	BodyAnimator->SetPlayRate(1.0f);
	bBodyHolding = true;
}

void AMusketWeapon::UpdateThirdPerson(float DeltaSeconds, bool bShow)
{
	if (ThirdPersonMesh->bHiddenInGame == bShow)
	{
		ThirdPersonMesh->SetHiddenInGame(!bShow);
	}
	if (!Puppet.IsValid() || !BodyAnimator || !Rules)
	{
		return;
	}

	// the shot and the reload play once, then the bean carries the musket again
	const bool bAction = GetWorld()->GetTimeSeconds() < BodyAnimationEndTime;
	if (!bAction && !bBodyHolding)
	{
		PlayBodyHold();
	}

	// upper body only, so the legs keep walking; eased in and out (equip, swim, carry an object, first person)
	const bool bDrive = bShow && (bAction || Rules->BodyHoldAnimation);
	UpperBodyAlpha = FMath::FInterpTo(UpperBodyAlpha, bDrive ? 1.0f : 0.0f, DeltaSeconds, 10.0f);
	if (!bDrive && UpperBodyAlpha < 0.01f)
	{
		UpperBodyAlpha = 0.0f;
	}
	const bool bEvaluate = UpperBodyAlpha > 0.0f;
	if (BodyAnimator->IsComponentTickEnabled() != bEvaluate)
	{
		// the hidden body costs a full bean evaluation: only while it drives the puppet
		BodyAnimator->SetComponentTickEnabled(bEvaluate);
	}
	Puppet->SetBoneOverrideSource(bEvaluate ? BodyAnimator.Get() : nullptr, Rules->UpperBodyBones, UpperBodyAlpha);
	if (bShow)
	{
		UpdateThirdPersonGrip();
	}
}

void AMusketWeapon::UpdateThirdPersonGrip()
{
	// the animations move the musket from hand to hand (the left one holds the barrel while the right one loads):
	// follow the musket of the clip relative to hand_r, blended with the plain grip while the body eases in or out
	const TArray<FTransform>* Track = nullptr;
	const UAnimSingleNodeInstance* Instance = BodyAnimator && UpperBodyAlpha > 0.0f ? BodyAnimator->GetSingleNodeInstance() : nullptr;
	const UAnimationAsset* Playing = Instance ? Instance->GetAnimationAsset() : nullptr;
	if (Playing && Playing == Rules->BodyReloadAnimation)
	{
		Track = &Rules->ReloadMusketTrack;
	}
	else if (Playing && Playing == Rules->BodyFireAnimation)
	{
		Track = &Rules->FireMusketTrack;
	}
	else if (Playing && Playing == Rules->BodyHoldAnimation)
	{
		Track = &Rules->HoldMusketTrack;
	}
	if (!Track)
	{
		SetThirdPersonGrip(Rules->HandOffset);
		return;
	}
	const FTransform Clip = MusketMath::SampleTrack(*Track, Instance->GetCurrentTime(), Rules->MusketTrackFrameRate, Rules->HandOffset);
	FTransform Grip;
	Grip.Blend(Rules->HandOffset, Clip, UpperBodyAlpha);
	SetThirdPersonGrip(Grip);
}

void AMusketWeapon::SetThirdPersonGrip(const FTransform& Grip)
{
	// the bean bones carry a scale of 100 (Blender FBX export, root bone at 100): the grip is in cm from an unscaled
	// hand, so take the bone scale out, otherwise the musket is 100 times too big and 100 times too far
	FVector BoneScale = FVector::OneVector;
	if (const USceneComponent* Parent = ThirdPersonMesh->GetAttachParent())
	{
		const FVector World = Parent->GetSocketTransform(ThirdPersonMesh->GetAttachSocketName(), RTS_World).GetScale3D();
		const FVector Body = Parent->GetComponentScale();
		BoneScale = FVector(World.X / FMath::Max(FMath::Abs(Body.X), UE_KINDA_SMALL_NUMBER), World.Y / FMath::Max(FMath::Abs(Body.Y), UE_KINDA_SMALL_NUMBER),
			World.Z / FMath::Max(FMath::Abs(Body.Z), UE_KINDA_SMALL_NUMBER));
		if (BoneScale.GetAbsMin() < UE_KINDA_SMALL_NUMBER)
		{
			// folded away (first person arms): keep the last grip
			return;
		}
	}
	ThirdPersonMesh->SetRelativeTransform(FTransform(Grip.GetRotation(), Grip.GetLocation() / BoneScale, FVector::OneVector / BoneScale));
}
