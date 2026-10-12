// monke_island

#include "Creatures/TikiCorpse.h"
#include "Boats/BoatActor.h"
#include "Creatures/TikiBlood.h"
#include "Creatures/TikiCorpseCollisionComponent.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiDismember.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRagdollCorpse.h"
#include "Creatures/TikiRules.h"
#include "Items/NetRestComponent.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "TimerManager.h"

namespace
{
	/** Parent index of every bone of a reference skeleton */
	TArray<int32> ParentsOf(const FReferenceSkeleton& Skeleton)
	{
		TArray<int32> Parents;
		Parents.Reserve(Skeleton.GetNum());
		for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
		{
			Parents.Add(Skeleton.GetParentIndex(Index));
		}
		return Parents;
	}

	/** Bone indices of names, INDEX_NONE for the unknown ones */
	TArray<int32> IndicesOf(const FReferenceSkeleton& Skeleton, const TArray<FName>& Names)
	{
		TArray<int32> Indices;
		for (const FName& Name : Names)
		{
			Indices.Add(Skeleton.FindBoneIndex(Name));
		}
		return Indices;
	}

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

	/** Pose of a snapshot, or the reference pose when it does not match the skeleton */
	const TArray<FTransform>& PoseOf(const FReferenceSkeleton& Skeleton, const TArray<FTransform>& Pose)
	{
		return Pose.Num() == Skeleton.GetNum() ? Pose : Skeleton.GetRefBonePose();
	}
}

ATikiCorpse::ATikiCorpse()
{
	PrimaryActorTick.bCanEverTick = false;

	// the server simulates, clients follow the replicated movement, like the other free objects (cahier §5)
	bReplicates = true;
	SetReplicateMovement(true);

	// the shapes come with the pose (ApplyLook): a rigid body, no joint to stretch (T-151)
	Body = CreateDefaultSubobject<UTikiCorpseCollisionComponent>(TEXT("Body"));
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Body->SetSimulatePhysics(true);
	Body->SetLinearDamping(0.2f);
	Body->SetAngularDamping(0.6f);
	// the network rest hears at once when the body wakes up (T-312)
	Body->BodyInstance.bGenerateWakeEvents = true;
	SetRootComponent(Body);

	// asleep on the ground it stops replicating; in the barque or on the galleon's deck it rides glued (T-312, R-28)
	NetRest = CreateDefaultSubobject<UNetRestComponent>(TEXT("NetRest"));

	Mesh = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Body);
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Held = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Held"));
	Held->SetupAttachment(Mesh);
	Held->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Cap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cap"));
	Cap->SetupAttachment(Mesh);
	Cap->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Cap->SetVisibility(false);
}

FTikiBodySnapshot ATikiCorpse::Snapshot(USkeletalMeshComponent* BodyMesh, USkeletalMeshComponent* InHeld, const TArray<FName>& InCutBones,
	UTikiDismemberSet* InDismemberSet)
{
	FTikiBodySnapshot Snap;
	if (!BodyMesh)
	{
		return Snap;
	}
	Snap.DismemberSet = InDismemberSet;
	Snap.Mesh = BodyMesh->GetSkeletalMeshAsset();
	Snap.Materials = BodyMesh->GetMaterials();
	// from the component space bones, which also hold a ragdoll (the bone space ones keep the animation)
	const TArray<FTransform>& Space = BodyMesh->GetComponentSpaceTransforms();
	if (Snap.Mesh && Space.Num() == Snap.Mesh->GetRefSkeleton().GetNum())
	{
		const FReferenceSkeleton& Skeleton = Snap.Mesh->GetRefSkeleton();
		Snap.Pose.SetNum(Space.Num());
		for (int32 Index = 0; Index < Space.Num(); ++Index)
		{
			const int32 Parent = Skeleton.GetParentIndex(Index);
			Snap.Pose[Index] = Parent >= 0 ? Space[Index].GetRelativeTransform(Space[Parent]) : Space[Index];
		}
	}
	else
	{
		Snap.Pose = BodyMesh->GetBoneSpaceTransforms();
	}
	Snap.MeshWorld = BodyMesh->GetComponentTransform();
	Snap.CutBones = InCutBones;
	if (InHeld && InHeld->IsVisible() && InHeld->GetSkeletalMeshAsset())
	{
		Snap.HeldMesh = InHeld->GetSkeletalMeshAsset();
		Snap.HeldMaterials = InHeld->GetMaterials();
	}
	return Snap;
}

ATikiCorpse* ATikiCorpse::SpawnPart(UWorld* World, UTikiRules* InRules, const FTikiBodySnapshot& Snap, ETikiBodyPart InPart, FName InPartBone, const FVector& Velocity)
{
	if (!World || !InRules || !Snap.Mesh)
	{
		return nullptr;
	}
	const FReferenceSkeleton& Skeleton = Snap.Mesh->GetRefSkeleton();
	const int32 CutIndex = InPartBone.IsNone() ? INDEX_NONE : Skeleton.FindBoneIndex(InPartBone);
	if (InPart != ETikiBodyPart::Whole && CutIndex == INDEX_NONE)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Tiki corpse: no bone %s in %s, the part cannot be cut"), *InPartBone.ToString(), *GetNameSafe(Snap.Mesh));
		return nullptr;
	}

	// the box around the joints the part keeps: the collision of a mesh without physics asset
	const TArray<int32> Parents = ParentsOf(Skeleton);
	const TArray<int32> Hidden = IndicesOf(Skeleton, Snap.CutBones);
	const float Margin = InPart == ETikiBodyPart::Limb ? InRules->CorpseBoxMargin * 0.4f : InRules->CorpseBoxMargin;
	const TArray<FTransform>& Local = PoseOf(Skeleton, Snap.Pose);
	FBox Box = TikiBody::PartBounds(Local, Parents, InPart, CutIndex, Hidden, Snap.MeshWorld, Margin);
	if (!Box.IsValid)
	{
		return nullptr;
	}
	// a lying body has its bones a few centimetres above the ground: the box must not start under it
	Box.Min.Z = FMath::Min(Box.Max.Z - 10.0, Box.Min.Z + Margin - 4.0);

	// the collision is the physics asset in the pose: the corpse rests where the visible body touches the ground (T-151)
	const FKAggregateGeom WorldGeom = PartGeometry(Snap.Mesh, Local, InPart, InPartBone, Snap.CutBones, Snap.MeshWorld);
	FVector Lowest(Box.GetCenter().X, Box.GetCenter().Y, Box.Min.Z);
	FBox Extent = Box;
	if (WorldGeom.GetElementCount() > 0)
	{
		TikiBody::LowestPoint(WorldGeom, FTransform::Identity, Lowest);
		Extent = WorldGeom.CalcAABB(FTransform::Identity);
	}
	// a frozen body (no speed) stays exactly where the creature's mesh is, same pose, same frame: no pop. If its
	// collision floats more than the dead zone, it is lowered slowly afterwards (never lifted). A flung piece is only
	// lifted out of the ground (it falls on its own)
	// a cut piece always falls by its weight (T-155): only a whole body without speed stays frozen
	const bool bInFrozen = InPart == ETikiBodyPart::Whole && Velocity.IsNearlyZero();
	FVector Lift = FVector::ZeroVector;
	float Settle = 0.0f;
	float GroundZ = 0.0f;
	if (FindGround(World, Lowest, static_cast<float>(Extent.Max.Z - Lowest.Z) + InRules->CorpseGroundTolerance, InRules->CorpseGroundTraceHeight, nullptr, GroundZ))
	{
		if (bInFrozen)
		{
			Settle = TikiMath::FrozenSettleOffset(static_cast<float>(Lowest.Z), GroundZ, InRules->CorpseSettleDeadZone);
		}
		else
		{
			Lift.Z = TikiMath::CorpseRestOffset(static_cast<float>(Lowest.Z), GroundZ, InRules->CorpseRestClearance, false);
		}
	}
	FTransform MeshWorld = Snap.MeshWorld;
	MeshWorld.AddToTranslation(Lift);
	Box = Box.ShiftBy(Lift);

	const FTransform BoxWorld(FQuat::Identity, Extent.GetCenter() + Lift);
	ATikiCorpse* Corpse = World->SpawnActorDeferred<ATikiCorpse>(ATikiCorpse::StaticClass(), BoxWorld, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Corpse)
	{
		return nullptr;
	}
	Corpse->Rules = InRules;
	Corpse->BodyMesh = Snap.Mesh;
	Corpse->BodyMaterials.Append(Snap.Materials);
	Corpse->HeldMesh = Snap.HeldMesh;
	Corpse->HeldMaterials.Append(Snap.HeldMaterials);
	Corpse->Pose = PoseOf(Skeleton, Snap.Pose);
	Corpse->MeshRelative = MeshWorld.GetRelativeTransform(BoxWorld);
	Corpse->Part = InPart;
	Corpse->PartBone = InPartBone;
	Corpse->CutBones = Snap.CutBones;
	Corpse->DismemberSet = Snap.DismemberSet;
	Corpse->BoxExtent = Box.GetExtent().ComponentMax(FVector(8.0));
	Corpse->BoxOffset = Box.GetCenter() - BoxWorld.GetLocation();
	Corpse->MassKg = InPart == ETikiBodyPart::Whole ? InRules->CorpseMassKg
		: InPart == ETikiBodyPart::Limb ? InRules->CorpseLimbMassKg : InRules->CorpseHalfMassKg;
	Corpse->CapMesh = InRules->CutCapMesh;
	Corpse->CapMaterial = InRules->CutCapMaterial;
	Corpse->CapRadius = InPart == ETikiBodyPart::Limb ? InRules->LimbCutRadius : InRules->BodyCutRadius;
	const AGameStateBase* GameState = World->GetGameState();
	Corpse->SpawnServerTime = static_cast<float>(GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds());
	Corpse->bFrozen = bInFrozen;
	Corpse->SettleLeft = Settle;
	Corpse->FinishSpawning(BoxWorld);
	// a piece leaves flat along the blow, slowly, whatever the caller, the ragdoll or the shot gave (T-155)
	const FVector Start = InPart == ETikiBodyPart::Whole ? TikiMath::CorpseSpawnVelocity(Velocity, InRules->CorpseMaxSpeed, bInFrozen)
		: TikiMath::CutPieceVelocity(Velocity, FMath::Min(InRules->CutPieceMaxSpeed, InRules->CorpseMaxSpeed));
	if (!Start.IsNearlyZero())
	{
		Corpse->Body->SetPhysicsLinearVelocity(Start);
	}
	return Corpse;
}

void ATikiCorpse::BeginPlay()
{
	Super::BeginPlay();

	ApplyLook();
	if (Rules)
	{
		GetWorldTimerManager().SetTimer(FreshTimer, this, &ATikiCorpse::CheckFresh, Rules->CorpseCheckInterval, true, Rules->CorpseCheckInterval);
	}
	if (!HasAuthority())
	{
		return;
	}
	// a frozen body floating above the ground goes down slowly, held (no physics) until it touches it
	if (bFrozen && SettleLeft < 0.0f && Rules)
	{
		Body->SetSimulatePhysics(false);
		GetWorldTimerManager().SetTimer(SettleTimer, this, &ATikiCorpse::SettleStep, SettleInterval, true);
	}
	if (Rules)
	{
		GetWorldTimerManager().SetTimer(FloodTimer, this, &ATikiCorpse::CheckFlood, Rules->CorpseFloodCheckInterval, true, Rules->CorpseFloodCheckInterval);
		if (Rules->CorpseLifetime > 0.0f)
		{
			SetLifeSpan(Rules->CorpseLifetime);
		}
		KeepCount(GetWorld(), Rules->CorpseMaxCount);
		NetRest->SetAwakeNetUpdateFrequency(Rules->CorpseNetUpdateFrequency);
	}
}

void ATikiCorpse::CheckFresh()
{
	if (!Rules)
	{
		GetWorldTimerManager().ClearTimer(FreshTimer);
		return;
	}
	const float Age = GetGameTimeSinceCreation();
	// apart from the other bodies: it collides with them again (a corpse in the boat, another one dropped on it)
	if (bGhost && !OverlapsOtherBody())
	{
		Body->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		bGhost = false;
	}
	// a piece lying still goes to sleep, like a frozen body (T-151); the server simulates, the clients follow
	if (HasAuthority() && !bRested && !bFrozen && Part != ETikiBodyPart::Whole && Body->IsSimulatingPhysics()
		&& TikiMath::IsPieceAtRest(static_cast<float>(Body->GetPhysicsLinearVelocity().Size()), Age, Rules->CutPieceRestSpeed, Rules->CutPieceMinFallTime))
	{
		Sleep();
		bRested = true;
	}
	const bool bRestDone = bRested || bFrozen || Part == ETikiBodyPart::Whole || !HasAuthority();
	if ((!bGhost && bRestDone) || Age >= Rules->CutPieceSettleTimeout)
	{
		GetWorldTimerManager().ClearTimer(FreshTimer);
	}
}

bool ATikiCorpse::OverlapsOtherBody() const
{
	TArray<FOverlapResult> Overlaps;
	FComponentQueryParams Params(SCENE_QUERY_STAT(TikiCorpseOverlap), this);
	GetWorld()->ComponentOverlapMulti(Overlaps, Body, Body->GetComponentLocation(), Body->GetComponentQuat(), Params, FCollisionObjectQueryParams(ECC_PhysicsBody));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const UPrimitiveComponent* Other = Overlap.GetComponent();
		if (Other && Other != Body && Other->IsSimulatingPhysics())
		{
			return true;
		}
	}
	return false;
}

void ATikiCorpse::SettleStep()
{
	const float Step = Rules ? TikiMath::SettleStep(SettleLeft, Rules->CorpseSettleSpeed, SettleInterval) : SettleLeft;
	SettleLeft -= Step;
	Body->SetWorldLocation(Body->GetComponentLocation() + FVector(0.0, 0.0, Step), false, nullptr, ETeleportType::TeleportPhysics);
	if (SettleLeft >= -KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().ClearTimer(SettleTimer);
		Sleep();
	}
}

void ATikiCorpse::Sleep()
{
	// simulating (a player can grab it) but asleep, at rest: nothing moves it until something wakes it
	Body->SetSimulatePhysics(true);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	Body->PutRigidBodyToSleep();
}

void ATikiCorpse::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SettleTimer);
	GetWorldTimerManager().ClearTimer(FloodTimer);
	GetWorldTimerManager().ClearTimer(FreshTimer);
	Super::EndPlay(EndPlayReason);
}

void ATikiCorpse::ApplyLook()
{
	// the collision: the physics asset bodies in the pose, relative to the root (every machine builds the same)
	FKAggregateGeom Geom = BodyMesh ? PartGeometry(BodyMesh, Pose, Part, PartBone, CutBones, MeshRelative) : FKAggregateGeom();
	if (Geom.GetElementCount() == 0)
	{
		FKBoxElem Box(static_cast<float>(BoxExtent.X * 2.0), static_cast<float>(BoxExtent.Y * 2.0), static_cast<float>(BoxExtent.Z * 2.0));
		Box.Center = BoxOffset;
		Geom.BoxElems.Add(Box);
	}
	// a frozen body starts asleep: no push out of the ground, no fall, no jump (T-151)
	Body->BodyInstance.bStartAwake = !bFrozen;
	Body->SetGeometry(Geom);
	// the players and the creatures walk through a corpse unless the rules say otherwise (no sticking on it); the
	// grab traces (visibility) still find it
	Body->SetCollisionResponseToChannel(ECC_Pawn, Rules && Rules->bCorpseBlocksPawns ? ECR_Block : ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// fresh: through the other bodies until apart from them, so the pieces of a cut never push each other (T-155)
	Body->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	bGhost = true;
	if (Rules)
	{
		Body->SetBlowSpeedLimit(Rules->CorpseBlowMaxSpeed);
		Body->SetLinearDamping(Rules->CorpseLinearDamping);
		Body->SetAngularDamping(Rules->CorpseAngularDamping);
		if (FBodyInstance* Instance = Body->GetBodyInstance())
		{
			Instance->SetMaxDepenetrationVelocity(Rules->CorpseMaxDepenetrationSpeed);
		}
	}
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	if (bFrozen)
	{
		Sleep();
	}
	if (!BodyMesh)
	{
		return;
	}

	Mesh->SetSkinnedAssetAndUpdate(BodyMesh, true);
	for (int32 Slot = 0; Slot < BodyMaterials.Num(); ++Slot)
	{
		if (BodyMaterials[Slot])
		{
			Mesh->SetMaterial(Slot, BodyMaterials[Slot]);
		}
	}
	Mesh->SetRelativeTransform(MeshRelative);

	const FReferenceSkeleton& Skeleton = BodyMesh->GetRefSkeleton();
	const TArray<int32> Parents = ParentsOf(Skeleton);
	const TArray<FTransform>& Local = PoseOf(Skeleton, Pose);
	const int32 CutIndex = PartBone.IsNone() ? INDEX_NONE : Skeleton.FindBoneIndex(PartBone);
	const TArray<FTransform> Masked = TikiBody::MaskPose(Local, Parents, Part, CutIndex, IndicesOf(Skeleton, CutBones));
	if (Mesh->BoneSpaceTransforms.Num() == Masked.Num())
	{
		Mesh->BoneSpaceTransforms = Masked;
		Mesh->RefreshBoneTransforms();
	}
	// the closed modules, when the set can close every cut of this part: the faces are modelled, no disc (T-155)
	const bool bModules = ApplyModules(Local);

	// the held item follows the masked pose: it goes with the part that keeps the hand
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
		Held->SetLeaderPoseComponent(Mesh);
	}
	else
	{
		Held->SetVisibility(false);
	}

	if (Part == ETikiBodyPart::Whole || !Skeleton.IsValidIndex(CutIndex))
	{
		SpawnBlood(FVector::ZeroVector, FVector::ZeroVector, NAME_None);
		return;
	}
	{
		const TArray<FTransform> Space = TikiBody::ComponentSpace(Local, Parents);
		const FVector Joint = Space[CutIndex].GetLocation();
		FVector Along = Space[CutIndex].GetRotation().GetAxisX();
		for (int32 Index = CutIndex + 1; Index < Space.Num(); ++Index)
		{
			if (Parents[Index] == CutIndex && !Space[Index].GetLocation().Equals(Joint, 0.1))
			{
				Along = (Space[Index].GetLocation() - Joint).GetSafeNormal();
				break;
			}
		}
		// the face looks out of the part: down the bone for the part that keeps it, up for the lower half
		const FVector Normal = Part == ETikiBodyPart::Lower ? Along : -Along;
		// a flat disc closes the cut, across the bone the part is cut at (mesh component space)
		if (CapMesh && !bModules)
		{
			const float Diameter = CapRadius * 2.0f / 100.0f;
			Cap->SetStaticMesh(CapMesh);
			if (CapMaterial)
			{
				Cap->SetMaterial(0, CapMaterial);
			}
			Cap->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromZ(Normal).ToQuat(), Joint, FVector(Diameter, Diameter, 0.03f)));
			Cap->SetVisibility(true);
		}
		// the lower half has its cut bone masked away: its jet hangs on the parent of that bone
		const int32 AttachIndex = Part == ETikiBodyPart::Lower ? Parents[CutIndex] : CutIndex;
		SpawnBlood(Joint, Normal, Skeleton.IsValidIndex(AttachIndex) ? Skeleton.GetBoneName(AttachIndex) : NAME_None);
	}
}

void ATikiCorpse::SpawnBlood(const FVector& JointLocal, const FVector& OutOfCut, FName AttachBone)
{
	UTikiBloodSubsystem* Blood = UTikiBloodSubsystem::Get(this);
	if (!Blood || !Rules || !Rules->bBloodEnabled)
	{
		return;
	}
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Now = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Age = FMath::Max(0.0f, static_cast<float>(Now - SpawnServerTime));
	Blood->Preload(Rules);
	// a body (or a half) bleeds into a pool on the ground under it, once laid
	if (Part != ETikiBodyPart::Limb)
	{
		Blood->SpawnPool(Rules, GetActorLocation(), this, Age);
	}
	if (Part == ETikiBodyPart::Whole || Age > Rules->BloodFreshWindow)
	{
		return;
	}
	// a fresh cut: the jet out of it, following the piece as it flies; the chunks and a splat once per cut body
	const FVector Joint = Mesh->GetComponentTransform().TransformPosition(JointLocal);
	const FVector Out = Mesh->GetComponentTransform().TransformVectorNoScale(OutOfCut);
	Blood->SpawnSliceSpray(Rules, Mesh, AttachBone, Joint, Out);
	if (Part == ETikiBodyPart::Upper)
	{
		Blood->SpawnChunks(Rules, Joint, Out);
		Blood->SpawnSplat(Rules, Joint, this);
	}
	// a limb has no pool: a stain on the ground where it falls, at the foot of the body (T-155)
	else if (Part == ETikiBodyPart::Limb)
	{
		Blood->SpawnSplat(Rules, GetActorLocation(), this);
	}
}

bool ATikiCorpse::ApplyModules(const TArray<FTransform>& Local)
{
	TArray<bool> Shown;
	if (!DismemberSet || !BodyMesh || !DismemberSet->Select(BodyMesh->GetRefSkeleton(), Part, PartBone, CutBones, Shown))
	{
		return false;
	}
	// every module first: one that does not match the skeleton keeps the whole mesh (never half drawn)
	const FReferenceSkeleton& Skeleton = BodyMesh->GetRefSkeleton();
	TArray<TPair<USkeletalMesh*, TArray<FTransform>>> Drawn;
	for (int32 Index = 0; Index < Shown.Num(); ++Index)
	{
		if (!Shown[Index])
		{
			continue;
		}
		USkeletalMesh* ModuleMesh = DismemberSet->Modules[Index].Mesh;
		TArray<FTransform> ModulePose;
		if (!TikiDismember::MapPose(Skeleton, ModuleMesh->GetRefSkeleton(), Local, ModulePose))
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Tiki corpse: module %s does not match the skeleton of %s, whole mesh kept"),
				*GetNameSafe(ModuleMesh), *GetNameSafe(BodyMesh));
			return false;
		}
		Drawn.Emplace(ModuleMesh, MoveTemp(ModulePose));
	}
	TArray<UMaterialInterface*> Materials;
	for (const TObjectPtr<UMaterialInterface>& Material : BodyMaterials)
	{
		Materials.Add(Material);
	}
	for (TPair<USkeletalMesh*, TArray<FTransform>>& Entry : Drawn)
	{
		UPoseableMeshComponent* Module = NewObject<UPoseableMeshComponent>(this);
		Module->SetupAttachment(Mesh);
		Module->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Module->RegisterComponent();
		Module->SetSkinnedAssetAndUpdate(Entry.Key, true);
		TikiDismember::ApplyMaterials(Module, BodyMesh, Materials);
		if (Module->BoneSpaceTransforms.Num() == Entry.Value.Num())
		{
			Module->BoneSpaceTransforms = MoveTemp(Entry.Value);
			Module->RefreshBoneTransforms();
		}
		Modules.Add(Module);
	}
	// the whole mesh stays for the held item and the blood to follow, unseen
	Mesh->SetVisibility(false);
	return true;
}

void ATikiCorpse::CheckFlood()
{
	if (!Rules)
	{
		return;
	}
	const float Depth = GetWaterDepth();
	if (TikiMath::IsCorpseWashedAway(Depth, Rules->CorpseFloodDepth) && !IsInBoat(GetWorld(), Body->Bounds.Origin))
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s washed away by the water (%.0f cm deep)"), *GetName(), Depth);
		Destroy();
	}
}

float ATikiCorpse::GetWaterDepth() const
{
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!Water || !Rules)
	{
		return 0.0f;
	}
	// the bounds of the collision itself (the shapes), the ground right under the corpse
	const FBox Bounds = Body->Bounds.GetBox();
	const FVector Bottom(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
	float GroundZ = 0.0f;
	const bool bGround = FindGround(GetWorld(), Bottom, static_cast<float>(Bounds.Max.Z - Bounds.Min.Z), Rules->CorpseGroundTraceHeight, this, GroundZ);
	return TikiMath::CorpseWaterDepth(Water->GetWaterSurfaceAt(Bottom), static_cast<float>(Bounds.Min.Z), bGround, GroundZ);
}

bool ATikiCorpse::FindGround(const UWorld* World, const FVector& Point, float Above, float Below, const AActor* Ignore, float& OutGroundZ)
{
	if (!World)
	{
		return false;
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiCorpseGround), false, Ignore);
	// a water plane with a collision is no ground (the sea of a test map, the lagoon)
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
	FHitResult Hit;
	const FVector Top = Point + FVector(0.0, 0.0, FMath::Max(0.0f, Above));
	const FVector End = Point - FVector(0.0, 0.0, FMath::Max(1.0f, Below));
	if (!World->LineTraceSingleByObjectType(Hit, Top, End, Objects, Params))
	{
		return false;
	}
	OutGroundZ = static_cast<float>(Hit.ImpactPoint.Z);
	return true;
}

FKAggregateGeom ATikiCorpse::PartGeometry(const USkeletalMesh* InMesh, const TArray<FTransform>& InPose, ETikiBodyPart InPart, FName InPartBone,
	const TArray<FName>& InCutBones, const FTransform& ComponentToTarget)
{
	const UPhysicsAsset* Asset = InMesh ? InMesh->GetPhysicsAsset() : nullptr;
	if (!Asset)
	{
		return FKAggregateGeom();
	}
	const FReferenceSkeleton& Skeleton = InMesh->GetRefSkeleton();
	TArray<TikiBody::FBoneGeometry> Bodies;
	for (const USkeletalBodySetup* Setup : Asset->SkeletalBodySetups)
	{
		const int32 Bone = Setup ? Skeleton.FindBoneIndex(Setup->BoneName) : INDEX_NONE;
		if (Bone != INDEX_NONE && Setup->CollisionReponse != EBodyCollisionResponse::BodyCollision_Disabled)
		{
			Bodies.Add({ Bone, Setup->AggGeom });
		}
	}
	const int32 CutIndex = InPartBone.IsNone() ? INDEX_NONE : Skeleton.FindBoneIndex(InPartBone);
	return TikiBody::PoseGeometry(Bodies, PoseOf(Skeleton, InPose), ParentsOf(Skeleton), InPart, CutIndex, IndicesOf(Skeleton, InCutBones), ComponentToTarget);
}

float ATikiCorpse::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Taken = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	const bool bCut = ATikiCreature::IsCutBlow(DamageCauser);
	if (!HasAuthority() || !bCut || Part != ETikiBodyPart::Whole || IsActorBeingDestroyed())
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

void ATikiCorpse::CutInTwo(const FVector& Direction)
{
	if (!HasAuthority() || Part != ETikiBodyPart::Whole || IsActorBeingDestroyed() || !Rules || !BodyMesh
		|| BodyMesh->GetRefSkeleton().FindBoneIndex(Rules->HalfCutBone) == INDEX_NONE)
	{
		return;
	}
	FTikiBodySnapshot Snap;
	Snap.Mesh = BodyMesh;
	Snap.Materials.Append(BodyMaterials);
	Snap.HeldMesh = HeldMesh;
	Snap.HeldMaterials.Append(HeldMaterials);
	Snap.Pose = Pose;
	Snap.MeshWorld = Mesh->GetComponentTransform();
	Snap.CutBones = CutBones;
	Snap.DismemberSet = DismemberSet;

	// the halves slide apart a little along the blow and fall where the body lay: no speed inherited (T-155)
	const FVector Blow = Direction.IsNearlyZero() ? GetActorForwardVector() : Direction;
	UTikiRules* KeptRules = Rules;
	UWorld* World = GetWorld();
	Destroy();
	SpawnPart(World, KeptRules, Snap, ETikiBodyPart::Upper, KeptRules->HalfCutBone, Blow * KeptRules->CutFlingSpeed);
	SpawnPart(World, KeptRules, Snap, ETikiBodyPart::Lower, KeptRules->HalfCutBone, -Blow * KeptRules->CutFlingSpeed * 0.3f);
	UE_LOG(Logmonke_island, Log, TEXT("%s cut in two"), *GetName());
}

void ATikiCorpse::KeepCount(UWorld* World, int32 MaxCount)
{
	TArray<AActor*> Corpses;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if ((It->IsA<ATikiCorpse>() || It->IsA<ATikiRagdollCorpse>()) && !It->IsActorBeingDestroyed())
		{
			Corpses.Add(*It);
		}
	}
	if (Corpses.Num() <= MaxCount)
	{
		return;
	}
	// the oldest first
	Corpses.Sort([](const AActor& A, const AActor& B) { return A.GetGameTimeSinceCreation() > B.GetGameTimeSinceCreation(); });
	for (int32 Index = 0; Index < Corpses.Num() - MaxCount; ++Index)
	{
		Corpses[Index]->Destroy();
	}
}

void ATikiCorpse::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATikiCorpse, BodyMesh, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, BodyMaterials, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, HeldMesh, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, HeldMaterials, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, Pose, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, MeshRelative, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, Part, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, PartBone, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, CutBones, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, DismemberSet, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, BoxExtent, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, BoxOffset, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, bFrozen, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, Rules, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, MassKg, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, CapMesh, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, CapMaterial, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, CapRadius, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, SpawnServerTime, COND_InitialOnly);
}
