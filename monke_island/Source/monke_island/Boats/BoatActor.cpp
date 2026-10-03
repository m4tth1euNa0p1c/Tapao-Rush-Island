// monke_island

#include "Boats/BoatActor.h"
#include "Boats/BoatPhysics.h"
#include "Boats/BoatRiderComponent.h"
#include "Boats/BoatRules.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderHealthComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"

namespace
{
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

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	SetRootComponent(Hull);
	Hull->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	// the third person camera boom must not zoom in on the player when the hull passes between them
	Hull->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Hull->SetSimulatePhysics(true);
	Hull->SetCanEverAffectNavigation(false);

	OarLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OarLeft"));
	OarLeft->SetupAttachment(Hull, TEXT("Oarlock_L"));
	OarLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OarLeft->SetCanEverAffectNavigation(false);

	OarRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OarRight"));
	OarRight->SetupAttachment(Hull, TEXT("Oarlock_R"));
	OarRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OarRight->SetCanEverAffectNavigation(false);

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

	WaterSubsystem = UIslandWaterSubsystem::Get(this);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_BoatRules: using the default rules"), *GetName());
		Rules = NewObject<UBoatRules>(this);
	}

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
	// server-side reach check: a client cannot sit in a boat across the map
	const float Reach = Rules->SeatReach + 100.0f;
	if (FVector::DistSquared(Hull->GetSocketLocation(SeatSockets[SeatIndex]), Character->GetActorLocation()) > FMath::Square(Reach))
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
	SwellTime += DeltaSeconds;

	if (HasAuthority())
	{
		ValidateSeats();
	}

	TArray<ACharacter*> People;
	float ObjectsKg = 0.0f;
	GatherPeople(People, ObjectsKg);

	// same forces everywhere from replicated state; the server's simulation corrects the clients
	if (Hull->IsSimulatingPhysics())
	{
		ApplyWaterForces(DeltaSeconds, SeaLevel);
		ApplyLoadForces(People);
		ApplyRowing(DeltaSeconds);
	}

	if (HasAuthority())
	{
		UpdateLoad(DeltaSeconds, People.Num(), ObjectsKg, SeaLevel);
	}

	UpdateGroundContact(DeltaSeconds);
	UpdateVisuals(DeltaSeconds);

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
	const FVector Drag = -Forward * (Rules->ForwardDrag * Along + Rules->ForwardQuadDrag * Along * FMath::Abs(Along))
		- Right * (Rules->LateralDrag * Across);
	Hull->AddForce(Drag * Wetness);

	// gentle swell so the boat is never perfectly still
	if (!bSunk)
	{
		const float Roll = Rules->SwellTorqueNm.X * FMath::Sin(UE_TWO_PI * SwellTime / Rules->SwellPeriod.X);
		const float Pitch = Rules->SwellTorqueNm.Y * FMath::Sin(UE_TWO_PI * SwellTime / Rules->SwellPeriod.Y + 1.3f);
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
		if ((Drive > 0.0f && Speed > Rules->MaxRowSpeed) || (Drive < 0.0f && Speed < -0.6f * Rules->MaxRowSpeed))
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

	// the sand holds a beached hull: friction grows with the keel length resting on the ground
	if (Grounded > 0 && Hull->IsSimulatingPhysics())
	{
		const float Friction = Rules->BeachedDragPerPoint * Grounded;
		Hull->AddForce(-GroundVelocity * Hull->GetMass() * Friction);
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
		if (!bRowing)
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
		Oar->SetRelativeRotation(FMath::RInterpTo(Oar->GetRelativeRotation(), Target, DeltaSeconds, 14.0f));
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
	for (ACharacter* Occupant : TArray<ACharacter*>(SeatOccupants))
	{
		if (IsValid(Occupant))
		{
			StandCharacter(Occupant);
			Occupant->LaunchCharacter(FVector(0.0f, 0.0f, 300.0f), false, true);
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
	if (bSunk)
	{
		PlaySinking();
	}
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
