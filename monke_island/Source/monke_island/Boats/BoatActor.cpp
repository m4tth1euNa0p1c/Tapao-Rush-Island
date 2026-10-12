// monke_island

#include "Boats/BoatActor.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Items/NetRestComponent.h"
#include "Boats/BoatPhysics.h"
#include "Boats/BoatRiderComponent.h"
#include "Boats/BoatRules.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Props/LootCrate/LootContainer.h"
#include "UObject/ConstructorHelpers.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"

namespace
{
	/** Boats keep their buoyancy and rowing physics, but never receive storm wind forces. */
	const FName BoatWindShelteredTag(TEXT("WindSheltered"));

	/** Flattened forward axis of a transform */
	FVector FlatForward(const FTransform& Transform)
	{
		const FVector X = Transform.GetUnitAxis(EAxis::X);
		return FVector(X.X, X.Y, 0.0).GetSafeNormal();
	}
}

ABoatActor::ABoatActor()
{
	// physics forces are applied every frame before the physics step
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	SetReplicateMovement(true);
	// ADR-003 B-07 (T-295, review R-05): the clients' hulls are driven toward the server's by velocity, the same
	// forces applied in between; DA_BoatRules can fall back on the engine default. Not 100 updates per second
	SetPhysicsReplicationMode(EPhysicsReplicationMode::PredictiveInterpolation);
	SetNetUpdateFrequency(30.0f);

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	SetRootComponent(Hull);
	Hull->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	// the third person camera boom must not zoom in on the player when the hull passes between them
	Hull->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Hull->SetSimulatePhysics(true);
	Hull->SetCanEverAffectNavigation(false);
	// owner request 2026-10-04: a player never lifts the boat with the grab (E); it is pushed on the sand and rowed
	Tags.AddUnique(UIslanderGrabComponent::NoGrabTag);
	Tags.AddUnique(BoatWindShelteredTag);

	OarLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OarLeft"));
	OarLeft->SetupAttachment(Hull, TEXT("Oarlock_L"));
	OarLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OarLeft->SetCanEverAffectNavigation(false);

	OarRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OarRight"));
	OarRight->SetupAttachment(Hull, TEXT("Oarlock_R"));
	OarRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OarRight->SetCanEverAffectNavigation(false);

	RowingCrate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RowingCrate"));
	RowingCrate->SetupAttachment(Hull);
	RowingCrate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RowingCrate->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	BilgeWater = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BilgeWater"));
	BilgeWater->SetupAttachment(Hull);
	BilgeWater->SetStaticMesh(PlaneMesh.Object);
	BilgeWater->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BilgeWater->SetCastShadow(false);
	BilgeWater->SetRelativeLocation(FVector(-20.0f, 0.0f, -9.0f));
	BilgeWater->SetRelativeScale3D(FVector(3.8f, 1.45f, 1.0f));
	BilgeWater->SetVisibility(false);

	// shape only: the load is measured with an explicit overlap query
	CargoVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoVolume"));
	CargoVolume->SetupAttachment(Hull);
	CargoVolume->SetBoxExtent(FVector(265.0f, 100.0f, 85.0f));
	CargoVolume->SetRelativeLocation(FVector(-15.0f, 0.0f, 65.0f));
	CargoVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CargoVolume->SetCanEverAffectNavigation(false);
}

void ABoatActor::BeginPlay()
{
	Super::BeginPlay();
	// Existing Blueprints and placed boats may override the native tag array: enforce the wind exclusion at runtime.
	Tags.AddUnique(BoatWindShelteredTag);

	WaterSubsystem = UIslandWaterSubsystem::Get(this);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_BoatRules: using the default rules"), *GetName());
		Rules = NewObject<UBoatRules>(this);
	}
	// how the clients follow the hull, how often it is sent (T-295, ADR-003 B-07)
	SetPhysicsReplicationMode(Rules->PhysicsReplicationMode);
	if (HasAuthority())
	{
		SetNetUpdateFrequency(Rules->NetUpdateFrequency);
	}
	// the crate under the rower (cosmetic, no collision: the seated rower does not collide either)
	RowingCrate->SetStaticMesh(Rules->bOnlyRowerSits ? Rules->RowingCrateMesh.Get() : nullptr);
	RowingCrate->SetRelativeTransform(Rules->RowingCrateTransform);

	SeatSockets.Reset();
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const FName Socket(*FString::Printf(TEXT("%s%d"), *Rules->SeatSocketPrefix, Index));
		if (!Hull->DoesSocketExist(Socket))
		{
			break;
		}
		SeatSockets.Add(Socket);
	}
	if (HasAuthority())
	{
		SeatOccupants.SetNum(SeatSockets.Num());
	}

	// buoyancy points along the keel line, narrower toward the bow; keel points for the ground contact
	const float HalfLength = Rules->HullSize.X * 0.5f;
	const float HalfBeam = Rules->HullSize.Y * 0.5f;
	FloatPoints.Reset();
	for (const float Fx : {-0.85f, -0.45f, 0.0f, 0.4f, 0.75f})
	{
		const float Width = HalfBeam * 0.62f * FMath::Max(0.3f, 1.0f - FMath::Square(FMath::Max(0.0f, Fx)));
		FloatPoints.Add(FVector(Fx * HalfLength, -Width, Rules->KeelLocalZ));
		FloatPoints.Add(FVector(Fx * HalfLength, Width, Rules->KeelLocalZ));
	}
	KeelPoints = {
		FVector(-0.78f * HalfLength, 0.0f, Rules->KeelLocalZ),
		FVector(0.0f, 0.0f, Rules->KeelLocalZ),
		FVector(0.62f * HalfLength, 0.0f, Rules->KeelLocalZ + 12.0f)};
	TrailDistance.Init(Rules->TrailSpacing, KeelPoints.Num());

	Hull->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Hull->SetMassOverrideInKg(NAME_None, Rules->HullMassKg, true);
	Hull->SetCenterOfMass(Rules->CenterOfMassOffset);
	Hull->SetAngularDamping(Rules->AngularDamping);
	Hull->SetLinearDamping(0.01f);
	if (FBodyInstance* Body = Hull->GetBodyInstance())
	{
		Body->SetMaxDepenetrationVelocity(Rules->MaxDepenetrationVelocity);
	}
}

void ABoatActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		for (ACharacter* Occupant : TArray<ACharacter*>(SeatOccupants))
		{
			if (IsValid(Occupant))
			{
				StandCharacter(Occupant);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ABoatActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABoatActor, SeatOccupants);
	DOREPLIFETIME(ABoatActor, RowerSeat);
	DOREPLIFETIME(ABoatActor, RowInput);
	DOREPLIFETIME(ABoatActor, LoadKg);
	DOREPLIFETIME(ABoatActor, BilgeWaterKg);
	DOREPLIFETIME(ABoatActor, bSunk);
}

float ABoatActor::GetMaxLoadKg() const
{
	return Rules ? Rules->MaxLoadKg : 0.0f;
}

float ABoatActor::GetBilgeFraction() const
{
	return Rules && Rules->FloodCapacityKg > 0.0f ? FMath::Clamp(BilgeWaterKg / Rules->FloodCapacityKg, 0.0f, 1.0f) : 0.0f;
}

float ABoatActor::GetSeatedCapsuleOffset() const
{
	return Rules ? Rules->SeatedCapsuleOffsetCm : 0.0f;
}

void ABoatActor::SetOarAnimOverride(float ThetaDeg, float PhiDeg)
{
	OarAnimOverride = FVector2D(ThetaDeg, PhiDeg);
	OarAnimOverrideTime = GetWorld()->GetTimeSeconds();
}

FVector ABoatActor::GetOarHandle(int32 Side) const
{
	const UStaticMeshComponent* Oar = Side == 0 ? OarLeft : OarRight;
	if (!Oar)
	{
		return GetActorLocation();
	}
	// SM_Barque_Oar: along +X from its pivot, handle end at -78 cm, the hand around -70 cm (barque.py oar_pivot)
	return Oar->GetComponentTransform().TransformPosition(FVector(-70.0, 0.0, 0.0));
}

bool ABoatActor::IsRower(const ACharacter* Character) const
{
	return Character && SeatOccupants.IsValidIndex(RowerSeat) && SeatOccupants[RowerSeat] == Character;
}

int32 ABoatActor::FindFreeSeat(const FVector& Location, float MaxDistance) const
{
	if (bSunk)
	{
		return INDEX_NONE;
	}

	int32 Best = INDEX_NONE;
	double BestDistSq = FMath::Square(static_cast<double>(MaxDistance));
	for (int32 Index = 0; Index < SeatSockets.Num(); ++Index)
	{
		if (SeatOccupants.IsValidIndex(Index) && SeatOccupants[Index])
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Hull->GetSocketLocation(SeatSockets[Index]), Location);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Index;
		}
	}
	return Best;
}

bool ABoatActor::SitCharacter(ACharacter* Character, int32 SeatIndex)
{
	if (!HasAuthority() || !IsValid(Character) || bSunk || !SeatOccupants.IsValidIndex(SeatIndex) || SeatOccupants[SeatIndex])
	{
		return false;
	}
	UBoatRiderComponent* Rider = Character->FindComponentByClass<UBoatRiderComponent>();
	if (!Rider || Rider->IsSeated())
	{
		return false;
	}
	// a knocked out or dead body does not sit: the seat guard would throw it off again (T-182)
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Character); Islander && Islander->IsRagdoll())
	{
		return false;
	}
	if (const UIslanderHealthComponent* Health = Character->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
	{
		return false;
	}
	// server-side reach check: a client cannot sit in a boat across the map
	const float Reach = Rules->SeatReach + 100.0f;
	if (FVector::DistSquared(Hull->GetSocketLocation(SeatSockets[SeatIndex]), Character->GetActorLocation()) > FMath::Square(Reach))
	{
		return false;
	}

	// only the rower sits: the others ride standing on the boat
	if (Rules->bOnlyRowerSits && RowerSeat != INDEX_NONE)
	{
		return false;
	}

	SeatOccupants[SeatIndex] = Character;
	if (RowerSeat == INDEX_NONE)
	{
		RowerSeat = SeatIndex;
		RowInput = FVector2D::ZeroVector;
	}
	Rider->SetSeat(this, SeatIndex);
	return true;
}

void ABoatActor::StandCharacter(ACharacter* Character)
{
	if (!HasAuthority() || !Character)
	{
		return;
	}

	const int32 SeatIndex = SeatOccupants.IndexOfByKey(Character);
	if (SeatIndex != INDEX_NONE)
	{
		SeatOccupants[SeatIndex] = nullptr;
		if (RowerSeat == SeatIndex)
		{
			// the oars go to the next seated player
			RowerSeat = SeatOccupants.IndexOfByPredicate([](const TObjectPtr<ACharacter>& Occupant) { return Occupant != nullptr; });
			RowInput = FVector2D::ZeroVector;
		}
	}
	if (UBoatRiderComponent* Rider = Character->FindComponentByClass<UBoatRiderComponent>())
	{
		if (Rider->GetSeatedBoat() == this)
		{
			Rider->SetSeat(nullptr, INDEX_NONE);
		}
		else if (Character->GetAttachParentActor() == this)
		{
			// hanging on this hull without sitting in it (T-182): taken off as well
			Rider->LeaveBoat();
		}
	}
}

void ABoatActor::SetRowInput(const ACharacter* Rower, const FVector2D& Input)
{
	if (!HasAuthority() || bSunk || !IsRower(Rower) || !FMath::IsFinite(Input.X) || !FMath::IsFinite(Input.Y))
	{
		return;
	}
	RowInput = FVector2D(FMath::Clamp(Input.X, -1.0f, 1.0f), FMath::Clamp(Input.Y, -1.0f, 1.0f));
}

FText ABoatActor::GetInteractionPrompt_Implementation(APawn* User) const
{
	const UBoatRiderComponent* Rider = User ? User->FindComponentByClass<UBoatRiderComponent>() : nullptr;
	if (Rider && Rider->GetSeatedBoat() == this)
	{
		return NSLOCTEXT("Boat", "StandUp", "Se lever");
	}
	return RowerSeat == INDEX_NONE
		? NSLOCTEXT("Boat", "TakeOars", "Prendre les rames")
		: NSLOCTEXT("Boat", "SitDown", "S'asseoir dans la barque");
}

bool ABoatActor::CanInteract_Implementation(APawn* User) const
{
	const UBoatRiderComponent* Rider = User ? User->FindComponentByClass<UBoatRiderComponent>() : nullptr;
	if (!Rider || !Rules)
	{
		return false;
	}
	if (Rider->IsSeated())
	{
		return Rider->GetSeatedBoat() == this;
	}
	if (Rules->bOnlyRowerSits && RowerSeat != INDEX_NONE)
	{
		return false;
	}
	const UIslanderHealthComponent* Health = User->FindComponentByClass<UIslanderHealthComponent>();
	return (!Health || !Health->IsDead()) && FindFreeSeat(User->GetActorLocation(), Rules->SeatReach) != INDEX_NONE;
}

void ABoatActor::Interact_Implementation(APawn* User)
{
	ACharacter* Character = Cast<ACharacter>(User);
	if (!HasAuthority() || !Character || !IIslanderInteractable::Execute_CanInteract(this, User))
	{
		return;
	}
	const UBoatRiderComponent* Rider = Character->FindComponentByClass<UBoatRiderComponent>();
	if (Rider && Rider->GetSeatedBoat() == this)
	{
		StandCharacter(Character);
		return;
	}
	SitCharacter(Character, FindFreeSeat(Character->GetActorLocation(), Rules->SeatReach));
}

void ABoatActor::ValidateSeats()
{
	for (int32 Index = 0; Index < SeatOccupants.Num(); ++Index)
	{
		ACharacter* Occupant = SeatOccupants[Index];
		if (!Occupant)
		{
			continue;
		}
		const UIslanderHealthComponent* Health = IsValid(Occupant) ? Occupant->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
		if (!IsValid(Occupant) || (Health && Health->IsDead()))
		{
			if (IsValid(Occupant))
			{
				StandCharacter(Occupant);
			}
			else
			{
				SeatOccupants[Index] = nullptr;
				if (RowerSeat == Index)
				{
					RowerSeat = INDEX_NONE;
					RowInput = FVector2D::ZeroVector;
				}
			}
		}
	}
}

void ABoatActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Rules || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const float SeaLevel = WaterSubsystem ? WaterSubsystem->GetSeaLevel() : 0.0f;

	if (HasAuthority())
	{
		ValidateSeats();
	}

	TArray<ACharacter*> People;
	float ObjectsKg = 0.0f;
	GatherPeople(People, ObjectsKg);
	if (HasAuthority())
	{
		KeepCargoAboard(DeltaSeconds);
	}

	// same forces everywhere from replicated state; the server's simulation corrects the clients
	if (Hull->IsSimulatingPhysics())
	{
		ApplyWaterForces(DeltaSeconds, SeaLevel);
		ApplyLoadForces(People);
		ApplyRowing(DeltaSeconds);
		if (HasAuthority())
		{
			ApplyShoreForces(DeltaSeconds);
		}
	}

	if (HasAuthority())
	{
		UpdateLoad(DeltaSeconds, People.Num(), ObjectsKg, SeaLevel);
	}

	UpdateGroundContact(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);
	UpdateSounds(DeltaSeconds);

	// tuning trace (enable with Logmonke_island Verbose)
	LoadUpdateTimer += DeltaSeconds;
	if (LoadUpdateTimer >= 2.0f)
	{
		LoadUpdateTimer = 0.0f;
		UE_LOG(Logmonke_island, Verbose, TEXT("BoatDiag %s %s keel-sea=%.1f cm rot=%s v=%.0f cm/s load=%.0f water=%.0f rower=%d people=%d"),
			*GetName(), HasAuthority() ? TEXT("srv") : TEXT("cli"),
			static_cast<float>(Hull->GetComponentTransform().TransformPosition(FVector(0.0f, 0.0f, Rules->KeelLocalZ)).Z) - SeaLevel,
			*GetActorRotation().ToCompactString(), Hull->GetPhysicsLinearVelocity().Size(), LoadKg, BilgeWaterKg, RowerSeat, People.Num());
	}
}

void ABoatActor::UpdateSounds(float DeltaSeconds)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// the oars: a splash as a blade bites the water, a drip as it leaves it (the drive part of each stroke)
	const bool bRowing = RowerSeat != INDEX_NONE && !bSunk;
	const FVector2D Drives = BoatPhysics::OarDrives(RowInput);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Drive = Side == 0 ? Drives.X : Drives.Y;
		const float Phase = Side == 0 ? StrokePhase.X : StrokePhase.Y;
		const bool bIn = bRowing && !FMath::IsNearlyZero(Drive) && Phase < Rules->StrokeDriveFraction;
		if (bIn == bBladeWasIn[Side])
		{
			continue;
		}
		bBladeWasIn[Side] = bIn;
		USoundBase* Sound = bIn ? Rules->OarSplashSound.Get() : Rules->OarDripSound.Get();
		const UStaticMeshComponent* Oar = Side == 0 ? OarLeft.Get() : OarRight.Get();
		if (Sound && Oar)
		{
			const FVector Blade = Oar->GetComponentTransform().TransformPosition(FVector(Rules->OarBladeCenterCm, 0.0, 0.0));
			UGameplayStatics::PlaySoundAtLocation(this, Sound, Blade, FMath::FRandRange(0.8f, 1.0f), FMath::FRandRange(0.9f, 1.1f), 0.0f,
				Rules->OarSoundAttenuation);
		}
	}

	// the water along the hull, with the speed on the water; none on the sand or sunk
	if (!Rules->WakeSound)
	{
		return;
	}
	if (!WakeAudio)
	{
		WakeAudio = UGameplayStatics::SpawnSoundAttached(Rules->WakeSound, Hull, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
			true, 0.001f, 1.0f, 0.0f, Rules->OarSoundAttenuation, nullptr, false);
		if (!WakeAudio)
		{
			return;
		}
	}
	const FVector Velocity = Hull->GetComponentVelocity();
	const float Afloat = KeelPoints.Num() > 0 ? 1.0f - static_cast<float>(GroundedPoints) / KeelPoints.Num() : 1.0f;
	const float Target = bSunk ? 0.0f : FMath::Clamp(static_cast<float>(Velocity.Size2D()) / Rules->WakeFullSpeed, 0.0f, 1.0f) * Afloat;
	WakeLevel = FMath::FInterpConstantTo(WakeLevel, Target, DeltaSeconds, 1.0f / Rules->WakeFadeSeconds);
	WakeAudio->SetVolumeMultiplier(FMath::Max(WakeLevel * Rules->WakeVolume, 0.001f));
	WakeAudio->SetPitchMultiplier(FMath::Lerp(static_cast<float>(Rules->WakePitch.X), static_cast<float>(Rules->WakePitch.Y), WakeLevel));
}

double ABoatActor::GetSwellTime() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

void ABoatActor::ReleaseGluedCargo()
{
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FLooseCargo>& Entry : LooseCargo)
	{
		const UPrimitiveComponent* Component = Entry.Key.Get();
		UNetRestComponent* Rest = Entry.Value.bGlued && Component && Component->GetOwner() ? Component->GetOwner()->FindComponentByClass<UNetRestComponent>() : nullptr;
		if (Rest && Rest->GetGlueBase() == Hull)
		{
			Rest->Release();
		}
	}
}

void ABoatActor::KeepCargoAboard(float DeltaSeconds)
{
	if (!Rules->bContainLooseCargo || bSunk)
	{
		ReleaseGluedCargo();
		LooseCargo.Reset();
		return;
	}
	// what lies in the hull now: loose physics objects (an object held by a player ignores PhysicsBody: it is carried)
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BoatLooseCargo), false, this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, CargoVolume->GetComponentLocation(), CargoVolume->GetComponentQuat(),
		ObjectTypes, CargoVolume->GetCollisionShape(), Params);
	auto IsHeld = [](const UPrimitiveComponent* Component) { return Component->GetCollisionResponseToChannel(ECC_PhysicsBody) == ECR_Ignore; };
	TSet<const UPrimitiveComponent*> Inside;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Component = Overlap.GetComponent();
		if (Component && Component->IsSimulatingPhysics() && !Cast<APawn>(Overlap.GetActor()) && !IsHeld(Component))
		{
			Inside.Add(Component);
			LooseCargo.FindOrAdd(Component);
		}
	}

	const FTransform& HullTransform = Hull->GetComponentTransform();
	const FVector HullAngular = Hull->GetPhysicsAngularVelocityInRadians();
	for (auto It = LooseCargo.CreateIterator(); It; ++It)
	{
		UPrimitiveComponent* Component = It->Key.Get();
		FLooseCargo& Cargo = It->Value;
		AActor* Item = Component ? Component->GetOwner() : nullptr;
		UNetRestComponent* Rest = Item ? Item->FindComponentByClass<UNetRestComponent>() : nullptr;
		// glued by its network rest, every machine carries it (T-295); once a player takes it the rest frees it
		if (Cargo.bGlued)
		{
			if (!Rest || Rest->GetGlueBase() != Hull)
			{
				It.RemoveCurrent();
			}
			continue;
		}
		// taken by a player, gone, or thrown out before it came to rest: free again
		if (!Component || !Component->IsSimulatingPhysics() || IsHeld(Component) || (!Cargo.bStowed && !Inside.Contains(Component)))
		{
			It.RemoveCurrent();
			continue;
		}
		const FVector HullVelocity = Hull->GetPhysicsLinearVelocityAtPoint(Component->GetComponentLocation());
		if (Cargo.bStowed)
		{
			// stowed: it rides where it lies, at the speed of the hull; acceleration, swell and wind do not move it
			const FTransform World = Cargo.Local * HullTransform;
			Component->SetWorldLocationAndRotation(World.GetLocation(), World.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
			Component->SetPhysicsLinearVelocity(Hull->GetPhysicsLinearVelocityAtPoint(World.GetLocation()));
			Component->SetPhysicsAngularVelocityInRadians(HullAngular);
			continue;
		}
		// at rest in the hull for a moment: stowed
		const float Relative = static_cast<float>((Component->GetPhysicsLinearVelocity() - HullVelocity).Size());
		Cargo.RestTime = Relative <= Rules->CargoStowMaxSpeed ? Cargo.RestTime + DeltaSeconds : 0.0f;
		if (Cargo.RestTime >= Rules->CargoStowDelay)
		{
			// with a network rest (props, corpses): glued to the hull on every machine, its movement no longer sent, the
			// clients carry it with their own hull instead of sliding it back by corrections (T-295, review R-05)
			if (Rest && Item->GetRootComponent() == Component)
			{
				Rest->GlueTo(Hull);
				if (Rest->GetGlueBase() == Hull)
				{
					Cargo.bGlued = true;
					continue;
				}
			}
			Cargo.bStowed = true;
			Cargo.Local = Component->GetComponentTransform().GetRelativeTransform(HullTransform);
			Cargo.Local.SetScale3D(Component->GetComponentScale());
		}
	}
}

void ABoatActor::GetCargoAboard(TArray<AActor*>& OutCargo) const
{
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BoatCargoAboard), false, this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, CargoVolume->GetComponentLocation(), CargoVolume->GetComponentQuat(),
		ObjectTypes, CargoVolume->GetCollisionShape(), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		// a free object lying aboard; one held by a player ignores PhysicsBody: it is carried, not stowed
		if (Actor && Actor != this && Component && Component->IsSimulatingPhysics() && !Cast<APawn>(Actor)
			&& Component->GetCollisionResponseToChannel(ECC_PhysicsBody) != ECR_Ignore)
		{
			// a loot crate aboard brings what it holds (hidden inside, out of reach of the overlap)
			LootContainers::AppendWithContents(Actor, OutCargo);
		}
	}
}

void ABoatActor::GatherPeople(TArray<ACharacter*>& OutPeople, float& OutObjectsKg) const
{
	OutObjectsKg = 0.0f;

	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_Pawn);
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BoatCargo), false, this);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, CargoVolume->GetComponentLocation(), CargoVolume->GetComponentQuat(),
		ObjectTypes, CargoVolume->GetCollisionShape(), Params);

	TSet<const AActor*> Seen;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || Actor == this || Seen.Contains(Actor))
		{
			continue;
		}
		if (ACharacter* Character = Cast<ACharacter>(Actor))
		{
			const UIslanderHealthComponent* Health = Character->FindComponentByClass<UIslanderHealthComponent>();
			if (!Health || !Health->IsDead())
			{
				Seen.Add(Actor);
				OutPeople.Add(Character);
			}
			continue;
		}
		const UPrimitiveComponent* Primitive = Overlap.GetComponent();
		if (Primitive && Primitive->IsSimulatingPhysics())
		{
			Seen.Add(Actor);
			// a loot crate's body weighs what it holds too (T-258: the objects inside have no body of their own)
			OutObjectsKg += Primitive->GetMass();
		}
	}
	for (ACharacter* Occupant : SeatOccupants)
	{
		if (IsValid(Occupant) && !Seen.Contains(Occupant))
		{
			Seen.Add(Occupant);
			OutPeople.Add(Occupant);
		}
	}
}

void ABoatActor::ApplyWaterForces(float DeltaSeconds, float SeaLevel)
{
	const FTransform Transform = Hull->GetComponentTransform();
	const float BuoyancyFactor = bSunk ? Rules->SunkBuoyancyFactor : 1.0f;
	const int32 PointCount = FloatPoints.Num();

	float Immersion = 0.0f;
	for (const FVector& Local : FloatPoints)
	{
		const FVector Point = Transform.TransformPosition(Local);
		// local surface: the sea, or a flood on top of it (UIslandWaterSubsystem::GetWaterSurfaceAt)
		const float Surface = WaterSubsystem ? WaterSubsystem->GetWaterSurfaceAt(Point) : SeaLevel;
		const float Depth = Surface - static_cast<float>(Point.Z);
		if (Depth <= 0.0f)
		{
			continue;
		}
		const float Fraction = FMath::Clamp(Depth / Rules->HullDepthCm, 0.0f, 1.0f);
		Immersion += Fraction;

		float Up = BoatPhysics::PointBuoyancy(Depth, Rules->HullDepthCm, Rules->FloatCapacityKg, PointCount) * BuoyancyFactor;
		const FVector PointVelocity = Hull->GetPhysicsLinearVelocityAtPoint(Point);
		Up -= Rules->VerticalDampingPerPoint * static_cast<float>(PointVelocity.Z) * FMath::Min(1.0f, Fraction * 4.0f);
		Hull->AddForceAtLocation(FVector(0.0f, 0.0f, Up), Point);
	}

	const float Wetness = PointCount > 0 ? FMath::Clamp(3.0f * Immersion / PointCount, 0.0f, 1.0f) : 0.0f;
	if (Wetness <= 0.0f)
	{
		return;
	}

	// water drag: little along the keel, a lot across it
	const FVector Forward = FlatForward(Transform);
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Velocity = Hull->GetPhysicsLinearVelocity();
	const float Along = static_cast<float>(Velocity | Forward);
	const float Across = static_cast<float>(Velocity | Right);
	// a loaded boat sits deeper and drags more water: heavier is slower
	const float LoadScale = BoatPhysics::LoadDragScale(LoadKg, Rules->MaxLoadKg, Rules->FullLoadDragScale);
	const FVector Drag = -Forward * (LoadScale * (Rules->ForwardDrag * Along + Rules->ForwardQuadDrag * Along * FMath::Abs(Along)))
		- Right * (Rules->LateralDrag * Across);
	Hull->AddForce(Drag * Wetness);

	// gentle swell so the boat is never perfectly still, on the server clock: in phase on every machine (T-295)
	if (!bSunk)
	{
		const double Time = GetSwellTime();
		const float Roll = static_cast<float>(Rules->SwellTorqueNm.X * FMath::Sin(UE_DOUBLE_TWO_PI * Time / FMath::Max(Rules->SwellPeriod.X, 0.1)));
		const float Pitch = static_cast<float>(Rules->SwellTorqueNm.Y * FMath::Sin(UE_DOUBLE_TWO_PI * Time / FMath::Max(Rules->SwellPeriod.Y, 0.1) + 1.3));
		const FVector LocalTorque(Roll, Pitch, 0.0f);
		Hull->AddTorqueInRadians(Transform.TransformVectorNoScale(LocalTorque) * BoatPhysics::NewtonMetreToUnreal * Wetness);
	}
}

void ABoatActor::ApplyLoadForces(const TArray<ACharacter*>& People)
{
	// characters standing on a physics body do not weigh on it: add their weight where they are
	for (const ACharacter* Person : People)
	{
		Hull->AddForceAtLocation(FVector(0.0f, 0.0f, -Rules->PersonMassKg * BoatPhysics::GravityCm), Person->GetActorLocation());
	}
	if (BilgeWaterKg > 0.0f)
	{
		const FVector At = Hull->GetComponentTransform().TransformPosition(FVector(-20.0f, 0.0f, -15.0f));
		Hull->AddForceAtLocation(FVector(0.0f, 0.0f, -BilgeWaterKg * BoatPhysics::GravityCm), At);
	}
}

void ABoatActor::ApplyRowing(float DeltaSeconds)
{
	if (RowerSeat == INDEX_NONE || bSunk)
	{
		return;
	}

	const FVector2D Drives = BoatPhysics::OarDrives(RowInput);
	const bool bLeft = !FMath::IsNearlyZero(Drives.X);
	const bool bRight = !FMath::IsNearlyZero(Drives.Y);
	const float Step = DeltaSeconds / Rules->StrokePeriod;
	if (bLeft)
	{
		StrokePhase.X = FMath::Fmod(StrokePhase.X + Step, 1.0f);
	}
	if (bRight)
	{
		StrokePhase.Y = bLeft ? StrokePhase.X : FMath::Fmod(StrokePhase.Y + Step, 1.0f);
	}

	const FTransform Transform = Hull->GetComponentTransform();
	const FVector Forward = FlatForward(Transform);
	const float Speed = static_cast<float>(Hull->GetPhysicsLinearVelocity() | Forward);

	const float MaxSpeed = Rules->MaxRowSpeed / FMath::Sqrt(BoatPhysics::LoadDragScale(LoadKg, Rules->MaxLoadKg, Rules->FullLoadDragScale));
	float Force = 0.0f;
	float Torque = 0.0f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Drive = Side == 0 ? Drives.X : Drives.Y;
		if (FMath::IsNearlyZero(Drive))
		{
			continue;
		}
		// the oars stop pushing once the boat goes as fast as a rower can make it go
		if ((Drive > 0.0f && Speed > MaxSpeed) || (Drive < 0.0f && Speed < -0.6f * MaxSpeed))
		{
			continue;
		}
		const float Envelope = BoatPhysics::StrokeEnvelope(Side == 0 ? StrokePhase.X : StrokePhase.Y, Rules->StrokeDriveFraction);
		Force += 0.5f * Rules->RowForceN * BoatPhysics::NewtonToUnreal * Drive * Envelope;
		// the port oar pulling forward turns the bow to starboard (positive yaw)
		Torque += (Side == 0 ? 1.0f : -1.0f) * 0.5f * Rules->TurnTorqueNm * BoatPhysics::NewtonMetreToUnreal * Drive * Envelope;
	}
	Hull->AddForce(Forward * Force);
	Hull->AddTorqueInRadians(FVector(0.0f, 0.0f, Torque));
}

void ABoatActor::UpdateLoad(float DeltaSeconds, int32 PersonCount, float ObjectsKg, float SeaLevel)
{
	LoadKg = BoatPhysics::LoadKg(PersonCount, Rules->PersonMassKg, ObjectsKg);
	if (bSunk)
	{
		return;
	}

	// a gunwale under the surface ships water whatever the load (heeling too far, swamped)
	const FTransform Transform = Hull->GetComponentTransform();
	const float HalfLength = Rules->HullSize.X * 0.5f;
	const float HalfBeam = Rules->HullSize.Y * 0.5f;
	bool bGunwaleUnder = false;
	for (const float Side : {-1.0f, 1.0f})
	{
		for (const float Fx : {-0.3f, 0.3f})
		{
			const FVector Point = Transform.TransformPosition(FVector(Fx * HalfLength, Side * HalfBeam * 0.95f, Rules->GunwaleLocalZ));
			bGunwaleUnder |= Point.Z < (WaterSubsystem ? WaterSubsystem->GetWaterSurfaceAt(Point) : SeaLevel);
		}
	}

	const float Rate = BoatPhysics::BilgeRate(LoadKg, Rules->MaxLoadKg, Rules->FloodRateKgPerS, Rules->FloodRatePerKgOver,
		Rules->DrainRateKgPerS, bGunwaleUnder, Rules->SwampRateKgPerS);
	BilgeWaterKg = FMath::Max(0.0f, BilgeWaterKg + Rate * DeltaSeconds);

	if (BilgeWaterKg >= Rules->FloodCapacityKg)
	{
		Sink();
	}
}

void ABoatActor::UpdateGroundContact(float DeltaSeconds)
{
	const FTransform Transform = Hull->GetComponentTransform();
	const FVector Up = Transform.GetUnitAxis(EAxis::Z);
	const FVector Velocity = Hull->GetPhysicsLinearVelocity();
	const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.0);
	const float GroundSpeed = static_cast<float>(GroundVelocity.Size());
	const bool bDrawTrail = TrailDecalMaterial && GetNetMode() != NM_DedicatedServer && GroundSpeed > Rules->TrailMinSpeed;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BoatKeel), false, this);
	const FCollisionObjectQueryParams Ground(ECC_WorldStatic);

	int32 Grounded = 0;
	for (int32 Index = 0; Index < KeelPoints.Num(); ++Index)
	{
		const FVector Point = Transform.TransformPosition(KeelPoints[Index]);
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Point + Up * 20.0f, Point - Up * Rules->GroundContactDistance, Ground, Params))
		{
			TrailDistance[Index] = Rules->TrailSpacing;
			continue;
		}
		++Grounded;

		// sand trail: a groove decal every TrailSpacing while the hull slides on the ground (cosmetic, local)
		if (bDrawTrail)
		{
			TrailDistance[Index] += GroundSpeed * DeltaSeconds;
			if (TrailDistance[Index] >= Rules->TrailSpacing)
			{
				TrailDistance[Index] = 0.0f;
				const FRotator Rotation = FRotationMatrix::MakeFromXZ(-Hit.ImpactNormal, GroundVelocity.GetSafeNormal()).Rotator();
				if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(this, TrailDecalMaterial, Rules->TrailDecalSize,
					Hit.ImpactPoint, Rotation, Rules->TrailLifeSpan))
				{
					Decal->SetFadeOut(Rules->TrailLifeSpan * 0.6f, Rules->TrailLifeSpan * 0.4f, false);
					Decal->SetFadeScreenSize(0.001f);
				}
			}
		}
	}

	// the sand holds a beached hull: friction grows with the keel length resting on the ground (less while skidding)
	GroundedPoints = Grounded;
	if (Grounded > 0 && Hull->IsSimulatingPhysics())
	{
		const float Friction = Rules->BeachedDragPerPoint * Grounded * (bSkidding ? 0.35f : 1.0f);
		Hull->AddForce(-GroundVelocity * Hull->GetMass() * Friction);
	}
}

void ABoatActor::ApplyShoreForces(float DeltaSeconds)
{
	const FTransform Transform = Hull->GetComponentTransform();
	const FVector Center = Transform.GetLocation();
	const bool bGrounded = GroundedPoints > 0;

	// players on foot walking into the hull push it (in the water or on the sand), never those standing on it
	int32 Pushers = 0;
	if (!bSunk)
	{
		FCollisionObjectQueryParams Pawns(ECC_Pawn);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(BoatPushers), false, this);
		TArray<FOverlapResult> Overlaps;
		const float Radius = 0.5f * static_cast<float>(Rules->HullSize.Size()) + Rules->PushReach + 60.0f;
		GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, Pawns, FCollisionShape::MakeSphere(Radius), Params);
		TSet<const ACharacter*> Seen;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const ACharacter* Character = Cast<ACharacter>(Overlap.GetActor());
			if (!Character || Seen.Contains(Character) || Pushers >= Rules->MaxPushers)
			{
				continue;
			}
			Seen.Add(Character);
			const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
			const UBoatRiderComponent* Rider = Character->FindComponentByClass<UBoatRiderComponent>();
			const UIslanderHealthComponent* Health = Character->FindComponentByClass<UIslanderHealthComponent>();
			if (!Move || (Rider && Rider->IsSeated()) || (Health && Health->IsDead())
				|| Character->GetMovementBaseObject() == Hull || !(Move->IsMovingOnGround() || Move->IsSwimming()))
			{
				continue;
			}
			FVector Point;
			const float Distance = Hull->GetClosestPointOnCollision(Character->GetActorLocation(), Point);
			if (Distance < 0.0f)
			{
				continue;
			}
			const float Gap = Distance - Character->GetSimpleCollisionRadius();
			const FVector Push = BoatPhysics::PushDirection(Move->GetCurrentAcceleration(), Point - Character->GetActorLocation(), Gap,
				Rules->PushReach, Rules->PushMinAlignment);
			if (!Push.IsZero())
			{
				++Pushers;
				// at the hands' height on the hull, not under the keel: a shove, not a flip
				const FVector At(Point.X, Point.Y, FMath::Max(Point.Z, Center.Z));
				Hull->AddForceAtLocation(Push * (Rules->PushForcePerPersonN * BoatPhysics::NewtonToUnreal), At);
			}
		}
	}

	// rowed in vain with the keel on the sand: work toward the deepest water around
	const bool bRowing = RowerSeat != INDEX_NONE && !RowInput.IsZero() && !bSunk;
	const float Speed = static_cast<float>(Hull->GetPhysicsLinearVelocity().Size2D());
	StuckSeconds = BoatPhysics::StuckTime(StuckSeconds, bRowing, bGrounded, Speed, Rules->StuckSpeed, DeltaSeconds);
	bool bFreeing = false;
	if (StuckSeconds >= Rules->UnstickAfterSeconds && WaterSubsystem)
	{
		TArray<float> Depths;
		TArray<FVector> Directions;
		const FVector Forward = FlatForward(Transform);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const FVector Direction = Forward.RotateAngleAxis(45.0f * Index, FVector::UpVector);
			Directions.Add(Direction);
			Depths.Add(WaterSubsystem->GetWaterDepthAt(Center + Direction * Rules->UnstickProbeDistance));
		}
		const int32 Best = BoatPhysics::DeepestProbe(Depths, WaterSubsystem->GetWaterDepthAt(Center));
		if (Best != INDEX_NONE)
		{
			bFreeing = true;
			Hull->AddForce(Directions[Best] * (Rules->UnstickForceN * BoatPhysics::NewtonToUnreal));
		}
	}

	// worked off the sand: the keel is partly lifted (the bow raised by the pushers, the oars levering), which takes
	// that much weight off the sand and its friction; never more than the boat's weight, so it never flies
	bSkidding = bGrounded && (Pushers > 0 || bFreeing);
	if (bSkidding)
	{
		const float WeightKg = Rules->HullMassKg + LoadKg + BilgeWaterKg;
		Hull->AddForce(FVector(0.0f, 0.0f, Rules->SkidLiftFraction * WeightKg * BoatPhysics::GravityCm));
	}
}

void ABoatActor::UpdateVisuals(float DeltaSeconds)
{
	const bool bRowing = RowerSeat != INDEX_NONE && !bSunk;
	const FVector2D Drives = BoatPhysics::OarDrives(RowInput);
	const float DriveFraction = Rules->StrokeDriveFraction;

	for (int32 Side = 0; Side < 2; ++Side)
	{
		UStaticMeshComponent* Oar = Side == 0 ? OarLeft : OarRight;
		if (!Oar || !Oar->GetStaticMesh())
		{
			continue;
		}
		// port oar points to -Y (yaw -90), starboard to +Y; "fore" is toward the bow
		const float Base = Side == 0 ? -90.0f : 90.0f;
		const float ForeSign = Side == 0 ? 1.0f : -1.0f;
		FRotator Target;
		const bool bAnimated = bRowing && Rules->bOarsFollowRower && OarAnimOverrideTime >= 0.0
			&& GetWorld()->GetTimeSeconds() - OarAnimOverrideTime < 0.25;
		if (bAnimated)
		{
			// the rower's clip: port yaw theta - 90, starboard 90 - theta (hull frame, +Y starboard), pitch phi
			const float Theta = OarAnimOverride.X;
			Target = FRotator(OarAnimOverride.Y, Side == 0 ? Theta - 90.0f : 90.0f - Theta, 0.0f);
		}
		else if (!bRowing)
		{
			// shipped along the gunwale, blade aft
			Target = FRotator(-4.0f, Side == 0 ? 170.0f : -170.0f, 0.0f);
		}
		else
		{
			const float Drive = Side == 0 ? Drives.X : Drives.Y;
			const float Phase = Side == 0 ? StrokePhase.X : StrokePhase.Y;
			if (FMath::IsNearlyZero(Drive))
			{
				Target = FRotator(Rules->OarPitchDeg.Y, Base, 0.0f);
			}
			else
			{
				const float Direction = Drive > 0.0f ? 1.0f : -1.0f;
				float Sweep;
				float Pitch;
				if (Phase < DriveFraction)
				{
					// drive: blade in the water, from fore to aft (aft to fore when backing)
					const float U = FMath::SmoothStep(0.0f, 1.0f, Phase / DriveFraction);
					Sweep = FMath::Lerp(1.0f, -1.0f, U);
					Pitch = Rules->OarPitchDeg.X;
				}
				else
				{
					// recovery: blade lifted, back to the start
					const float U = FMath::SmoothStep(0.0f, 1.0f, (Phase - DriveFraction) / (1.0f - DriveFraction));
					Sweep = FMath::Lerp(-1.0f, 1.0f, U);
					Pitch = Rules->OarPitchDeg.Y;
				}
				Target = FRotator(Pitch, Base + ForeSign * Direction * Sweep * Rules->OarSweepDeg, 0.0f);
			}
		}
		// Target is in the hull frame; the oar is attached to an oarlock socket that the FBX import rotated (Z down)
		const FQuat SocketInHull = Hull->GetSocketTransform(Oar->GetAttachSocketName(), RTS_Component).GetRotation();
		const FQuat Relative = BoatPhysics::OarRelativeRotation(SocketInHull, Target);
		Oar->SetRelativeRotation(FMath::QInterpTo(Oar->GetRelativeRotation().Quaternion(), Relative, DeltaSeconds, 14.0f));
	}

	if (!bRowing)
	{
		StrokePhase = FVector2D::ZeroVector;
	}

	// water shipped: a plane rising from the floor
	const float Fill = GetBilgeFraction();
	const bool bShowWater = BilgeWaterKg > 1.0f && !bSunk;
	if (BilgeWater->IsVisible() != bShowWater)
	{
		BilgeWater->SetVisibility(bShowWater);
	}
	if (bShowWater)
	{
		BilgeWater->SetRelativeLocation(FVector(-20.0f, 0.0f, -9.0f + Fill * 0.8f * (Rules->GunwaleLocalZ + 9.0f)));
		BilgeWater->SetRelativeScale3D(FVector(3.8f + 0.6f * Fill, 1.45f + 0.45f * Fill, 1.0f));
	}
}

void ABoatActor::Sink()
{
	if (!HasAuthority() || bSunk)
	{
		return;
	}

	bSunk = true;
	const FVector Eject(0.0f, 0.0f, Rules->SinkEjectUpSpeed);
	for (ACharacter* Occupant : TArray<ACharacter*>(SeatOccupants))
	{
		if (IsValid(Occupant))
		{
			StandCharacter(Occupant);
			Occupant->LaunchCharacter(Eject, false, true);
			// the client of a remote player launches its own pawn too: predicted, not corrected (review R-12)
			UBoatRiderComponent* Rider = Occupant->FindComponentByClass<UBoatRiderComponent>();
			if (Rider && !Occupant->IsLocallyControlled() && Occupant->IsPlayerControlled())
			{
				Rider->ClientLaunch(Eject, false, true);
			}
		}
	}
	RowerSeat = INDEX_NONE;
	RowInput = FVector2D::ZeroVector;
	UE_LOG(Logmonke_island, Warning, TEXT("%s sank: load %.0f kg for %.0f kg allowed"), *GetName(), LoadKg, Rules->MaxLoadKg);

	PlaySinking();
	SetLifeSpan(Rules->SunkLifeSpan);
}

void ABoatActor::OnRep_Sunk()
{
	if (!bSunk)
	{
		return;
	}
	// a client receiving a boat sunk long ago (joining late, back in relevancy) is not told it sinks now (review R-19)
	if (!HasActorBegunPlay())
	{
		OnSunk.Broadcast(this);
		return;
	}
	PlaySinking();
}

void ABoatActor::PlaySinking()
{
	OnSunk.Broadcast(this);
	// provisional feedback until the HUD lot (cahier §19)
	if (GEngine && GetNetMode() != NM_DedicatedServer)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor(255, 80, 60), TEXT("La barque a coulé : surcharge ! Tout ce qu'elle portait est perdu."));
	}
}

ABoatActor* ABoatActor::SpawnOnWaterNear(UWorld* World, const FVector& From, const FVector& Forward, TSubclassOf<ABoatActor> BoatClass)
{
	if (!World || !BoatClass)
	{
		return nullptr;
	}

	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(World);
	const float SeaLevel = Water ? Water->GetSeaLevel() : 0.0f;
	const FVector Ahead = FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	const FCollisionObjectQueryParams Ground(ECC_WorldStatic);

	// straight ahead first, then around the player: the first spot with enough water
	for (int32 Turn = 0; Turn < 8; ++Turn)
	{
		const FVector Direction = Ahead.RotateAngleAxis(45.0f * Turn, FVector::UpVector);
		for (float Distance = 400.0f; Distance <= 4000.0f; Distance += 100.0f)
		{
			const FVector Spot = From + Direction * Distance;
			FHitResult Hit;
			const FVector Top(Spot.X, Spot.Y, SeaLevel + 3000.0f);
			const FVector Bottom(Spot.X, Spot.Y, SeaLevel - 5000.0f);
			const float GroundZ = World->LineTraceSingleByObjectType(Hit, Top, Bottom, Ground) ? static_cast<float>(Hit.ImpactPoint.Z) : SeaLevel - 5000.0f;
			if (SeaLevel - GroundZ < 90.0f)
			{
				continue;
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FVector Location(Spot.X, Spot.Y, SeaLevel + 14.0f);
			return World->SpawnActor<ABoatActor>(BoatClass, Location, FRotator(0.0f, Direction.Rotation().Yaw + 90.0f, 0.0f), Params);
		}
	}
	return nullptr;
}
