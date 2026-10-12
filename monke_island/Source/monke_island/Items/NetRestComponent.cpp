// monke_island

#include "Items/NetRestComponent.h"
#include "Items/NetRestMath.h"
#include "Items/ValuableComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "PhysicsReplicationInterface.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogMonkeNetRest);

UNetRestComponent::UNetRestComponent()
{
	// ticks only while glued to a base, before the physics step, once the base has moved
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);
}

void UNetRestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UNetRestComponent, Glue);
}

void UNetRestComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	if (Owner->HasAuthority())
	{
		ApplyAwakeRate(false);
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			Root->OnComponentWake.AddDynamic(this, &UNetRestComponent::HandleWake);
		}
		// the looks of the many objects of a level are spread over the interval
		Owner->GetWorldTimerManager().SetTimer(CheckTimer, this, &UNetRestComponent::CheckRest, CheckInterval, true,
			FMath::FRandRange(0.5f, 1.5f) * CheckInterval);
	}
	// a client receiving the object already glued carries it at once
	ApplyGlueLocally();
}

void UNetRestComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	if (AActor* Owner = GetOwner())
	{
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
		{
			Root->OnComponentWake.RemoveDynamic(this, &UNetRestComponent::HandleWake);
		}
	}
	if (AActor* Prerequisite = FollowedBaseOwner.Get())
	{
		RemoveTickPrerequisiteActor(Prerequisite);
	}
	FollowedBaseOwner.Reset();
	Super::EndPlay(EndPlayReason);
}

UPrimitiveComponent* UNetRestComponent::GetBody() const
{
	const AActor* Owner = GetOwner();
	UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	return Root && Root->IsSimulatingPhysics() ? Root : nullptr;
}

bool UNetRestComponent::IsHeld(const UPrimitiveComponent* Body)
{
	// the grab ghosts what it holds against the physics bodies (UIslanderGrabComponent::SetHeldGhost); a fresh corpse
	// does the same until it is apart from the others, and stays awake meanwhile
	return Body && Body->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Ignore;
}

bool UNetRestComponent::IsGlued() const
{
	return IsValid(Glue.Base);
}

UPrimitiveComponent* UNetRestComponent::GetGlueBase() const
{
	return IsValid(Glue.Base) ? Glue.Base.Get() : nullptr;
}

void UNetRestComponent::SetAwakeNetUpdateFrequency(float Frequency)
{
	AwakeNetUpdateFrequency = FMath::Clamp(Frequency, 1.0f, 100.0f);
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority() && HasBegunPlay())
	{
		AppliedRate = 0.0f;
		ApplyAwakeRate(IsHeld(Cast<UPrimitiveComponent>(Owner->GetRootComponent())));
	}
}

void UNetRestComponent::ApplyAwakeRate(bool bHeld)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	const float Rate = NetRestMath::AwakeRate(bHeld, IsGlued(), AwakeNetUpdateFrequency, HeldNetUpdateFrequency, GluedNetUpdateFrequency);
	if (Rate == AppliedRate)
	{
		return;
	}
	AppliedRate = Rate;
	Owner->SetNetUpdateFrequency(Rate);
	Owner->SetMinNetUpdateFrequency(FMath::Min(AwakeMinNetUpdateFrequency, Rate));
}

void UNetRestComponent::WakeNet()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	RestSeconds = 0.0f;
	if (bNetDormant)
	{
		ApplyNetState(false);
	}
	else
	{
		Owner->ForceNetUpdate();
	}
}

void UNetRestComponent::WakeActorNet(AActor* Actor)
{
	if (!Actor || !Actor->HasAuthority())
	{
		return;
	}
	if (UNetRestComponent* Rest = Actor->FindComponentByClass<UNetRestComponent>())
	{
		Rest->WakeNet();
	}
	else if (Actor->NetDormancy > DORM_Awake)
	{
		Actor->FlushNetDormancy();
	}
}

void UNetRestComponent::HandleWake(UPrimitiveComponent* WakingComponent, FName BoneName)
{
	// moving again: the clients follow it from now on (a glued object is awake all along, the base carries it)
	if (bNetDormant && !IsGlued())
	{
		ApplyNetState(false);
	}
}

void UNetRestComponent::GlueTo(UPrimitiveComponent* Base)
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Body = GetBody();
	if (!Owner || !Owner->HasAuthority() || !Body || !IsValid(Base) || Base->GetOwner() == Owner || IsHeld(Body))
	{
		return;
	}
	const FTransform Local = Body->GetComponentTransform().GetRelativeTransform(Base->GetComponentTransform());
	const FVector At = Local.GetLocation();
	Glue.Base = Base;
	// the server carries it where the clients do: at the place the replicated glue rounds to (1/100 cm)
	Glue.Location = FVector_NetQuantize100(FMath::RoundToDouble(At.X * 100.0) / 100.0, FMath::RoundToDouble(At.Y * 100.0) / 100.0,
		FMath::RoundToDouble(At.Z * 100.0) / 100.0);
	Glue.Rotation = Local.Rotator();
	RestSeconds = 0.0f;
	bCandidateBaseMoved = false;
	CandidateBase.Reset();
	ApplyGlueLocally();
	// awake, its movement off, hardly ever looked at: the glue goes out at once, then nothing more (NetRestMath::Decide)
	ApplyAwakeRate(false);
	ApplyNetState(false);
	Owner->ForceNetUpdate();
	UE_LOG(LogMonkeNetRest, Verbose, TEXT("%s glued to %s.%s"), *Owner->GetName(), *GetNameSafe(Base->GetOwner()), *Base->GetName());
}

void UNetRestComponent::Release()
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Glue.Base)
	{
		return;
	}
	Glue = FNetRestGlue();
	ApplyGlueLocally();
	ApplyAwakeRate(IsHeld(Cast<UPrimitiveComponent>(Owner->GetRootComponent())));
	ApplyNetState(false);
	// awake already (on the barque): its release and its movement go out at once all the same
	Owner->ForceNetUpdate();
	UE_LOG(LogMonkeNetRest, Verbose, TEXT("%s released"), *Owner->GetName());
}

void UNetRestComponent::OnRep_Glue()
{
	ApplyGlueLocally();
}

void UNetRestComponent::ApplyGlueLocally()
{
	SetFollowing(GetGlueBase());
}

void UNetRestComponent::SetFollowing(UPrimitiveComponent* Base)
{
	const bool bGlue = Base != nullptr;
	if (bGlue == bFollowing && (!bGlue || Base == FollowedBase.Get()))
	{
		return;
	}
	if (AActor* Previous = FollowedBaseOwner.Get())
	{
		RemoveTickPrerequisiteActor(Previous);
	}
	FollowedBaseOwner.Reset();
	FollowedBase = Base;
	bFollowing = bGlue;
	bHasLastFollow = false;
	SetComponentTickEnabled(bGlue);
	if (!bGlue)
	{
		return;
	}
	// after the base moved this frame (the galleon sways in its tick, the barque gets its forces)
	if (AActor* BaseOwner = Base->GetOwner())
	{
		AddTickPrerequisiteActor(BaseOwner);
		FollowedBaseOwner = BaseOwner;
	}
	// a client: the last replicated movement no longer pulls the body, its base carries it
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	UWorld* World = GetWorld();
	if (Root && World && !Owner->HasAuthority())
	{
		if (FPhysScene* Scene = World->GetPhysicsScene())
		{
			if (IPhysicsReplication* Replication = Scene->GetPhysicsReplication())
			{
				Replication->RemoveReplicatedTarget(Root);
			}
		}
	}
}

void UNetRestComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FollowBase(DeltaTime);
}

void UNetRestComponent::FollowBase(float DeltaSeconds)
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	UPrimitiveComponent* Base = GetGlueBase();
	// its base gone, taken by a player (the grab ghosts it), carried or not simulating any more: free on this machine at
	// once; the server frees it for everyone
	if (!Root || !Base || IsHeld(Root) || !Root->IsSimulatingPhysics() || Root->GetAttachParent())
	{
		if (Owner && Owner->HasAuthority())
		{
			Release();
		}
		SetFollowing(nullptr);
		return;
	}
	const FTransform Place = FTransform(Glue.Rotation, Glue.Location) * Base->GetComponentTransform();
	const FVector Location = Place.GetLocation();
	const FQuat Rotation = Place.GetRotation();
	Root->SetWorldLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	// at the speed of the base: nothing builds up between two frames, and it leaves with it when freed
	Root->SetAllPhysicsLinearVelocity(bHasLastFollow ? NetRestMath::FollowVelocity(LastFollowLocation, Location, DeltaSeconds) : FVector::ZeroVector);
	Root->SetAllPhysicsAngularVelocityInRadians(bHasLastFollow ? NetRestMath::FollowAngularVelocity(LastFollowRotation, Rotation, DeltaSeconds) : FVector::ZeroVector);
	LastFollowLocation = Location;
	LastFollowRotation = Rotation;
	bHasLastFollow = true;
}

void UNetRestComponent::ApplyNetState(bool bDormant)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	// glued: every machine carries it, its movement is not sent; free: the server's movement again
	const bool bReplicateMovement = !IsGlued();
	if (Owner->IsReplicatingMovement() != bReplicateMovement)
	{
		Owner->SetReplicateMovement(bReplicateMovement);
	}
	if (bDormant == bNetDormant)
	{
		return;
	}
	bNetDormant = bDormant;
	if (bDormant)
	{
		// the changes of this frame still go out before the channel sleeps
		DormantSignature = ComputeSignature();
		Owner->SetNetDormancy(DORM_DormantAll);
	}
	else
	{
		Owner->SetNetDormancy(DORM_Awake);
		Owner->ForceNetUpdate();
	}
	UE_LOG(LogMonkeNetRest, Verbose, TEXT("%s %s"), *Owner->GetName(), bDormant ? TEXT("dormant") : TEXT("awake"));
}

uint32 UNetRestComponent::ComputeSignature() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return 0;
	}
	const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	const uint32 Flags = (Owner->IsHidden() ? 1u : 0u) | (Owner->GetActorEnableCollision() ? 2u : 0u) | (Root && Root->IsSimulatingPhysics() ? 4u : 0u);
	uint32 Hash = HashCombine(GetTypeHash(Flags), PointerHash(Owner->GetAttachParentActor()));
	if (const UValuableComponent* Valuable = Owner->FindComponentByClass<UValuableComponent>())
	{
		Hash = HashCombine(Hash, GetTypeHash(Valuable->GetValue()));
	}
	return Hash;
}

UPrimitiveComponent* UNetRestComponent::FindMovingBase(const UPrimitiveComponent* Body) const
{
	UWorld* World = GetWorld();
	if (!World || !Body)
	{
		return nullptr;
	}
	const FBox Box = Body->Bounds.GetBox();
	const FVector Center = Box.GetCenter();
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(NetRestBase), false, GetOwner());
	FHitResult Hit;
	if (!World->LineTraceSingleByObjectType(Hit, Center, FVector(Center.X, Center.Y, Box.Min.Z - BaseProbeDepth), Objects, Params))
	{
		return nullptr;
	}
	UPrimitiveComponent* Floor = Hit.GetComponent();
	const AActor* FloorOwner = Hit.GetActor();
	if (!Floor || Cast<APawn>(FloorOwner))
	{
		return nullptr;
	}
	// on an object already glued there: the same base
	if (const UNetRestComponent* Other = FloorOwner ? FloorOwner->FindComponentByClass<UNetRestComponent>() : nullptr)
	{
		return Other->GetGlueBase();
	}
	// a base that moves without simulating (the galleon's deck); a simulating one stows its own cargo (the barque)
	return Floor->Mobility == EComponentMobility::Movable && !Floor->IsSimulatingPhysics() ? Floor : nullptr;
}

void UNetRestComponent::CheckRest()
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	if (!Root || !Owner->HasAuthority() || Owner->IsActorBeingDestroyed())
	{
		return;
	}
	UPrimitiveComponent* Body = GetBody();
	const bool bHeld = IsHeld(Root);

	// glued: free again once its base is gone, it stops simulating, it is attached or a player takes it
	if (Glue.Base && (!IsGlued() || !Body || bHeld || Root->GetAttachParent()))
	{
		Release();
	}

	// dormant, it changed without moving (hidden, attached, its value...): the clients get that once
	if (bNetDormant)
	{
		const uint32 Signature = ComputeSignature();
		if (Signature != DormantSignature)
		{
			DormantSignature = Signature;
			Owner->FlushNetDormancy();
			UE_LOG(LogMonkeNetRest, Verbose, TEXT("%s changed while dormant: flushed"), *Owner->GetName());
		}
	}

	const bool bAwakeBody = Body && Body->IsAnyRigidBodyAwake();

	// on a base that moves under it (it never falls asleep there): glued once it keeps its place on it long enough
	UPrimitiveComponent* Base = Body && !bHeld && !IsGlued() && bAwakeBody && GlueDelay > 0.0f ? FindMovingBase(Body) : nullptr;
	if (!Base)
	{
		CandidateBase.Reset();
		RestSeconds = 0.0f;
		bCandidateBaseMoved = false;
	}
	else
	{
		const FTransform BaseWorld = Base->GetComponentTransform();
		const FTransform Local = Body->GetComponentTransform().GetRelativeTransform(BaseWorld);
		if (CandidateBase.Get() == Base)
		{
			const bool bStill = NetRestMath::IsStillOnBase(CandidateLocal, Local, GlueStillDistance, GlueStillAngle);
			RestSeconds = NetRestMath::AccumulateRest(RestSeconds, bStill, CheckInterval);
			bCandidateBaseMoved = bStill && (bCandidateBaseMoved || NetRestMath::HasBaseMoved(CandidateBaseWorld, BaseWorld, 0.05f, 0.01f));
		}
		else
		{
			RestSeconds = 0.0f;
			bCandidateBaseMoved = false;
		}
		CandidateBase = Base;
		CandidateBaseWorld = BaseWorld;
		CandidateLocal = Local;
		if (NetRestMath::ShouldGlue(RestSeconds, GlueDelay, bCandidateBaseMoved))
		{
			GlueTo(Base);
			return;
		}
	}

	// asleep on a base that moves without simulating: should the physics leave it asleep there while the deck rolls, it is
	// woken up once the deck has moved, and glued at the next looks
	if (Body && !bAwakeBody && !IsGlued())
	{
		UPrimitiveComponent* Floor = SleepFloor.Get();
		if (!bNetDormant || !Floor)
		{
			Floor = bNetDormant ? nullptr : FindMovingBase(Body);
			SleepFloor = Floor;
			SleepFloorWorld = Floor ? Floor->GetComponentTransform() : FTransform::Identity;
		}
		else if (NetRestMath::HasBaseMoved(SleepFloorWorld, Floor->GetComponentTransform(), 0.05f, 0.01f))
		{
			SleepFloor.Reset();
			Body->WakeAllRigidBodies();
			UE_LOG(LogMonkeNetRest, Verbose, TEXT("%s asleep on a moving base: woken up to be glued"), *Owner->GetName());
		}
	}
	else
	{
		SleepFloor.Reset();
	}

	// held: its holder sees it through the server, more often than when it merely moves; glued: hardly ever
	ApplyAwakeRate(bHeld);
	ApplyNetState(NetRestMath::Decide(Body != nullptr, bHeld, IsGlued(), bAwakeBody, bDormantAtRest) == ENetRestState::Dormant);
}
