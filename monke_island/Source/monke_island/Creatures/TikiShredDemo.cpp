// monke_island

#include "Creatures/TikiShredDemo.h"
#include "Creatures/TikiAnimInstance.h"
#include "Creatures/TikiBlood.h"
#include "Creatures/TikiRules.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

TArray<TWeakObjectPtr<ATikiShredDemo>> ATikiShredDemo::Kept;

namespace TikiShredDemoParams
{
	/** Scalar parameter written to the fragment materials during the fade (0 to 1) */
	static const FName Fade(TEXT("Fade"));
	/** Niagara user parameters of the burst effect (as the blood effects) */
	static const FName Color(TEXT("BloodColor"));
	static const FName Direction(TEXT("Direction"));
	/** Steps of the fade per second */
	static constexpr float FadeRate = 30.0f;
}

const FTikiShredDemoClip* UTikiShredDemoSet::Find(ETikiCombatClip Clip) const
{
	if (Clip == ETikiCombatClip::None)
	{
		return nullptr;
	}
	for (const FTikiShredDemoClip& Demo : Demos)
	{
		if (Demo.Clip == Clip && !Demo.Mesh.IsNull() && !Demo.Anim.IsNull())
		{
			return &Demo;
		}
	}
	return nullptr;
}

uint32 UTikiShredDemoSet::AvailableMask() const
{
	uint32 Mask = 0;
	for (const FTikiShredDemoClip& Demo : Demos)
	{
		if (Demo.Clip != ETikiCombatClip::None && !Demo.Mesh.IsNull() && !Demo.Anim.IsNull())
		{
			Mask |= 1u << static_cast<uint8>(Demo.Clip);
		}
	}
	return Mask;
}

void UTikiShredDemoSet::Preload(const UObject* WorldContext, const UTikiRules* Rules)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (PreloadHandle.IsValid() || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const FTikiShredDemoClip& Demo : Demos)
	{
		if (!Demo.Mesh.IsNull() && !Demo.Anim.IsNull())
		{
			Paths.Add(Demo.Mesh.ToSoftObjectPath());
			Paths.Add(Demo.Anim.ToSoftObjectPath());
		}
	}
	if (Rules && !Rules->ShredDemoBurstSystem.IsNull())
	{
		Paths.Add(Rules->ShredDemoBurstSystem.ToSoftObjectPath());
	}
	if (!Paths.IsEmpty())
	{
		// kept: the demos stay loaded while the set is, ready for the first death (no hitch then)
		PreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}
}

ATikiShredDemo::ATikiShredDemo()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Fragments = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Fragments"));
	Fragments->SetupAttachment(Root);
	Fragments->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Fragments->SetGenerateOverlapEvents(false);
	// the demo clip plays through the tikis' native instance, which puts the reference scale back on the root bone
	// (PoseRoot) as the single node mode does not: the GoreBox demo clips carry a root at 1 on meshes whose reference
	// root is at 100, and drew the fragments a hundred times too small, the body seemed to vanish at a close or heavy
	// shot (owner 2026-10-10: "fix les bugs de dépop des mobs")
	Fragments->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	Fragments->SetAnimInstanceClass(UTikiAnimInstance::StaticClass());
	Fragments->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
}

ATikiShredDemo* ATikiShredDemo::Spawn(UWorld* World, UTikiRules* Rules, const FTikiShredDemoClip& Demo, const FVector& FeetLocation, float Yaw,
	const FVector& Scale, float Age, const AActor* Ignore, bool bFreshBlood, FName HiddenBone)
{
	if (!World || !Rules || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	// preloaded at BeginPlay; a demo still loading is finished here (one hitch rather than no demo)
	USkeletalMesh* Mesh = Demo.Mesh.LoadSynchronous();
	UAnimSequenceBase* Anim = Demo.Anim.LoadSynchronous();
	if (!Mesh || !Anim)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Shred demo %s: mesh or animation missing (%s, %s)"), *UEnum::GetValueAsString(Demo.Clip),
			*Demo.Mesh.ToString(), *Demo.Anim.ToString());
		return nullptr;
	}
	if (Anim->GetSkeleton() != Mesh->GetSkeleton())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Shred demo %s: %s is not on the skeleton of %s"), *UEnum::GetValueAsString(Demo.Clip),
			*GetNameSafe(Anim), *GetNameSafe(Mesh));
		return nullptr;
	}

	// on the ground under the feet: the fragments land on the pack's ground plane, neither sunk nor floating
	FVector Origin = FeetLocation;
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiShredDemoGround), false, Ignore);
	if (const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(World))
	{
		for (const TWeakObjectPtr<AActor>& Surface : Water->GetWaterSurfaces())
		{
			if (const AActor* SurfaceActor = Surface.Get())
			{
				Params.AddIgnoredActor(SurfaceActor);
			}
		}
	}
	FHitResult Ground;
	if (World->LineTraceSingleByObjectType(Ground, FeetLocation + FVector(0.0, 0.0, Rules->ShredDemoGroundTraceHeight),
		FeetLocation - FVector(0.0, 0.0, Rules->ShredDemoGroundTraceDepth), Objects, Params))
	{
		Origin.Z = Ground.ImpactPoint.Z;
	}
	Origin.Z += Rules->ShredDemoGroundClearance;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATikiShredDemo* Shred = World->SpawnActor<ATikiShredDemo>(ATikiShredDemo::StaticClass(), Origin, FRotator(0.0f, Yaw, 0.0f), SpawnParams);
	if (!Shred)
	{
		return nullptr;
	}
	Shred->Rules = Rules;
	Shred->SeveredBones = Demo.SeveredBones;
	Shred->Length = Anim->GetPlayLength();
	Shred->StartTime = TikiShredDemo::StartTime(Demo.ContactTime, Age, Shred->Length);
	Shred->SpawnTime = World->GetTimeSeconds();
	Shred->SetActorScale3D(Scale);

	USkeletalMeshComponent* Fragments = Shred->Fragments;
	Fragments->SetRelativeRotation(Rules->ShredDemoMeshRotation);
	Fragments->SetBoundsScale(Rules->ShredDemoBoundsScale);
	Fragments->SetSkeletalMeshAsset(Mesh);
	// played once from the contact, then held on its last frame, the root at the reference scale
	if (UTikiAnimInstance* Instance = Cast<UTikiAnimInstance>(Fragments->GetAnimInstance()))
	{
		Instance->PlayClip(Anim, false, 0.0f, 1.0f, true, Shred->StartTime);
	}
	else
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Shred demo %s: no tiki animation instance on %s, single node instead (root scale not kept)"),
			*UEnum::GetValueAsString(Demo.Clip), *GetNameSafe(Mesh));
		Fragments->PlayAnimation(Anim, false);
		Fragments->SetPosition(Shred->StartTime, false);
	}
	Shred->RestRelativeLocation = Fragments->GetRelativeLocation();
	// the weapon the mob dropped as a pickup is that pickup: its fragment stays out of the demo (T-161)
	if (!HiddenBone.IsNone() && Fragments->GetBoneIndex(HiddenBone) != INDEX_NONE)
	{
		Fragments->HideBoneByName(HiddenBone, PBO_None);
		Shred->SeveredBones.Remove(HiddenBone);
	}

	FTimerManager& Timers = World->GetTimerManager();
	// the blood leaves the cuts at the contact frame: the animation starts there, the pose is evaluated next frame
	if (bFreshBlood && Rules->bBloodEnabled)
	{
		Timers.SetTimerForNextTick(FTimerDelegate::CreateUObject(Shred, &ATikiShredDemo::SpawnContactBlood));
	}
	const float PlayLeft = FMath::Max(0.0f, Shred->Length - Shred->StartTime);
	if (Rules->bBloodEnabled)
	{
		Timers.SetTimer(Shred->LandTimer, Shred, &ATikiShredDemo::SpawnLandBlood, FMath::Max(0.01f, PlayLeft), false);
	}
	Timers.SetTimer(Shred->EndTimer, Shred, &ATikiShredDemo::EndHold, FMath::Max(0.01f, PlayLeft + Rules->ShredDemoHoldTime), false);

	// the burst effect of the shot at the contact (T-169), at the body's middle, along the shot
	if (bFreshBlood && Demo.Clip == ETikiCombatClip::MusketBurst && !Rules->ShredDemoBurstSystem.IsNull())
	{
		if (UNiagaraSystem* Burst = Rules->ShredDemoBurstSystem.LoadSynchronous())
		{
			const FVector Along = -Shred->GetActorForwardVector();
			const FVector Middle = Fragments->Bounds.Origin;
			if (UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Burst, Middle, Along.Rotation()))
			{
				Effect->SetVariableLinearColor(TikiShredDemoParams::Color, Rules->BloodColor);
				Effect->SetVariableVec3(TikiShredDemoParams::Direction, Along);
			}
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("Shred demo %s (%s) from %.2f s at %s, yaw %.0f"), *UEnum::GetValueAsString(Demo.Clip), *GetNameSafe(Mesh),
		Shred->StartTime, *Origin.ToCompactString(), Yaw);
	return Shred;
}

void ATikiShredDemo::SpawnContactBlood()
{
	UTikiBloodSubsystem* Blood = UTikiBloodSubsystem::Get(this);
	if (!Blood || !Rules || SeveredBones.IsEmpty())
	{
		return;
	}
	// the middle of the fragments: each jet leaves its severed fragment away from it, attached to the fragment
	FVector Middle = FVector::ZeroVector;
	int32 Count = 0;
	for (int32 Bone = 0; Bone < Fragments->GetNumBones(); ++Bone)
	{
		Middle += Fragments->GetBoneTransform(Bone).GetLocation();
		++Count;
	}
	Middle = Count > 0 ? Middle / Count : GetActorLocation();
	const FVector Blow = -GetActorForwardVector();
	for (const FName& Bone : SeveredBones)
	{
		if (Fragments->GetBoneIndex(Bone) == INDEX_NONE)
		{
			continue;
		}
		const FVector Location = Fragments->GetBoneLocation(Bone);
		const FVector Along = (Location - Middle).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		Blood->SpawnSliceSpray(Rules, Fragments, Bone, Location, Along);
	}
	Blood->SpawnChunks(Rules, Middle, Blow);
	Blood->SpawnSplat(Rules, Middle, this);
}

void ATikiShredDemo::SpawnLandBlood()
{
	UTikiBloodSubsystem* Blood = UTikiBloodSubsystem::Get(this);
	if (!Blood || !Rules)
	{
		return;
	}
	// a splat where each severed fragment lies
	for (const FName& Bone : SeveredBones)
	{
		if (Fragments->GetBoneIndex(Bone) != INDEX_NONE)
		{
			Blood->SpawnSplat(Rules, Fragments->GetBoneLocation(Bone), this);
		}
	}
}

void ATikiShredDemo::EndHold()
{
	if (!Rules || Rules->ShredDemoEnd == ETikiShredDemoEnd::Fade)
	{
		StartFade();
		return;
	}
	// kept as pieces on the ground; beyond the most of the rules, the oldest of this world fades
	Kept.RemoveAll([](const TWeakObjectPtr<ATikiShredDemo>& Demo) { return !Demo.IsValid(); });
	Kept.Add(this);
	int32 InWorld = 0;
	for (const TWeakObjectPtr<ATikiShredDemo>& Demo : Kept)
	{
		InWorld += Demo->GetWorld() == GetWorld() ? 1 : 0;
	}
	for (int32 Index = 0; Index < Kept.Num() && InWorld > Rules->ShredDemoKeepMax; ++Index)
	{
		ATikiShredDemo* Oldest = Kept[Index].Get();
		if (Oldest && Oldest->GetWorld() == GetWorld())
		{
			Kept.RemoveAt(Index--);
			--InWorld;
			Oldest->StartFade();
		}
	}
}

void ATikiShredDemo::StartFade()
{
	Kept.Remove(this);
	SpawnTime = GetWorld()->GetTimeSeconds();
	if (!Rules || Rules->ShredDemoFadeTime <= 0.0f)
	{
		Destroy();
		return;
	}
	GetWorldTimerManager().SetTimer(FadeTimer, this, &ATikiShredDemo::StepFade, 1.0f / TikiShredDemoParams::FadeRate, true);
}

void ATikiShredDemo::StepFade()
{
	// SpawnTime holds the start of the fade here: a fade with no play nor hold before it
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - SpawnTime);
	const float Alpha = TikiShredDemo::FadeAlpha(Elapsed, 0.0f, 0.0f, 0.0f, Rules ? Rules->ShredDemoFadeTime : 0.0f);
	Fragments->SetRelativeLocation(RestRelativeLocation - FVector(0.0, 0.0, (Rules ? Rules->ShredDemoSinkDepth : 0.0f) * Alpha));
	Fragments->SetScalarParameterValueOnMaterials(TikiShredDemoParams::Fade, Alpha);
	if (Alpha >= 1.0f)
	{
		GetWorldTimerManager().ClearTimer(FadeTimer);
		Destroy();
	}
}

void ATikiShredDemo::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Kept.Remove(this);
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}
