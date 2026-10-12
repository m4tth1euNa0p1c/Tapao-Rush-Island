// monke_island

#include "Creatures/TikiDirector.h"
#include "Ceremony/FlagCeremonyMath.h"
#include "Ceremony/FlagCeremonyRules.h"
#include "Ceremony/FlagCeremonySite.h"
#include "Creatures/TikiCorpse.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRagdollCorpse.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiTacticsMath.h"
#include "Creatures/TikiVillager.h"
#include "Creatures/TikiWarrior.h"
#include "DayNight/DayNightCycleActor.h"
#include "Items/ConsumablePickupActor.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Stealth/StealthRules.h"
#include "Stealth/StealthSubsystem.h"
#include "Village/VillageHouseKit.h"
#include "Village/VillageHouseNav.h"
#include "Village/VillageMath.h"
#include "Village/VillageSpawner.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/BillboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Run/RunCampaign.h"
#include "Run/RunSubsystem.h"

namespace
{
	/** Side of a cell of the bush grid (cm) */
	constexpr double BushCellSize = 2000.0;

	/** Multiplier of the hostile counts for the time of day (DaySpawnScale, NightSpawnScale) */
	float SpawnScaleOf(const UTikiRules* Rules, bool bNight, const UObject* WorldContext)
	{
		const float TimeOfDay = Rules ? FMath::Max(0.0f, bNight ? Rules->NightSpawnScale : Rules->DaySpawnScale) : 1.0f;
		// ADR-007 §11: the zone of the contract and a greed day add their danger; the campaign brings more of them out
		// of the bushes by day from its day (owner 2026-10-07); the danger of the run also carries the mob count of the
		// day's difficulty (DA_RunRules.DifficultyByDay.MobCountScale, owner 2026-10-09: the bushes, the night, the villages)
		const URunSubsystem* Run = URunSubsystem::Get(WorldContext);
		const float ByDay = Run && !bNight ? Run->GetDayBushScale() : 1.0f;
		return TimeOfDay * ByDay * (Run ? Run->GetDangerScale() : 1.0f);
	}

	/** Quiet zones of the level (a sacred site such as the marae): actors tagged TikiQuietZone, with a sphere
	 *  component or a "Radius.<cm>" tag. Their bushes hide no tiki and no item is scattered there */
	TArray<FSphere> FindQuietZones(UWorld* World)
	{
		static const FName QuietZoneTag(TEXT("TikiQuietZone"));
		TArray<FSphere> Zones;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->ActorHasTag(QuietZoneTag))
			{
				continue;
			}
			const USphereComponent* Sphere = It->FindComponentByClass<USphereComponent>();
			const FName RadiusTag = TikiMath::TagValue(It->Tags, TEXT("Radius"));
			const double Radius = Sphere ? Sphere->GetScaledSphereRadius() : (RadiusTag.IsNone() ? 0.0 : FCString::Atod(*RadiusTag.ToString()));
			if (Radius > 0.0)
			{
				Zones.Emplace(It->GetActorLocation(), Radius);
			}
		}
		return Zones;
	}

	/** True when Location (2D) lies in one of the zones */
	bool IsInAnyZone(const FVector& Location, const TArray<FSphere>& Zones)
	{
		for (const FSphere& Zone : Zones)
		{
			if (FVector::DistSquared2D(Location, Zone.Center) <= FMath::Square(Zone.W))
			{
				return true;
			}
		}
		return false;
	}

	/** True when the flame of a lit torch shows Pawn at night to a monster hidden in a bush Distance away (T-275): the
	 *  same rule as for the warriors out of their bushes; crouched, the player stays hidden */
	bool IsTorchSeenFromBush(const UTikiRules* Rules, const UStealthSubsystem* Stealth, const APawn* Pawn, double Distance)
	{
		const ACharacter* Player = Cast<ACharacter>(Pawn);
		bool bRaised = false;
		return Rules && Stealth && Player && Rules->Pack.bTorchRevealsAtNight
			&& TikiTactics::TorchReveals(Stealth->IsNight(), Player->bIsCrouched, ATikiWarrior::HoldsLitTorch(Player, bRaised), static_cast<float>(Distance),
				FMath::Min(Rules->Pack.TorchNightSightCm, Rules->AmbushTriggerRadius));
	}
}

ATikiDirector::ATikiDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	RootComponent = Sprite;
}

ATikiDirector* ATikiDirector::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ATikiDirector> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ATikiDirector::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_TikiRules, the tiki stay away"), *GetName());
		return;
	}

	FindVillages();

	// the bushes of the island hide a crouched player (T-207) from every mob that asks the stealth (T-262)
	if (UStealthSubsystem* Stealth = UStealthSubsystem::Get(this))
	{
		Stealth->SetCoverQuery(FStealthCoverQuery::CreateWeakLambda(this, [this](const FVector& Location, float Radius)
		{
			FVector Bush;
			return FindNearestBush(Location, Radius, Bush);
		}));
	}

	// the villages of the fragment played, built now or in a moment (once the server knows where the players are)
	if (AVillageSpawner* Spawner = AVillageSpawner::Find(this))
	{
		if (Spawner->IsBuilt())
		{
			UseFragmentVillages(Spawner);
		}
		else
		{
			Spawner->OnVillagesBuilt.AddUObject(this, &ATikiDirector::UseFragmentVillages);
		}
	}

	// coconuts already placed in a village are gathered too
	for (TActorIterator<AConsumablePickupActor> It(GetWorld()); It; ++It)
	{
		for (const FTikiVillage& Village : Villages)
		{
			if (FVector::Dist2D(It->GetActorLocation(), Village.Center) <= Village.Radius)
			{
				Coconuts.Add(*It);
				break;
			}
		}
	}

	GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &ATikiDirector::Think, Rules->ThinkInterval, true, 0.5f);

	// the bush grid is built once, early in the day, not on the first night spawn; the guardians wait for it
	FTimerHandle CacheTimer;
	GetWorld()->GetTimerManager().SetTimer(CacheTimer, this, &ATikiDirector::CacheBushes, 2.0f, false);
	FTimerHandle GuardianTimer;
	GetWorld()->GetTimerManager().SetTimer(GuardianTimer, this, &ATikiDirector::SpawnGuardians, FMath::Max(Rules->GuardianSpawnDelay, 2.1f), false);
	GetWorld()->GetTimerManager().SetTimer(TotemTimer, this, &ATikiDirector::ScatterItems, FMath::Max(Rules->GuardianSpawnDelay, 2.1f) + 0.5f, false);
}

void ATikiDirector::FindVillages()
{
	static const FName ZoneTag(TEXT("PacificationZone"));
	static const FName SpawnTag(TEXT("NpcSpawn"));
	static const FName PoiTag(TEXT("VillagePOI"));
	static const FName GuardianRole(TEXT("guardian"));
	Villages.Reset();
	TMap<FName, int32> ById;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const AActor* Marker = *It;
		const FName Id = Marker->ActorHasTag(ZoneTag) ? TikiMath::TagValue(Marker->Tags, TEXT("Village")) : NAME_None;
		if (Id.IsNone() || ById.Contains(Id))
		{
			continue;
		}
		FTikiVillage& Village = Villages.AddDefaulted_GetRef();
		Village.Id = Id;
		Village.Center = Marker->GetActorLocation();
		const USphereComponent* Sphere = Marker->FindComponentByClass<USphereComponent>();
		Village.Radius = Sphere ? Sphere->GetScaledSphereRadius() : VillageRadius;
		ById.Add(Id, Villages.Num() - 1);
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const AActor* Marker = *It;
		static const FName DoorTag(TEXT("VillageDoor"));
		const bool bSpawn = Marker->ActorHasTag(SpawnTag);
		const bool bPoi = !bSpawn && Marker->ActorHasTag(PoiTag);
		const bool bDoor = !bSpawn && !bPoi && Marker->ActorHasTag(DoorTag);
		const int32* Index = bSpawn || bPoi || bDoor ? ById.Find(TikiMath::TagValue(Marker->Tags, TEXT("Village"))) : nullptr;
		if (!Index)
		{
			continue;
		}
		FTikiVillage& Village = Villages[*Index];
		if (bDoor)
		{
			// the gate is on the marker's forward vector, "Gate.<cm>" away
			const FName GateTag = TikiMath::TagValue(Marker->Tags, TEXT("Gate"));
			const double GateDistance = GateTag.IsNone() ? 0.0 : FCString::Atod(*GateTag.ToString());
			FTikiDoor& Door = Village.Doors.AddDefaulted_GetRef();
			Door.Door = Marker->GetActorLocation();
			Door.Gate = Door.Door + Marker->GetActorForwardVector().GetSafeNormal2D() * GateDistance;
			continue;
		}
		if (bSpawn)
		{
			(TikiMath::TagValue(Marker->Tags, TEXT("Role")) == GuardianRole ? Village.GuardianSpawns : Village.VillagerSpawns).Add(Marker->GetActorLocation());
			continue;
		}
		const ETikiActivity Activity = TikiMath::ActivityFromName(TikiMath::TagValue(Marker->Tags, TEXT("Activity")));
		if (Activity != ETikiActivity::None)
		{
			FTikiPoi& Poi = Village.Pois.AddDefaulted_GetRef();
			Poi.Location = Marker->GetActorLocation();
			Poi.Facing = Marker->GetActorForwardVector().GetSafeNormal2D();
			Poi.Activity = Activity;
		}
	}
	for (const FTikiVillage& Village : Villages)
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: village %s, radius %.0f m, %d villager spawn points, %d guardian points, %d points of interest, %d houses"),
			*GetName(), *Village.Id.ToString(), Village.Radius / 100.0f, Village.VillagerSpawns.Num(), Village.GuardianSpawns.Num(), Village.Pois.Num(), Village.Doors.Num());
	}
	if (Villages.IsEmpty())
	{
		FTikiVillage& Village = Villages.AddDefaulted_GetRef();
		Village.Id = FName(TEXT("Director"));
		Village.Center = GetActorLocation();
		Village.Radius = VillageRadius;
		UE_LOG(Logmonke_island, Log, TEXT("%s: no village marker in the level, the villagers stay around the director"), *GetName());
	}
}

void ATikiDirector::UseFragmentVillages(const AVillageSpawner* Spawner)
{
	if (!Spawner || Spawner->GetVillages().IsEmpty())
	{
		return;
	}
	// the villages of the markers stand far from the fragment played: whoever already came out of them leaves
	int32 Gone = 0;
	for (FTikiVillage& Village : Villages)
	{
		for (const TWeakObjectPtr<ATikiVillager>& Villager : Village.Villagers)
		{
			if (Villager.IsValid())
			{
				Villager->Destroy();
				++Gone;
			}
		}
		for (const TWeakObjectPtr<ATikiWarrior>& Guardian : Village.Guardians)
		{
			if (Guardian.IsValid())
			{
				Guardian->Destroy();
				++Gone;
			}
		}
	}
	Villages.Reset();

	static const FName GuardianRole(TEXT("guardian"));
	for (const FVillageLayout& Layout : Spawner->GetVillages())
	{
		FTikiVillage& Village = Villages.AddDefaulted_GetRef();
		Village.Id = Layout.Id;
		Village.Center = Layout.Center;
		Village.Radius = Layout.Radius;
		Village.bZone = Layout.bZone;
		Village.VillagerCount = Layout.VillagerCount;
		for (const FVillageSpawnPoint& Spawn : Layout.Spawns)
		{
			(Spawn.Role == GuardianRole ? Village.GuardianSpawns : Village.VillagerSpawns).Add(Spawn.Location);
		}
		for (const FVillagePoiPoint& Point : Layout.Pois)
		{
			const ETikiActivity Activity = TikiMath::ActivityFromName(Point.Activity);
			if (Activity != ETikiActivity::None)
			{
				FTikiPoi& Poi = Village.Pois.AddDefaulted_GetRef();
				Poi.Location = Point.Location;
				Poi.Facing = FRotator(0.0f, Point.Yaw, 0.0f).Vector();
				Poi.Activity = Activity;
			}
		}
		for (const FVillageDoorPoint& Point : Layout.Doors)
		{
			FTikiDoor& Door = Village.Doors.AddDefaulted_GetRef();
			Door.Door = Point.Location;
			Door.Gate = VillageMath::GateOf(Point.Location, Point.Yaw, Point.GateDistance);
		}
		// one villager per resident of each house of the village built from a kit (they appear when a player comes)
		const int32 VillageIndex = Villages.Num() - 1;
		for (int32 House = 0; House < Spawner->GetHouseCount(); ++House)
		{
			const FVillageHouseInfo* Info = Spawner->GetHouse(House);
			if (Info && Info->Kit && Info->Village == VillageIndex && !Info->Kit->Nodes.IsEmpty())
			{
				for (int32 Resident = 0; Resident < Info->Kit->Residents; ++Resident)
				{
					Village.ResidentHouses.Add(House);
				}
			}
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s: village %s (%s) of %s, radius %.0f m, %d villager spawn points, %d guardian points, %d points of interest, %d houses"),
			*GetName(), *Village.Id.ToString(), *Layout.Name, *Spawner->GetActiveFragment().ToString(), Village.Radius / 100.0f,
			Village.VillagerSpawns.Num(), Village.GuardianSpawns.Num(), Village.Pois.Num(), Village.Doors.Num());
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: the %d villages of %s replace those of the markers (%d tiki sent away)"),
		*GetName(), Villages.Num(), *Spawner->GetActiveFragment().ToString(), Gone);
	FragmentSpawner = AVillageSpawner::Find(this);

	// the vegetation under the villages is gone: the bushes are read again, and the ambush zones with them
	if (bBushesCached)
	{
		bBushesCached = false;
		CacheBushes();
	}
	// past the guardian delay, the new villages get theirs at once
	if (bGuardianTimeReached)
	{
		SpawnGuardians();
	}
}

void ATikiDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bTotemPlacementStarted && NextTotemRequest < TotemRequests.Num())
	{
		FinishTotemPlacement(true);
	}
	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
	if (AVillageSpawner* Spawner = AVillageSpawner::Find(this))
	{
		Spawner->OnVillagesBuilt.RemoveAll(this);
	}
	if (UStealthSubsystem* Stealth = UStealthSubsystem::Get(this))
	{
		Stealth->ClearCoverQuery(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool ATikiDirector::IsNight() const
{
	const ADayNightCycleActor* Cycle = ADayNightCycleActor::Get(this);
	return Cycle && Cycle->GetPhase() == EDayPhase::Night;
}

void ATikiDirector::Think()
{
	const double Now = GetWorld()->GetTimeSeconds();
	UpdateActivity();
	TickVillage(Now);
	TickNight(Now);
	TickAmbush(Now);
	CheckTheft();
#if !UE_BUILD_SHIPPING
	RunDebugActions();
#endif

	Defenders.RemoveAll([](const TWeakObjectPtr<ATikiWarrior>& W) { return !W.IsValid(); });
}

void ATikiDirector::TickVillage(double Now)
{
	Coconuts.RemoveAll([](const TWeakObjectPtr<AActor>& C) { return !C.IsValid() || C->IsActorBeingDestroyed(); });
	for (auto It = Claims.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// one villager per think over all the villages, so they do not all appear on the same frame
	bool bSpawned = false;
	int32 ResidentsSpawned = 0;
	for (int32 Index = 0; Index < Villages.Num(); ++Index)
	{
		FTikiVillage& Village = Villages[Index];
		Village.Villagers.RemoveAll([](const TWeakObjectPtr<ATikiVillager>& V) { return !V.IsValid() || V->IsDead(); });
		Village.Residents.RemoveAll([](const TWeakObjectPtr<ATikiVillager>& V) { return !V.IsValid() || V->IsDead(); });
		// the villagers of the houses appear inside once a player comes near, a few per think
		while (VillagerClass && Village.bActive && !Village.ResidentHouses.IsEmpty() && ResidentsSpawned < Rules->ResidentSpawnPerThink
			&& Now - Village.LastAlarmTime >= Rules->HideDuration)
		{
			if (SpawnResident(Index))
			{
				++ResidentsSpawned;
			}
		}
		Village.Guardians.RemoveAll([](const TWeakObjectPtr<ATikiWarrior>& W) { return !W.IsValid() || W->IsDead(); });
		// nobody new comes to a village that has just been attacked: its villagers hide in their houses
		const int32 Wanted = Village.VillagerCount > 0 ? Village.VillagerCount : Rules->VillagerCount;
		if (!bSpawned && VillagerClass && Village.Villagers.Num() < Wanted && Now - Village.LastAlarmTime >= Rules->HideDuration)
		{
			bSpawned = SpawnVillager(Index);
		}
	}

	if (CoconutClass && !Villages.IsEmpty() && Coconuts.Num() < Rules->CoconutCount && Now - LastCoconutDropTime >= Rules->CoconutDropInterval)
	{
		const FTikiVillage& Village = Villages[FMath::RandRange(0, Villages.Num() - 1)];
		FVector Point;
		if (FindGroundPointNear(Village.Center, 0.0f, Village.Radius * Rules->VillagerWanderFraction, Point))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AActor* Coconut = GetWorld()->SpawnActor<AActor>(CoconutClass, Point + FVector(0.0f, 0.0f, 12.0f), FRotator(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f), Params))
			{
				Coconuts.Add(Coconut);
				LastCoconutDropTime = Now;
			}
		}
	}
}

void ATikiDirector::UpdateActivity()
{
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr)
		{
			Players.Add(Pawn->GetActorLocation());
		}
	}
	for (FTikiVillage& Village : Villages)
	{
		const bool bWasActive = Village.bActive;
		// the fallback village around the director always lives, as before the villages
		Village.bActive = (Village.Pois.IsEmpty() && Village.VillagerSpawns.IsEmpty())
			|| TikiMath::IsAnyWithin(Village.Center, Players, Rules->RoutineActivationRadius);
		if (Village.bActive != bWasActive)
		{
			UE_LOG(Logmonke_island, Log, TEXT("%s: village %s %s its routine"), *GetName(), *Village.Id.ToString(), Village.bActive ? TEXT("starts") : TEXT("stops"));
		}
	}
}

bool ATikiDirector::IsVillageActive(int32 VillageIndex) const
{
	return Villages.IsValidIndex(VillageIndex) && Villages[VillageIndex].bActive;
}

int32 ATikiDirector::ClaimPoi(ATikiVillager* Villager, int32 VillageIndex)
{
	if (!Villages.IsValidIndex(VillageIndex))
	{
		return INDEX_NONE;
	}
	ReleasePoi(Villager);
	FTikiVillage& Village = Villages[VillageIndex];
	TArray<int32, TInlineAllocator<64>> Free;
	for (int32 Index = 0; Index < Village.Pois.Num(); ++Index)
	{
		const ATikiVillager* Holder = Village.Pois[Index].Claimant.Get();
		if (!Holder || Holder->IsDead())
		{
			Free.Add(Index);
		}
	}
	if (Free.IsEmpty())
	{
		return INDEX_NONE;
	}
	const int32 Pick = Free[FMath::RandRange(0, Free.Num() - 1)];
	Village.Pois[Pick].Claimant = Villager;
	return Pick;
}

void ATikiDirector::ReleasePoi(ATikiVillager* Villager)
{
	for (FTikiVillage& Village : Villages)
	{
		for (FTikiPoi& Poi : Village.Pois)
		{
			if (Poi.Claimant.Get() == Villager)
			{
				Poi.Claimant.Reset();
			}
		}
	}
}

bool ATikiDirector::GetPoi(int32 VillageIndex, int32 PoiIndex, FTikiPoi& OutPoi) const
{
	if (!Villages.IsValidIndex(VillageIndex) || !Villages[VillageIndex].Pois.IsValidIndex(PoiIndex))
	{
		return false;
	}
	OutPoi = Villages[VillageIndex].Pois[PoiIndex];
	return true;
}

void ATikiDirector::RaiseAlarm(ATikiVillager* Villager, APawn* Attacker)
{
	if (!Rules || !Villager || !Attacker)
	{
		return;
	}
	const int32 Index = Villager->GetVillage();
	if (!Villages.IsValidIndex(Index) || Villages[Index].VillagerSpawns.IsEmpty())
	{
		// no village markers: the hit villager runs away and one defender comes out, as before
		Villager->Alarm(Attacker, Rules->FleeDuration);
		CallDefender(Villager, Attacker);
		return;
	}
	FTikiVillage& Village = Villages[Index];
	int32 Fleeing = 0;
	// the villagers of the houses panic when the attacker is near them, the others stay in
	for (const TArray<TWeakObjectPtr<ATikiVillager>>* List : { &Village.Villagers, &Village.Residents })
	{
		for (const TWeakObjectPtr<ATikiVillager>& Other : *List)
		{
			if (Other.IsValid() && !Other->IsDead())
			{
				Other->Alarm(Attacker, Rules->AlarmFleeDuration);
				++Fleeing;
			}
		}
	}
	for (const TWeakObjectPtr<ATikiWarrior>& Guardian : Village.Guardians)
	{
		if (Guardian.IsValid())
		{
			Guardian->Engage(Attacker);
		}
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - Village.LastAlarmTime < Rules->AlarmCooldown)
	{
		return;
	}
	Village.LastAlarmTime = Now;
	// never more than AlarmMaxAlivePerPlayer tiki after the same player: only what is missing comes out
	// four times fewer by day, a few more at night (DaySpawnScale, NightSpawnScale)
	const float Scale = SpawnScaleOf(Rules, IsNight(), this);
	const int32 Count = TikiMath::MissingWarriors(TikiMath::ScaleSpawnCount(Rules->AlarmWarriorCount, Scale),
		TikiMath::ScaleSpawnCount(Rules->AlarmMaxAlivePerPlayer, Scale), CountDefendersAgainst(Attacker));
	const float Phase = FMath::FRand();
	int32 Spawned = 0;
	for (int32 Warrior = 0; Warrior < Count; ++Warrior)
	{
		const FVector Point = TikiMath::AlarmRingPoint(Village.Center, Village.Radius * Rules->AlarmRingFraction, Warrior, Count, Phase);
		if (ATikiWarrior* Tiki = SpawnWarriorFromBush(Point, Rules->AlarmBushRadius, Attacker, ETikiWarriorRole::Defender))
		{
			Defenders.Add(Tiki);
			++Spawned;
		}
	}
	LastDefenderTime.Add(Attacker, Now);
	UE_LOG(Logmonke_island, Log, TEXT("%s hit by %s: village %s alarmed, %d villagers flee, %d tiki out of the bushes around it"),
		*Villager->GetName(), *Attacker->GetName(), *Village.Id.ToString(), Fleeing, Spawned);
}

void ATikiDirector::AvengeVillager(ATikiVillager* Villager, APawn* Killer)
{
	if (!Rules || !Villager || !Killer)
	{
		return;
	}
	// the killing blow may be the first one (a musket shot): the village runs away and its guardians attack as for a
	// hit; a second call in the same frame is held by the alarm cooldown
	RaiseAlarm(Villager, Killer);

	// then all the villagers run into their houses and hide there
	if (Villages.IsValidIndex(Villager->GetVillage()))
	{
		int32 Hiding = 0;
		const FTikiVillage& Village = Villages[Villager->GetVillage()];
		for (const TArray<TWeakObjectPtr<ATikiVillager>>* List : { &Village.Villagers, &Village.Residents })
		{
			for (const TWeakObjectPtr<ATikiVillager>& Other : *List)
			{
				if (Other.IsValid() && !Other->IsDead())
				{
					Other->HideAtHome(Killer, Rules->HideDuration);
					++Hiding;
				}
			}
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s killed: %d villagers of %s run into their houses"), *Villager->GetName(), Hiding,
			*Villages[Villager->GetVillage()].Id.ToString());
	}

	const float Scale = SpawnScaleOf(Rules, IsNight(), this);
	const int32 Count = TikiMath::MissingWarriors(TikiMath::ScaleSpawnCount(Rules->KillWarriorCount, Scale),
		TikiMath::ScaleSpawnCount(Rules->KillMaxAlivePerPlayer, Scale), CountDefendersAgainst(Killer));
	const FVector Around = Killer->GetActorLocation();
	const float Phase = FMath::FRand();
	int32 Spawned = 0;
	for (int32 Warrior = 0; Warrior < Count; ++Warrior)
	{
		const FVector Point = TikiMath::AlarmRingPoint(Around, Rules->KillRingRadius, Warrior, Count, Phase);
		if (ATikiWarrior* Tiki = SpawnWarriorFromBush(Point, Rules->KillBushRadius, Killer, ETikiWarriorRole::Defender))
		{
			Defenders.Add(Tiki);
			++Spawned;
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s killed by %s: %d tiki out of the bushes around the killer"), *Villager->GetName(), *Killer->GetName(), Spawned);
}

int32 ATikiDirector::CountDefendersAgainst(const APawn* Player) const
{
	int32 Alive = 0;
	for (const TWeakObjectPtr<ATikiWarrior>& Defender : Defenders)
	{
		if (Defender.IsValid() && !Defender->IsDead() && Defender->GetTarget() == Player)
		{
			++Alive;
		}
	}
	return Alive;
}

bool ATikiDirector::SpawnResident(int32 VillageIndex)
{
	FTikiVillage& Village = Villages[VillageIndex];
	AVillageSpawner* Spawner = FragmentSpawner.Get();
	if (Village.ResidentHouses.IsEmpty())
	{
		return false;
	}
	const int32 House = Village.ResidentHouses[0];
	Village.ResidentHouses.RemoveAt(0);
	const FVillageHouseInfo* Info = Spawner ? Spawner->GetHouse(House) : nullptr;
	if (!Info || !Info->Kit || Info->Kit->Nodes.IsEmpty())
	{
		return false;
	}
	// at a point of its routine, inside
	const TArray<int32> Points = VillageHouseNav::ActivityNodes(Info->Kit->Nodes);
	const int32 Node = Points.IsEmpty() ? Info->Kit->DoorNode : Points[FMath::RandRange(0, Points.Num() - 1)];
	if (!Info->Kit->Nodes.IsValidIndex(Node))
	{
		return false;
	}
	const FVillageHouseNode& Point = Info->Kit->Nodes[Node];
	const FVector Location = Info->Transform.TransformPosition(Point.Location) + FVector(0.0f, 0.0f, 70.0f);
	const FTransform Transform(FRotator(0.0f, Info->Transform.Rotator().Yaw + Point.Yaw, 0.0f), Location);
	ATikiVillager* Villager = GetWorld()->SpawnActorDeferred<ATikiVillager>(VillagerClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Villager)
	{
		return false;
	}
	Villager->Rules = Rules;
	Villager->SetDirector(this);
	Villager->SetHome(Village.Center, Village.Radius * Rules->VillagerWanderFraction);
	Villager->SetVillage(VillageIndex);
	Villager->SetResidence(Spawner, House);
	Villager->FinishSpawning(Transform);
	Village.Residents.Add(Villager);
	UE_LOG(Logmonke_island, Log, TEXT("%s: villager %s lives in house %d of %s (%s, %d still to come)"), *GetName(), *Villager->GetName(), House,
		*Village.Id.ToString(), *Point.Room.ToString(), Village.ResidentHouses.Num());
	return true;
}

void ATikiDirector::CheckTheft()
{
	AVillageSpawner* Spawner = FragmentSpawner.Get();
	if (!Spawner || !Rules)
	{
		return;
	}
	for (const FTikiVillage& Village : Villages)
	{
		for (const TWeakObjectPtr<ATikiVillager>& Resident : Village.Residents)
		{
			ATikiVillager* Villager = Resident.Get();
			// only a villager at home sees what goes out of its house
			if (!Villager || !Villager->IsWatchingHouse())
			{
				continue;
			}
			const int32 HouseIndex = Villager->GetResidence();
			const FVillageHouseInfo* House = Spawner->GetHouse(HouseIndex);
			if (!House || !House->Kit)
			{
				continue;
			}
			TArray<AActor*, TInlineAllocator<8>> Contents;
			if (AActor* Relic = House->Relic.Get())
			{
				Contents.Add(Relic);
			}
			for (const TWeakObjectPtr<AActor>& Item : House->Items)
			{
				if (AActor* Object = Item.Get())
				{
					Contents.Add(Object);
				}
			}
			for (AActor* Content : Contents)
			{
				if (VillageHouseNav::IsInside(House->Kit->Interior, House->Transform.InverseTransformPosition(Content->GetActorLocation()), Rules->TheftMargin))
				{
					continue;
				}
				APawn* Thief = FindThief(Content);
				Spawner->ForgetHouseContent(HouseIndex, Content);
				// a thief it does not see (crouched, the night: T-262) takes the object away unnoticed
				const UStealthSubsystem* Stealth = UStealthSubsystem::Get(this);
				if (Thief && Stealth && !Stealth->CanSee(Villager, Thief, 0.0f))
				{
					UE_LOG(LogMonkeStealth, Log, TEXT("%s: %s carried %s out of house %d unseen by %s"), *GetName(), *Thief->GetName(),
						*Content->GetName(), HouseIndex, *Villager->GetName());
					continue;
				}
				if (Thief)
				{
					UE_LOG(Logmonke_island, Log, TEXT("%s: %s carried %s out of house %d: %s panics"), *GetName(), *Thief->GetName(),
						*Content->GetName(), HouseIndex, *Villager->GetName());
					Villager->Panic(Thief);
					break;
				}
			}
		}
	}
}

APawn* ATikiDirector::FindThief(const AActor* Content) const
{
	APawn* Nearest = nullptr;
	double Best = FMath::Square(static_cast<double>(Rules->ThiefSearchRadius));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Pawn))
		{
			const UIslanderGrabComponent* Grab = Islander->GetGrabComponent();
			const UPrimitiveComponent* Held = Grab ? Grab->GetHeldComponent() : nullptr;
			if (Held && Held->GetOwner() == Content)
			{
				return Pawn;
			}
		}
		const double Distance = FVector::DistSquared(Pawn->GetActorLocation(), Content->GetActorLocation());
		if (Distance <= Best)
		{
			Best = Distance;
			Nearest = Pawn;
		}
	}
	return Nearest;
}

void ATikiDirector::StartGathering(AFlagCeremonySite* Site)
{
	const UFlagCeremonyRules* CeremonyRules = Site ? Site->Rules.Get() : nullptr;
	if (!CeremonyRules || !Rules)
	{
		return;
	}
	const FVector Centre = Site->GetGatherCenter();
	const FVector Cairn = Site->GetActorLocation();
	TArray<ATikiVillager*> Gatherers;
	for (int32 Index = 0; Index < Villages.Num(); ++Index)
	{
		const FTikiVillage& Village = Villages[Index];
		const double Distance = FVector::Dist2D(Village.Center, Cairn);
		const bool bOwn = Index == Site->VillageIndex || (Site->VillageIndex == INDEX_NONE && Distance <= Village.Radius);
		if (!bOwn && (CeremonyRules->NeighbourVillageRadius <= 0.0f || Distance > CeremonyRules->NeighbourVillageRadius))
		{
			continue;
		}
		for (const TArray<TWeakObjectPtr<ATikiVillager>>* List : { &Village.Villagers, &Village.Residents })
		{
			for (const TWeakObjectPtr<ATikiVillager>& Villager : *List)
			{
				if (Villager.IsValid() && !Villager->IsDead() && !Villager->IsHiding() && !Villager->IsInCeremony())
				{
					Gatherers.Add(Villager.Get());
				}
			}
		}
	}
	// the slots in the order the villagers stand around the centre: nobody crosses another's way
	Gatherers.Sort([&Centre](const ATikiVillager& A, const ATikiVillager& B)
	{
		return FlagCeremonyMath::AngleAround(Centre, A.GetActorLocation()) < FlagCeremonyMath::AngleAround(Centre, B.GetActorLocation());
	});
	const float Phase = Gatherers.IsEmpty() ? 0.0f : FlagCeremonyMath::AngleAround(Centre, Gatherers[0]->GetActorLocation());
	TArray<TWeakObjectPtr<ATikiVillager>>& Gathered = Gatherings.FindOrAdd(Site);
	Gathered.Reset();
	for (int32 Slot = 0; Slot < Gatherers.Num(); ++Slot)
	{
		Gatherers[Slot]->JoinCeremony(Site->GetOfficiant(), Centre, Cairn, FlagCeremonyMath::SlotAngle(Slot, Gatherers.Num(), Phase), CeremonyRules);
		Gathered.Add(Gatherers[Slot]);
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d villagers come to the flag of %s"), *GetName(), Gatherers.Num(), *Site->ZoneId.ToString());
}

void ATikiDirector::FinishGathering(AFlagCeremonySite* Site, bool bRaised)
{
	TArray<TWeakObjectPtr<ATikiVillager>> Gathered;
	if (!Gatherings.RemoveAndCopyValue(Site, Gathered))
	{
		return;
	}
	for (const TWeakObjectPtr<ATikiVillager>& Villager : Gathered)
	{
		if (Villager.IsValid() && !Villager->IsDead())
		{
			Villager->EndCeremony(bRaised);
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: the villagers leave the flag of %s (%s)"), *GetName(), *Site->ZoneId.ToString(),
		bRaised ? TEXT("raised, they lay their relics") : TEXT("interrupted, they go home"));
}

bool ATikiDirector::SpawnVillager(int32 VillageIndex)
{
	FTikiVillage& Village = Villages[VillageIndex];
	FVector Point;
	bool bFound = false;
	if (!Village.VillagerSpawns.IsEmpty())
	{
		TArray<FVector> Occupied;
		for (const TWeakObjectPtr<ATikiVillager>& Other : Village.Villagers)
		{
			if (Other.IsValid())
			{
				Occupied.Add(Other->GetActorLocation());
			}
		}
		const int32 Index = TikiMath::PickFreeSpawn(Village.VillagerSpawns, Occupied, Rules->SpawnPointGap, FMath::FRand());
		bFound = Index != INDEX_NONE && ProjectToGround(Village.VillagerSpawns[Index], Point);
	}
	else
	{
		bFound = FindGroundPointNear(Village.Center, 0.0f, Village.Radius * 0.6f, Point);
	}
	if (!bFound)
	{
		return false;
	}

	const FTransform Transform(FRotator(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f), Point + FVector(0.0f, 0.0f, 60.0f));
	ATikiVillager* Villager = GetWorld()->SpawnActorDeferred<ATikiVillager>(VillagerClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Villager)
	{
		return false;
	}
	Villager->Rules = Rules;
	Villager->SetDirector(this);
	Villager->SetHome(Village.Center, Village.Radius * Rules->VillagerWanderFraction);
	Villager->SetVillage(VillageIndex);
	// one house each, in turn
	if (!Village.Doors.IsEmpty())
	{
		const FTikiDoor& Door = Village.Doors[Village.NextDoor++ % Village.Doors.Num()];
		Villager->SetHouse(Door.Door, Door.Gate);
	}
	Villager->FinishSpawning(Transform);
	Village.Villagers.Add(Villager);
	UE_LOG(Logmonke_island, Log, TEXT("%s: villager %s in %s at %s (%d/%d)"), *GetName(), *Villager->GetName(), *Village.Id.ToString(), *Point.ToCompactString(), Village.Villagers.Num(), Rules->VillagerCount);
	return true;
}

void ATikiDirector::SpawnGuardians()
{
	bGuardianTimeReached = true;
	if (!Rules || Rules->GuardiansPerVillage <= 0)
	{
		return;
	}
	for (FTikiVillage& Village : Villages)
	{
		// the fallback village around the director has no guardian point, a hamlet is no zone to guard
		const int32 Points = Village.GuardianSpawns.Num();
		if (Points == 0 || Village.bGuardiansSpawned || !Village.bZone)
		{
			continue;
		}
		Village.bGuardiansSpawned = true;
		// at least one designated threat per zone, whatever the time of day
		const int32 Guardians = TikiMath::ScaleSpawnCount(Rules->GuardiansPerVillage, SpawnScaleOf(Rules, IsNight(), this));
		for (int32 Index = 0; Index < Guardians; ++Index)
		{
			const FVector& Point = Village.GuardianSpawns[(Index * Points / Guardians) % Points];
			if (ATikiWarrior* Guardian = SpawnWarriorFromBush(Point, Rules->GuardianBushRadius, nullptr, ETikiWarriorRole::Guardian))
			{
				Guardian->SetPost(Guardian->GetActorLocation(), Rules->GuardianPostRadius);
				Village.Guardians.Add(Guardian);
			}
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s: %d guardians out of the bushes around village %s"), *GetName(), Village.Guardians.Num(), *Village.Id.ToString());
	}
}

void ATikiDirector::TickNight(double Now)
{
	NightWarriors.RemoveAll([](const TWeakObjectPtr<ATikiWarrior>& W) { return !W.IsValid() || W->IsDead(); });

	if (!IsNight())
	{
		if (!NightWarriors.IsEmpty())
		{
			UE_LOG(Logmonke_island, Log, TEXT("%s: dawn, %d night warriors go back into the ground"), *GetName(), NightWarriors.Num());
		}
		for (const TWeakObjectPtr<ATikiWarrior>& Warrior : NightWarriors)
		{
			Warrior->Burrow();
		}
		NightWarriors.Reset();
		return;
	}

	const int32 NightCount = TikiMath::ScaleSpawnCount(Rules->NightWarriorCount, SpawnScaleOf(Rules, true, this));
	if (NightWarriors.Num() >= NightCount || Now - LastNightSpawnTime < Rules->NightSpawnInterval)
	{
		return;
	}

	// a random player, a point on the ring around it, the bush nearest to that point
	TArray<APawn*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr)
		{
			Players.Add(Pawn);
		}
	}
	if (Players.IsEmpty())
	{
		return;
	}
	APawn* Player = Players[FMath::RandRange(0, Players.Num() - 1)];
	const FVector Near = TikiMath::PointOnRing(Player->GetActorLocation(), FMath::FRandRange(0.0f, 2.0f * PI), FMath::FRand(), Rules->NightSpawnMinDistance, Rules->NightSpawnMaxDistance);
	// a failed try (sea, no ground) also waits for the next interval
	LastNightSpawnTime = Now;
	if (ATikiWarrior* Warrior = SpawnWarriorFromBush(Near, Rules->NightSpawnMaxDistance - Rules->NightSpawnMinDistance, nullptr, ETikiWarriorRole::Night))
	{
		NightWarriors.Add(Warrior);
		UE_LOG(Logmonke_island, Log, TEXT("%s: night, %s comes out at %s (%d/%d)"), *GetName(), *Warrior->GetName(), *Warrior->GetActorLocation().ToCompactString(), NightWarriors.Num(), NightCount);
	}
}

AActor* ATikiDirector::ClaimCoconutNear(ATikiVillager* Villager, const FVector& Location, float Radius)
{
	AActor* Best = nullptr;
	double BestDistSq = FMath::Square(static_cast<double>(Radius));
	for (const TWeakObjectPtr<AActor>& Coconut : Coconuts)
	{
		if (!Coconut.IsValid() || Claims.Contains(Coconut))
		{
			continue;
		}
		if (const AConsumablePickupActor* Pickup = Cast<AConsumablePickupActor>(Coconut.Get()); Pickup && Pickup->IsConsumed())
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Coconut->GetActorLocation(), Location);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Coconut.Get();
		}
	}
	if (Best)
	{
		Claims.Add(Best, Villager);
	}
	return Best;
}

void ATikiDirector::ReleaseCoconut(AActor* Coconut)
{
	Claims.Remove(Coconut);
}

bool ATikiDirector::CollectCoconut(ATikiVillager* Villager, AActor* Coconut)
{
	Claims.Remove(Coconut);
	if (!IsValid(Coconut) || Coconut->IsActorBeingDestroyed())
	{
		return false;
	}
	if (const AConsumablePickupActor* Pickup = Cast<AConsumablePickupActor>(Coconut); Pickup && Pickup->IsConsumed())
	{
		return false;
	}
	Coconuts.Remove(Coconut);
	Coconut->Destroy();
	UE_LOG(Logmonke_island, Verbose, TEXT("%s gathers a coconut"), *GetNameSafe(Villager));
	return true;
}

ATikiWarrior* ATikiDirector::CallDefender(ATikiVillager* Villager, APawn* Attacker)
{
	if (!Rules || !Villager || !Attacker)
	{
		return nullptr;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastDefenderTime.Find(Attacker); Last && Now - *Last < Rules->DefenderCooldown)
	{
		return nullptr;
	}
	if (CountDefendersAgainst(Attacker) >= TikiMath::ScaleSpawnCount(Rules->MaxDefendersPerPlayer, SpawnScaleOf(Rules, IsNight(), this)))
	{
		return nullptr;
	}

	ATikiWarrior* Warrior = SpawnWarriorFromBush(Villager->GetActorLocation(), Rules->BushSearchRadius, Attacker, ETikiWarriorRole::Defender);
	if (Warrior)
	{
		Defenders.Add(Warrior);
		LastDefenderTime.Add(Attacker, Now);
		UE_LOG(Logmonke_island, Log, TEXT("%s was hit by %s: %s leaps out of a bush"), *Villager->GetName(), *Attacker->GetName(), *Warrior->GetName());
	}
	return Warrior;
}

ATikiWarrior* ATikiDirector::SpawnWarriorFromBush(const FVector& Near, float BushRadius, APawn* Target, ETikiWarriorRole WarriorRole)
{
	const TSubclassOf<ATikiWarrior> WarriorClass = PickWarriorClass();
	if (!WarriorClass || !Rules)
	{
		return nullptr;
	}

	// out of the nearest bush, on the side facing the target; out of the ground when no bush is near
	FVector Bush;
	FVector Ground;
	const FVector Facing = Target ? Target->GetActorLocation() : Near;
	if (FindNearestBush(Near, BushRadius, Bush))
	{
		const FVector ToTarget = (Facing - Bush).GetSafeNormal2D();
		if (!ProjectToGround(Bush + ToTarget * 60.0f, Ground))
		{
			return nullptr;
		}
	}
	else if (!FindGroundPointNear(Near, 100.0f, 400.0f, Ground))
	{
		return nullptr;
	}
	// no tiki comes out of a quiet zone (a camp, the marae), even from a bush known before the zone cleared its ground
	if (IsInAnyZone(Ground, FindQuietZones(GetWorld())))
	{
		return nullptr;
	}

	const FVector LeapDirection = (Facing - Ground).GetSafeNormal2D();
	const FTransform Transform(LeapDirection.IsNearlyZero() ? FRotator::ZeroRotator : LeapDirection.Rotation(), Ground + FVector(0.0f, 0.0f, 80.0f));
	ATikiWarrior* Warrior = GetWorld()->SpawnActorDeferred<ATikiWarrior>(WarriorClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Warrior)
	{
		return nullptr;
	}
	Warrior->Rules = Rules;
	Warrior->SetDirector(this);
	Warrior->InitWarrior(WarriorRole, Target, LeapDirection.IsNearlyZero() ? Warrior->GetActorForwardVector() : LeapDirection);
	// the guardians and defenders of the villages come out with a musket once the campaign opened it (T-214)
	if ((WarriorRole == ETikiWarriorRole::Guardian || WarriorRole == ETikiWarriorRole::Defender) && Rules->Musket.Mesh
		&& URunSubsystem::IsContentUnlocked(this, ERunUnlock::ArmedMobs) && FMath::FRand() < Rules->Musket.ArmedShare)
	{
		Warrior->SetArmed(true);
	}
	Warrior->FinishSpawning(Transform);
	// the difficulty of the day (owner 2026-10-09): sturdier warriors, their health set by their BeginPlay just above
	const float HealthScale = URunSubsystem::DifficultyOf(this).MobHealthScale;
	if (UIslanderHealthComponent* WarriorHealth = Warrior->GetHealth(); WarriorHealth && !FMath::IsNearlyEqual(HealthScale, 1.0f))
	{
		WarriorHealth->ScaleMaxHealth(HealthScale);
	}
	if (Warrior->IsArmed())
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: %s comes out armed with a musket (%d balls)"), *GetName(), *Warrior->GetName(), Warrior->GetShotsLeft());
	}
	return Warrior;
}

TSubclassOf<ATikiWarrior> ATikiDirector::PickWarriorClass() const
{
	TArray<TSubclassOf<ATikiWarrior>> Valid;
	for (const TSubclassOf<ATikiWarrior>& Class : WarriorClasses)
	{
		if (Class)
		{
			Valid.Add(Class);
		}
	}
	return Valid.IsEmpty() ? nullptr : Valid[FMath::RandRange(0, Valid.Num() - 1)];
}

bool ATikiDirector::ProjectToGround(const FVector& Location, FVector& OutPoint) const
{
	const FVector Start(Location.X, Location.Y, Location.Z + 5000.0);
	const FVector End(Location.X, Location.Y, Location.Z - 5000.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiGround), false, this);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);

	// the vegetation crowns are skipped: only the ground counts; nothing under a village building (its roof is an
	// instanced mesh too) nor on a steep surface
	for (int32 Try = 0; Try < 8; ++Try)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params))
		{
			return false;
		}
		if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
		{
			const UStaticMesh* Mesh = Instances->GetStaticMesh();
			if (Mesh && Rules && TikiMath::HasKeyword(Mesh->GetName(), Rules->BuildingMeshKeywords))
			{
				return false;
			}
			Params.AddIgnoredComponent(Hit.GetComponent());
			continue;
		}
		if (Rules && Hit.ImpactNormal.Z < Rules->MinGroundNormalZ)
		{
			return false;
		}
		const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
		const float WaterZ = Water ? Water->GetWaterSurfaceAt(Hit.ImpactPoint) : 0.0f;
		if (Hit.ImpactPoint.Z < WaterZ + 10.0f)
		{
			return false;
		}
		OutPoint = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool ATikiDirector::FindGroundPointNear(const FVector& Center, float MinRadius, float MaxRadius, FVector& OutPoint) const
{
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const FVector Candidate = TikiMath::PointOnRing(Center, FMath::FRandRange(0.0f, 2.0f * PI), FMath::Sqrt(FMath::FRand()), MinRadius, MaxRadius);
		if (ProjectToGround(Candidate, OutPoint))
		{
			return true;
		}
	}
	return false;
}

FIntPoint ATikiDirector::BushCellOf(const FVector& Location)
{
	return FIntPoint(FMath::FloorToInt32(Location.X / BushCellSize), FMath::FloorToInt32(Location.Y / BushCellSize));
}

void ATikiDirector::CacheBushes()
{
	if (bBushesCached)
	{
		return;
	}
	bBushesCached = true;
	BushCells.Reset();
	if (!Rules)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();
	const TArray<FSphere> Quiet = FindQuietZones(GetWorld());
	int32 Meshes = 0;
	int32 Instances = 0;
	int32 Quieted = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
		for (UInstancedStaticMeshComponent* Component : Components)
		{
			const UStaticMesh* Mesh = Component->GetStaticMesh();
			const int32 Count = Component->GetInstanceCount();
			if (!Mesh || Count <= 0 || !TikiMath::IsBushMeshName(Mesh->GetName(), Rules->BushMeshKeywords))
			{
				continue;
			}
			++Meshes;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FTransform Transform;
				if (!Component->GetInstanceTransform(Index, Transform, true) || Transform.GetLocation().Z < Rules->BushMinZ)
				{
					continue;
				}
				// the bushes of a quiet zone hide no tiki
				if (IsInAnyZone(Transform.GetLocation(), Quiet))
				{
					++Quieted;
					continue;
				}
				BushCells.FindOrAdd(BushCellOf(Transform.GetLocation())).Add(Transform.GetLocation());
				++Instances;
			}
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d bush meshes, %d bushes in %d cells, %d left out in %d quiet zones (%.0f ms)"),
		*GetName(), Meshes, Instances, BushCells.Num(), Quieted, Quiet.Num(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
	if (HasAuthority())
	{
		BuildAmbushZones();
	}
}

void ATikiDirector::ScatterItems()
{
	// A non-replicating level actor can have local authority on a client: check NetMode as well.
	if (!HasAuthority() || GetNetMode() == NM_Client || !Rules || bTotemPlacementStarted)
	{
		return;
	}
	const AVillageSpawner* Spawner = AVillageSpawner::Find(this);
	if (Spawner && !Spawner->IsBuilt())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (TotemWaitStarted < 0.0) { TotemWaitStarted = Now; }
		if (Now - TotemWaitStarted < FMath::Max(1.0f, Rules->TotemVillageWaitTimeout))
		{
			GetWorldTimerManager().SetTimer(TotemTimer, this, &ATikiDirector::ScatterItems,
				FMath::Max(0.1f, Rules->TotemVillageRetryInterval), false);
			return;
		}
		bTotemPlacementStarted = true;
		UE_LOG(Logmonke_island, Warning, TEXT("%s: totems cancelled: fragment villages not ready after %.1f s; ground=0 houses=0"),
			*GetName(), Now - TotemWaitStarted);
		return;
	}
	bTotemPlacementStarted = true;
	TotemPlacementStarted = FPlatformTime::Seconds();
	for (const TSubclassOf<AActor>& Variant : Rules->ScatterItemVariants)
	{
		if (Variant) { TotemClasses.Add(Variant); }
	}
	if (TotemClasses.IsEmpty() && Rules->ScatterItemClass)
	{
		TotemClasses.Add(Rules->ScatterItemClass);
	}
	if (TotemClasses.IsEmpty())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no totem class in DA_TikiRules; ground=0 houses=0"), *GetName());
		return;
	}
	TotemQuietZones = FindQuietZones(GetWorld());
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	if (!Water)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no water subsystem; totem placement cancelled"), *GetName());
		FinishTotemPlacement(true);
		return;
	}
	TotemSeaLevel = Water->GetSeaLevel();
	TotemStream.Initialize(Rules->GroundTotemSeed);
	FBox2D Bounds(ForceInit);
	double BaseZ = GetActorLocation().Z;
	FName FragmentId;
	if (Spawner)
	{
		FragmentId = Spawner->GetActiveFragment();
		for (const FVillageFragmentEntry& Entry : Spawner->Fragments)
		{
			if (Entry.FragmentId == FragmentId)
			{
				Bounds += FVector2D(Entry.Arrival);
				BaseZ = Entry.Arrival.Z;
				break;
			}
		}
		for (const FVillageLayout& Village : Spawner->GetVillages())
		{
			Bounds += FVector2D(Village.Center);
			for (const FVillageClearArea& Area : Village.Clearing)
			{
				const FBox2D AreaBox = VillageMath::AreaBounds(Area, 0.0f);
				if (AreaBox.bIsValid) { Bounds += AreaBox; }
				if (Area.Kind == EVillageClearKind::Building || Area.Kind == EVillageClearKind::Plaza)
				{
					TotemClearAreas.Add(Area);
				}
			}
			for (const FVillageMeshGroup& Group : Village.Meshes)
			{
				if (!IsValid(Group.Mesh) || !TikiMath::HasKeyword(Group.Mesh->GetName(), Rules->TotemHouseMeshKeywords))
				{
					continue;
				}
				const FBox MeshBounds = Group.Mesh->GetBoundingBox();
				for (int32 Index = 0; Index < Group.Locations.Num(); ++Index)
				{
					const float Yaw = Group.Yaws.IsValidIndex(Index) ? Group.Yaws[Index] : 0.0f;
					const FTransform House(FRotator(0.0f, Yaw, 0.0f), Group.Locations[Index]);
					// Also protect houses from layouts without an exported clearing area.
					FVillageClearArea Footprint;
					Footprint.Center = FVector2D(House.TransformPosition(MeshBounds.GetCenter()));
					Footprint.HalfSize = FVector2D(MeshBounds.GetExtent());
					Footprint.Yaw = Yaw;
					TotemClearAreas.Add(Footprint);
					Bounds += VillageMath::AreaBounds(Footprint, 0.0f);
					if (Rules->bTotemInEveryHouse)
					{
						TikiMath::FTotemRequest& Request = TotemRequests.AddDefaulted_GetRef();
						Request.bHouse = true;
						Request.Location = TikiMath::HouseTotemPoint(House, MeshBounds, Rules->HouseTotemLocalOffset, Rules->HouseTotemInset);
						Request.Yaw = TikiMath::HouseTotemYaw(Yaw, Rules->HouseTotemYawRange, TotemStream.FRand());
					}
				}
			}
		}
	}
	else
	{
		// Legacy marker levels: their villages are the only runtime land envelope available.
		for (const FTikiVillage& Village : Villages)
		{
			Bounds += FVector2D(Village.Center) - FVector2D(Village.Radius);
			Bounds += FVector2D(Village.Center) + FVector2D(Village.Radius);
		}
	}
	HouseTotemsRequested = TotemRequests.Num();
	// T-143 : les DA de fragment n'exposent pas de polygone. Repli explicite sur les emprises des villages actifs
	// (hameaux inclus) + leur arriv?e, avec marge. Aucun JSON de l'atoll entier ni chargement synchrone.
	if (Bounds.bIsValid) { Bounds = Bounds.ExpandBy(FMath::Max(0.0f, Rules->GroundTotemBoundsPadding)); }
	GroundTotemLimit = FMath::Clamp(Rules->bUseLegacyScatterSpots ? Rules->ScatterCount : Rules->GroundTotemMax, 0, 1000);
	GroundTotemMinSpacing = FMath::Max(0.0f, Rules->bUseLegacyScatterSpots ? Rules->ScatterMinSpacing : Rules->GroundTotemSpacing);
	if (Rules->bUseLegacyScatterSpots)
	{
		FRandomStream Legacy(Rules->ScatterSeed != 0 ? Rules->ScatterSeed : Rules->GroundTotemSeed);
		for (const int32 Index : TikiMath::ShuffledIndices(Rules->ScatterSpots.Num(), Legacy))
		{
			if (Bounds.bIsValid && Bounds.IsInside(FVector2D(Rules->ScatterSpots[Index])))
			{
				TikiMath::FTotemRequest& Request = TotemRequests.AddDefaulted_GetRef();
				Request.Location = Rules->ScatterSpots[Index];
				Request.Yaw = Legacy.FRandRange(-180.0f, 180.0f);
			}
		}
	}
	else if (GroundTotemLimit > 0)
	{
		for (const FVector& Point : TikiMath::GroundTotemCandidates(Bounds, GroundTotemMinSpacing, Rules->GroundTotemSeed,
			Rules->GroundTotemJitter, Rules->GroundTotemCandidateBudget))
		{
			TikiMath::FTotemRequest& Request = TotemRequests.AddDefaulted_GetRef();
			Request.Location = FVector(Point.X, Point.Y, BaseZ);
			Request.Yaw = TotemStream.FRandRange(-180.0f, 180.0f);
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: totems fragment=%s seed=%d bounds=villages+arrival pad=%.0f cm houses=%d groundCandidates=%d mode=%s"),
		*GetName(), *FragmentId.ToString(), Rules->GroundTotemSeed, Rules->GroundTotemBoundsPadding,
		HouseTotemsRequested, TotemRequests.Num() - HouseTotemsRequested, Rules->bUseLegacyScatterSpots ? TEXT("legacy") : TEXT("poisson"));
	PlaceTotemBatch();
}

TikiMath::ETotemReject ATikiDirector::TraceTotemFloor(const TikiMath::FTotemRequest& Request, FVector& OutFloor) const
{
	using TikiMath::ETotemReject;
	const float Up = Request.bHouse ? Rules->HouseTotemTraceHeight : Rules->GroundTotemTraceHeight;
	const float Down = Request.bHouse ? Rules->HouseTotemTraceDepth : Rules->GroundTotemTraceHeight;
	const FVector Start = Request.Location + FVector(0.0, 0.0, FMath::Max(1.0f, Up));
	const FVector End = Request.Location - FVector(0.0, 0.0, FMath::Max(1.0f, Down));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TotemFloor), false, this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	for (int32 Pass = 0; Pass < FMath::Clamp(Rules->TotemTraceMaxPasses, 1, 64); ++Pass)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params)) { return ETotemReject::NoGround; }
		if (Hit.bStartPenetrating) { return ETotemReject::NoGround; }
		if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
		{
			const UStaticMesh* Mesh = Instances->GetStaticMesh();
			const bool bHouseMesh = Mesh && TikiMath::HasKeyword(Mesh->GetName(), Rules->TotemHouseMeshKeywords);
			const bool bBuilding = bHouseMesh || (Mesh && TikiMath::HasKeyword(Mesh->GetName(), Rules->BuildingMeshKeywords));
			if (!Request.bHouse && bBuilding) { return ETotemReject::Building; }
			if (!Request.bHouse || !bHouseMesh)
			{
				Params.AddIgnoredComponent(Hit.GetComponent());
				continue;
			}
		}
		OutFloor = Hit.ImpactPoint;
		return TikiMath::EvaluateTotemCandidate(OutFloor, true, TotemSeaLevel, Rules->TotemWaterClearance,
			Hit.ImpactNormal.Z, Rules->MinGroundNormalZ, GroundTotemTaken, GroundTotemMinSpacing, GroundTotemLimit,
			TotemQuietZones, TotemClearAreas, Rules->GroundTotemBuildingMargin, Request.bHouse);
	}
	return ETotemReject::NoGround;
}

bool ATikiDirector::SpawnRestingTotem(const TikiMath::FTotemRequest& Request, const FVector& Floor)
{
	if (!HasAuthority() || GetNetMode() == NM_Client || TotemClasses.IsEmpty()) { return false; }
	const TSubclassOf<AActor> Class = TotemClasses[TotemStream.RandRange(0, TotemClasses.Num() - 1)];
	const FTransform Transform(FRotator(0.0f, Request.Yaw, 0.0f), Floor);
	AActor* Item = GetWorld()->SpawnActorDeferred<AActor>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Item)) { return false; }
	Item->FinishSpawning(Transform);
	if (!IsValid(Item)) { return false; }
	Item->SetReplicates(true);
	Item->SetReplicateMovement(true);
	// The collision mesh can have a centred pivot. Put its bottom on the floor before sleeping, never mid-air.
	const FBox Box = Item->GetComponentsBoundingBox(false);
	if (!Box.IsValid)
	{
		Item->Destroy();
		return false;
	}
	const double FloorZ = Floor.Z + (Request.bHouse ? Rules->HouseTotemLocalOffset.Z : 0.0);
	Item->SetActorLocation(TikiMath::RestTotemOnFloor(Item->GetActorLocation(), Box.Min.Z, FloorZ, Rules->TotemRestClearance),
		false, nullptr, ETeleportType::TeleportPhysics);
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
	Item->ForceNetUpdate();
	ScatteredItems.Add(Item);
	return true;
}

void ATikiDirector::PlaceTotemBatch()
{
	using TikiMath::ETotemReject;
	if (!HasAuthority() || GetNetMode() == NM_Client || !IsValid(Rules))
	{
		FinishTotemPlacement(true);
		return;
	}
	const int32 End = FMath::Min(NextTotemRequest + FMath::Clamp(Rules->TotemPlacementsPerFrame, 1, 50), TotemRequests.Num());
	for (; NextTotemRequest < End; ++NextTotemRequest)
	{
		const TikiMath::FTotemRequest& Request = TotemRequests[NextTotemRequest];
		if (!Request.bHouse && GroundTotemTaken.Num() >= GroundTotemLimit)
		{
			TotemRejects.FindOrAdd(ETotemReject::Cap) += TotemRequests.Num() - NextTotemRequest;
			NextTotemRequest = TotemRequests.Num();
			break;
		}
		ETotemReject Reject = TikiMath::TotemExclusion(Request.Location, TotemQuietZones, TotemClearAreas,
			Rules->GroundTotemBuildingMargin, Request.bHouse);
		if (Reject == ETotemReject::None && !Request.bHouse
			&& !TikiMath::IsFarFromAll(Request.Location, GroundTotemTaken, GroundTotemMinSpacing))
		{
			Reject = ETotemReject::Spacing;
		}
		FVector Floor;
		if (Reject == ETotemReject::None) { Reject = TraceTotemFloor(Request, Floor); }
		if (Reject == ETotemReject::None && !Request.bHouse && Rules->bUseLegacyScatterSpots)
		{
			FVector Bush;
			if (FindNearestBush(Floor, Rules->ScatterBushClearance, Bush)) { Reject = ETotemReject::Bush; }
		}
		if (Reject == ETotemReject::None)
		{
			if (SpawnRestingTotem(Request, Floor))
			{
				if (Request.bHouse) { ++HouseTotemsPlaced; }
				else { GroundTotemTaken.Add(Floor); }
			}
			else { Reject = ETotemReject::Spawn; }
		}
		if (Reject != ETotemReject::None) { ++TotemRejects.FindOrAdd(Reject); }
	}
	if (NextTotemRequest < TotemRequests.Num())
	{
		TotemTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ATikiDirector::PlaceTotemBatch);
	}
	else
	{
		FinishTotemPlacement(false);
	}
}

void ATikiDirector::FinishTotemPlacement(bool bCancelled)
{
	using TikiMath::ETotemReject;
	GetWorldTimerManager().ClearTimer(TotemTimer);
	UE_LOG(Logmonke_island, Log, TEXT("%s: totems ground=%d houses=%d/%d rejected(water=%d quiet=%d building=%d noGround=%d slope=%d spacing=%d cap=%d bush=%d invalid=%d spawn=%d) duration=%.1f ms cancelled=%d"),
		*GetName(), GroundTotemTaken.Num(), HouseTotemsPlaced, HouseTotemsRequested,
		TotemRejects.FindRef(ETotemReject::Water), TotemRejects.FindRef(ETotemReject::Quiet), TotemRejects.FindRef(ETotemReject::Building),
		TotemRejects.FindRef(ETotemReject::NoGround), TotemRejects.FindRef(ETotemReject::Slope), TotemRejects.FindRef(ETotemReject::Spacing),
		TotemRejects.FindRef(ETotemReject::Cap), TotemRejects.FindRef(ETotemReject::Bush), TotemRejects.FindRef(ETotemReject::Invalid),
		TotemRejects.FindRef(ETotemReject::Spawn), (FPlatformTime::Seconds() - TotemPlacementStarted) * 1000.0, bCancelled);
	TotemRequests.Empty();
	TotemClasses.Empty();
	TotemClearAreas.Empty();
	TotemQuietZones.Empty();
	GroundTotemTaken.Empty();
	TotemRejects.Empty();
}

FIntPoint ATikiDirector::AmbushCellOf(const FVector& Location) const
{
	const double Size = Rules ? FMath::Max(static_cast<double>(Rules->AmbushZoneSize), 1000.0) : 6000.0;
	return FIntPoint(FMath::FloorToInt32(Location.X / Size), FMath::FloorToInt32(Location.Y / Size));
}

void ATikiDirector::BuildAmbushZones()
{
	AmbushZones.Reset();
	OutAmbushSlots.Reset();
	if (!Rules)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();
	TMap<FIntPoint, TArray<const FVector*>> ByZone;
	for (const TPair<FIntPoint, TArray<FVector>>& Cell : BushCells)
	{
		for (const FVector& Bush : Cell.Value)
		{
			ByZone.FindOrAdd(AmbushCellOf(Bush)).Add(&Bush);
		}
	}
	const double Size = FMath::Max(static_cast<double>(Rules->AmbushZoneSize), 1000.0);
	int32 Slots = 0;
	for (const TPair<FIntPoint, TArray<const FVector*>>& Zone : ByZone)
	{
		// slots for the busiest time of day; the cap and the pace below follow the time of day
		const int32 MaxPerZone = TikiMath::ScaleSpawnCount(Rules->AmbushMaxPerZone, FMath::Max(SpawnScaleOf(Rules, false, this), SpawnScaleOf(Rules, true, this)));
		const int32 Count = TikiMath::AmbushSlots(Zone.Value.Num(), Rules->AmbushMinBushes, Rules->AmbushBushesPerMonster, MaxPerZone);
		const FVector Center((Zone.Key.X + 0.5) * Size, (Zone.Key.Y + 0.5) * Size, 0.0);
		for (int32 Slot = 0; Slot < Count; ++Slot)
		{
			// spread over the zone: the bush nearest to evenly turned points around its centre
			const FVector Aim = Count == 1 ? Center : TikiMath::AlarmRingPoint(Center, static_cast<float>(Size) * 0.3f, Slot, Count, 0.0f);
			const FVector* Best = nullptr;
			double BestDistSq = TNumericLimits<double>::Max();
			for (const FVector* Bush : Zone.Value)
			{
				const double DistSq = FVector::DistSquared2D(*Bush, Aim);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Best = Bush;
				}
			}
			// the villages keep their own guardians: no monster lies in wait inside a pacification zone
			bool bInVillage = false;
			for (const FTikiVillage& Village : Villages)
			{
				bInVillage = bInVillage || (Best && !Village.VillagerSpawns.IsEmpty() && FVector::Dist2D(*Best, Village.Center) < Village.Radius);
			}
			if (Best && !bInVillage)
			{
				AmbushZones.FindOrAdd(Zone.Key).AddDefaulted_GetRef().Bush = *Best;
				++Slots;
			}
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d bush zones hide %d monsters (%.0f ms)"),
		*GetName(), AmbushZones.Num(), Slots, (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

void ATikiDirector::TickAmbush(double Now)
{
	if (!Rules || AmbushZones.IsEmpty())
	{
		return;
	}

	// monsters back in their bush or dead: the bush hides another one later
	for (int32 Index = OutAmbushSlots.Num() - 1; Index >= 0; --Index)
	{
		TArray<FTikiAmbushSlot>* Zone = AmbushZones.Find(OutAmbushSlots[Index].Key);
		FTikiAmbushSlot* Slot = Zone && Zone->IsValidIndex(OutAmbushSlots[Index].Value) ? &(*Zone)[OutAmbushSlots[Index].Value] : nullptr;
		const ATikiWarrior* Monster = Slot ? Slot->Monster.Get() : nullptr;
		if (Monster && !Monster->IsDead())
		{
			continue;
		}
		if (Slot)
		{
			Slot->ArmedAt = Now + (Monster ? Rules->AmbushKilledDelay : Rules->AmbushRearmDelay);
			Slot->Monster.Reset();
		}
		OutAmbushSlots.RemoveAtSwap(Index);
	}

	const int32 Reach = FMath::CeilToInt32(Rules->AmbushTriggerRadius / FMath::Max(Rules->AmbushZoneSize, 1000.0f));
	TArray<FSphere> Quiet;
	bool bQuietRead = false;
	// the stealth of the players (T-262), and the body of a monster hidden in its bush (the warrior's capsule)
	const UStealthSubsystem* Stealth = UStealthSubsystem::Get(this);
	const UCapsuleComponent* HiddenBody = GetDefault<ATikiWarrior>()->GetCapsuleComponent();
	const float HiddenRadius = HiddenBody ? HiddenBody->GetUnscaledCapsuleRadius() : 0.0f;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
		const UIslanderHealthComponent* PawnHealth = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
		if (!Pawn || (PawnHealth && PawnHealth->IsDead()))
		{
			continue;
		}
		// one monster at a time around a player, never for a player over the water (boat, swimming) or speeding past
		const FVector Here = Pawn->GetActorLocation();
		const double* Last = LastAmbushTime.Find(Pawn);
		FVector Ground;
		// four times fewer by day: the pace stretches with the scale
		const float Scale = SpawnScaleOf(Rules, IsNight(), this);
		const double Interval = Rules->AmbushPlayerInterval / FMath::Max(Scale, 0.05f);
		if ((Last && Now - *Last < Interval) || Pawn->GetVelocity().Size2D() > Rules->AmbushMaxPlayerSpeed || !ProjectToGround(Here, Ground))
		{
			continue;
		}
		// those hunting the player count wherever they are, the idle ones only near it
		int32 Out = 0;
		for (const TPair<FIntPoint, int32>& Taken : OutAmbushSlots)
		{
			const ATikiWarrior* Monster = AmbushZones[Taken.Key][Taken.Value].Monster.Get();
			Out += Monster && !Monster->IsDead() && (Monster->GetTarget() == Pawn
				|| FVector::Dist2D(Monster->GetActorLocation(), Here) < Rules->AmbushTriggerRadius * 2.0f) ? 1 : 0;
		}
		const int32 Cap = FMath::Min(Out + 1, TikiMath::ScaleSpawnCount(Rules->AmbushMaxAlivePerPlayer, Scale));
		const FIntPoint Cell = AmbushCellOf(Here);
		for (int32 X = Cell.X - Reach; X <= Cell.X + Reach && Out < Cap; ++X)
		{
			for (int32 Y = Cell.Y - Reach; Y <= Cell.Y + Reach && Out < Cap; ++Y)
			{
				TArray<FTikiAmbushSlot>* Zone = AmbushZones.Find(FIntPoint(X, Y));
				for (int32 Index = 0; Zone && Index < Zone->Num() && Out < Cap; ++Index)
				{
					FTikiAmbushSlot& Slot = (*Zone)[Index];
					const double Distance = FVector::Dist2D(Slot.Bush, Here);
					if (Slot.Monster.IsValid() || Now < Slot.ArmedAt || Distance > Rules->AmbushTriggerRadius || Distance < Rules->AmbushMinDistance)
					{
						continue;
					}
					// a quiet zone that appeared after the bushes were read (a camp): its bushes hide nobody, for good
					if (!bQuietRead)
					{
						Quiet = FindQuietZones(GetWorld());
						bQuietRead = true;
					}
					if (IsInAnyZone(Slot.Bush, Quiet))
					{
						Slot.ArmedAt = TNumericLimits<double>::Max();
						continue;
					}
					// the hidden monster must see the player (a crouched one, or any at night, only at contact; a lit torch
					// at night from afar, T-275) or hear it; heard only, it leaps out toward the noise and looks there (T-262)
					FStealthNoise Heard;
					bool bHeardOnly = false;
					if (Stealth && !Stealth->CanSeeFrom(Slot.Bush, HiddenRadius, Pawn, Rules->AmbushTriggerRadius)
						&& !IsTorchSeenFromBush(Rules, Stealth, Pawn, Distance))
					{
						bHeardOnly = Stealth->FindHeardNoiseAt(Slot.Bush, nullptr, Stealth->GetRules().WarriorHearingScale, Heard) && Heard.Instigator.Get() == Pawn;
						if (!bHeardOnly)
						{
							continue;
						}
					}
					ATikiWarrior* Monster = SpawnWarriorFromBush(Slot.Bush, 300.0f, bHeardOnly ? nullptr : Pawn, ETikiWarriorRole::Ambusher);
					if (!Monster)
					{
						// water, slope or building around the bush: try again later
						Slot.ArmedAt = Now + 30.0;
						continue;
					}
					if (bHeardOnly)
					{
						Monster->HearNoise(Heard);
					}
					Monster->SetPost(Monster->GetActorLocation(), Rules->AmbushPostRadius);
					Slot.Monster = Monster;
					OutAmbushSlots.Emplace(FIntPoint(X, Y), Index);
					LastAmbushTime.Add(Pawn, Now);
					++Out;
					UE_LOG(Logmonke_island, Log, TEXT("%s: %s leaps out of its bush at %.0f m from %s (zone %d,%d)"),
						*GetName(), *Monster->GetName(), Distance / 100.0, *Pawn->GetName(), X, Y);
				}
			}
		}
	}
}

int32 ATikiDirector::WakeAmbushers(const FVector& Where, APawn* Suspect, float Radius, int32 MaxCount)
{
	if (!Rules || MaxCount <= 0 || Radius <= 0.0f || AmbushZones.IsEmpty())
	{
		return 0;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// the cap around the player, as for an ambush: those hunting it wherever they are, the idle ones near the place
	int32 Out = 0;
	for (const TPair<FIntPoint, int32>& Taken : OutAmbushSlots)
	{
		const TArray<FTikiAmbushSlot>* TakenZone = AmbushZones.Find(Taken.Key);
		const ATikiWarrior* Monster = TakenZone && TakenZone->IsValidIndex(Taken.Value) ? (*TakenZone)[Taken.Value].Monster.Get() : nullptr;
		Out += Monster && !Monster->IsDead() && ((Suspect && Monster->GetTarget() == Suspect)
			|| FVector::Dist2D(Monster->GetActorLocation(), Where) < Rules->AmbushTriggerRadius * 2.0f) ? 1 : 0;
	}
	const int32 Cap = TikiMath::ScaleSpawnCount(Rules->AmbushMaxAlivePerPlayer, SpawnScaleOf(Rules, IsNight(), this));
	// the bushes hiding a monster within the radius, nearest first
	struct FWakeCandidate
	{
		FIntPoint Zone;
		int32 Index = 0;
		double DistSq = 0.0;
	};
	TArray<FWakeCandidate> Candidates;
	const FIntPoint Cell = AmbushCellOf(Where);
	const int32 Reach = FMath::CeilToInt32(Radius / FMath::Max(Rules->AmbushZoneSize, 1000.0f));
	const double RadiusSq = FMath::Square(static_cast<double>(Radius));
	for (int32 X = Cell.X - Reach; X <= Cell.X + Reach; ++X)
	{
		for (int32 Y = Cell.Y - Reach; Y <= Cell.Y + Reach; ++Y)
		{
			const TArray<FTikiAmbushSlot>* Zone = AmbushZones.Find(FIntPoint(X, Y));
			for (int32 Index = 0; Zone && Index < Zone->Num(); ++Index)
			{
				const FTikiAmbushSlot& Slot = (*Zone)[Index];
				const double DistSq = FVector::DistSquared2D(Slot.Bush, Where);
				if (!Slot.Monster.IsValid() && Now >= Slot.ArmedAt && DistSq <= RadiusSq)
				{
					Candidates.Add({ FIntPoint(X, Y), Index, DistSq });
				}
			}
		}
	}
	Candidates.Sort([](const FWakeCandidate& A, const FWakeCandidate& B) { return A.DistSq < B.DistSq; });
	const TArray<FSphere> Quiet = Candidates.IsEmpty() ? TArray<FSphere>() : FindQuietZones(GetWorld());
	int32 Woken = 0;
	for (const FWakeCandidate& Candidate : Candidates)
	{
		if (Woken >= MaxCount || Out >= Cap)
		{
			break;
		}
		TArray<FTikiAmbushSlot>* Zone = AmbushZones.Find(Candidate.Zone);
		if (!Zone || !Zone->IsValidIndex(Candidate.Index))
		{
			continue;
		}
		FTikiAmbushSlot& Slot = (*Zone)[Candidate.Index];
		// a quiet zone that appeared after the bushes were read (a camp): its bushes hide nobody, for good
		if (IsInAnyZone(Slot.Bush, Quiet))
		{
			Slot.ArmedAt = TNumericLimits<double>::Max();
			continue;
		}
		ATikiWarrior* Monster = SpawnWarriorFromBush(Slot.Bush, 300.0f, nullptr, ETikiWarriorRole::Ambusher);
		if (!Monster)
		{
			Slot.ArmedAt = Now + 30.0;
			continue;
		}
		Monster->SetPost(Monster->GetActorLocation(), Rules->AmbushPostRadius);
		// it runs to look where the watcher saw the foe: it attacks only if it then sees it (T-262)
		Monster->HearHowl(Where, Suspect);
		Slot.Monster = Monster;
		OutAmbushSlots.Emplace(Candidate.Zone, Candidate.Index);
		if (Suspect)
		{
			LastAmbushTime.Add(Suspect, Now);
		}
		++Woken;
		++Out;
		UE_LOG(Logmonke_island, Log, TEXT("%s: a howl wakes %s in its bush %.0f m away"), *GetName(), *Monster->GetName(), FMath::Sqrt(Candidate.DistSq) / 100.0);
	}
	return Woken;
}

#if !UE_BUILD_SHIPPING
void ATikiDirector::RunDebugActions()
{
	if (DebugKillVillagers + DebugCutKillVillagers + DebugCutArms + DebugCutCorpses + DebugKillWarriors <= 0)
	{
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	if (DebugKillWarriors > 0)
	{
		--DebugKillWarriors;
		ATikiWarrior* Target = nullptr;
		double TargetDistSq = TNumericLimits<double>::Max();
		for (TActorIterator<ATikiWarrior> It(GetWorld()); It; ++It)
		{
			const double DistSq = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
			if (!It->IsDead() && DistSq < TargetDistSq)
			{
				TargetDistSq = DistSq;
				Target = *It;
			}
		}
		if (Target)
		{
			UE_LOG(Logmonke_island, Log, TEXT("Debug: %s kills %s"), *Pawn->GetName(), *Target->GetName());
			UGameplayStatics::ApplyDamage(Target, 100000.0f, PC, Pawn, UDamageType::StaticClass());
		}
		return;
	}
	ATikiVillager* Nearest = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const FTikiVillage& Village : Villages)
	{
		for (const TWeakObjectPtr<ATikiVillager>& Villager : Village.Villagers)
		{
			if (Villager.IsValid() && !Villager->IsDead() && FVector::DistSquared(Villager->GetActorLocation(), Pawn->GetActorLocation()) < BestDistSq)
			{
				BestDistSq = FVector::DistSquared(Villager->GetActorLocation(), Pawn->GetActorLocation());
				Nearest = Villager.Get();
			}
		}
	}
	if (DebugKillVillagers > 0 || DebugCutKillVillagers > 0)
	{
		const bool bCut = DebugCutKillVillagers > 0;
		--(bCut ? DebugCutKillVillagers : DebugKillVillagers);
		if (Nearest)
		{
			UE_LOG(Logmonke_island, Log, TEXT("Debug: %s kills %s%s"), *Pawn->GetName(), *Nearest->GetName(), bCut ? TEXT(" with a cut") : TEXT(""));
			if (bCut)
			{
				Nearest->MarkNextBlowCut();
			}
			UGameplayStatics::ApplyDamage(Nearest, 100000.0f, PC, Pawn, UDamageType::StaticClass());
		}
		return;
	}
	if (DebugCutArms > 0)
	{
		--DebugCutArms;
		for (const FName& Bone : Rules ? Rules->LimbBones : TArray<FName>())
		{
			if (Nearest && Nearest->CutOffLimb(Bone, (Nearest->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D()))
			{
				break;
			}
		}
		return;
	}
	--DebugCutCorpses;
	AActor* Corpse = nullptr;
	BestDistSq = TNumericLimits<double>::Max();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const ATikiCorpse* Rigid = Cast<ATikiCorpse>(*It);
		const bool bWhole = It->IsA<ATikiRagdollCorpse>() || (Rigid && Rigid->GetPart() == ETikiBodyPart::Whole);
		if (bWhole && FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < BestDistSq)
		{
			BestDistSq = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
			Corpse = *It;
		}
	}
	if (Corpse)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Debug: %s cuts %s"), *Pawn->GetName(), *Corpse->GetName());
		const FVector Direction = (Corpse->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal2D();
		if (ATikiRagdollCorpse* Ragdoll = Cast<ATikiRagdollCorpse>(Corpse))
		{
			Ragdoll->CutInTwo(Direction);
		}
		else
		{
			CastChecked<ATikiCorpse>(Corpse)->CutInTwo(Direction);
		}
	}
}
#else
void ATikiDirector::RunDebugActions()
{
}
#endif

bool ATikiDirector::FindNearestBush(const FVector& Location, float Radius, FVector& OutBush)
{
	CacheBushes();
	const FIntPoint Center = BushCellOf(Location);
	const int32 Reach = FMath::CeilToInt32(Radius / BushCellSize);
	TArray<FVector> Candidates;
	for (int32 X = Center.X - Reach; X <= Center.X + Reach; ++X)
	{
		for (int32 Y = Center.Y - Reach; Y <= Center.Y + Reach; ++Y)
		{
			if (const TArray<FVector>* Cell = BushCells.Find(FIntPoint(X, Y)))
			{
				Candidates.Append(*Cell);
			}
		}
	}
	const int32 Best = TikiMath::NearestIndex(Candidates, Location, Radius);
	if (Best == INDEX_NONE)
	{
		return false;
	}
	OutBush = Candidates[Best];
	return true;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GTikiDefenderCommand(
	TEXT("monke.TikiDefender"),
	TEXT("A tiki warrior leaps out of the bush nearest to the local player and attacks it (server / host)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ATikiDirector* Director = ATikiDirector::Get(World);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Director || !Pawn || World->GetNetMode() == NM_Client)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("monke.TikiDefender: needs a TikiDirector in the level and a local player on the server"));
			return;
		}
		Director->SpawnWarriorFromBush(Pawn->GetActorLocation(), 3000.0f, Pawn, ETikiWarriorRole::Defender);
	}));

/** Runs a debug request of the director of the world at its next think (server / host) */
static void TikiDebugRequest(UWorld* World, int32 ATikiDirector::* Counter, const TCHAR* Command)
{
	ATikiDirector* Director = ATikiDirector::Get(World);
	if (!Director || World->GetNetMode() == NM_Client)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: needs a TikiDirector in the level, on the server"), Command);
		return;
	}
	++(Director->*Counter);
}

static FAutoConsoleCommandWithWorldAndArgs GTikiKillVillagerCommand(
	TEXT("monke.TikiKillVillager"),
	TEXT("Kills the villager nearest to the local player, as if the player did (\"cut\": with a cut blow, the body falls in two)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const bool bCut = Args.Num() > 0 && Args[0].Equals(TEXT("cut"), ESearchCase::IgnoreCase);
		TikiDebugRequest(World, bCut ? &ATikiDirector::DebugCutKillVillagers : &ATikiDirector::DebugKillVillagers, TEXT("monke.TikiKillVillager"));
	}));

static FAutoConsoleCommandWithWorld GTikiCutArmCommand(
	TEXT("monke.TikiCutArm"),
	TEXT("Cuts an arm off the villager nearest to the local player."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		TikiDebugRequest(World, &ATikiDirector::DebugCutArms, TEXT("monke.TikiCutArm"));
	}));

static FAutoConsoleCommandWithWorld GTikiKillWarriorCommand(
	TEXT("monke.TikiKillWarrior"),
	TEXT("Kills the warrior nearest to the local player, as if the player did (it drops its weapon)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		TikiDebugRequest(World, &ATikiDirector::DebugKillWarriors, TEXT("monke.TikiKillWarrior"));
	}));

static FAutoConsoleCommandWithWorld GTikiCutCorpseCommand(
	TEXT("monke.TikiCutCorpse"),
	TEXT("Cuts in two the whole corpse nearest to the local player."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		TikiDebugRequest(World, &ATikiDirector::DebugCutCorpses, TEXT("monke.TikiCutCorpse"));
	}));
#endif
