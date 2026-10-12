// monke_island

#include "Capture/CapturePrisoner.h"
#include "Capture/CaptiveComponent.h"
#include "Capture/CaptureCage.h"
#include "Capture/CaptureMath.h"
#include "Capture/CapturePrisonerAnimInstance.h"
#include "Capture/CaptureRules.h"
#include "Capture/CaptureSubsystem.h"
#include "Capture/CaptureTargets.h"
#include "Capture/HandcuffsItem.h"
#include "Player/IslanderHealthComponent.h"
#include "AIController.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** Below this speed the prisoner stands (cm/s) */
	constexpr float StandSpeed = 5.0f;
}

ACapturePrisoner::ACapturePrisoner()
{
	// its pose is built every frame (no AnimBP), its steps and the chain follow the jailer: one tick, as the mobs
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(2.0f);

	// a bean of 1.05 m until the look of the mob it replaces arrives
	GetCapsuleComponent()->InitCapsuleSize(30.0f, 55.0f);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	Move->MaxWalkSpeed = 150.0f;
	Move->bRunPhysicsWithNoController = true;

	// the bean library played without an AnimBP: this actor builds the frame (as the fort soldiers)
	USkeletalMeshComponent* Body = GetMesh();
	Body->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Body->SetAnimInstanceClass(UCapturePrisonerAnimInstance::StaticClass());
	Body->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	// the handcuffs ride on a hand bone and the chain starts there, seen even when the body is off screen
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Body->bComponentUseFixedSkelBounds = true;
	Body->SetBoundsScale(1.8f);

	Captive = CreateDefaultSubobject<UCaptiveComponent>(TEXT("Captive"));

	CuffsMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cuffs"));
	CuffsMesh->SetupAttachment(Body);
	CuffsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CuffsMesh->SetCanEverAffectNavigation(false);
	CuffsMesh->SetGenerateOverlapEvents(false);

	// the links are placed in the world each frame (identity component, absolute)
	ChainLinks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ChainLinks"));
	ChainLinks->SetupAttachment(GetCapsuleComponent());
	ChainLinks->SetUsingAbsoluteLocation(true);
	ChainLinks->SetUsingAbsoluteRotation(true);
	ChainLinks->SetUsingAbsoluteScale(true);
	ChainLinks->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChainLinks->SetCanEverAffectNavigation(false);
	ChainLinks->SetGenerateOverlapEvents(false);
	ChainLinks->SetCastShadow(false);
}

void ACapturePrisoner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACapturePrisoner, Rules, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACapturePrisoner, Look, COND_InitialOnly);
}

ACapturePrisoner* ACapturePrisoner::CaptureFrom(ACharacter* Original, APawn* Jailer, AHandcuffsItem* Handcuffs, UCaptureRules* InRules)
{
	UWorld* World = Original ? Original->GetWorld() : nullptr;
	if (!World || !InRules || !Original->HasAuthority() || !IsValid(Jailer))
	{
		return nullptr;
	}
	FCapturePrisonerLook InLook;
	CaptureTargets::CopyLook(Original, *InRules, InLook);
	if (!InLook.Mesh)
	{
		UE_LOG(LogMonkeCapture, Warning, TEXT("Capture: %s has no skeletal mesh to copy, it stays as it is"), *GetNameSafe(Original));
		return nullptr;
	}
	const FTransform Transform(FRotator(0.0f, Original->GetActorRotation().Yaw, 0.0f), Original->GetActorLocation(), InLook.ActorScale);
	UClass* Class = InRules->PrisonerClass ? InRules->PrisonerClass.Get() : ACapturePrisoner::StaticClass();
	ACapturePrisoner* Prisoner = World->SpawnActorDeferred<ACapturePrisoner>(Class, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Prisoner)
	{
		UE_LOG(LogMonkeCapture, Warning, TEXT("Capture: the prisoner of %s could not be spawned"), *GetNameSafe(Original));
		return nullptr;
	}
	Prisoner->Rules = InRules;
	Prisoner->Look = InLook;
	Prisoner->Captive->SetHandcuffs(Handcuffs);
	Prisoner->FinishSpawning(Transform);
	Prisoner->Captive->SetLed(Jailer);
	UE_LOG(LogMonkeCapture, Log, TEXT("Capture: %s cuffed by %s, now %s"), *GetNameSafe(Original), *GetNameSafe(Jailer), *GetNameSafe(Prisoner));
	// the soldier or the Veilleur leaves the fight for good: its fort, its arena and its AI let go of it
	Original->Destroy();
	return Prisoner;
}

void ACapturePrisoner::BeginPlay()
{
	Super::BeginPlay();
	if (UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this))
	{
		Capture->RegisterPrisoner(this);
	}
	Captive->OnStateChanged.AddUObject(this, &ACapturePrisoner::HandleStateChanged);
	ApplyLook();
	HandleStateChanged(Captive->GetState());
	if (!HasAuthority())
	{
		return;
	}
	if (!Rules)
	{
		UE_LOG(LogMonkeCapture, Error, TEXT("%s: no DA_CaptureRules, the prisoner stays still"), *GetName());
		return;
	}
	Captive->OnCageReached.AddUObject(this, &ACapturePrisoner::HandleCageReached);
	// the bounty is not lost to a stray ball (and the other sides never see it: Creatures/Hostility knows no prisoner)
	SetCanBeDamaged(!Rules->bPrisonerInvulnerable);
}

void ACapturePrisoner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	if (HasAuthority())
	{
		// gone for any reason: its handcuffs are free again
		FreeHandcuffs();
	}
	if (UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this))
	{
		Capture->UnregisterPrisoner(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ACapturePrisoner::OnRep_Look()
{
	ApplyLook();
}

void ACapturePrisoner::ApplyLook()
{
	if (!Look.Mesh || !HasActorBegunPlay())
	{
		return;
	}
	USkeletalMeshComponent* Body = GetMesh();
	if (Body->GetSkeletalMeshAsset() != Look.Mesh)
	{
		Body->SetSkeletalMeshAsset(Look.Mesh);
	}
	for (int32 Slot = 0; Slot < Look.Materials.Num(); ++Slot)
	{
		if (UMaterialInterface* Material = Look.Materials[Slot])
		{
			Body->SetMaterial(Slot, Material);
		}
	}
	Body->SetRelativeLocationAndRotation(Look.MeshLocation, Look.MeshRotation);
	Body->SetRelativeScale3D(Look.MeshScale);
	SetActorScale3D(Look.ActorScale);
	GetCapsuleComponent()->SetCapsuleSize(FMath::Max(Look.CapsuleRadius, 5.0f), FMath::Max(Look.CapsuleHalfHeight, Look.CapsuleRadius));
	// what it lost before the capture stays lost
	for (const FName& Bone : Look.HiddenBones)
	{
		if (Body->GetBoneIndex(Bone) != INDEX_NONE)
		{
			Body->HideBoneByName(Bone, PBO_None);
		}
	}
	AttachCuffs();
	if (Rules && Rules->ChainLinkMesh && ChainLinks->GetStaticMesh() != Rules->ChainLinkMesh)
	{
		ChainLinks->SetStaticMesh(Rules->ChainLinkMesh);
	}
	bLookApplied = true;
}

void ACapturePrisoner::AttachCuffs()
{
	if (!Rules || !Rules->HandcuffsMesh)
	{
		CuffsMesh->SetVisibility(false);
		return;
	}
	CuffsMesh->SetStaticMesh(Rules->HandcuffsMesh);
	USkeletalMeshComponent* Body = GetMesh();
	FName Bone = NAME_None;
	for (const FName& Candidate : Rules->CuffBones)
	{
		if (Body->GetBoneIndex(Candidate) != INDEX_NONE && !Body->IsBoneHiddenByName(Candidate))
		{
			Bone = Candidate;
			break;
		}
	}
	CuffsMesh->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Bone);
	// the bean bones carry the FBX scale of 100: the offset is in cm from an unscaled bone (as the gear of the soldiers)
	const FVector World = Body->GetSocketTransform(Bone, RTS_World).GetScale3D();
	const FVector Own = Body->GetComponentScale();
	FVector BoneScale(World.X / FMath::Max(FMath::Abs(Own.X), UE_KINDA_SMALL_NUMBER), World.Y / FMath::Max(FMath::Abs(Own.Y), UE_KINDA_SMALL_NUMBER),
		World.Z / FMath::Max(FMath::Abs(Own.Z), UE_KINDA_SMALL_NUMBER));
	if (BoneScale.GetAbsMin() < UE_KINDA_SMALL_NUMBER)
	{
		BoneScale = FVector::OneVector;
	}
	const FTransform& Offset = Rules->CuffsOnBone;
	CuffsMesh->SetRelativeTransform(FTransform(Offset.GetRotation(), Offset.GetLocation() / BoneScale, Offset.GetScale3D() / BoneScale));
	CuffsMesh->SetVisibility(true);
}

FVector ACapturePrisoner::GetCuffsAnchor() const
{
	const FName Socket = Rules ? Rules->CuffsChainSocket : NAME_None;
	if (CuffsMesh && CuffsMesh->GetStaticMesh())
	{
		return CuffsMesh->DoesSocketExist(Socket) ? CuffsMesh->GetSocketLocation(Socket) : CuffsMesh->GetComponentLocation();
	}
	return GetActorLocation();
}

void ACapturePrisoner::HandleStateChanged(ECaptiveState NewState)
{
	// let go: the crosshair finds it to take its chain back; led, the aim of its jailer passes it (to the cage behind)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, NewState == ECaptiveState::Abandoned ? ECR_Block : ECR_Ignore);
	}
	if (NewState != ECaptiveState::Led && ChainLinks && ChainLinks->GetInstanceCount() > 0)
	{
		ChainLinks->ClearInstances();
	}
}

void ACapturePrisoner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		Captive->ServerUpdate(DeltaSeconds);
	}
	UpdateAnimation(DeltaSeconds);
	if (GetNetMode() != NM_DedicatedServer)
	{
		Captive->UpdateChain(ChainLinks, GetCuffsAnchor());
	}
}

void ACapturePrisoner::UpdateAnimation(float DeltaSeconds)
{
	UCapturePrisonerAnimInstance* Anim = Cast<UCapturePrisonerAnimInstance>(GetMesh()->GetAnimInstance());
	if (!Anim || !Rules || !bLookApplied)
	{
		return;
	}
	const float Speed = static_cast<float>(GetVelocity().Size2D());
	FBeanAnimFrame Frame;
	if (Look.bCrawling && Look.Crawl)
	{
		// without a leg it drags itself, the crawl at the pace of its steps
		CrawlTime += DeltaSeconds * (Speed > StandSpeed ? CaptureMath::LocomotionRate(Speed, Look.CrawlNoSlideSpeed, Rules->MinPlayRate, Rules->MaxPlayRate) : 0.0f);
		Frame.Base.Add(FBeanAnimSample{ Look.Crawl.Get(), CrawlTime, 1.0f, true });
	}
	else
	{
		const float Target = Speed > StandSpeed ? FMath::Clamp(Speed / FMath::Max(Rules->WalkFullSpeed, 1.0f), 0.0f, 1.0f) : 0.0f;
		WalkAlpha = FMath::FInterpConstantTo(WalkAlpha, Target, DeltaSeconds, Rules->WalkBlendSpeed);
		IdleTime += DeltaSeconds;
		WalkTime += DeltaSeconds * CaptureMath::LocomotionRate(Speed, Look.WalkNoSlideSpeed, Rules->MinPlayRate, Rules->MaxPlayRate);
		if (Look.Idle && WalkAlpha < 1.0f)
		{
			Frame.Base.Add(FBeanAnimSample{ Look.Idle.Get(), IdleTime, 1.0f - WalkAlpha, true });
		}
		if (Look.Walk && WalkAlpha > 0.0f)
		{
			Frame.Base.Add(FBeanAnimSample{ Look.Walk.Get(), WalkTime, WalkAlpha, true });
		}
	}
	Anim->SetFrame(Frame);

	// its own clips swing the arms: both hands pulled together in front of it (the captive clips bind them already)
	FCaptureBoundHands Hands;
	Hands.Alpha = !Look.bCaptiveClips && !Look.bCrawling ? FMath::Clamp(Rules->BoundHandsAlpha, 0.0f, 1.0f) : 0.0f;
	if (Hands.Alpha > 0.0f)
	{
		const FTransform& ToWorld = GetMesh()->GetComponentTransform();
		const float Scale = FMath::Max(static_cast<float>(ToWorld.GetScale3D().GetAbsMax()), UE_KINDA_SMALL_NUMBER);
		Hands.Forward = ToWorld.InverseTransformVectorNoScale(GetActorForwardVector());
		Hands.Up = ToWorld.InverseTransformVectorNoScale(GetActorUpVector());
		Hands.Right = ToWorld.InverseTransformVectorNoScale(GetActorRightVector());
		Hands.ForwardCm = Rules->BoundHandsForwardCm / Scale;
		Hands.UpCm = Rules->BoundHandsUpCm / Scale;
		Hands.GapCm = Rules->BoundHandsGapCm / Scale;
		Hands.Pelvis = Rules->PelvisBone;
		Hands.UpperArmL = Rules->UpperArmLBone;
		Hands.LowerArmL = Rules->LowerArmLBone;
		Hands.HandL = Rules->HandLBone;
		Hands.UpperArmR = Rules->UpperArmRBone;
		Hands.LowerArmR = Rules->LowerArmRBone;
		Hands.HandR = Rules->HandRBone;
	}
	Anim->SetBoundHands(Hands);
}

// ---------------------------------------------------------------- cage, escape

void ACapturePrisoner::EnterCage(ACaptureCage* InCage)
{
	if (!HasAuthority() || !IsValid(InCage))
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(EscapeTimer);
	Cage = InCage;
	Captive->WalkIntoCage(InCage->GetEntryPoint(), InCage->GetPrisonerPoint(), InCage->GetPrisonerYaw());
}

void ACapturePrisoner::HandleCageReached()
{
	if (ACaptureCage* Into = Cage.Get())
	{
		Into->OnPrisonerInside(this);
	}
}

void ACapturePrisoner::SetCaged()
{
	if (!HasAuthority())
	{
		return;
	}
	Captive->SetCaged();
	FreeHandcuffs();
}

void ACapturePrisoner::FreeHandcuffs()
{
	if (AHandcuffsItem* Handcuffs = Captive ? Captive->GetHandcuffs() : nullptr)
	{
		Handcuffs->ReleasePrisoner(this);
	}
	if (Captive)
	{
		Captive->SetHandcuffs(nullptr);
	}
}

void ACapturePrisoner::OnAbandoned(APawn* FormerJailer, ECaptiveRelease Reason)
{
	if (!HasAuthority())
	{
		return;
	}
	MulticastReleased(FormerJailer, Reason);
	if (Rules && Rules->AbandonedEscapeSeconds > 0.0f)
	{
		GetWorldTimerManager().SetTimer(EscapeTimer, this, &ACapturePrisoner::Escape, Rules->AbandonedEscapeSeconds, false);
	}
}

void ACapturePrisoner::MulticastReleased_Implementation(APawn* FormerJailer, ECaptiveRelease Reason)
{
	// only the player who held the chain is told
	UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	if (Capture && Rules && FormerJailer && FormerJailer->IsLocallyControlled())
	{
		Capture->PostNotice(FText::Format(Rules->ReleasedText, Rules->GetKindName(Look.Kind)), UCaptureSubsystem::NoticeSeconds, true);
	}
}

void ACapturePrisoner::Escape()
{
	if (!HasAuthority() || Captive->GetState() != ECaptiveState::Abandoned)
	{
		return;
	}
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: let go too long, it flees"), *GetName());
	MulticastEscaped();
	FreeHandcuffs();
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	// a moment for the message to leave before the actor does
	SetLifeSpan(1.0f);
}

void ACapturePrisoner::MulticastEscaped_Implementation()
{
	if (UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this); Capture && Rules)
	{
		Capture->PostNotice(FText::Format(Rules->EscapedText, Rules->GetKindName(Look.Kind)), UCaptureSubsystem::NoticeSeconds, true);
	}
}

// ---------------------------------------------------------------- the chain of a prisoner let go

FText ACapturePrisoner::GetInteractionPrompt_Implementation(APawn* User) const
{
	return Rules ? Rules->TakeChainPromptText : FText::GetEmpty();
}

bool ACapturePrisoner::CanInteract_Implementation(APawn* User) const
{
	if (!Rules || !Captive || Captive->GetState() != ECaptiveState::Abandoned || !IsValid(User) || !User->IsPlayerControlled())
	{
		return false;
	}
	if (const UIslanderHealthComponent* Health = User->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
	{
		return false;
	}
	// one chain in hand per player
	const UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	return !(Capture && Capture->FindPrisonerLedBy(User));
}

void ACapturePrisoner::Interact_Implementation(APawn* User)
{
	// the grab component checked the reach and the sight; the state and the user again here (two players on one chain:
	// the first wins, §12 n°2)
	if (!HasAuthority() || !CanInteract_Implementation(User))
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(EscapeTimer);
	Captive->SetLed(User);
}
