// monke_island

#include "Creatures/TikiCorpse.h"
#include "Boats/BoatActor.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiRagdollCorpse.h"
#include "Creatures/TikiRules.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/BoxComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
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

	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Body->SetSimulatePhysics(true);
	Body->SetLinearDamping(0.2f);
	Body->SetAngularDamping(0.6f);
	Body->SetBoxExtent(BoxExtent);
	SetRootComponent(Body);

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

FTikiBodySnapshot ATikiCorpse::Snapshot(USkeletalMeshComponent* BodyMesh, USkeletalMeshComponent* InHeld, const TArray<FName>& InCutBones)
{
	FTikiBodySnapshot Snap;
	if (!BodyMesh)
	{
		return Snap;
	}
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

	// the box hugs the joints the part keeps: the body is wider than its bones (margin), a limb much less
	const TArray<int32> Parents = ParentsOf(Skeleton);
	const TArray<int32> Hidden = IndicesOf(Skeleton, Snap.CutBones);
	const float Margin = InPart == ETikiBodyPart::Limb ? InRules->CorpseBoxMargin * 0.4f : InRules->CorpseBoxMargin;
	FBox Box = TikiBody::PartBounds(PoseOf(Skeleton, Snap.Pose), Parents, InPart, CutIndex, Hidden, Snap.MeshWorld, Margin);
	if (!Box.IsValid)
	{
		return nullptr;
	}
	// a lying body has its bones a few centimetres above the ground: the box must not start under it
	Box.Min.Z = FMath::Min(Box.Max.Z - 10.0, Box.Min.Z + Margin - 4.0);

	const FTransform BoxWorld(FQuat::Identity, Box.GetCenter());
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
	Corpse->MeshRelative = Snap.MeshWorld.GetRelativeTransform(BoxWorld);
	Corpse->Part = InPart;
	Corpse->PartBone = InPartBone;
	Corpse->CutBones = Snap.CutBones;
	Corpse->BoxExtent = Box.GetExtent().ComponentMax(FVector(8.0));
	Corpse->MassKg = InPart == ETikiBodyPart::Whole ? InRules->CorpseMassKg
		: InPart == ETikiBodyPart::Limb ? InRules->CorpseLimbMassKg : InRules->CorpseHalfMassKg;
	Corpse->CapMesh = InRules->CutCapMesh;
	Corpse->CapMaterial = InRules->CutCapMaterial;
	Corpse->CapRadius = InPart == ETikiBodyPart::Limb ? InRules->LimbCutRadius : InRules->BodyCutRadius;
	Corpse->FinishSpawning(BoxWorld);
	if (!Velocity.IsNearlyZero())
	{
		Corpse->Body->SetPhysicsLinearVelocity(Velocity);
	}
	return Corpse;
}

void ATikiCorpse::BeginPlay()
{
	Super::BeginPlay();

	ApplyLook();
	if (!HasAuthority())
	{
		return;
	}
	if (Rules)
	{
		GetWorldTimerManager().SetTimer(FloodTimer, this, &ATikiCorpse::CheckFlood, Rules->CorpseFloodCheckInterval, true, Rules->CorpseFloodCheckInterval);
		if (Rules->CorpseLifetime > 0.0f)
		{
			SetLifeSpan(Rules->CorpseLifetime);
		}
		KeepCount(GetWorld(), Rules->CorpseMaxCount);
	}
}

void ATikiCorpse::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FloodTimer);
	Super::EndPlay(EndPlayReason);
}

void ATikiCorpse::ApplyLook()
{
	Body->SetBoxExtent(BoxExtent, true);
	Body->SetMassOverrideInKg(NAME_None, MassKg, true);
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

	// a flat disc closes the cut, across the bone the part is cut at (mesh component space)
	if (CapMesh && Part != ETikiBodyPart::Whole && Skeleton.IsValidIndex(CutIndex))
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
		const float Diameter = CapRadius * 2.0f / 100.0f;
		Cap->SetStaticMesh(CapMesh);
		if (CapMaterial)
		{
			Cap->SetMaterial(0, CapMaterial);
		}
		Cap->SetRelativeTransform(FTransform(FRotationMatrix::MakeFromZ(Normal).ToQuat(), Joint, FVector(Diameter, Diameter, 0.03f)));
		Cap->SetVisibility(true);
	}
}

void ATikiCorpse::CheckFlood()
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

	const FVector Moving = Body->GetPhysicsLinearVelocity();
	const FVector Blow = Direction.IsNearlyZero() ? GetActorForwardVector() : Direction;
	UTikiRules* KeptRules = Rules;
	UWorld* World = GetWorld();
	Destroy();
	SpawnPart(World, KeptRules, Snap, ETikiBodyPart::Upper, KeptRules->HalfCutBone,
		Moving + Blow * KeptRules->CutFlingSpeed + FVector(0.0f, 0.0f, KeptRules->CutFlingSpeed * 0.5f));
	SpawnPart(World, KeptRules, Snap, ETikiBodyPart::Lower, KeptRules->HalfCutBone, Moving - Blow * KeptRules->CutFlingSpeed * 0.3f);
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
	DOREPLIFETIME_CONDITION(ATikiCorpse, BoxExtent, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, MassKg, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, CapMesh, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, CapMaterial, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiCorpse, CapRadius, COND_InitialOnly);
}
