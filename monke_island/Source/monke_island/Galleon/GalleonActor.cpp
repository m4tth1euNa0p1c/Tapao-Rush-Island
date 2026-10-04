// monke_island

#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonRules.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatRiderComponent.h"
#include "Flood/FloodSimulationActor.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Items/ValuableComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

AGalleonActor::AGalleonActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"));
	RootComponent = Anchor;

	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	Hull->SetupAttachment(Anchor);
	Hull->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Hull->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Hull->SetCanEverAffectNavigation(false);

	auto MakePart = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Hull);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		return Part;
	};
	Sails = MakePart(TEXT("Sails"));
	Rigging = MakePart(TEXT("Rigging"));
	Flag = MakePart(TEXT("Flag"));
	Wheel = MakePart(TEXT("Wheel"));
	Rudder = MakePart(TEXT("Rudder"));

	auto MakeVolume = [this](const TCHAR* Name)
	{
		UBoxComponent* Box = CreateDefaultSubobject<UBoxComponent>(Name);
		Box->SetupAttachment(Hull);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetCanEverAffectNavigation(false);
		return Box;
	};
	AboardVolume = MakeVolume(TEXT("AboardVolume"));
	CargoVolume = MakeVolume(TEXT("CargoVolume"));
}

void AGalleonActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AGalleonActor, Rules, COND_InitialOnly);
}

AGalleonActor* AGalleonActor::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<AGalleonActor> It(World);
	return It ? *It : nullptr;
}

void AGalleonActor::BeginPlay()
{
	Super::BeginPlay();
	WaterSubsystem = UIslandWaterSubsystem::Get(this);
	ApplyRules();
}

void AGalleonActor::ApplyRules()
{
	if (bRulesApplied || !Rules)
	{
		return;
	}
	bRulesApplied = true;
	Hull->SetStaticMesh(Rules->HullMesh);
	Sails->SetStaticMesh(Rules->SailsMesh);
	Rigging->SetStaticMesh(Rules->RiggingMesh);
	Flag->SetStaticMesh(Rules->FlagMesh);
	Flag->SetRelativeLocation(Rules->FlagPivot);
	Wheel->SetStaticMesh(Rules->WheelMesh);
	Wheel->SetRelativeLocation(Rules->WheelPivot);
	Rudder->SetStaticMesh(Rules->RudderMesh);
	Rudder->SetRelativeLocation(Rules->RudderPivot);
	AboardVolume->SetRelativeLocation(Rules->AboardCentre);
	AboardVolume->SetBoxExtent(Rules->AboardExtent, false);
	CargoVolume->SetRelativeLocation(Rules->CargoCentre);
	CargoVolume->SetBoxExtent(Rules->CargoExtent, false);
	// calm zone of the wind around the ship (shape only, read by AWindStormActor)
	if (Rules->WindCalmRadius > 0.0f)
	{
		Tags.AddUnique(TEXT("WindCalmZone"));
		USphereComponent* Calm = NewObject<USphereComponent>(this, TEXT("WindCalmZone"));
		Calm->SetupAttachment(Anchor);
		Calm->SetSphereRadius(Rules->WindCalmRadius, false);
		Calm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Calm->SetCanEverAffectNavigation(false);
		Calm->RegisterComponent();
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: galleon anchored at %s"), *GetName(), *GetActorLocation().ToCompactString());
}

void AGalleonActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules)
	{
		return;
	}
	// the rules arrive with the actor on clients, possibly after BeginPlay
	ApplyRules();

	// on the water where it is anchored: the sea, or the flood standing over it there (the rise is local, in the grid
	// of AFloodSimulationActor, and goes on after the submersion); without the waves, and eased so the hull never jumps
	const FVector Location = GetActorLocation();
	float Level = WaterSubsystem ? WaterSubsystem->GetSeaLevel() : static_cast<float>(Location.Z);
	for (TActorIterator<AFloodSimulationActor> It(GetWorld()); It; ++It)
	{
		float Flood = 0.0f;
		if (It->GetFloodSurfaceAt(Location, Flood))
		{
			Level = FMath::Max(Level, Flood);
		}
	}
	if (!FMath::IsNearlyEqual(static_cast<float>(Location.Z), Level, 0.1f))
	{
		const float Z = FMath::FInterpConstantTo(static_cast<float>(Location.Z), Level, DeltaSeconds, Rules->FloodFollowSpeed);
		SetActorLocation(FVector(Location.X, Location.Y, Z));
	}

	// cosmetic sway on the server clock: the same pose on every machine; the bow is +Y, so the ship rolls around Y
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Time = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	auto Wave = [Time](double Period, double Phase) { return Period > 0.0 ? FMath::Sin(UE_TWO_PI * Time / Period + Phase) : 0.0; };
	const FRotator Sway(Rules->RollDeg * Wave(Rules->SwayPeriods.X, 0.0), 0.0, Rules->PitchDeg * Wave(Rules->SwayPeriods.Y, 1.3));
	Hull->SetRelativeLocationAndRotation(FVector(0.0, 0.0, Rules->HeaveCm * Wave(Rules->SwayPeriods.Z, 2.1)), Sway);
}

// ---------------------------------------------------------------- aboard

bool AGalleonActor::IsActorAboard(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	const FVector Local = AboardVolume->GetComponentTransform().InverseTransformPosition(Actor->GetActorLocation());
	const FVector Extent = AboardVolume->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

void AGalleonActor::GetCargoAboard(TArray<AActor*>& OutCargo) const
{
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GalleonCargo), false, this);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, CargoVolume->GetComponentLocation(), CargoVolume->GetComponentQuat(),
		ObjectTypes, CargoVolume->GetCollisionShape(), Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && Actor->FindComponentByClass<UValuableComponent>())
		{
			OutCargo.AddUnique(Actor);
		}
	}
}

int32 AGalleonActor::GetCargoValue() const
{
	TArray<AActor*> Cargo;
	GetCargoAboard(Cargo);
	int32 Total = 0;
	for (const AActor* Actor : Cargo)
	{
		Total += Actor->FindComponentByClass<UValuableComponent>()->GetValue();
	}
	return Total;
}

// ---------------------------------------------------------------- barque

FTransform AGalleonActor::GetDockTransform() const
{
	const FName Socket = Rules ? Rules->DockSocket : NAME_None;
	const FVector Location = Hull->DoesSocketExist(Socket) ? Hull->GetSocketLocation(Socket) : GetActorLocation();
	return FTransform(FRotator(0.0f, GetActorRotation().Yaw + (Rules ? Rules->DockYawOffset : 0.0f), 0.0f), Location);
}

bool AGalleonActor::IsBoatMoored(const ABoatActor* Boat) const
{
	if (!Boat || !Rules || Boat->IsSunk())
	{
		return false;
	}
	return FVector::Dist2D(Boat->GetActorLocation(), GetDockTransform().GetLocation()) <= Rules->MooringRadius
		&& Boat->GetVelocity().Size() <= Rules->MooringMaxSpeed;
}

ABoatActor* AGalleonActor::FindMooredBoat() const
{
	for (TActorIterator<ABoatActor> It(GetWorld()); It; ++It)
	{
		if (IsBoatMoored(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

void AGalleonActor::MoorBoat(ABoatActor* Boat)
{
	if (!HasAuthority() || !Boat || !Boat->GetHull())
	{
		return;
	}
	const FTransform Dock = GetDockTransform();
	UStaticMeshComponent* BoatHull = Boat->GetHull();
	// it keeps its height on the water
	const FVector Location(Dock.GetLocation().X, Dock.GetLocation().Y, BoatHull->GetComponentLocation().Z);
	BoatHull->SetWorldLocationAndRotation(Location, Dock.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	BoatHull->SetPhysicsLinearVelocity(FVector::ZeroVector);
	BoatHull->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s moored at the dock %s"), *GetName(), *Boat->GetName(), *Location.ToCompactString());
}

// ---------------------------------------------------------------- crew

void AGalleonActor::TeleportCharacter(ACharacter* Character, const FVector& Location, float Yaw) const
{
	if (const UBoatRiderComponent* Rider = Character->FindComponentByClass<UBoatRiderComponent>(); Rider && Rider->IsSeated())
	{
		Rider->GetSeatedBoat()->StandCharacter(Character);
	}
	const FRotator Rotation(0.0f, Yaw, 0.0f);
	Character->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(Rotation);
	}
	// what the player carries comes along (it would be dragged through the hull)
	const UIslanderGrabComponent* Grab = Character->FindComponentByClass<UIslanderGrabComponent>();
	if (UPrimitiveComponent* Held = Grab ? Grab->GetHeldComponent() : nullptr)
	{
		Held->SetWorldLocation(Location + Rotation.Vector() * 70.0, false, nullptr, ETeleportType::TeleportPhysics);
		Held->SetPhysicsLinearVelocity(FVector::ZeroVector);
	}
}

void AGalleonActor::PlaceOnDeck(ACharacter* Character, int32 Index)
{
	if (!HasAuthority() || !Character || !Rules)
	{
		return;
	}
	const FTransform& HullTransform = Hull->GetComponentTransform();
	const FVector Base = Hull->DoesSocketExist(Rules->DeckSpawnSocket) ? Hull->GetSocketLocation(Rules->DeckSpawnSocket)
		: HullTransform.TransformPosition(FVector(0.0, 160.0, 160.0));
	// side by side across the deck, looking at the island (the ladder side, -X)
	const float Side = (static_cast<float>(Index % 4) - 1.5f) * Rules->DeckSpawnSpacing;
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Location = Base + HullTransform.GetUnitAxis(EAxis::Y) * Side + FVector(0.0, 0.0, HalfHeight + 10.0);
	TeleportCharacter(Character, Location, GetActorRotation().Yaw + 180.0f);
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s on the deck"), *GetName(), *Character->GetName());
}

// ---------------------------------------------------------------- boarding ladder

AGalleonActor::ELadderMove AGalleonActor::GetLadderMove(const APawn* User) const
{
	if (!User || !Rules)
	{
		return ELadderMove::None;
	}
	if (const UIslanderHealthComponent* Health = User->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
	{
		return ELadderMove::None;
	}
	const FVector Location = User->GetActorLocation();
	if (IsActorAboard(User))
	{
		const FVector Top = Hull->GetComponentTransform().TransformPosition(Rules->LadderDeckPoint);
		return FVector::Dist2D(Location, Top) <= Rules->LadderReach ? ELadderMove::Leave : ELadderMove::None;
	}
	const FVector Foot = Hull->DoesSocketExist(Rules->LadderSocket) ? Hull->GetSocketLocation(Rules->LadderSocket) : GetDockTransform().GetLocation();
	return FVector::Dist2D(Location, Foot) <= Rules->LadderReach ? ELadderMove::Board : ELadderMove::None;
}

FText AGalleonActor::GetInteractionPrompt_Implementation(APawn* User) const
{
	switch (GetLadderMove(User))
	{
	case ELadderMove::Board:	return Rules->BoardPrompt;
	case ELadderMove::Leave:	return FindMooredBoat() ? Rules->ToBoatPrompt : Rules->ToWaterPrompt;
	default:					return FText::GetEmpty();
	}
}

bool AGalleonActor::CanInteract_Implementation(APawn* User) const
{
	return GetLadderMove(User) != ELadderMove::None;
}

void AGalleonActor::Interact_Implementation(APawn* User)
{
	ACharacter* Character = Cast<ACharacter>(User);
	if (!HasAuthority() || !Character)
	{
		return;
	}
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	switch (GetLadderMove(User))
	{
	case ELadderMove::Board:
	{
		const FVector Top = Hull->GetComponentTransform().TransformPosition(Rules->LadderDeckPoint);
		TeleportCharacter(Character, Top + FVector(0.0, 0.0, HalfHeight + 10.0), GetActorRotation().Yaw);
		break;
	}
	case ELadderMove::Leave:
	{
		// into the barque when it waits at the dock, else at the foot of the ladder, in the water
		const ABoatActor* Boat = FindMooredBoat();
		const FVector Down = Boat ? Boat->GetActorLocation() + FVector(0.0, 0.0, HalfHeight + 45.0)
			: GetDockTransform().GetLocation() + FVector(0.0, 0.0, HalfHeight);
		TeleportCharacter(Character, Down, GetActorRotation().Yaw + 180.0f);
		break;
	}
	default:
		break;
	}
}
