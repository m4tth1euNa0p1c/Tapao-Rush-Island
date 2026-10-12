// monke_island

#include "Creatures/GiantCrabFeedback.h"
#include "Creatures/GiantCrab.h"
#include "Creatures/GiantCrabBody.h"
#include "Creatures/GiantCrabLag.h"
#include "Creatures/GiantCrabMath.h"
#include "Creatures/GiantCrabRules.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"

namespace
{
	/** Seconds between two looks for a local camera near enough to hear the steps */
	constexpr float CrabAudibleCheckInterval = 0.5f;

	/** The lowest height of a tip rises this fast (cm/s): the step contact follows a crab standing up again */
	constexpr float CrabFootGroundRise = 3.0f;

	/** Mouth of the kit (the voice comes from there) */
	const FName CrabMouthSocket(TEXT("SOCKET_Mouth"));

	/** A moment of an action received late still plays this long after its time (s): a packet a little late keeps its
	 *  sound, a crab back in view does not replay its cries (presentation only) */
	constexpr float CrabLateCueSeconds = 0.3f;
}

UGiantCrabFeedbackComponent::UGiantCrabFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false);
}

void UGiantCrabFeedbackComponent::Setup(UGiantCrabRules* InRules, USkeletalMeshComponent* InMesh, UGiantCrabBodyComponent* InBody)
{
	// nobody hears a dedicated server
	if (!InRules || !InMesh || GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
		return;
	}
	Crab = Cast<AGiantCrab>(GetOwner());
	Rules = InRules;
	Mesh = InMesh;
	Body = InBody;
	Feet.Reset();
	if (Mesh->GetSkinnedAsset())
	{
		for (const FGiantCrabFoot& Spec : Rules->Feet)
		{
			FFoot& Foot = Feet.AddDefaulted_GetRef();
			Foot.Bone = Spec.Bone;
			Foot.BoneIndex = Mesh->GetBoneIndex(Spec.Bone);
			Foot.bHeavy = Spec.bHeavy;
			Foot.TipOffset = Spec.TipOffsetCm;
		}
	}
	const double Now = GetWorld()->GetTimeSeconds();
	NextVoice = Now + FMath::FRandRange(Rules->GrowlInterval.X, FMath::Max(Rules->GrowlInterval.X, Rules->GrowlInterval.Y));
	SetComponentTickEnabled(Crab != nullptr);
}

void UGiantCrabFeedbackComponent::OnActionStarted(const TArray<FGiantCrabMoment>& InMoments, float Age)
{
	Moments = InMoments;
	ActionTime = FMath::Max(0.0f, Age);
	// an action received late plays its moments still due, and the ones just missed; not those long past (a crab that
	// comes back into view, a player who joins: no death cry, no roar again)
	TArray<float, TInlineAllocator<8>> Times;
	for (const FGiantCrabMoment& Moment : Moments)
	{
		Times.Add(Moment.Time);
	}
	MomentsPlayed = GiantCrabLag::FirstFreshMoment(Times, ActionTime, CrabLateCueSeconds);
}

void UGiantCrabFeedbackComponent::OnClipPlayed(UAnimSequenceBase* InClip, bool bLoop, float Rate, bool bRestart, float StartTime)
{
	ClipRate = Rate;
	bClipLoop = bLoop;
	if (InClip == Clip && !bRestart)
	{
		return;
	}
	Clip = InClip;
	ClipLength = InClip ? InClip->GetPlayLength() : 0.0f;
	// a clip started further in (an action received late) does not play the notifies it skipped
	ClipTime = FMath::Clamp(StartTime, 0.0f, ClipLength);
	bClipStarted = ClipTime <= 0.0f;
	ClipEvents.Reset();
	ClipEventTimes.Reset();
	bClipHasSteps = false;
	if (!InClip || !Rules)
	{
		return;
	}
	// the notifies this machine plays itself (the native animation instance fires none): steps and cues
	for (const FAnimNotifyEvent& Event : InClip->Notifies)
	{
		const FString Name = Event.NotifyName.ToString();
		FClipEvent Entry;
		Entry.Time = Event.GetTriggerTime();
		if (!Rules->StepNotifyPrefix.IsEmpty() && Name.StartsWith(Rules->StepNotifyPrefix))
		{
			const FName Bone(*Name.RightChop(Rules->StepNotifyPrefix.Len()));
			Entry.Foot = Feet.IndexOfByPredicate([&Bone](const FFoot& Foot) { return Foot.Bone == Bone; });
			bClipHasSteps = true;
			if (Entry.Foot == INDEX_NONE)
			{
				continue;
			}
		}
		else if (const EGiantCrabCue* Cue = Rules->NotifyCues.Find(Event.NotifyName))
		{
			Entry.Cue = *Cue;
		}
		else
		{
			continue;
		}
		ClipEvents.Add(Entry);
		ClipEventTimes.Add(Entry.Time);
	}
}

void UGiantCrabFeedbackComponent::OnClipRate(float Rate)
{
	ClipRate = Rate;
}

void UGiantCrabFeedbackComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Crab || !Rules || !Mesh)
	{
		return;
	}

	// the eyes of the dead crab cloud over
	if (DeadEyesTime >= 0.0 && GetWorld()->GetTimeSeconds() >= DeadEyesTime)
	{
		DeadEyesTime = -1.0;
		for (UMaterialInstanceDynamic* Eye : GetEyeMaterials())
		{
			Eye->SetScalarParameterValue(Rules->DeadEyesParam, 1.0f);
		}
	}

	// moments of the action under way (rare, played wherever the camera is: the sound fades with the distance)
	ActionTime += DeltaTime;
	while (MomentsPlayed < Moments.Num() && Moments[MomentsPlayed].Time <= ActionTime)
	{
		const FGiantCrabMoment& Moment = Moments[MomentsPlayed++];
		PlayCue(Moment.Cue, Crab->GetMomentLocation(Moment));
	}

	AudibleCheck -= DeltaTime;
	if (AudibleCheck <= 0.0f)
	{
		AudibleCheck = CrabAudibleCheckInterval;
		float Distance = 0.0f;
		bAudible = NearestLocalCamera(Crab->GetActorLocation(), Distance) && Distance <= Rules->StepAudibleDistanceCm;
	}
	UpdateClipClock(DeltaTime);
	if (!bAudible)
	{
		return;
	}
	if (!bClipHasSteps)
	{
		UpdateFeet(DeltaTime);
	}
	UpdateVoice(DeltaTime);
}

void UGiantCrabFeedbackComponent::UpdateClipClock(float DeltaTime)
{
	if (!Clip || ClipLength <= 0.0f)
	{
		return;
	}
	// the clock of the native animation instance (TikiMath::AdvanceClip): loops wrap, the others hold their end
	const float Previous = bClipStarted ? -UE_KINDA_SMALL_NUMBER : ClipTime;
	bClipStarted = false;
	const float Next = ClipTime + DeltaTime * ClipRate;
	ClipTime = bClipLoop ? FMath::Fmod(FMath::Max(Next, 0.0f), ClipLength) : FMath::Clamp(Next, 0.0f, ClipLength);
	if (ClipEvents.IsEmpty() || !bAudible)
	{
		return;
	}
	// a clip that went round several times in one frame (a huge rate) plays its notifies once; the feet that land
	// together (four at a time in the walks of T-260) make one step, heavy when a hind foot is among them
	GiantCrabMath::CrossedTimes(ClipEventTimes, Previous, ClipTime, ClipLength, bClipLoop, Crossed);
	int32 StepFoot = INDEX_NONE;
	for (const int32 Index : Crossed)
	{
		const FClipEvent& Event = ClipEvents[Index];
		if (Event.Foot != INDEX_NONE && Feet.IsValidIndex(Event.Foot))
		{
			if (StepFoot == INDEX_NONE || (Feet[Event.Foot].bHeavy && !Feet[StepFoot].bHeavy))
			{
				StepFoot = Event.Foot;
			}
		}
		else if (Event.Cue != EGiantCrabCue::None)
		{
			PlayCue(Event.Cue, Crab->GetGroundPoint());
		}
	}
	if (StepFoot != INDEX_NONE)
	{
		PlayStep(StepFoot, GetFootTip(Feet[StepFoot]));
	}
}

FVector UGiantCrabFeedbackComponent::GetFootTip(const FFoot& Foot) const
{
	// the end of the dactyl (manifest v2 of T-260): its bone, the tip along it; the bone's rotation only, the kit's
	// bones carry a scale of 100
	if (Foot.BoneIndex != INDEX_NONE)
	{
		const FTransform Bone = Mesh->GetBoneTransform(Foot.BoneIndex);
		return Bone.GetLocation() + Bone.GetRotation().RotateVector(Foot.TipOffset);
	}
	// a mesh without the bone: the lowest point of the leg's hit body
	if (const UCapsuleComponent* Capsule = Body ? Cast<UCapsuleComponent>(Body->FindBody(Foot.Bone)) : nullptr)
	{
		const FVector Center = Capsule->GetComponentLocation();
		const FVector Axis = Capsule->GetUpVector() * (Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius());
		const FVector Low = (Center + Axis).Z < (Center - Axis).Z ? Center + Axis : Center - Axis;
		return Low - FVector(0.0f, 0.0f, Capsule->GetScaledCapsuleRadius());
	}
	return Crab->GetGroundPoint();
}

void UGiantCrabFeedbackComponent::UpdateFeet(float DeltaTime)
{
	const float MeshZ = static_cast<float>(Mesh->GetComponentLocation().Z);
	int32 StepFoot = INDEX_NONE;
	FVector StepTip = FVector::ZeroVector;
	for (int32 Index = 0; Index < Feet.Num(); ++Index)
	{
		FFoot& Foot = Feet[Index];
		const FVector Tip = GetFootTip(Foot);
		const float Height = static_cast<float>(Tip.Z) - MeshZ;
		// the lowest point of the tip lately: the sand under it, whatever the pose (standing, turned over)
		Foot.Ground = FMath::Min(Foot.Ground + CrabFootGroundRise * DeltaTime, Height);
		// the feet that land in the same frame make one step, heavy when a hind foot is among them
		if (GiantCrabMath::FootStep(Height - Foot.Ground, Rules->StepLiftCm, Rules->StepContactCm, Foot.bLifted)
			&& (StepFoot == INDEX_NONE || (Foot.bHeavy && !Feet[StepFoot].bHeavy)))
		{
			StepFoot = Index;
			StepTip = Tip;
		}
	}
	if (StepFoot != INDEX_NONE)
	{
		PlayStep(StepFoot, StepTip);
	}
}

void UGiantCrabFeedbackComponent::PlayStep(int32 FootIndex, const FVector& Tip)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextStepTime)
	{
		return;
	}
	NextStepTime = Now + 1.0 / FMath::Max(1.0f, Rules->MaxStepsPerSecond);
	const bool bHeavy = Feet[FootIndex].bHeavy;
	const FGiantCrabCueRules& Cue = bHeavy ? Rules->HeavyStepCue : Rules->StepCue;
	if (!Cue.Sounds.IsEmpty())
	{
		if (USoundBase* Sound = Cue.Sounds[FMath::RandRange(0, Cue.Sounds.Num() - 1)])
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Tip, Cue.Volume, 1.0f + FMath::FRandRange(-Cue.PitchJitter, Cue.PitchJitter));
		}
	}
	// the heavy steps shake the ground a little, not every one of them
	if (bHeavy && Cue.Shake && Now >= NextStepShake)
	{
		NextStepShake = Now + Rules->StepShakeInterval;
		ShakeLocalCameras(Cue.Shake, Tip, Cue.ShakeScale, Cue.ShakeInnerRadiusCm, Cue.ShakeOuterRadiusCm);
	}
}

void UGiantCrabFeedbackComponent::UpdateVoice(float DeltaTime)
{
	if (Crab->IsDead())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bEngaged = Crab->IsEngaged();
	// it noticed someone: a hiss at once
	if (bEngaged && !bWasEngaged)
	{
		NextVoice = Now;
	}
	bWasEngaged = bEngaged;
	if (Now < NextVoice)
	{
		return;
	}
	const FVector2D Interval = bEngaged ? Rules->HissInterval : Rules->GrowlInterval;
	NextVoice = Now + FMath::FRandRange(Interval.X, FMath::Max(Interval.X, Interval.Y));
	// not over its own roar or its blows
	const EGiantCrabState State = Crab->GetState();
	if (State == EGiantCrabState::Roar || State == EGiantCrabState::ChargeWindup || State == EGiantCrabState::BellySlam)
	{
		return;
	}
	const FVector Mouth = Mesh->DoesSocketExist(CrabMouthSocket) ? Mesh->GetSocketLocation(CrabMouthSocket) : Crab->GetActorLocation();
	PlayCue(bEngaged ? EGiantCrabCue::Hiss : EGiantCrabCue::Growl, Mouth);
}

void UGiantCrabFeedbackComponent::PlayCue(EGiantCrabCue Cue, const FVector& Location, float VolumeScale)
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FGiantCrabCueRules* CueRules = Rules->GetCue(Cue);
	if (!CueRules)
	{
		return;
	}
	USoundBase* Sound = CueRules->Sounds.IsEmpty() ? nullptr : CueRules->Sounds[FMath::RandRange(0, CueRules->Sounds.Num() - 1)].Get();
	if (!Sound)
	{
		// the single sounds of the older rules (bound by the expedition audio)
		Sound = Cue == EGiantCrabCue::Hit ? Rules->HitSound.Get()
			: Cue == EGiantCrabCue::Clack ? Rules->ClackSound.Get()
			: Cue == EGiantCrabCue::Slam || Cue == EGiantCrabCue::BellySlam || Cue == EGiantCrabCue::Stomp ? Rules->SlamSound.Get() : nullptr;
	}
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, CueRules->Volume * VolumeScale,
			1.0f + FMath::FRandRange(-CueRules->PitchJitter, CueRules->PitchJitter));
	}
	if (CueRules->Shake)
	{
		ShakeLocalCameras(CueRules->Shake, Location, CueRules->ShakeScale, CueRules->ShakeInnerRadiusCm, CueRules->ShakeOuterRadiusCm);
	}
	if (CueRules->DustScale > 0.0f && Rules->DustSystem)
	{
		// the effect sprays along its local +X: up from the sand
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Rules->DustSystem, Location, FRotator(90.0f, 0.0f, 0.0f), FVector(CueRules->DustScale),
			true, true, ENCPoolMethod::AutoRelease);
	}
}

void UGiantCrabFeedbackComponent::ShakeLocalCameras(TSubclassOf<UCameraShakeBase> Shake, const FVector& Location, float Scale, float Inner, float Outer) const
{
	UWorld* World = GetWorld();
	if (!World || !Shake || Scale <= 0.0f)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!Controller || !Controller->IsLocalController() || !Controller->PlayerCameraManager)
		{
			continue;
		}
		const float Distance = static_cast<float>(FVector::Dist(Controller->PlayerCameraManager->GetCameraLocation(), Location));
		const float Strength = Scale * GiantCrabMath::ShakeFalloff(Distance, Inner, Outer);
		if (Strength > 0.01f)
		{
			Controller->PlayerCameraManager->StartCameraShake(Shake, Strength);
		}
	}
}

bool UGiantCrabFeedbackComponent::NearestLocalCamera(const FVector& Location, float& OutDistance) const
{
	UWorld* World = GetWorld();
	bool bFound = false;
	OutDistance = TNumericLimits<float>::Max();
	if (!World)
	{
		return false;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (Controller && Controller->IsLocalController() && Controller->PlayerCameraManager)
		{
			OutDistance = FMath::Min(OutDistance, static_cast<float>(FVector::Dist(Controller->PlayerCameraManager->GetCameraLocation(), Location)));
			bFound = true;
		}
	}
	return bFound;
}

void UGiantCrabFeedbackComponent::SetFrenzy(bool bFrenzy)
{
	if (!Rules || !Mesh || bFrenzy == bFrenzyShown || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	bFrenzyShown = bFrenzy;
	if (bFrenzy)
	{
		// red eyes: the eye materials take the frenzy colour and glow
		for (UMaterialInstanceDynamic* Red : GetEyeMaterials())
		{
			for (const FName& Param : Rules->FrenzyEyeColorParams)
			{
				Red->SetVectorParameterValue(Param, Rules->FrenzyEyeColor);
			}
			for (const TPair<FName, float>& Param : Rules->FrenzyEyeScalarParams)
			{
				Red->SetScalarParameterValue(Param.Key, Param.Value);
			}
		}
		// and a red light at each eye
		if (Rules->FrenzyEyeLightIntensity > 0.0f)
		{
			for (const FName& Socket : Rules->EyeSockets)
			{
				if (!Mesh->DoesSocketExist(Socket))
				{
					continue;
				}
				UPointLightComponent* Light = NewObject<UPointLightComponent>(GetOwner(), NAME_None, RF_Transient);
				Light->SetupAttachment(Mesh, Socket);
				Light->SetUsingAbsoluteScale(true);
				Light->SetIntensityUnits(ELightUnits::Candelas);
				Light->SetIntensity(Rules->FrenzyEyeLightIntensity);
				Light->SetLightColor(FLinearColor(1.0f, 0.05f, 0.02f));
				Light->SetAttenuationRadius(Rules->FrenzyEyeLightRadiusCm);
				Light->SetCastShadows(false);
				Light->RegisterComponent();
				EyeLights.Add(Light);
			}
		}
		return;
	}
	// calm again (never in play: the phases do not go back)
	for (int32 Index = 0; Index < EyeSlots.Num() && Index < CalmMaterials.Num(); ++Index)
	{
		Mesh->SetMaterial(EyeSlots[Index], CalmMaterials[Index]);
	}
	RemoveEyeLights();
	FrenzyMaterials.Reset();
	CalmMaterials.Reset();
	EyeSlots.Reset();
}

const TArray<TObjectPtr<UMaterialInstanceDynamic>>& UGiantCrabFeedbackComponent::GetEyeMaterials()
{
	if (!FrenzyMaterials.IsEmpty() || !Rules || !Mesh)
	{
		return FrenzyMaterials;
	}
	for (const FName& SlotName : Rules->EyeMaterialSlots)
	{
		const int32 Slot = Mesh->GetMaterialIndex(SlotName);
		UMaterialInterface* Calm = Slot != INDEX_NONE ? Mesh->GetMaterial(Slot) : nullptr;
		UMaterialInstanceDynamic* Eye = Calm ? UMaterialInstanceDynamic::Create(Calm, this) : nullptr;
		if (!Eye)
		{
			continue;
		}
		Mesh->SetMaterial(Slot, Eye);
		EyeSlots.Add(Slot);
		CalmMaterials.Add(Calm);
		FrenzyMaterials.Add(Eye);
	}
	return FrenzyMaterials;
}

void UGiantCrabFeedbackComponent::RemoveEyeLights()
{
	for (UPointLightComponent* Light : EyeLights)
	{
		if (IsValid(Light))
		{
			Light->DestroyComponent();
		}
	}
	EyeLights.Reset();
}

void UGiantCrabFeedbackComponent::OnDeath(float DeadEyesDelay)
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// the glow goes out, the eyes cloud over at the moment of the clip
	RemoveEyeLights();
	DeadEyesTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, DeadEyesDelay);
}

void UGiantCrabFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// the lights are components of the actor: they go with it
	EyeLights.Reset();
	Super::EndPlay(EndPlayReason);
}
