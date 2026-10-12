// monke_island

#include "Village/VillageSpawner.h"
#include "Ceremony/FlagCeremonySite.h"
#include "Village/VillageAmbienceActor.h"
#include "Village/VillageDoorMath.h"
#include "Village/VillageDoorsComponent.h"
#include "Village/VillageHouseKit.h"
#include "Village/VillageHouseNav.h"
#include "Village/VillageMath.h"
#include "Items/LootMath.h"
#include "Items/LootRules.h"
#include "Boats/BoatStartSubsystem.h"
#include "Expedition/Islands/ExpeditionIslandValidation.h"
#include "monke_island.h"
#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AVillageSpawner::AVillageSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	// the clients need the fragment wherever they are on the atoll
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.0f);

	UBillboardComponent* Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetMobility(EComponentMobility::Static);
	RootComponent = Sprite;

	Doors = CreateDefaultSubobject<UVillageDoorsComponent>(TEXT("Doors"));
}

AVillageSpawner* AVillageSpawner::Find(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AVillageSpawner> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

const TArray<FVillageLayout>& AVillageSpawner::GetVillages() const
{
	static const TArray<FVillageLayout> None;
	return bBuilt && ActiveSet ? ActiveSet->Villages : None;
}

void AVillageSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AVillageSpawner, ActiveFragment);
}

void AVillageSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		// the server's choice may have come before the start of the game
		if (!ActiveFragment.IsNone())
		{
			Build(ActiveFragment);
		}
		return;
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_VillageRules, no village in the fragments"), *GetName());
		return;
	}
	if (Fragments.IsEmpty())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no fragment listed, no village in the fragments"), *GetName());
		return;
	}

	// a game started from L_Boot names its fragment in the travel URL: known at once
	const FString OptionMatch = FString::Printf(TEXT("%s="), ExpeditionIslands::FragmentOptionKey);
	const FName FromUrl = ExpeditionIslands::ParseFragmentOption(GetWorld()->URL.GetOption(*OptionMatch, nullptr));
	if (!FromUrl.IsNone())
	{
		if (Fragments.ContainsByPredicate([FromUrl](const FVillageFragmentEntry& Entry) { return Entry.FragmentId == FromUrl; }))
		{
			SetActiveFragment(FromUrl);
		}
		else
		{
			UE_LOG(Logmonke_island, Warning, TEXT("%s: the URL plays %s, which has no villages here"), *GetName(), *FromUrl.ToString());
		}
		return;
	}
	// a PIE opened on the level: the fragment is where the players are, once the boat start has placed them
	ResolveStartTime = GetWorld()->GetTimeSeconds();
	GetWorldTimerManager().SetTimer(ResolveTimer, this, &AVillageSpawner::ResolveFragment, Rules->ResolveInterval, true, FMath::Max(Rules->ResolveDelay, 0.01f));
}

void AVillageSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ResolveTimer);
	GetWorldTimerManager().ClearTimer(ContentTimer);
	for (AActor* Ambience : AmbienceActors)
	{
		if (IsValid(Ambience))
		{
			Ambience->Destroy();
		}
	}
	AmbienceActors.Reset();
	Super::EndPlay(EndPlayReason);
}

void AVillageSpawner::OnRep_ActiveFragment()
{
	// before BeginPlay, BeginPlay builds it
	if (HasActorBegunPlay())
	{
		Build(ActiveFragment);
	}
}

void AVillageSpawner::ResolveFragment()
{
	// the boat start knows the fragment of the arrival it used: exact, even where two arrivals are 331 m apart
	if (const UBoatStartSubsystem* BoatStart = UBoatStartSubsystem::Get(this))
	{
		const FName FromBoat = BoatStart->HasResolved() ? BoatStart->GetArrivalFragmentId() : NAME_None;
		if (!FromBoat.IsNone() && Fragments.ContainsByPredicate([FromBoat](const FVillageFragmentEntry& Entry) { return Entry.FragmentId == FromBoat; }))
		{
			GetWorldTimerManager().ClearTimer(ResolveTimer);
			UE_LOG(Logmonke_island, Log, TEXT("%s: the boat starts at the arrival of %s"), *GetName(), *FromBoat.ToString());
			SetActiveFragment(FromBoat);
			return;
		}
	}

	// otherwise where the players are
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Players.Add(Pawn->GetActorLocation());
		}
	}
	TArray<FVector> Arrivals;
	Arrivals.Reserve(Fragments.Num());
	for (const FVillageFragmentEntry& Entry : Fragments)
	{
		Arrivals.Add(Entry.Arrival);
	}
	const int32 Index = VillageMath::PickFragment(Arrivals, Players, Rules->ArrivalMatchRadius);
	if (Index != INDEX_NONE)
	{
		GetWorldTimerManager().ClearTimer(ResolveTimer);
		UE_LOG(Logmonke_island, Log, TEXT("%s: a player is at the arrival of %s"), *GetName(), *Fragments[Index].FragmentId.ToString());
		SetActiveFragment(Fragments[Index].FragmentId);
		return;
	}
	if (GetWorld()->GetTimeSeconds() - ResolveStartTime >= Rules->ResolveTimeout)
	{
		GetWorldTimerManager().ClearTimer(ResolveTimer);
		UE_LOG(Logmonke_island, Log, TEXT("%s: no player near the arrival of a fragment after %.0f s, no village in the fragments (the villages placed in the level stay)"),
			*GetName(), Rules->ResolveTimeout);
	}
}

void AVillageSpawner::SetActiveFragment(FName FragmentId)
{
	ActiveFragment = FragmentId;
	ForceNetUpdate();
	Build(FragmentId);
}

void AVillageSpawner::Build(FName FragmentId)
{
	if (bBuilt || FragmentId.IsNone())
	{
		return;
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_VillageRules, the villages of %s are not built"), *GetName(), *FragmentId.ToString());
		return;
	}
	const FVillageFragmentEntry* Entry = Fragments.FindByPredicate([FragmentId](const FVillageFragmentEntry& Candidate) { return Candidate.FragmentId == FragmentId; });
	if (!Entry)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: %s has no villages here"), *GetName(), *FragmentId.ToString());
		return;
	}
	UVillageSetDefinition* Set = Entry->Set.LoadSynchronous();
	if (!Set)
	{
		UE_LOG(Logmonke_island, Error, TEXT("%s: the villages of %s do not load (%s)"), *GetName(), *FragmentId.ToString(), *Entry->Set.ToString());
		return;
	}
	ActiveSet = Set;
	const double StartTime = FPlatformTime::Seconds();

	// the vegetation first: the tiki read the bushes as soon as the villages stand
	const int32 Cleared = ClearVegetation(Set->Villages);
	// one draw of the house contents for the fragment: the seed of a kit, otherwise a new one every game
	int32 ContentSeed = 0;
	for (const UVillageHouseKit* Kit : Rules->HouseKits)
	{
		if (Kit)
		{
			ContentBatch = Kit->ContentSpawnBatch;
			if (Kit->ContentSeed != 0)
			{
				ContentSeed = Kit->ContentSeed;
				break;
			}
		}
	}
	FRandomStream ContentStream(ContentSeed != 0 ? ContentSeed : FMath::Rand());
	int32 Instances = 0;
	int32 Lights = 0;
	for (int32 VillageIndex = 0; VillageIndex < Set->Villages.Num(); ++VillageIndex)
	{
		const FVillageLayout& Village = Set->Villages[VillageIndex];
		Instances += BuildMeshes(Village, VillageIndex, ContentStream);
		// nobody looks at a dedicated server
		if (GetNetMode() != NM_DedicatedServer)
		{
			Lights += BuildLights(Village);
		}
	}
	Doors->FinishBuild(Rules->bHousesBlockCamera);
	bBuilt = true;
	if (HasAuthority() && GetNetMode() != NM_Client)
	{
		SpawnFlagSites();
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: fragment %s, %d villages, %d instances, %d lights, %d vegetation instances cleared, %d houses with %d doorways (%d sliding panels), %d relics and %d objects to put in them (%.0f ms)"),
		*GetName(), *FragmentId.ToString(), Set->Villages.Num(), Instances, Lights, Cleared, Doors->GetHouseCount(), Doors->GetDoorwayCount(),
		Doors->GetPanelCount(), RelicsQueued, PendingContents.Num() - RelicsQueued, (FPlatformTime::Seconds() - StartTime) * 1000.0);
	if (!PendingContents.IsEmpty())
	{
		GetWorldTimerManager().SetTimer(ContentTimer, this, &AVillageSpawner::SpawnContentBatch, 0.05f, true, 0.0f);
	}
	OnVillagesBuilt.Broadcast(this);
}

int32 AVillageSpawner::ClearVegetation(const TArray<FVillageLayout>& Villages) const
{
	const float Pad = Rules->GetMaxMargin();
	TArray<FBox2D> VillageBoxes;
	FBox Area(ForceInit);
	for (const FVillageLayout& Village : Villages)
	{
		FBox2D Box(ForceInit);
		for (const FVillageClearArea& Clear : Village.Clearing)
		{
			Box += VillageMath::AreaBounds(Clear, Pad);
		}
		VillageBoxes.Add(Box);
		if (Box.bIsValid)
		{
			Area += FVector(Box.Min, -HALF_WORLD_MAX);
			Area += FVector(Box.Max, HALF_WORLD_MAX);
		}
	}
	if (!Area.IsValid)
	{
		return 0;
	}

	int32 Moved = 0;
	TArray<int32> Doomed;
	const double Hidden = Rules->HiddenDepth;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It == this)
		{
			continue;
		}
		TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
		for (UInstancedStaticMeshComponent* Component : Components)
		{
			const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
			const int32 Count = Component ? Component->GetInstanceCount() : 0;
			if (!Mesh || Count <= 0 || !Component->Bounds.GetBox().Intersect(Area) || !Rules->IsClearable(Mesh->GetPathName()))
			{
				continue;
			}
			const EVillagePlant Plant = Rules->GetPlant(Mesh->GetName());
			Doomed.Reset();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FTransform Transform;
				// already moved away (a camp, a marae)
				if (!Component->GetInstanceTransform(Index, Transform, true) || Transform.GetLocation().Z < -0.5 * Hidden)
				{
					continue;
				}
				const FVector2D Point(Transform.GetLocation());
				for (int32 VillageIndex = 0; VillageIndex < Villages.Num(); ++VillageIndex)
				{
					if (!VillageBoxes[VillageIndex].bIsValid || !VillageBoxes[VillageIndex].IsInside(Point))
					{
						continue;
					}
					const bool bUnder = Villages[VillageIndex].Clearing.ContainsByPredicate([this, &Point, Plant](const FVillageClearArea& Clear)
					{
						return VillageMath::IsInArea(Clear, Point, Rules->GetMargin(Clear.Kind, Plant));
					});
					if (bUnder)
					{
						Doomed.Add(Index);
						break;
					}
				}
			}
			// moved out of sight rather than removed: every instance keeps its index for the systems that count by index
			for (const int32 Index : Doomed)
			{
				FTransform Transform;
				Component->GetInstanceTransform(Index, Transform, true);
				Transform.AddToTranslation(FVector(0.0, 0.0, -Hidden));
				Component->UpdateInstanceTransform(Index, Transform, true, false, true);
			}
			if (!Doomed.IsEmpty())
			{
				Component->MarkRenderStateDirty();
				Moved += Doomed.Num();
			}
		}
	}
	return Moved;
}

int32 AVillageSpawner::BuildMeshes(const FVillageLayout& Village, int32 VillageIndex, FRandomStream& ContentStream)
{
	// the server alone puts the objects in the houses: they replicate
	const bool bContents = HasAuthority() && GetNetMode() != NM_Client;
	int32 Count = 0;
	for (const FVillageMeshGroup& Group : Village.Meshes)
	{
		if (!Group.Mesh || Group.Locations.IsEmpty())
		{
			continue;
		}
		// a house with a kit is built as its shell; its doors and its contents follow
		UVillageHouseKit* Kit = Rules->FindHouseKit(Group.Mesh);
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Component->SetupAttachment(RootComponent);
		Component->SetMobility(EComponentMobility::Static);
		Component->SetStaticMesh(Kit ? Kit->ShellMesh.Get() : Group.Mesh.Get());
		// the collision of the mesh (UCX) blocking everything, like the villages of the level
		Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		if (!Rules->bHousesBlockCamera)
		{
			Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		Component->SetCanEverAffectNavigation(false);
		Component->RegisterComponent();

		TArray<FTransform> Transforms;
		Transforms.Reserve(Group.Locations.Num());
		for (int32 Index = 0; Index < Group.Locations.Num(); ++Index)
		{
			const float Yaw = Group.Yaws.IsValidIndex(Index) ? Group.Yaws[Index] : 0.0f;
			const FTransform& House = Transforms.Emplace_GetRef(FRotator(0.0f, Yaw, 0.0f), Group.Locations[Index]);
			if (Kit)
			{
				const int32 HouseIndex = AddHouseInfo(*Kit, House, Village, VillageIndex);
				Doors->AddHouse(Kit, House, VillageIndex);
				if (bContents)
				{
					QueueHouseContents(*Kit, House, HouseIndex, ContentStream);
				}
			}
		}
		Component->AddInstances(Transforms, false, true, false);
		MeshComponents.Add(Component);
		Count += Transforms.Num();
	}
	return Count;
}

int32 AVillageSpawner::BuildLights(const FVillageLayout& Village)
{
	// the ambience dims the lights by day: without it they would burn all day long
	if (!Rules->AmbienceRules)
	{
		return 0;
	}
	UClass* AmbienceClass = Rules->AmbienceClass ? Rules->AmbienceClass.Get() : AVillageAmbienceActor::StaticClass();
	const FTransform Transform(Village.Center);
	AVillageAmbienceActor* Ambience = GetWorld()->SpawnActorDeferred<AVillageAmbienceActor>(AmbienceClass, Transform, this, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Ambience)
	{
		return 0;
	}
	Ambience->Rules = Rules->AmbienceRules;
	Ambience->VillageId = Village.Id;
	Ambience->VillageRadius = Village.Radius;
	Ambience->FinishSpawning(Transform);
	AmbienceActors.Add(Ambience);

	int32 Count = 0;
	for (const FVillageLightPoint& Point : Village.Lights)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
		Light->SetupAttachment(RootComponent);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Point.Candela);
		Light->SetAttenuationRadius(Point.Radius);
		Light->SetLightColor(FLinearColor::FromSRGBColor(Point.bLantern ? Rules->LanternColor : Rules->InteriorColor));
		Light->SetCastShadows(Point.bShadows);
		Light->SetSourceRadius(Rules->LightSourceRadius);
		Light->MaxDrawDistance = Rules->LightMaxDrawDistance;
		Light->RegisterComponent();
		Light->SetWorldLocation(Point.Location);
		LightComponents.Add(Light);
		Ambience->AddLight(Light, !Point.bLantern);
		++Count;
	}
	return Count;
}

void AVillageSpawner::QueueHouseContents(const UVillageHouseKit& Kit, const FTransform& House, int32 HouseIndex, FRandomStream& Stream)
{
	TArray<TSubclassOf<AActor>> Relics;
	for (const TSubclassOf<AActor>& Variant : Kit.RelicVariants)
	{
		if (Variant)
		{
			Relics.Add(Variant);
		}
	}
	if (Relics.IsEmpty() && Kit.RelicClass)
	{
		Relics.Add(Kit.RelicClass);
	}
	if (!Relics.IsEmpty() && !Kit.RelicSlots.IsEmpty())
	{
		const TSubclassOf<AActor> Relic = Relics[Stream.RandRange(0, Relics.Num() - 1)];
		const FVillageHouseSlot& Slot = Kit.RelicSlots[Stream.RandRange(0, Kit.RelicSlots.Num() - 1)];
		const FTransform Home = FTransform(FRotator(0.0f, Slot.Yaw, 0.0f), Slot.Location) * House;
		PendingContents.Add({ Relic, Home, Kit.RestClearance, HouseIndex, true });
		if (Houses.IsValidIndex(HouseIndex))
		{
			Houses[HouseIndex].RelicHome = Home;
		}
		++RelicsQueued;
	}
	const ULootRules* Table = Kit.ItemTable;
	if (!Table || Kit.ItemSlots.IsEmpty())
	{
		return;
	}
	TArray<float> Weights;
	Weights.Reserve(Table->Items.Num());
	for (const FLootEntry& Entry : Table->Items)
	{
		Weights.Add(Entry.ItemClass ? Entry.Weight : 0.0f);
	}
	const int32 Count = Stream.RandRange(FMath::Min(Kit.MinItemsPerHouse, Kit.MaxItemsPerHouse), FMath::Max(Kit.MinItemsPerHouse, Kit.MaxItemsPerHouse));
	for (const int32 SlotIndex : VillageDoorMath::PickDistinct(Kit.ItemSlots.Num(), Count, Stream))
	{
		const int32 Kind = LootMath::PickWeighted(Weights, Stream.FRand());
		if (Kind == INDEX_NONE)
		{
			return;
		}
		const FVillageHouseSlot& Slot = Kit.ItemSlots[SlotIndex];
		const float Yaw = Slot.Yaw + Stream.FRandRange(-Kit.ItemYawJitter, Kit.ItemYawJitter);
		PendingContents.Add({ Table->Items[Kind].ItemClass, FTransform(FRotator(0.0f, Yaw, 0.0f), Slot.Location) * House, Kit.RestClearance, HouseIndex, false });
	}
}

void AVillageSpawner::SpawnContentBatch()
{
	const int32 Batch = FMath::Min(FMath::Max(ContentBatch, 1), PendingContents.Num());
	for (int32 Index = 0; Index < Batch; ++Index)
	{
		const FHouseContent Content = PendingContents.Pop(EAllowShrinking::No);
		AActor* Item = GetWorld()->SpawnActorDeferred<AActor>(Content.Class, Content.Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Item)
		{
			continue;
		}
		Item->FinishSpawning(Content.Transform);
		if (!IsValid(Item))
		{
			continue;
		}
		// the pivot of a prop may be at its centre: the bottom of its collision rests on the slot, never mid-air
		const FBox Box = Item->GetComponentsBoundingBox(false);
		if (Box.IsValid)
		{
			Item->SetActorLocation(VillageDoorMath::RestOnSurface(Item->GetActorLocation(), Box.Min.Z, Content.Transform.GetLocation().Z, Content.Clearance),
				false, nullptr, ETeleportType::TeleportPhysics);
		}
		// asleep until someone touches it: houses full of objects cost the physics nothing
		TInlineComponentArray<UPrimitiveComponent*> Bodies(Item);
		for (UPrimitiveComponent* Body : Bodies)
		{
			if (Body->IsSimulatingPhysics())
			{
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				Body->PutAllRigidBodiesToSleep();
			}
		}
		HouseContents.Add(Item);
		// the house knows what is its own: its villager watches it and brings its relic to the flag ceremony
		if (Houses.IsValidIndex(Content.House))
		{
			FVillageHouseInfo& Home = Houses[Content.House];
			if (Content.bRelic)
			{
				Home.Relic = Item;
				Home.RelicHome = Item->GetActorTransform();
			}
			else
			{
				Home.Items.Add(Item);
			}
		}
	}
	if (PendingContents.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(ContentTimer);
		UE_LOG(Logmonke_island, Log, TEXT("%s: %d objects put in the houses (%d relics)"), *GetName(), HouseContents.Num(), RelicsQueued);
	}
}

int32 AVillageSpawner::AddHouseInfo(const UVillageHouseKit& Kit, const FTransform& House, const FVillageLayout& Village, int32 VillageIndex)
{
	FVillageHouseInfo& Info = Houses.AddDefaulted_GetRef();
	Info.Kit = &Kit;
	Info.Transform = House;
	Info.Village = VillageIndex;
	const FVector Outside = House.TransformPosition(Kit.OutsideLocation);
	Info.Door = Outside;
	Info.Gate = Outside;
	// the door point of the layout in front of this house: its villagers go round by the gate of its plot
	double Best = FMath::Square(400.0);
	for (const FVillageDoorPoint& Point : Village.Doors)
	{
		const double Distance = FVector::DistSquared2D(Point.Location, Outside);
		if (Distance < Best)
		{
			Best = Distance;
			Info.Door = Point.Location;
			Info.Gate = VillageMath::GateOf(Point.Location, Point.Yaw, Point.GateDistance);
		}
	}
	return Houses.Num() - 1;
}

int32 AVillageSpawner::FindHouseAt(const FVector& Location, float Margin) const
{
	for (int32 Index = 0; Index < Houses.Num(); ++Index)
	{
		const FVillageHouseInfo& House = Houses[Index];
		if (House.Kit && VillageHouseNav::IsInside(House.Kit->Interior, House.Transform.InverseTransformPosition(Location), Margin))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void AVillageSpawner::ForgetHouseContent(int32 House, const AActor* Content)
{
	if (!Houses.IsValidIndex(House) || !Content)
	{
		return;
	}
	FVillageHouseInfo& Info = Houses[House];
	if (Info.Relic.Get() == Content)
	{
		Info.Relic.Reset();
	}
	Info.Items.RemoveAll([Content](const TWeakObjectPtr<AActor>& Item) { return Item.Get() == Content; });
}

void AVillageSpawner::RestoreHouseRelic(int32 House, AActor* Relic)
{
	if (Houses.IsValidIndex(House) && Relic)
	{
		Houses[House].Relic = Relic;
	}
}

void AVillageSpawner::OpenSealedHouses(int32 Village)
{
	Doors->UnsealVillage(Village);
}

void AVillageSpawner::SpawnFlagSites()
{
	if (!Rules || !Rules->FlagSiteClass || !Rules->FlagCeremonyRules || !ActiveSet)
	{
		return;
	}
	int32 Spawned = 0;
	int32 Placed = 0;
	int32 Hamlets = 0;
	const TArray<FVillageLayout>& Villages = ActiveSet->Villages;
	for (int32 Index = 0; Index < Villages.Num(); ++Index)
	{
		const FVillageLayout& Village = Villages[Index];
		// the zones, and the hamlets between them when the rules say so (no guardian there: bZone stays false)
		if (!Village.bZone && !Rules->bFlagSitesInHamlets)
		{
			continue;
		}
		// a site placed in the level near the square stands for this village
		bool bPlaced = false;
		for (TActorIterator<AFlagCeremonySite> It(GetWorld()); It; ++It)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Village.Center) <= Rules->FlagSiteMatchRadius)
			{
				It->VillageIndex = Index;
				if (It->ZoneId.IsNone())
				{
					It->ZoneId = Village.Id;
				}
				bPlaced = true;
			}
		}
		if (bPlaced)
		{
			++Placed;
			continue;
		}
		const FTransform Transform(FRotator::ZeroRotator, Village.Center);
		AFlagCeremonySite* Site = GetWorld()->SpawnActorDeferred<AFlagCeremonySite>(Rules->FlagSiteClass, Transform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Site)
		{
			continue;
		}
		Site->Rules = Rules->FlagCeremonyRules;
		Site->ZoneId = Village.Id;
		Site->VillageIndex = Index;
		Site->FinishSpawning(Transform);
		FlagSites.Add(Site);
		++Spawned;
		Hamlets += Village.bZone ? 0 : 1;
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d flag sites on the squares of %s, %d of them in hamlets (%d already in the level)"), *GetName(), Spawned,
		*ActiveFragment.ToString(), Hamlets, Placed);
}
