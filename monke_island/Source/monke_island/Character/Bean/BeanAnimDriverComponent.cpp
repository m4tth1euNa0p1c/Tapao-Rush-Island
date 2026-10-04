// monke_island

#include "Character/Bean/BeanAnimDriverComponent.h"
#include "Character/Bean/BeanAnimLibrary.h"
#include "Character/Bean/BeanFirstPersonArmsComponent.h"
#include "Character/Bean/BeanLibraryAnimInstance.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatRiderComponent.h"
#include "Emotes/TrumpetEmoteComponent.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Weapons/CutlassMath.h"
#include "Weapons/CutlassWeapon.h"
#include "Weapons/TorchItem.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr int32 MaxInitAttempts = 120;

	float ClipLength(const UAnimSequence* Clip)
	{
		return Clip ? FMath::Max(Clip->GetPlayLength(), 0.01f) : 1.0f;
	}

	void AddSample(TArray<FBeanAnimSample>& Out, const UAnimSequence* Clip, float Time, float Weight, bool bLoop)
	{
		if (Clip && Weight > KINDA_SMALL_NUMBER)
		{
			FBeanAnimSample& Sample = Out.AddDefaulted_GetRef();
			Sample.Sequence = Clip;
			Sample.Time = Time;
			Sample.Weight = Weight;
			Sample.bLoop = bLoop;
		}
	}
}

// ------------------------------------------------------------------ hands on the oars

UBeanHandIKComponent::UBeanHandIKComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// after the puppet built its pose this frame
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UBeanHandIKComponent::SetPuppet(UBeanPuppetComponent* InPuppet)
{
	Puppet = InPuppet;
	if (InPuppet)
	{
		AddTickPrerequisiteComponent(InPuppet);
	}
}

void UBeanHandIKComponent::SetTargets(const FVector& Right, const FVector& Left, float InAlpha, float InGripReachCm)
{
	RightTarget = Right;
	LeftTarget = Left;
	Alpha = FMath::Clamp(InAlpha, 0.0f, 1.0f);
	GripReachCm = InGripReachCm;
}

void UBeanHandIKComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UBeanPuppetComponent* Body = Puppet.Get();
	if (!Body || Alpha <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FTransform& ToWorld = Body->GetComponentTransform();
	// mesh space: -X is the bean's right, the elbows bend out and down
	SolveArm(TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"), ToWorld.InverseTransformPosition(RightTarget), FVector(-1.0, 0.0, -1.0));
	SolveArm(TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"), ToWorld.InverseTransformPosition(LeftTarget), FVector(1.0, 0.0, -1.0));
}

void UBeanHandIKComponent::SolveArm(FName UpperBone, FName LowerBone, FName HandBone, const FVector& TargetCS, const FVector& Pole)
{
	UBeanPuppetComponent* Body = Puppet.Get();
	if (Body->GetBoneIndex(UpperBone) == INDEX_NONE || Body->GetBoneIndex(LowerBone) == INDEX_NONE || Body->GetBoneIndex(HandBone) == INDEX_NONE)
	{
		return;
	}
	// component space bones carry the FBX root scale (x100): only their locations and rotations are used
	const FTransform Upper = Body->GetBoneTransformByName(UpperBone, EBoneSpaces::ComponentSpace);
	const FTransform Lower = Body->GetBoneTransformByName(LowerBone, EBoneSpaces::ComponentSpace);
	const FVector Shoulder = Upper.GetLocation();
	const FVector Elbow = Lower.GetLocation();
	const FVector Hand = Body->GetBoneTransformByName(HandBone, EBoneSpaces::ComponentSpace).GetLocation();
	const float UpperLength = FVector::Dist(Shoulder, Elbow);
	const float LowerLength = FVector::Dist(Elbow, Hand) + GripReachCm;
	if (UpperLength < 0.1f || LowerLength < 0.1f)
	{
		return;
	}
	const FVector Grip = Hand + (Hand - Elbow).GetSafeNormal() * GripReachCm;
	const FVector Target = FMath::Lerp(Grip, TargetCS, Alpha);
	const FVector NewElbow = CutlassMath::SolveElbow(Shoulder, Target, UpperLength, LowerLength, Pole);
	const FQuat UpperDelta = FQuat::FindBetweenNormals((Elbow - Shoulder).GetSafeNormal(), (NewElbow - Shoulder).GetSafeNormal());
	Body->SetBoneRotationByName(UpperBone, (UpperDelta * Upper.GetRotation()).Rotator(), EBoneSpaces::ComponentSpace);
	const FVector LowerNow = UpperDelta.RotateVector(Hand - Elbow).GetSafeNormal();
	const FQuat LowerDelta = FQuat::FindBetweenNormals(LowerNow, (Target - NewElbow).GetSafeNormal());
	Body->SetBoneRotationByName(LowerBone, (LowerDelta * UpperDelta * Lower.GetRotation()).Rotator(), EBoneSpaces::ComponentSpace);
}

// ------------------------------------------------------------------ driver

UBeanAnimDriverComponent::UBeanAnimDriverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// decided before the hidden mesh animates, which the puppet copies in TG_PostUpdateWork
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UBeanAnimDriverComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Library)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_BeanAnimLibrary, the bean keeps the mannequin animations"), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner()))
	{
		if (UIslanderHealthComponent* Health = Islander->GetHealthComponent())
		{
			Health->OnHealthChanged.AddDynamic(this, &UBeanAnimDriverComponent::HandleHealthChanged);
		}
	}
	StateWeights[static_cast<int32>(EBodyState::Ground)] = 1.0f;
}

void UBeanAnimDriverComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Puppet.IsValid())
	{
		Puppet->SetBasePoseSource(nullptr, 0.0f);
		Puppet->bHideArmsForOwnerInFirstPerson = bPuppetHidesArmsDefault;
	}
	if (FirstPersonArms.IsValid())
	{
		FirstPersonArms->SetHiddenInGame(false, true);
	}
	if (HandIK)
	{
		HandIK->DestroyComponent();
		HandIK = nullptr;
	}
	if (AnimSource)
	{
		AnimSource->DestroyComponent();
		AnimSource = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool UBeanAnimDriverComponent::InitSource()
{
	AActor* Owner = GetOwner();
	UBeanPuppetComponent* Body = Owner ? Owner->FindComponentByClass<UBeanPuppetComponent>() : nullptr;
	USkeletalMesh* Mesh = Body ? Cast<USkeletalMesh>(Body->GetSkinnedAsset()) : nullptr;
	if (!Mesh)
	{
		return false;
	}
	Puppet = Body;
	bPuppetHidesArmsDefault = Body->bHideArmsForOwnerInFirstPerson;
	FirstPersonArms = Owner->FindComponentByClass<UBeanFirstPersonArmsComponent>();

	AnimSource = NewObject<USkeletalMeshComponent>(Owner, TEXT("BeanLibraryAnimSource"));
	// not under the puppet: an attached component ticks after its parent, and the puppet ticks after this mesh (cycle)
	AnimSource->SetupAttachment(Body->GetAttachParent() ? Body->GetAttachParent() : Owner->GetRootComponent());
	AnimSource->SetSkeletalMesh(Mesh);
	AnimSource->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AnimSource->SetGenerateOverlapEvents(false);
	AnimSource->SetCastShadow(false);
	// never drawn, but it must keep evaluating its pose for the puppet
	AnimSource->SetVisibility(false);
	AnimSource->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	AnimSource->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	AnimSource->SetAnimInstanceClass(UBeanLibraryAnimInstance::StaticClass());
	AnimSource->RegisterComponent();
	AnimSource->AddTickPrerequisiteComponent(this);
	AnimInstance = Cast<UBeanLibraryAnimInstance>(AnimSource->GetAnimInstance());

	HandIK = NewObject<UBeanHandIKComponent>(Owner, TEXT("BeanHandIK"));
	HandIK->RegisterComponent();
	HandIK->SetPuppet(Body);
	if (!AnimInstance)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: the bean library anim instance did not start"), *GetNameSafe(Owner));
	}
	return AnimInstance != nullptr;
}

void UBeanAnimDriverComponent::PlayFullBody(UAnimSequence* Sequence)
{
	if (Sequence)
	{
		FullBodyClip = Sequence;
		FullBodyTime = 0.0f;
	}
}

void UBeanAnimDriverComponent::HandleHealthChanged(float NewHealth, float Delta)
{
	if (Delta < -1.0f && NewHealth > 0.0f && Library)
	{
		PlayFullBody(Library->HitReact);
	}
}

UBeanAnimDriverComponent::EBodyState UBeanAnimDriverComponent::PickState(float DeltaTime)
{
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	const UCharacterMovementComponent* Move = Islander ? Islander->GetCharacterMovement() : nullptr;
	const FVector Velocity = Islander ? Islander->GetVelocity() : FVector::ZeroVector;
	Speed = FVector(Velocity.X, Velocity.Y, 0.0).Size();

	if (const UIslanderHealthComponent* Health = Islander ? Islander->GetHealthComponent() : nullptr; Health && Health->IsDead())
	{
		return EBodyState::Dead;
	}
	if (const UBoatRiderComponent* Rider = Islander ? Islander->FindComponentByClass<UBoatRiderComponent>() : nullptr; Rider && Rider->IsSeated())
	{
		const ABoatActor* Boat = Rider->GetSeatedBoat();
		BoatSpeed = Boat && Boat->IsRower(Islander) ? Boat->GetVelocity().Size2D() : 0.0f;
		return EBodyState::Boat;
	}

	const bool bFalling = Move && Move->IsFalling();
	if (bFalling)
	{
		AirTime = bWasFalling ? AirTime + DeltaTime : 0.0f;
		bWasFalling = true;
		return EBodyState::Air;
	}
	if (bWasFalling)
	{
		bWasFalling = false;
		if (AirTime >= Library->LandMinAirTime)
		{
			StateTimes[static_cast<int32>(EBodyState::Land)] = 0.0f;
			LastState = EBodyState::Land;
		}
	}
	if (LastState == EBodyState::Land && StateTimes[static_cast<int32>(EBodyState::Land)] < ClipLength(Library->JumpLand) * 0.8f)
	{
		return EBodyState::Land;
	}

	const UIslanderGrabComponent* Grab = Islander ? Islander->GetGrabComponent() : nullptr;
	const bool bHolding = Grab && Grab->IsHolding();
	if (bHolding && !bWasHolding)
	{
		PlayFullBody(Library->Pickup);
	}
	bWasHolding = bHolding;
	return bHolding ? EBodyState::Carry : EBodyState::Ground;
}

void UBeanAnimDriverComponent::StateSamples(EBodyState State, TArray<FBeanAnimSample>& Out) const
{
	const UBeanAnimLibrary& L = *Library;
	const float Time = StateTimes[static_cast<int32>(State)];
	switch (State)
	{
	case EBodyState::Ground:
	{
		// idle -> walk -> run along the speed, walk and run in step
		const float WalkAlpha = FMath::Clamp(Speed / L.WalkSpeed, 0.0f, 1.0f);
		const float RunAlpha = FMath::Clamp((Speed - L.WalkSpeed) / FMath::Max(L.RunSpeed - L.WalkSpeed, 1.0f), 0.0f, 1.0f);
		AddSample(Out, L.Idle, IdleTime, 1.0f - WalkAlpha, true);
		AddSample(Out, L.Walk, GroundPhase * ClipLength(L.Walk), WalkAlpha * (1.0f - RunAlpha), true);
		AddSample(Out, L.Run, GroundPhase * ClipLength(L.Run), WalkAlpha * RunAlpha, true);
		break;
	}
	case EBodyState::Carry:
	{
		const float WalkAlpha = FMath::Clamp(Speed / L.WalkSpeed, 0.0f, 1.0f);
		AddSample(Out, L.CarryIdle, IdleTime, 1.0f - WalkAlpha, true);
		AddSample(Out, L.CarryWalk, CarryTime, WalkAlpha, true);
		break;
	}
	case EBodyState::Air:
	{
		// the take off, then the loop in the air
		const float StartLength = ClipLength(L.JumpStart);
		const float StartAlpha = L.JumpStart ? FMath::Clamp(1.0f - (Time - StartLength * 0.7f) / (StartLength * 0.3f), 0.0f, 1.0f) : 0.0f;
		AddSample(Out, L.JumpStart, Time, StartAlpha, false);
		AddSample(Out, L.JumpLoop, Time, 1.0f - StartAlpha, true);
		break;
	}
	case EBodyState::Land:
		AddSample(Out, L.JumpLand, Time, 1.0f, false);
		break;
	case EBodyState::Boat:
	{
		const float RowAlpha = FMath::Clamp(BoatSpeed / FMath::Max(L.BoatRowSpeed, 1.0f) - 0.5f, 0.0f, 1.0f);
		AddSample(Out, L.BoatSit, Time, 1.0f - RowAlpha, true);
		AddSample(Out, L.BoatRow, Time, RowAlpha, true);
		break;
	}
	case EBodyState::Dead:
		AddSample(Out, L.Death, Time, 1.0f, false);
		break;
	default:
		break;
	}
}

float UBeanAnimDriverComponent::UpperSamples(TArray<FBeanAnimSample>& Out) const
{
	const AActor* Owner = GetOwner();
	// the trumpet emote (key V) over everything held: the item is put away while it plays
	if (const UTrumpetEmoteComponent* Emote = Owner ? Owner->FindComponentByClass<UTrumpetEmoteComponent>() : nullptr)
	{
		float Elapsed = 0.0f;
		float Duration = 0.0f;
		const UBeanAnimLibrary& L = *Library;
		const UAnimSequence* Clip = nullptr;
		switch (Emote->GetPhase(Elapsed, Duration))
		{
		case ETrumpetPhase::Equip:		Clip = L.TrumpetEquip; break;
		case ETrumpetPhase::Fanfare:	Clip = L.TrumpetFanfare; break;
		case ETrumpetPhase::Unequip:	Clip = L.TrumpetUnequip; break;
		default: break;
		}
		if (Clip)
		{
			AddSample(Out, Clip, Elapsed / FMath::Max(Duration, 0.01f) * ClipLength(Clip), 1.0f, false);
			return 1.0f;
		}
	}
	const UIslanderInventoryComponent* Inventory = Owner ? Owner->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	if (!Inventory)
	{
		return 0.0f;
	}
	// the torch (in hand, or being hung back on the hip)
	for (int32 Slot = 0; Slot < 9; ++Slot)
	{
		const ATorchItem* Torch = Cast<ATorchItem>(Inventory->GetItem(Slot));
		if (!Torch || (!Torch->IsEquipped() && Torch->GetLocalMove() != ETorchMove::Unequip))
		{
			continue;
		}
		const UBeanAnimLibrary& L = *Library;
		const UAnimSequence* Clip = nullptr;
		switch (Torch->GetLocalMove())
		{
		case ETorchMove::Equip:		Clip = L.TorchEquip; break;
		case ETorchMove::Ignite:	Clip = L.TorchIgnite; break;
		case ETorchMove::Swing:		Clip = L.TorchSwing; break;
		case ETorchMove::Unequip:	Clip = L.TorchUnequip; break;
		default: break;
		}
		if (Clip)
		{
			const float Duration = FMath::Max(Torch->GetLocalMoveDuration(), 0.01f);
			AddSample(Out, Clip, Torch->GetLocalMoveElapsed() / Duration * ClipLength(Clip), 1.0f, false);
			return 1.0f;
		}
		if (!Torch->IsUsable())
		{
			return 0.0f;
		}
		AddSample(Out, Torch->IsRaised() && L.TorchRaise ? L.TorchRaise.Get() : L.TorchIdle.Get(), IdleTime, 1.0f, true);
		return 1.0f;
	}
	// the sabre (in hand, or being put away); the musket drives the arms itself (AMusketWeapon)
	for (int32 Slot = 0; Slot < 9; ++Slot)
	{
		const ACutlassWeapon* Cutlass = Cast<ACutlassWeapon>(Inventory->GetItem(Slot));
		if (!Cutlass)
		{
			continue;
		}
		const ECutlassMove Move = Cutlass->GetLocalMove();
		const UBeanAnimLibrary& L = *Library;
		const UAnimSequence* Clip = nullptr;
		switch (Move)
		{
		case ECutlassMove::Draw:		Clip = L.SabreDraw; break;
		case ECutlassMove::SlashRight:	Clip = L.SabreSlashRight; break;
		case ECutlassMove::SlashLeft:	Clip = L.SabreSlashLeft; break;
		case ECutlassMove::Thrust:		Clip = L.SabreThrust; break;
		case ECutlassMove::Finisher:	Clip = L.SabreFinisher; break;
		case ECutlassMove::Sheathe:		Clip = L.SabreSheathe; break;
		default: break;
		}
		if (Clip)
		{
			// the clip is stretched over the move as the cutlass plays it (its hit time stays the gameplay one)
			const float Duration = FMath::Max(Cutlass->GetLocalMoveDuration(), 0.01f);
			AddSample(Out, Clip, Cutlass->GetLocalMoveElapsed() / Duration * ClipLength(Clip), 1.0f, false);
			return 1.0f;
		}
		if (!Cutlass->IsUsable())
		{
			return 0.0f;
		}
		if (Cutlass->IsParrying() && L.SabreBlock)
		{
			// held in the middle of the block clip, the blade across
			AddSample(Out, L.SabreBlock, ClipLength(L.SabreBlock) * 0.45f, 1.0f, false);
			return 1.0f;
		}
		// the guard clip is 7 frames (0.29 s): looped it shakes the blade three times a second, so its middle pose is held
		AddSample(Out, L.SabreIdle, ClipLength(L.SabreIdle) * 0.5f, 1.0f, false);
		return 1.0f;
	}
	return 0.0f;
}

void UBeanAnimDriverComponent::UpdateFirstPersonArms()
{
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	bool bShow = false;
	if (bBodyArmsInFirstPerson && bDriving && Islander && Islander->IsLocallyControlled() && Islander->IsFirstPerson())
	{
		// hands free or the sabre: the body arms; the musket keeps its first person arms
		const UIslanderInventoryComponent* Inventory = Islander->FindComponentByClass<UIslanderInventoryComponent>();
		const AActor* InHand = Inventory ? Inventory->GetEquippedItem() : nullptr;
		const UTrumpetEmoteComponent* Emote = Islander->FindComponentByClass<UTrumpetEmoteComponent>();
		bShow = !InHand || InHand->IsA<ACutlassWeapon>() || InHand->IsA<ATorchItem>() || (Emote && Emote->IsPlaying());
	}
	if (bShow == bBodyArmsShown)
	{
		return;
	}
	bBodyArmsShown = bShow;
	if (Puppet.IsValid())
	{
		Puppet->bHideArmsForOwnerInFirstPerson = bShow ? false : bPuppetHidesArmsDefault;
	}
	if (FirstPersonArms.IsValid())
	{
		FirstPersonArms->SetHiddenInGame(bShow, true);
	}
}

void UBeanAnimDriverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Library)
	{
		return;
	}
	if (!AnimInstance)
	{
		if (++InitAttempts > MaxInitAttempts || !InitSource())
		{
			if (InitAttempts > MaxInitAttempts)
			{
				SetComponentTickEnabled(false);
			}
			return;
		}
	}

	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	const UCharacterMovementComponent* Move = Islander ? Islander->GetCharacterMovement() : nullptr;
	// no clip for the ragdoll and the water: the mannequin retarget takes the body back
	const bool bActive = Islander && !Islander->IsRagdoll() && !(Move && Move->IsSwimming());
	BodyAlpha = FMath::FInterpConstantTo(BodyAlpha, bActive ? 1.0f : 0.0f, DeltaTime, 1.0f / Library->StateBlendTime);

	// whole body state, blended
	const EBodyState State = PickState(DeltaTime);
	if (State != LastState && State != EBodyState::Land)
	{
		StateTimes[static_cast<int32>(State)] = State == EBodyState::Air ? AirTime : 0.0f;
	}
	LastState = State;
	const float Step = DeltaTime / Library->StateBlendTime;
	for (int32 Index = 0; Index < static_cast<int32>(EBodyState::Count); ++Index)
	{
		StateWeights[Index] = FMath::Clamp(StateWeights[Index] + (Index == static_cast<int32>(State) ? Step : -Step), 0.0f, 1.0f);
		StateTimes[Index] += DeltaTime;
	}

	// clocks of the cycles: the feet keep pace with the speed
	const UBeanAnimLibrary& L = *Library;
	IdleTime += DeltaTime;
	const float WalkAlpha = FMath::Clamp(Speed / L.WalkSpeed, 0.0f, 1.0f);
	const float RunAlpha = FMath::Clamp((Speed - L.WalkSpeed) / FMath::Max(L.RunSpeed - L.WalkSpeed, 1.0f), 0.0f, 1.0f);
	const float WalkRate = FMath::Clamp(Speed / L.WalkNoSlideSpeed, L.MinPlayRate, L.MaxPlayRate);
	const float RunRate = FMath::Clamp(Speed / L.RunNoSlideSpeed, L.MinPlayRate, L.MaxPlayRate);
	const float CyclesPerSecond = FMath::Lerp(WalkRate / ClipLength(L.Walk), RunRate / ClipLength(L.Run), RunAlpha);
	GroundPhase = FMath::Fmod(GroundPhase + DeltaTime * CyclesPerSecond * (WalkAlpha > 0.0f ? 1.0f : 0.0f), 1.0f);
	CarryTime += DeltaTime * FMath::Clamp(Speed / L.CarryNoSlideSpeed, L.MinPlayRate, L.MaxPlayRate);

	FBeanAnimFrame Frame;
	float Total = 0.0f;
	for (int32 Index = 0; Index < static_cast<int32>(EBodyState::Count); ++Index)
	{
		Total += StateWeights[Index];
	}
	for (int32 Index = 0; Index < static_cast<int32>(EBodyState::Count); ++Index)
	{
		if (StateWeights[Index] > KINDA_SMALL_NUMBER)
		{
			TArray<FBeanAnimSample> Samples;
			StateSamples(static_cast<EBodyState>(Index), Samples);
			for (FBeanAnimSample& Sample : Samples)
			{
				Sample.Weight *= StateWeights[Index] / FMath::Max(Total, KINDA_SMALL_NUMBER);
				Frame.Base.Add(Sample);
			}
		}
	}

	// one shot of the whole body, with its blend in and out
	if (FullBodyClip)
	{
		FullBodyTime += DeltaTime;
		const float Length = ClipLength(FullBodyClip);
		if (FullBodyTime >= Length || State == EBodyState::Dead || State == EBodyState::Boat)
		{
			FullBodyClip = nullptr;
		}
		else
		{
			Frame.FullBody.Sequence = FullBodyClip;
			Frame.FullBody.Time = FullBodyTime;
			Frame.FullBody.Weight = FMath::Min3(FullBodyTime / L.OneShotBlendIn, (Length - FullBodyTime) / L.OneShotBlendOut, 1.0f);
		}
	}

	// upper body: the sabre
	const float UpperTarget = State == EBodyState::Dead || State == EBodyState::Boat ? 0.0f : UpperSamples(Frame.Upper);
	UpperWeight = FMath::FInterpConstantTo(UpperWeight, UpperTarget, DeltaTime, 1.0f / L.OneShotBlendIn);
	Frame.UpperWeight = Frame.Upper.IsEmpty() ? 0.0f : UpperWeight;
	Frame.UpperBodyRoot = L.UpperBodyRootBone;

	AnimInstance->SetFrame(Frame);
	bDriving = BodyAlpha > KINDA_SMALL_NUMBER;
	if (Puppet.IsValid())
	{
		Puppet->SetBasePoseSource(bDriving ? AnimSource.Get() : nullptr, BodyAlpha);
	}
	UpdateFirstPersonArms();
	UpdateRowing(State);
}

void UBeanAnimDriverComponent::UpdateRowing(EBodyState State)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UBoatRiderComponent* Rider = Character ? Character->FindComponentByClass<UBoatRiderComponent>() : nullptr;
	ABoatActor* Boat = Rider ? Rider->GetSeatedBoat() : nullptr;
	const float BoatWeight = StateWeights[static_cast<int32>(EBodyState::Boat)];
	if (!Boat || !Boat->IsRower(Character) || State != EBodyState::Boat || !bDriving)
	{
		if (HandIK)
		{
			HandIK->SetTargets(FVector::ZeroVector, FVector::ZeroVector, 0.0f, Library->HandGripReachCm);
		}
		return;
	}
	// the oars follow the clip: at the catch while sitting still, along the stroke while rowing
	const TArray<FVector2D>& Curve = Library->BoatRowOarCurve;
	if (!Curve.IsEmpty())
	{
		const float RowAlpha = FMath::Clamp(BoatSpeed / FMath::Max(Library->BoatRowSpeed, 1.0f) - 0.5f, 0.0f, 1.0f);
		const float Frame = FMath::Fmod(StateTimes[static_cast<int32>(EBodyState::Boat)] * Library->BoatRowOarCurveRate, static_cast<float>(Curve.Num()));
		const int32 A = FMath::Clamp(FMath::FloorToInt32(Frame), 0, Curve.Num() - 1);
		const int32 B = (A + 1) % Curve.Num();
		const FVector2D Stroke = FMath::Lerp(Curve[A], Curve[B], Frame - A);
		const FVector2D Angles = FMath::Lerp(Curve[0], Stroke, RowAlpha);
		Boat->SetOarAnimOverride(Angles.X, Angles.Y);
	}
	// the mittens on the handles: each hand takes the nearer one
	if (HandIK && Puppet.IsValid())
	{
		const FVector Port = Boat->GetOarHandle(0);
		const FVector Starboard = Boat->GetOarHandle(1);
		const FVector RightShoulder = Puppet->GetBoneLocation(TEXT("upperarm_r"));
		const bool bRightPort = FVector::DistSquared(RightShoulder, Port) < FVector::DistSquared(RightShoulder, Starboard);
		HandIK->SetTargets(bRightPort ? Port : Starboard, bRightPort ? Starboard : Port, Library->RowHandIKAlpha * BoatWeight, Library->HandGripReachCm);
	}
}
