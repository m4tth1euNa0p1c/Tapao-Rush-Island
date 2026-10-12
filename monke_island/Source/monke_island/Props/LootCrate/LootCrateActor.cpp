// monke_island

#include "Props/LootCrate/LootCrateActor.h"
#include "Props/LootCrate/LootCrateMath.h"
#include "Props/LootCrate/LootCrateRules.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Items/NetRestComponent.h"
#include "Items/ValuableComponent.h"
#include "monke_island.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	/** Clients: longest wait for an object that came out to be taken off the crate by its own replication (s, network
	 *  safety, not a gameplay value) */
	constexpr float RestoreTimeoutSeconds = 2.0f;
}

ALootCrateActor::ALootCrateActor()
{
	// the lid and the objects coming out tick for a moment, then the crate sleeps
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	// the box is the body: simulated on the server, followed by the clients (cahier §5: free objects simulate physics)
	Body = CreateDefaultSubobject<UBoxComponent>(TEXT("Body"));
	Body->InitBoxExtent(FVector(40.0f, 22.5f, 15.0f));
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	// a crate held in front of the player never pushes the third person camera
	Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Body->SetSimulatePhysics(true);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	// the network rest hears at once when the body wakes up (T-316)
	Body->BodyInstance.bGenerateWakeEvents = true;
	SetRootComponent(Body);

	CrateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CrateMesh"));
	CrateMesh->SetupAttachment(Body);
	CrateMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -15.0f));
	CrateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CrateMesh->SetGenerateOverlapEvents(false);
	CrateMesh->SetCanEverAffectNavigation(false);

	LidHinge = CreateDefaultSubobject<USceneComponent>(TEXT("LidHinge"));
	LidHinge->SetupAttachment(CrateMesh);
	LidHinge->SetRelativeLocation(FVector(0.0f, 22.5f, 30.0f));

	LidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LidMesh"));
	LidMesh->SetupAttachment(LidHinge);
	LidMesh->SetRelativeLocation(FVector(0.0f, -22.5f, 0.0f));
	LidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LidMesh->SetGenerateOverlapEvents(false);
	LidMesh->SetCanEverAffectNavigation(false);

	// 30 updates a second while it moves instead of the engine's 100, none at all at rest, glued to the galleon's deck or
	// the barque's hull on every machine with what it holds (T-316, review R-04)
	NetRest = CreateDefaultSubobject<UNetRestComponent>(TEXT("NetRest"));

	DefaultRules = TSoftObjectPtr<ULootCrateRules>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Props/LootCrate/DA_LootCrateRules.DA_LootCrateRules")));
}

void ALootCrateActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALootCrateActor, Contents);
}

const ULootCrateRules* ALootCrateActor::GetRules() const
{
	return Rules ? Rules.Get() : GetDefault<ULootCrateRules>();
}

void ALootCrateActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyRules();
}

void ALootCrateActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyRules();
}

void ALootCrateActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// destroyed during the game (out of the world, removed): what it held falls out where it was, never lost hidden
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed && !Contents.IsEmpty())
	{
		EmptyContents(GetActorLocation() + GetActorForwardVector() * 100.0);
	}
	PendingRestore.Reset();
	Super::EndPlay(EndPlayReason);
}

void ALootCrateActor::ApplyRules()
{
	// the bare class (spawned by the galleon) takes DA_LootCrateRules when it exists, without a warning when it does not
	if (!Rules && !DefaultRules.IsNull() && FPackageName::DoesPackageExist(DefaultRules.GetLongPackageName()))
	{
		Rules = DefaultRules.LoadSynchronous();
	}
	const ULootCrateRules* R = GetRules();
	// the meshes of the blueprint first, the rules' when the components have none (the bare class)
	if (!CrateMesh->GetStaticMesh() && !R->CrateMesh.IsNull())
	{
		CrateMesh->SetStaticMesh(R->CrateMesh.LoadSynchronous());
	}
	if (!LidMesh->GetStaticMesh() && !R->LidMesh.IsNull())
	{
		LidMesh->SetStaticMesh(R->LidMesh.LoadSynchronous());
	}
	Body->SetBoxExtent(R->BodyHalfSizeCm, false);
	Body->SetLinearDamping(R->LinearDamping);
	Body->SetAngularDamping(R->AngularDamping);

	// the pivot of SM_LootCrate is the middle of its bottom, the box is centred on the actor
	CrateMesh->SetRelativeLocation(FVector(0.0, 0.0, -R->BodyHalfSizeCm.Z));
	const FVector Closed = CrateMesh->DoesSocketExist(R->LidSocket)
		? CrateMesh->GetSocketTransform(R->LidSocket, RTS_Component).GetLocation() : R->LidClosedCm;
	LidHinge->SetRelativeLocation(R->LidHingeCm);
	LidMesh->SetRelativeLocation(Closed - R->LidHingeCm);
	// a positive roll lowers +Y and raises -Y: the free edge goes up when it lies on the -Y side of the hinge
	LidRollSign = Closed.Y <= R->LidHingeCm.Y ? 1.0f : -1.0f;
	SetLidAngle(0.0f);
	UpdateMass();
}

float ALootCrateActor::GetContentMassKg() const
{
	float Total = 0.0f;
	for (const FLootCrateEntry& Entry : Contents)
	{
		Total += FMath::Max(0.0f, Entry.MassKg);
	}
	return Total;
}

float ALootCrateActor::GetTotalMassKg() const
{
	return LootCrateMath::TotalMassKg(GetRules()->EmptyMassKg, GetContentMassKg());
}

void ALootCrateActor::UpdateMass()
{
	// the barque weighs what lies in it through this mass (its load), the holders through the grab component
	Body->SetMassOverrideInKg(NAME_None, GetTotalMassKg(), true);
}

void ALootCrateActor::PurgeContents()
{
	const int32 Removed = Contents.RemoveAll([this](const FLootCrateEntry& Entry)
	{
		return !IsValid(Entry.Item) || Entry.Item->GetAttachParentActor() != this;
	});
	if (Removed > 0)
	{
		UpdateMass();
		// asleep (dormant), the crate would keep showing them on the clients (T-316)
		UNetRestComponent::WakeActorNet(this);
		UE_LOG(Logmonke_island, Log, TEXT("%s: %d object(s) gone from inside"), *GetName(), Removed);
	}
}

// ---------------------------------------------------------------- container

void ALootCrateActor::GetContainedActors(TArray<AActor*>& OutActors) const
{
	for (const FLootCrateEntry& Entry : Contents)
	{
		if (IsValid(Entry.Item))
		{
			OutActors.Add(Entry.Item);
		}
	}
}

bool ALootCrateActor::CanStoreItem(const APawn* User, const AActor* Item) const
{
	const ULootCrateRules* R = GetRules();
	// no player, no crate in a crate, nothing fixed to something else, nothing the rules keep out
	if (!IsValid(Item) || Item == this || IsActorBeingDestroyed() || Item->IsA<APawn>() || Item->Implements<ULootContainer>()
		|| Item->ActorHasTag(R->RefusedTag) || Item->ActorHasTag(UIslanderGrabComponent::NoGrabTag) || Item->GetAttachParentActor())
	{
		return false;
	}
	// a free object (the one in a player's hands is)
	const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Item->GetRootComponent());
	if (!Root || !Root->IsSimulatingPhysics())
	{
		return false;
	}
	if (R->bOnlyValuables && !Item->FindComponentByClass<UValuableComponent>())
	{
		return false;
	}
	for (const FLootCrateEntry& Entry : Contents)
	{
		if (Entry.Item == Item)
		{
			return false;
		}
	}
	// its own size, whatever its turn in the hands
	const FBoxSphereBounds Local = Root->CalcBounds(FTransform(FQuat::Identity, FVector::ZeroVector, Root->GetComponentScale()));
	return LootCrateMath::FitsInside(Local.BoxExtent * 2.0, R->InnerSizeCm)
		&& LootCrateMath::CanAccept(Contents.Num(), GetContentMassKg(), Root->GetMass(), R->MaxItems, R->MaxContentKg);
}

bool ALootCrateActor::StoreItem(APawn* User, AActor* Item)
{
	if (!HasAuthority())
	{
		return false;
	}
	PurgeContents();
	if (!CanStoreItem(User, Item))
	{
		return false;
	}
	UPrimitiveComponent* Root = CastChecked<UPrimitiveComponent>(Item->GetRootComponent());
	FLootCrateEntry& Entry = Contents.AddDefaulted_GetRef();
	Entry.Item = Item;
	Entry.MassKg = Root->GetMass();

	// inside: no body (it would shove the box from within), no collision, not drawn, carried along by the crate
	ApplyStoredState(Item, true);
	Item->AttachToComponent(Body, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld, false));
	UpdateMass();
	Body->WakeRigidBody();
	// the crate may sleep (dormant, or glued to the deck): its contents and the hidden object reach every client now, and
	// the crate is woken before its lid so that the multicast finds an open channel (T-316)
	UNetRestComponent::WakeActorNet(Item);
	UNetRestComponent::WakeActorNet(this);
	MulticastLidPulse(false);
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s puts %s in (%d/%d, %.1f kg inside)"), *GetName(), *GetNameSafe(User), *Item->GetName(),
		Contents.Num(), GetRules()->MaxItems, GetContentMassKg());
	return true;
}

FText ALootCrateActor::GetStorePrompt(const APawn* User, const AActor* Item) const
{
	const ULootCrateRules* R = GetRules();
	return FText::FormatNamed(R->StorePrompt, TEXT("Count"), Contents.Num(), TEXT("Max"), R->MaxItems);
}

int32 ALootCrateActor::EmptyContents(const FVector& Toward)
{
	if (!HasAuthority())
	{
		return 0;
	}
	PurgeContents();
	if (Contents.IsEmpty())
	{
		return 0;
	}
	const ULootCrateRules* R = GetRules();
	const TArray<FLootCrateEntry> Leaving = MoveTemp(Contents);
	Contents.Reset();
	UpdateMass();

	// in a fan in front of the crate, toward the player
	const FVector Center = Body->GetComponentLocation();
	const double HalfHeight = Body->GetScaledBoxExtent().Z;
	FVector Facing = (Toward - Center).GetSafeNormal2D();
	if (Facing.IsNearlyZero())
	{
		Facing = GetActorForwardVector().GetSafeNormal2D();
	}
	const FRotator Frame(0.0, Facing.Rotation().Yaw, 0.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LootCrateEmpty), false, this);
	for (const FLootCrateEntry& Entry : Leaving)
	{
		Params.AddIgnoredActor(Entry.Item.Get());
	}

	int32 Index = 0;
	int32 Out = 0;
	for (const FLootCrateEntry& Entry : Leaving)
	{
		AActor* Item = Entry.Item;
		if (!IsValid(Item))
		{
			continue;
		}
		const FVector2D Spot = LootCrateMath::EmptySpot(Index++, Leaving.Num(), R->EmptyDistanceCm, R->EmptySpreadDeg, R->EmptyPerRow, R->EmptyRowStepCm);
		// from the top of the crate: never through a wall, a hull or a bulwark
		const FVector From = Center + FVector(0.0, 0.0, HalfHeight);
		FVector Target = From + Frame.RotateVector(FVector(Spot.X, Spot.Y, 0.0));
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, Target, ECC_Visibility, Params))
		{
			Target = Hit.Location - (Target - From).GetSafeNormal() * 10.0;
		}
		// on what lies under that place (sand, a deck, the floor of the barque), a short drop
		double GroundZ = Center.Z - HalfHeight;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Target + FVector(0.0, 0.0, 50.0), Target - FVector(0.0, 0.0, HalfHeight * 2.0 + 200.0),
			ECC_Visibility, Params))
		{
			GroundZ = Hit.ImpactPoint.Z;
		}
		Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		const FBox Bounds = Item->GetComponentsBoundingBox(true);
		const double Lift = Bounds.IsValid ? Item->GetActorLocation().Z - Bounds.Min.Z : 0.0;
		Item->SetActorLocation(FVector(Target.X, Target.Y, GroundZ + Lift + R->EmptyDropHeightCm), false, nullptr, ETeleportType::TeleportPhysics);
		ApplyStoredState(Item, false);
		// shown and solid again: its own replication takes it out on every client now (T-316)
		UNetRestComponent::WakeActorNet(Item);
		++Out;
	}
	if (!IsActorBeingDestroyed())
	{
		// the crate may sleep (dormant): its emptied contents and its lid reach every client now (T-316)
		UNetRestComponent::WakeActorNet(this);
		MulticastLidPulse(true);
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: emptied, %d object(s) out"), *GetName(), Out);
	return Out;
}

void ALootCrateActor::ApplyStoredState(AActor* Item, bool bStored)
{
	if (!IsValid(Item))
	{
		return;
	}
	UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Item->GetRootComponent());
	if (bStored)
	{
		// the body off before the attachment: a simulated body would leave its parent
		if (Root)
		{
			Root->SetSimulatePhysics(false);
		}
		Item->SetActorEnableCollision(false);
		Item->SetActorHiddenInGame(true);
		return;
	}
	Item->SetActorHiddenInGame(false);
	Item->SetActorEnableCollision(true);
	// the server simulates it again; the clients follow its replicated movement (the engine syncs their simulation)
	if (Root && Item->HasAuthority())
	{
		Root->SetSimulatePhysics(true);
		Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Root->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		Root->WakeRigidBody();
	}
}

void ALootCrateActor::OnRep_Contents(const TArray<FLootCrateEntry>& PreviousContents)
{
	// the collision of an actor does not replicate: a hidden object left solid inside the box would shove it
	TSet<const AActor*> Inside;
	for (const FLootCrateEntry& Entry : Contents)
	{
		if (IsValid(Entry.Item))
		{
			Inside.Add(Entry.Item.Get());
			ApplyStoredState(Entry.Item, true);
		}
	}
	for (const FLootCrateEntry& Entry : PreviousContents)
	{
		if (IsValid(Entry.Item) && !Inside.Contains(Entry.Item.Get()))
		{
			PendingRestore.AddUnique(Entry.Item.Get());
		}
	}
	UpdateMass();
	if (!PendingRestore.IsEmpty())
	{
		PendingRestoreSeconds = 0.0f;
		SetActorTickEnabled(true);
	}
}

void ALootCrateActor::RestorePending()
{
	const bool bTimedOut = PendingRestoreSeconds >= RestoreTimeoutSeconds;
	for (int32 Index = PendingRestore.Num() - 1; Index >= 0; --Index)
	{
		AActor* Item = PendingRestore[Index].Get();
		const AActor* Parent = IsValid(Item) ? Item->GetAttachParentActor() : nullptr;
		// gone, back inside, or already put in another container (which hides it itself)
		const bool bInsideAgain = IsValid(Item) && (Contents.ContainsByPredicate([Item](const FLootCrateEntry& Entry) { return Entry.Item == Item; })
			|| (Parent && Parent != this && Parent->Implements<ULootContainer>()));
		if (!IsValid(Item) || bInsideAgain)
		{
			PendingRestore.RemoveAtSwap(Index);
			continue;
		}
		// still on the crate: its own replication has not landed yet (shown and solid now, it would shove the box)
		if (Parent == this)
		{
			if (!bTimedOut)
			{
				continue;
			}
			Item->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
		ApplyStoredState(Item, false);
		PendingRestore.RemoveAtSwap(Index);
	}
}

// ---------------------------------------------------------------- interaction

bool ALootCrateActor::WantsEmpty(const APawn* User) const
{
	const ACharacter* Character = Cast<ACharacter>(User);
	return GetRules()->bEmptyWhenCrouched && !Contents.IsEmpty() && Character && Character->bIsCrouched;
}

FVector ALootCrateActor::GetClosestBodyPoint(const FVector& Location) const
{
	const FTransform& Transform = Body->GetComponentTransform();
	const FVector Extent = Body->GetUnscaledBoxExtent();
	const FVector Local = Transform.InverseTransformPosition(Location);
	return Transform.TransformPosition(FVector(FMath::Clamp(Local.X, -Extent.X, Extent.X), FMath::Clamp(Local.Y, -Extent.Y, Extent.Y),
		FMath::Clamp(Local.Z, -Extent.Z, Extent.Z)));
}

FText ALootCrateActor::GetInteractionPrompt_Implementation(APawn* User) const
{
	const ULootCrateRules* R = GetRules();
	if (WantsEmpty(User))
	{
		return FText::FormatNamed(R->EmptyPrompt, TEXT("Count"), Contents.Num());
	}
	return FText::FormatNamed(R->CarryPrompt, TEXT("Count"), Contents.Num(), TEXT("Mass"), FMath::RoundToInt(GetTotalMassKg()));
}

bool ALootCrateActor::CanInteract_Implementation(APawn* User) const
{
	return IsValid(User) && !IsActorBeingDestroyed() && Body->IsSimulatingPhysics();
}

void ALootCrateActor::Interact_Implementation(APawn* User)
{
	if (!HasAuthority() || !User)
	{
		return;
	}
	PurgeContents();
	if (WantsEmpty(User))
	{
		EmptyContents(User->GetActorLocation());
		return;
	}
	// lifted like any heavy object by the player's grab component (alone, or with those already holding it)
	UIslanderGrabComponent* Grab = User->FindComponentByClass<UIslanderGrabComponent>();
	if (!Grab)
	{
		return;
	}
	FVector Eyes, Direction;
	Grab->GetEyes(Eyes, Direction);
	if (!Grab->GrabFromInteraction(Body, GetClosestBodyPoint(Eyes)))
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: %s cannot lift it now"), *GetName(), *User->GetName());
	}
}

// ---------------------------------------------------------------- lid and objects coming out (every machine)

void ALootCrateActor::MulticastLidPulse_Implementation(bool bEmptied)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	LidTime = 0.0f;
	SetActorTickEnabled(true);
	const ULootCrateRules* R = GetRules();
	if (USoundBase* Sound = bEmptied ? R->EmptySound.Get() : R->StoreSound.Get())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Body->GetComponentLocation(), 1.0f, 1.0f, 0.0f, R->SoundAttenuation.Get());
	}
}

void ALootCrateActor::SetLidAngle(float AngleDeg)
{
	LidHinge->SetRelativeRotation(FRotator(0.0f, 0.0f, LidRollSign * AngleDeg));
}

void ALootCrateActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (LidTime >= 0.0f)
	{
		const ULootCrateRules* R = GetRules();
		LidTime += DeltaSeconds;
		const float Open = LootCrateMath::LidOpenFraction(LidTime, R->LidOpenSeconds, R->LidHoldSeconds, R->LidCloseSeconds);
		if (Open < 0.0f)
		{
			LidTime = -1.0f;
		}
		SetLidAngle(FMath::Max(Open, 0.0f) * R->LidOpenAngleDeg);
	}
	if (!PendingRestore.IsEmpty())
	{
		PendingRestoreSeconds += DeltaSeconds;
		RestorePending();
	}
	if (LidTime < 0.0f && PendingRestore.IsEmpty())
	{
		SetActorTickEnabled(false);
	}
}
