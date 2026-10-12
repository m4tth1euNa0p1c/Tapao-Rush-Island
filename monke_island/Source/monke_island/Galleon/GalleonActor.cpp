// monke_island

#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonDeparture.h"
#include "Galleon/GalleonDepartureComponent.h"
#include "Galleon/GalleonRules.h"
#include "Shop/ShopkeeperActor.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatRiderComponent.h"
#include "Flood/FloodSimulationActor.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Items/ValuableComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Props/LootCrate/LootContainer.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

namespace
{
	/** Where the crew stands on the deck at the start, at the height of the socket: side by side across the deck from
	 *  DeckSpawnSocket, DeckSpawnSpacing apart (PlaceOnDeck; 4 places) */
	FVector DeckSpot(const UStaticMeshComponent* Hull, const UGalleonRules* Rules, int32 Index)
	{
		const FTransform& HullTransform = Hull->GetComponentTransform();
		const FVector Base = Hull->DoesSocketExist(Rules->DeckSpawnSocket) ? Hull->GetSocketLocation(Rules->DeckSpawnSocket)
			: HullTransform.TransformPosition(FVector(0.0, 160.0, 160.0));
		const float Side = (static_cast<float>(Index % 4) - 1.5f) * Rules->DeckSpawnSpacing;
		return Base + HullTransform.GetUnitAxis(EAxis::Y) * Side;
	}

	/**
	 *  Server (T-258, owner 2026-10-09): the loot crate of the expedition, on the deck beside the merchant. It takes the first
	 *  free place on rings around DeckSpawnSocket + LootCrateOffset (hull frame): the deck under it at the crew's level
	 *  (not a castle), nothing it would stand in (bulwark, hatch coaming, merchant, mast), the crew's places clear. Put
	 *  down a few centimetres above the deck, it settles there: a free physics object, its contents count with the cargo.
	 */
	AActor* SpawnDeckCrate(AGalleonActor* Galleon, const UStaticMeshComponent* Hull, const UGalleonRules* Rules)
	{
		if (Rules->LootCrateClass.IsNull())
		{
			return nullptr;
		}
		UClass* CrateClass = Rules->LootCrateClass.LoadSynchronous();
		if (!CrateClass)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("%s: loot crate class %s missing, no crate on the deck"), *Galleon->GetName(),
				*Rules->LootCrateClass.ToString());
			return nullptr;
		}
		UWorld* World = Galleon->GetWorld();
		const FTransform& HullTransform = Hull->GetComponentTransform();
		const FVector Base = Hull->DoesSocketExist(Rules->DeckSpawnSocket) ? Hull->GetSocketLocation(Rules->DeckSpawnSocket)
			: HullTransform.TransformPosition(FVector(0.0, 160.0, 160.0));
		const FRotator Rotation(0.0, Galleon->GetActorRotation().Yaw + Rules->LootCrateYaw, 0.0);
		FActorSpawnParameters Spawn;
		Spawn.Owner = Galleon;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// above the deck first: its size is known once it exists, then it is moved to its place in the same frame
		AActor* Crate = World->SpawnActor<AActor>(CrateClass, Base + FVector(0.0, 0.0, 600.0), Rotation, Spawn);
		if (!Crate)
		{
			return nullptr;
		}
		const FBox Bounds = Crate->GetComponentsBoundingBox(false);
		const FVector Origin = Crate->GetActorLocation();
		const FVector HalfSize = Bounds.IsValid ? Bounds.GetExtent() : FVector(40.0, 40.0, 15.0);
		const FVector CenterOffset = Bounds.IsValid ? Bounds.GetCenter() - Origin : FVector::ZeroVector;
		const double Bottom = Bounds.IsValid ? Origin.Z - Bounds.Min.Z : HalfSize.Z;
		const double Clearance = Rules->LootCrateClearanceCm;
		const FCollisionShape Room = FCollisionShape::MakeBox(FVector(HalfSize.X + Clearance, HalfSize.Y + Clearance, HalfSize.Z));
		const double KeepFromCrew = Rules->DeckSpawnSpacing * 0.5 + HalfSize.Size2D();
		FCollisionQueryParams Query(SCENE_QUERY_STAT(GalleonDeckCrate), true, Crate);

		const FVector AxisX = HullTransform.GetUnitAxis(EAxis::X);
		const FVector AxisY = HullTransform.GetUnitAxis(EAxis::Y);
		const FVector Wanted = Base + AxisX * Rules->LootCrateOffset.X + AxisY * Rules->LootCrateOffset.Y;
		FVector Place = FVector(Wanted.X, Wanted.Y, Base.Z + Bottom + 20.0);
		bool bFound = false;
		for (int32 Ring = 0; Ring <= Rules->LootCrateSearchRings && !bFound; ++Ring)
		{
			const int32 Count = Ring == 0 ? 1 : 8 * Ring;
			for (int32 Index = 0; Index < Count && !bFound; ++Index)
			{
				const double Angle = UE_TWO_PI * Index / Count;
				const FVector Spot = Wanted + (AxisX * FMath::Cos(Angle) + AxisY * FMath::Sin(Angle)) * (Ring * Rules->LootCrateSearchStep);
				// the deck of the crew under it
				FHitResult Deck;
				if (!World->LineTraceSingleByChannel(Deck, FVector(Spot.X, Spot.Y, Base.Z + 150.0), FVector(Spot.X, Spot.Y, Base.Z - 150.0),
					ECC_Visibility, Query) || Deck.GetActor() != Galleon || FMath::Abs(Deck.ImpactPoint.Z - Base.Z) > Rules->LootCrateMaxDeckStep)
				{
					continue;
				}
				// the crew's places stay clear
				bool bClear = true;
				for (int32 Crew = 0; Crew < 4 && bClear; ++Crew)
				{
					bClear = FVector::Dist2D(Spot, DeckSpot(Hull, Rules, Crew)) >= KeepFromCrew;
				}
				const FVector Candidate(Spot.X, Spot.Y, Deck.ImpactPoint.Z + Bottom + Clearance);
				// room for it, a little above the planks
				if (bClear && !World->OverlapBlockingTestByChannel(Candidate + CenterOffset + FVector(0.0, 0.0, Clearance), FQuat::Identity,
					ECC_PhysicsBody, Room, Query))
				{
					Place = Candidate;
					bFound = true;
				}
			}
		}
		Crate->SetActorLocationAndRotation(Place, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Crate->GetRootComponent()); Root && Root->IsSimulatingPhysics())
		{
			Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Root->SetPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s: loot crate %s on the deck at %s%s"), *Galleon->GetName(), *Crate->GetName(), *Place.ToCompactString(),
			bFound ? TEXT("") : TEXT(" (no free place found around the wanted one: put there anyway)"));
		return Crate;
	}
}

AGalleonActor::AGalleonActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	Anchor = CreateDefaultSubobject<USceneComponent>(TEXT("Anchor"));
	RootComponent = Anchor;

	// the decks the crew walks on: still on the anchor, never rendered (network review T-268 R-04, T-317)
	Hull = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hull"));
	Hull->SetupAttachment(Anchor);
	Hull->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Hull->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Hull->SetCanEverAffectNavigation(false);
	Hull->SetHiddenInGame(true);
	Hull->SetCastShadow(false);
	Hull->bAffectDistanceFieldLighting = false;

	// what the players see: the same hull, swaying, through which nothing collides
	HullVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullVisual"));
	HullVisual->SetupAttachment(Anchor);
	HullVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HullVisual->SetGenerateOverlapEvents(false);
	HullVisual->SetCanEverAffectNavigation(false);

	auto MakePart = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(HullVisual);
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

void AGalleonActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DepartureSoundsHandle.IsValid())
	{
		DepartureSoundsHandle->CancelHandle();
		DepartureSoundsHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void AGalleonActor::LoadDepartureSounds()
{
	if (!Rules || GetNetMode() == NM_DedicatedServer || DepartureSoundsHandle.IsValid())
	{
		return;
	}
	TArray<FSoftObjectPath> Sounds;
	for (const TSoftObjectPtr<USoundBase>* Sound : { &Rules->DepartureBellSound, &Rules->DepartureHornSound, &Rules->DepartureTickSound })
	{
		if (!Sound->IsNull())
		{
			Sounds.Add(Sound->ToSoftObjectPath());
		}
	}
	if (!Sounds.IsEmpty())
	{
		// asynchronous, kept by the handle until the galleon goes: the first bell rings without a hitch
		DepartureSoundsHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Sounds, FStreamableDelegate());
	}
}

void AGalleonActor::PlayDepartureCue(EGalleonDepartureCue Cue) const
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const TSoftObjectPtr<USoundBase>& Soft = Cue == EGalleonDepartureCue::Bell ? Rules->DepartureBellSound
		: (Cue == EGalleonDepartureCue::Horn ? Rules->DepartureHornSound : Rules->DepartureTickSound);
	USoundBase* Sound = Soft.Get();
	if (!Sound)
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("%s: departure sound %s not loaded (yet), not played"), *GetName(), *Soft.ToString());
		return;
	}
	UGameplayStatics::PlaySound2D(this, Sound, Cue == EGalleonDepartureCue::Tick ? Rules->DepartureTickVolume : Rules->DepartureSoundVolume);
}

void AGalleonActor::ApplyRules()
{
	if (bRulesApplied || !Rules)
	{
		return;
	}
	bRulesApplied = true;
	Hull->SetStaticMesh(Rules->HullMesh);
	HullVisual->SetStaticMesh(Rules->HullMesh);
	// the visual hull turns around the crew's deck: the planks under the players barely move against the still collision
	SwayPivot = Hull->DoesSocketExist(Rules->DeckSpawnSocket) ? Hull->GetSocketTransform(Rules->DeckSpawnSocket, RTS_Component).GetLocation()
		: FVector::ZeroVector;
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
	// the bell, the foghorn and the ticks of the departure (T-286), ready before they ring
	LoadDepartureSounds();
	// lamp of the captain's cabin (it has no window worth the name)
	if (Rules->CabinLightIntensity > 0.0f && Hull->DoesSocketExist(Rules->CabinLightSocket))
	{
		UPointLightComponent* Lamp = NewObject<UPointLightComponent>(this, TEXT("CabinLight"));
		Lamp->SetupAttachment(Hull);
		Lamp->SetRelativeLocation(Hull->GetSocketTransform(Rules->CabinLightSocket, RTS_Component).GetLocation() + FVector(0.0, 0.0, Rules->CabinLightHeight));
		Lamp->SetIntensityUnits(ELightUnits::Candelas);
		Lamp->SetIntensity(Rules->CabinLightIntensity);
		Lamp->SetAttenuationRadius(Rules->CabinLightRadius);
		Lamp->SetUseTemperature(true);
		Lamp->SetTemperature(Rules->CabinLightTemperature);
		Lamp->SetCastShadows(false);
		Lamp->RegisterComponent();
	}
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
	// the merchant of the deck (server; replicated actor attached to the hull)
	if (HasAuthority() && !Rules->ShopkeeperClass.IsNull())
	{
		if (UClass* ShopClass = Rules->ShopkeeperClass.LoadSynchronous())
		{
			const FTransform& HullTransform = Hull->GetComponentTransform();
			const FVector Base = Hull->DoesSocketExist(Rules->DeckSpawnSocket) ? Hull->GetSocketLocation(Rules->DeckSpawnSocket)
				: HullTransform.TransformPosition(FVector(0.0, 160.0, 160.0));
			const FVector Location = Base + HullTransform.TransformVectorNoScale(FVector(Rules->ShopkeeperOffset.X, Rules->ShopkeeperOffset.Y, 0.0))
				+ FVector(0.0, 0.0, Rules->ShopkeeperOffset.Z);
			const FRotator Rotation(0.0, GetActorRotation().Yaw + Rules->ShopkeeperYaw, 0.0);
			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AActor* Shopkeeper = GetWorld()->SpawnActor<AActor>(ShopClass, Location, Rotation, Params))
			{
				Shopkeeper->AttachToComponent(Hull, FAttachmentTransformRules::KeepWorldTransform);
				// the merchant and its stand keep clear of the crew's places (the first player stood in the stand: the
				// view went brown through its panel as the ship rocked): slid along the deck away from the places, half
				// a spacing of room around each
				const FVector Away = (Location - Base).GetSafeNormal2D();
				const double Room = Rules->DeckSpawnSpacing * 0.5;
				const double Step = FMath::Max(Rules->DeckSpawnSpacing / 9.0, 5.0);
				double Slid = 0.0;
				for (int32 Move = 0; Move < 60 && !Away.IsNearlyZero(); ++Move)
				{
					const FBox Stand = Shopkeeper->GetComponentsBoundingBox(true).ExpandBy(FVector(Room, Room, 0.0));
					bool bClear = true;
					for (int32 Index = 0; Index < 4 && bClear; ++Index)
					{
						bClear = !Stand.IsInsideXY(DeckSpot(Hull, Rules, Index));
					}
					if (bClear)
					{
						break;
					}
					Shopkeeper->AddActorWorldOffset(Away * Step);
					Slid += Step;
				}
				UE_LOG(Logmonke_island, Log, TEXT("%s: merchant %s on the deck at %s (slid %.0f cm clear of the crew's places)"), *GetName(),
					*Shopkeeper->GetName(), *Shopkeeper->GetActorLocation().ToCompactString(), Slid);
			}
		}
	}
	// the loot crate of the expedition beside the merchant (server, after it: the crate keeps clear of its stand)
	if (HasAuthority())
	{
		SpawnDeckCrate(this, Hull, Rules);
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

	// cosmetic sway on the server clock, the same pose on every machine, on the visual hull only: the collision hull, the
	// base of the players aboard and of what lies on the deck, never rolls (R-04, T-317)
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	const double Time = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	HullVisual->SetRelativeTransform(ComputeVisualSway(*Rules, Time, SwayPivot));
}

FTransform AGalleonActor::ComputeVisualSway(const UGalleonRules& InRules, double ServerTime, const FVector& Pivot)
{
	auto Wave = [ServerTime](double Period, double Phase)
	{
		return Period > 0.0 && FMath::IsFinite(ServerTime) ? FMath::Sin(UE_TWO_PI * ServerTime / Period + Phase) : 0.0;
	};
	// the bow is +Y: the ship rolls around Y (the rotator's pitch) and pitches around X (its roll)
	const FQuat Rotation = FRotator(InRules.RollDeg * Wave(InRules.SwayPeriods.X, 0.0), 0.0, InRules.PitchDeg * Wave(InRules.SwayPeriods.Y, 1.3)).Quaternion();
	const FVector Heave(0.0, 0.0, InRules.HeaveCm * Wave(InRules.SwayPeriods.Z, 2.1));
	// turning around the pivot: the pivot itself only heaves
	return FTransform(Rotation, Pivot - Rotation.RotateVector(Pivot) + Heave);
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
		// a loot crate aboard brings what it holds (T-258: hidden inside, out of reach of the overlap)
		TArray<AActor*> WithContents;
		LootContainers::AppendWithContents(Overlap.GetActor(), WithContents);
		for (AActor* Actor : WithContents)
		{
			if (Actor->FindComponentByClass<UValuableComponent>())
			{
				OutCargo.AddUnique(Actor);
			}
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
	// off any barque first (T-182): the seat is freed, the capsule taken off a hull it may hang on (even without a seat),
	// movement and collision given back; otherwise it would follow the barque wherever the barque goes (MoorBoat)
	if (UBoatRiderComponent* Rider = Character->FindComponentByClass<UBoatRiderComponent>())
	{
		Rider->LeaveBoat();
	}
	else if (Cast<ABoatActor>(Character->GetAttachParentActor()))
	{
		Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
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
		// the owner's view too: the control rotation of a remote client is its own (network review T-268 R-11)
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			PlayerController->ClientSetRotation(Rotation, true);
		}
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
	// side by side across the deck, looking at the island (the ladder side, -X)
	const float HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Location = DeckSpot(Hull, Rules, Index) + FVector(0.0, 0.0, HalfHeight + 10.0);
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

bool AGalleonActor::IsAtHelm(const APawn* User) const
{
	if (!User || !Rules || !IsActorAboard(User))
	{
		return false;
	}
	if (const UIslanderHealthComponent* Health = User->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
	{
		return false;
	}
	// in the frame of the still collision hull, never of the swaying wheel: the server and the clients agree on the reach
	const FVector Helm = Hull->DoesSocketExist(Rules->HelmSocket) ? Hull->GetSocketLocation(Rules->HelmSocket)
		: Hull->GetComponentTransform().TransformPosition(Rules->WheelPivot);
	return FVector::Dist2D(User->GetActorLocation(), Helm) <= Rules->HelmReach;
}

bool AGalleonActor::IsHelmUsableBy(const APawn* User) const
{
	const UGalleonDepartureComponent* Departure = UGalleonDepartureComponent::Get(this);
	return Departure && Departure->IsHelmUsable() && IsAtHelm(User);
}

FText AGalleonActor::GetInteractionPrompt_Implementation(APawn* User) const
{
	// the wheel (T-286): held to raise the anchor, pressed to cancel a vote or a notice (UGalleonCrewComponent)
	if (IsHelmUsableBy(User))
	{
		const UGalleonDepartureComponent* Departure = UGalleonDepartureComponent::Get(this);
		return Departure && Departure->GetStage() != EGalleonDepartureStage::None ? Rules->HelmCancelPrompt : Rules->HelmPrompt;
	}
	switch (GetLadderMove(User))
	{
	case ELadderMove::Board:	return Rules->BoardPrompt;
	case ELadderMove::Leave:	return FindMooredBoat() ? Rules->ToBoatPrompt : Rules->ToWaterPrompt;
	default:					return FText::GetEmpty();
	}
}

bool AGalleonActor::CanInteract_Implementation(APawn* User) const
{
	return IsHelmUsableBy(User) || GetLadderMove(User) != ELadderMove::None;
}

void AGalleonActor::Interact_Implementation(APawn* User)
{
	ACharacter* Character = Cast<ACharacter>(User);
	if (!HasAuthority() || !Character)
	{
		return;
	}
	// at the wheel the key belongs to the departure (UGalleonCrewComponent: held 2 s, checked by the server): a tap that
	// reaches the interaction raises nothing, ends nothing (network review T-268 R-01: one press no longer ends the day)
	if (IsHelmUsableBy(User))
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("%s: %s tapped the wheel: the anchor is raised by holding its key"), *GetName(), *GetNameSafe(User));
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
