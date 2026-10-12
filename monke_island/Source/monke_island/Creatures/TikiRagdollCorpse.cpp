// monke_island

#include "Creatures/TikiRagdollCorpse.h"
#include "Boats/BoatActor.h"
#include "Creatures/TikiBody.h"
#include "Creatures/TikiCorpse.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "Items/NetRestComponent.h"
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
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TimerManager.h"

namespace
{
	const FName RagdollProfile(TEXT("Ragdoll"));

	/** True when the corpse lies in a boat: its floor is below the sea outside, the water does not wash it away */
	bool IsRagdollInBoat(const UWorld* World, const FVector& Location)
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

	// laid down and asleep it stops replicating; in the barque or on the galleon's deck it rides glued (T-312, R-28)
	NetRest = CreateDefaultSubobject<UNetRestComponent>(TEXT("NetRest"));
}

ATikiRagdollCorpse* ATikiRagdollCorpse::Spawn(UWorld* World, UTikiRules* InRules, const FTikiBodySnapshot& Snap, const USkeletalMeshComponent* MovingFrom, bool bInFrozen)
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
	Corpse->DismemberSet = Snap.DismemberSet;
	Corpse->MassKg = InRules->CorpseMassKg;
	Corpse->bFrozen = bInFrozen;
	// the bodies keep falling as they fell: same speed, bone by bone, bounded (a dead body does not fly, T-151)
	if (MovingFrom && !bInFrozen)
	{
		const FReferenceSkeleton& Skeleton = Snap.Mesh->GetRefSkeleton();
		for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
		{
			const FName Bone = Skeleton.GetBoneName(Index);
			if (MovingFrom->GetBodyInstance(Bone))
			{
				Corpse->StartVelocities.Add(Bone, TikiMath::BoundedVelocity(MovingFrom->GetPhysicsLinearVelocity(Bone), InRules->CorpseMaxSpeed));
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
	NetRest->SetAwakeNetUpdateFrequency(Rules->CorpseNetUpdateFrequency);
	// a frozen pose lies down at once; a ragdoll moves a little, then lies down (every machine ends on the server pose)
	if (bFrozen)
	{
		Settle();
	}
	else
	{
		GetWorldTimerManager().SetTimer(SettleTimer, this, &ATikiRagdollCorpse::Settle, Rules->CorpseSettleTime, false);
	}
}

void ATikiRagdollCorpse::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FloodTimer);
	GetWorldTimerManager().ClearTimer(SettleTimer);
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

	// calm: damped, joints held together, no jump out of the ground (T-151)
	CalmRagdoll(Body, Rules);
	if (bFrozen)
	{
		Body->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		SnapToGround(Body, Rules);
		Body->PutAllRigidBodiesToSleep();
	}
	// the server laid it down before this machine built it
	if (!SettledBodies.IsEmpty())
	{
		OnRep_SettledBodies();
	}
}

void ATikiRagdollCorpse::CalmRagdoll(USkeletalMeshComponent* Mesh, const UTikiRules* InRules)
{
	if (!Mesh || !InRules)
	{
		return;
	}
	for (FBodyInstance* Instance : Mesh->Bodies)
	{
		if (!Instance)
		{
			continue;
		}
		Instance->LinearDamping = InRules->CorpseLinearDamping;
		Instance->AngularDamping = InRules->CorpseAngularDamping;
		Instance->UpdateDampingProperties();
		Instance->SetMaxDepenetrationVelocity(InRules->CorpseMaxDepenetrationSpeed);
		Instance->SetMaxAngularVelocityInRadians(FMath::DegreesToRadians(InRules->CorpseMaxSpin), false);
	}
	if (InRules->bCorpseLockJoints)
	{
		// the bones stay joined: no linear freedom, and the projection pulls back a joint that opens anyway
		for (FConstraintInstance* Constraint : Mesh->Constraints)
		{
			if (Constraint)
			{
				Constraint->SetLinearLimits(LCM_Locked, LCM_Locked, LCM_Locked, 0.0f);
				Constraint->SetProjectionParams(true, 1.0f, 0.5f, InRules->CorpseJointLinearTolerance, InRules->CorpseJointAngularTolerance);
			}
		}
	}

	// a body without a joint to its parent drifts off the others: the physics asset must be fixed (warned once per asset)
	static TSet<FName> Warned;
	const UPhysicsAsset* Asset = Mesh->GetPhysicsAsset();
	const USkeletalMesh* SkelMesh = Mesh->GetSkeletalMeshAsset();
	if (!Asset || !SkelMesh || Warned.Contains(Asset->GetFName()))
	{
		return;
	}
	Warned.Add(Asset->GetFName());
	const FReferenceSkeleton& Skeleton = SkelMesh->GetRefSkeleton();
	const FBodyInstance* RootBody = Mesh->GetBodyInstance();
	int32 Free = 0;
	for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
	{
		const FName Bone = Skeleton.GetBoneName(Index);
		const FBodyInstance* Instance = Mesh->GetBodyInstance(Bone);
		if (!Instance || Instance == RootBody)
		{
			continue;
		}
		bool bJoined = false;
		for (const FConstraintInstance* Constraint : Mesh->Constraints)
		{
			bJoined = bJoined || (Constraint && Constraint->GetChildBoneName() == Bone);
		}
		if (!bJoined)
		{
			++Free;
			UE_LOG(Logmonke_island, Warning, TEXT("%s: body %s has no constraint to its parent, it can drift off the corpse"), *Asset->GetName(), *Bone.ToString());
		}
	}
	if (Free > 0)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: %d unconstrained bodies besides the root (fix the physics asset, T-151)"), *Asset->GetName(), Free);
	}
}

void ATikiRagdollCorpse::BoundSpeeds(USkeletalMeshComponent* Mesh, float MaxSpeed)
{
	if (!Mesh)
	{
		return;
	}
	for (FBodyInstance* Instance : Mesh->Bodies)
	{
		if (Instance && Instance->IsInstanceSimulatingPhysics())
		{
			const FVector Velocity = Instance->GetUnrealWorldVelocity();
			if (Velocity.SizeSquared() > FMath::Square(static_cast<double>(MaxSpeed)))
			{
				Instance->SetLinearVelocity(TikiMath::BoundedVelocity(Velocity, MaxSpeed), false, false);
			}
		}
	}
}

bool ATikiRagdollCorpse::SnapToGround(USkeletalMeshComponent* Mesh, const UTikiRules* InRules)
{
	UWorld* World = Mesh ? Mesh->GetWorld() : nullptr;
	if (!World || !InRules)
	{
		return false;
	}
	// the lowest body, and the ground right below it
	FBox Lowest(ForceInit);
	for (const FBodyInstance* Instance : Mesh->Bodies)
	{
		if (!Instance || !Instance->IsValidBodyInstance())
		{
			continue;
		}
		const FBox Box = Instance->GetBodyBounds();
		if (Box.IsValid && (!Lowest.IsValid || Box.Min.Z < Lowest.Min.Z))
		{
			Lowest = Box;
		}
	}
	if (!Lowest.IsValid)
	{
		return false;
	}
	// the ground under the lowest body (the corpse itself and the water planes left out, T-151)
	const FVector Point(Lowest.GetCenter().X, Lowest.GetCenter().Y, Lowest.Min.Z);
	float GroundZ = 0.0f;
	if (!ATikiCorpse::FindGround(World, Point, static_cast<float>(Lowest.Max.Z - Lowest.Min.Z) + InRules->CorpseGroundTolerance,
		InRules->CorpseGroundTraceHeight, Mesh->GetOwner(), GroundZ))
	{
		return false;
	}
	const float Offset = TikiMath::GroundSnapOffset(static_cast<float>(Lowest.Min.Z), GroundZ, InRules->CorpseGroundTolerance);
	if (Offset == 0.0f)
	{
		return false;
	}
	for (FBodyInstance* Instance : Mesh->Bodies)
	{
		if (Instance && Instance->IsValidBodyInstance())
		{
			FTransform Transform = Instance->GetUnrealWorldTransform();
			Transform.AddToTranslation(FVector(0.0f, 0.0f, Offset));
			Instance->SetBodyTransform(Transform, ETeleportType::TeleportPhysics, false);
		}
	}
	return true;
}

void ATikiRagdollCorpse::Settle()
{
	if (!HasAuthority() || !BodyMesh || IsActorBeingDestroyed())
	{
		return;
	}
	SnapToGround(Body, Rules);
	Body->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	Body->PutAllRigidBodiesToSleep();
	TArray<FTransform> Bodies;
	const FReferenceSkeleton& Skeleton = BodyMesh->GetRefSkeleton();
	for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
	{
		if (const FBodyInstance* Instance = Body->GetBodyInstance(Skeleton.GetBoneName(Index)))
		{
			Bodies.Add(Instance->GetUnrealWorldTransform());
		}
	}
	SettledBodies = MoveTemp(Bodies);
	ForceNetUpdate();
}

void ATikiRagdollCorpse::OnRep_SettledBodies()
{
	if (!BodyMesh || Body->GetSkeletalMeshAsset() != BodyMesh)
	{
		// the look is not built yet: ApplyLook applies the pose
		return;
	}
	const FReferenceSkeleton& Skeleton = BodyMesh->GetRefSkeleton();
	int32 Next = 0;
	for (int32 Index = 0; Index < Skeleton.GetNum() && Next < SettledBodies.Num(); ++Index)
	{
		if (FBodyInstance* Instance = Body->GetBodyInstance(Skeleton.GetBoneName(Index)))
		{
			Instance->SetBodyTransform(SettledBodies[Next++], ETeleportType::TeleportPhysics, false);
		}
	}
	Body->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	Body->PutAllRigidBodiesToSleep();
}

void ATikiRagdollCorpse::BoundSpeedsAfterHit()
{
	if (!Rules || IsActorBeingDestroyed())
	{
		return;
	}
	BoundSpeeds(Body, Rules->CorpseMaxSpeed);
	// pushed: it lies down again once it stops
	GetWorldTimerManager().SetTimer(SettleTimer, this, &ATikiRagdollCorpse::Settle, Rules->CorpseSettleTime, false);
}

void ATikiRagdollCorpse::CheckFlood()
{
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!Water || !Rules)
	{
		return;
	}
	// the physics bodies themselves (the component bounds may be stale or wider than the body), and never deeper
	// than the ground under the corpse: a sea lower than the dry ground does not cover it (T-151)
	FBox Bounds(ForceInit);
	for (const FBodyInstance* Instance : Body->Bodies)
	{
		if (Instance && Instance->IsValidBodyInstance())
		{
			Bounds += Instance->GetBodyBounds();
		}
	}
	if (!Bounds.IsValid)
	{
		return;
	}
	const FVector Bottom(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
	float GroundZ = 0.0f;
	const bool bGround = ATikiCorpse::FindGround(GetWorld(), Bottom, static_cast<float>(Bounds.Max.Z - Bounds.Min.Z), Rules->CorpseGroundTraceHeight, this, GroundZ);
	const float Depth = TikiMath::CorpseWaterDepth(Water->GetWaterSurfaceAt(Bottom), static_cast<float>(Bounds.Min.Z), bGround, GroundZ);
	if (TikiMath::IsCorpseWashedAway(Depth, Rules->CorpseFloodDepth) && !IsRagdollInBoat(GetWorld(), Bounds.GetCenter()))
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s washed away by the water (%.0f cm deep)"), *GetName(), Depth);
		Destroy();
		return;
	}
	// never asleep in the air (dropped by a player, the ground moved under it): laid down again
	if (!Body->IsAnyRigidBodyAwake() && !GetWorldTimerManager().IsTimerActive(SettleTimer) && SnapToGround(Body, Rules))
	{
		Settle();
	}
}

float ATikiRagdollCorpse::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Taken = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (HasAuthority() && !IsActorBeingDestroyed() && Rules)
	{
		// the weapon pushes the hit body after the damage (a musket ball: 40000 kg cm/s): bounded on the next tick
		GetWorldTimerManager().SetTimerForNextTick(this, &ATikiRagdollCorpse::BoundSpeedsAfterHit);
		// pushed: the clients follow it again at once, not at the next look of the network rest (T-312)
		NetRest->WakeNet();
	}
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
	const FTikiBodySnapshot Snap = ATikiCorpse::Snapshot(Body, HeldMesh ? Held.Get() : nullptr, CutBones, DismemberSet);
	// the halves drop where the body lay, nudged apart along the blow: no speed of the ragdoll inherited (T-155)
	const FVector Blow = Direction.IsNearlyZero() ? GetActorForwardVector() : Direction;
	UTikiRules* KeptRules = Rules;
	UWorld* World = GetWorld();
	Destroy();
	ATikiCorpse::SpawnPart(World, KeptRules, Snap, ETikiBodyPart::Upper, KeptRules->HalfCutBone, Blow * KeptRules->CutFlingSpeed);
	ATikiCorpse::SpawnPart(World, KeptRules, Snap, ETikiBodyPart::Lower, KeptRules->HalfCutBone, -Blow * KeptRules->CutFlingSpeed * 0.3f);
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
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, bFrozen, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiRagdollCorpse, Rules, COND_InitialOnly);
	DOREPLIFETIME(ATikiRagdollCorpse, SettledBodies);
}
