// monke_island

#include "Creatures/TikiRagdollCorpse.h"
#include "Boats/BoatActor.h"
#include "Creatures/TikiBody.h"
#include "Creatures/TikiCorpse.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiRules.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TimerManager.h"

namespace
{
	const FName RagdollProfile(TEXT("Ragdoll"));

	/** True when the corpse lies in a boat: its floor is below the sea outside, the water does not wash it away */
	bool IsInBoat(const UWorld* World, const FVector& Location)
	{
		for (TActorIterator<ABoatActor> It(World); It; ++It)
		{
			if (It->GetComponentsBoundingBox().ExpandBy(30.0).IsInside(Location))
			{
				return true;
			}
		}
		return false;
	}
}

ATikiRagdollCorpse::ATikiRagdollCorpse()
{
	PrimaryActorTick.bCanEverTick = false;

	// the server simulates, clients follow the replicated root body, like the other free objects (cahier §5)
	bReplicates = true;
	SetReplicateMovement(true);

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetCollisionProfileName(RagdollProfile);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->bBlendPhysics = true;
	SetRootComponent(Body);

	Held = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Held"));
	Held->SetupAttachment(Body);
	Held->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

ATikiRagdollCorpse* ATikiRagdollCorpse::Spawn(UWorld* World, UTikiRules* InRules, const FTikiBodySnapshot& Snap, const USkeletalMeshComponent* MovingFrom)
{
	if (!World || !InRules || !Snap.Mesh || !Snap.Mesh->GetPhysicsAsset())
	{
		return nullptr;
	}
	const FTransform At(Snap.MeshWorld.GetRotation(), Snap.MeshWorld.GetLocation());
	ATikiRagdollCorpse* Corpse = World->SpawnActorDeferred<ATikiRagdollCorpse>(ATikiRagdollCorpse::StaticClass(), At, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Corpse)
	{
		return nullptr;
	}
	Corpse->Rules = InRules;
	Corpse->BodyMesh = Snap.Mesh;
	Corpse->BodyMaterials.Append(Snap.Materials);
	Corpse->HeldMesh = Snap.HeldMesh;
	Corpse->HeldMaterials.Append(Snap.HeldMaterials);
	Corpse->Pose = Snap.Pose;
	Corpse->CutBones = Snap.CutBones;
	Corpse->MassKg = InRules->CorpseMassKg;
	// the bodies keep falling as they fell: same speed, bone by bone
	if (MovingFrom)
	{
		const FReferenceSkeleton& Skeleton = Snap.Mesh->GetRefSkeleton();
		for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
		{
			const FName Bone = Skeleton.GetBoneName(Index);
			if (MovingFrom->GetBodyInstance(Bone))
			{
				Corpse->StartVelocities.Add(Bone, MovingFrom->GetPhysicsLinearVelocity(Bone));
			}
		}
	}
	Corpse->FinishSpawning(At);
	return Corpse;
}

void ATikiRagdollCorpse::BeginPlay()
{
	Super::BeginPlay();

	ApplyLook();
	if (!HasAuthority() || !Rules)
	{
		return;
	}
	GetWorldTimerManager().SetTimer(FloodTimer, this, &ATikiRagdollCorpse::CheckFlood, Rules->CorpseFloodCheckInterval, true, Rules->CorpseFloodCheckInterval);
	if (Rules->CorpseLifetime > 0.0f)
	{
		SetLifeSpan(Rules->CorpseLifetime);
	}
	ATikiCorpse::KeepCount(GetWorld(), Rules->CorpseMaxCount);
}

void ATikiRagdollCorpse::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FloodTimer);
	Super::EndPlay(EndPlayReason);
}

void ATikiRagdollCorpse::ApplyLook()
{
	if (!BodyMesh)
	{
		return;
	}
	Body->SetSkeletalMeshAsset(BodyMesh);
	for (int32 Slot = 0; Slot < BodyMaterials.Num(); ++Slot)
	{
		if (BodyMaterials[Slot])
		{
			Body->SetMaterial(Slot, BodyMaterials[Slot]);
		}
	}
	if (HeldMesh)
	{
		Held->SetSkeletalMeshAsset(HeldMesh);
		for (int32 Slot = 0; Slot < HeldMaterials.Num(); ++Slot)
		{
			if (HeldMaterials[Slot])
			{
				Held->SetMaterial(Slot, HeldMaterials[Slot]);
			}
		}
		Held->SetLeaderPoseComponent(Body);
	}
	else
	{
		Held->SetVisibility(false);
	}

	// ragdoll; the players' grab traces on the visibility channel, which the engine Ragdoll profile ignores
	Body->SetCollisionProfileName(RagdollProfile);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetAllBodiesSimulatePhysics(true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();

	// every body where its bone was when the creature fell
	const FReferenceSkeleton& Skeleton = BodyMesh->GetRefSkeleton();
	if (Pose.Num() == Skeleton.GetNum())
	{
		TArray<int32> Parents;
		for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
		{
			Parents.Add(Skeleton.GetParentIndex(Index));
		}
		const TArray<FTransform> Space = TikiBody::ComponentSpace(Pose, Parents);
		const FTransform World = Body->GetComponentTransform();
		for (int32 Bone = 0; Bone < Space.Num(); ++Bone)
		{
			if (FBodyInstance* Instance = Body->GetBodyInstance(Skeleton.GetBoneName(Bone)))
			{
				Instance->SetBodyTransform(Space[Bone] * World, ETeleportType::TeleportPhysics);
			}
		}
	}
	for (const TPair<FName, FVector>& Start : StartVelocities)
	{
		Body->SetPhysicsLinearVelocity(Start.Value, false, Start.Key);
	}
	StartVelocities.Reset();

	// the bones cut off before death go with their bodies
	for (const FName& Cut : CutBones)
	{
		Body->HideBoneByName(Cut, PBO_Term);
	}

	// heavy as a body, spread over its bodies
	const float Current = Body->GetMass();
	if (Current > KINDA_SMALL_NUMBER)
	{
		Body->SetAllMassScale(MassKg / Current);
	}
}

void ATikiRagdollCorpse::CheckFlood()
{
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!Water || !Rules)
	{
		return;
	}
	const FBox Bounds = Body->Bounds.GetBox();
	const float Surface = Water->GetWaterSurfaceAt(Bounds.GetCenter());
	if (Surface - Bounds.Min.Z > Rules->CorpseFloodDepth && !IsInBoat(GetWorld(), Bounds.GetCenter()))
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s washed away by the water (%.0f cm deep)"), *GetName(), Surface - Bounds.Min.Z);
		Destroy();
	}
}

float ATikiRagdollCorpse::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Taken = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (!HasAuthority() || IsActorBeingDestroyed() || !ATikiCreature::IsCutBlow(DamageCauser))
	{
		return Taken;
	}
	FVector Direction = FVector::ZeroVector;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		Direction = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection;
	}
	else if (DamageCauser)
	{
		Direction = GetActorLocation() - DamageCauser->GetActorLocation();
	}
	CutInTwo(Direction.GetSafeNormal2D());
	return Taken;
}

void ATikiRagdollCorpse::CutInTwo(const FVector& Direction)
{
	if (!HasAuthority() || IsActorBeingDestroyed() || !Rules || !BodyMesh || BodyMesh->GetRefSkeleton().FindBoneIndex(Rules->HalfCutBone) == INDEX_NONE)
	{
		return;
	}
	// the halves are rigid pieces in the pose the ragdoll has now
	const FTikiBodySnapshot Snap = ATikiCorpse::Snapshot(Body, HeldMesh ? Held.Get() : nullptr, CutBones);
	const FVector Moving = Body->GetPhysicsLinearVelocity();
	const FVector Blow = Direction.IsNearlyZero() ? GetActorForwardVector() : Direction;
	UTikiRules* KeptRules = Rules;
	UWorld* World = GetWorld();
	Destroy();
	ATikiCorpse::SpawnPart(World, KeptRules, Snap, ETikiBodyPart::Upper, KeptRules->HalfCutBone,
		Moving + Blow * KeptRules->CutFlingSpeed + FVector(0.0f, 0.0f, KeptRules->CutFlingSpeed * 0.5f));
	ATikiCorpse::SpawnPart(World, KeptRules, Snap, ETikiBodyPart::Lower, KeptRules->HalfCutBone, Moving - Blow * KeptRules->CutFlingSpeed * 0.3f);
	UE_LOG(Logmonke_island, Log, TEXT("%s cut in two"), *GetName());
}

void ATikiRagdollCorpse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, BodyMesh, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, BodyMaterials, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, HeldMesh, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, HeldMaterials, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, Pose, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, CutBones, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, MassKg, COND_InitialOnly);
}
