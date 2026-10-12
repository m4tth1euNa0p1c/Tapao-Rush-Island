// monke_island

#include "Fort/FortGarrisonZone.h"
#include "Fort/FortGarrisonMath.h"
#include "Fort/FortSoldier.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunDifficulty.h"
#include "Run/RunSubsystem.h"
#include "monke_island.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

AFortGarrisonZone::AFortGarrisonZone()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

bool AFortGarrisonZone::IsInside(const FVector& Location) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(Location);
	return Local.Z >= MinZ && Local.Z <= MaxZ && FortGarrisonMath::IsInside(Outline, FVector2D(Local.X, Local.Y));
}

FVector AFortGarrisonZone::ClampInside(const FVector& Location) const
{
	const FTransform& Frame = GetActorTransform();
	const FVector Local = Frame.InverseTransformPosition(Location);
	const FVector2D Flat = FortGarrisonMath::IsInside(Outline, FVector2D(Local.X, Local.Y))
		? FVector2D(Local.X, Local.Y)
		: FortGarrisonMath::ClampInside(Outline, FVector2D(Local.X, Local.Y), InnerMargin);
	return Frame.TransformPosition(FVector(Flat.X, Flat.Y, Local.Z));
}

FVector AFortGarrisonZone::GetPatrolPoint(int32 Index) const
{
	return PatrolPoints.IsValidIndex(Index) ? GetActorTransform().TransformPosition(PatrolPoints[Index]) : GetActorLocation();
}

bool AFortGarrisonZone::IsPatrolPostTaken(int32 Index, const AFortSoldier* Asker) const
{
	// the garrison's own registry (no look through every actor of the world: T-332), a soldier still on its way in
	// (seen at the next check) not counted
	for (const TPair<TWeakObjectPtr<AFortSoldier>, FVector>& Entry : Soldiers)
	{
		const AFortSoldier* Other = Entry.Key.Get();
		if (Other && Other != Asker && !Other->IsDead() && Other->GetPost() == Index)
		{
			return true;
		}
	}
	return false;
}

FVector AFortGarrisonZone::GetBuildingPostLocation(int32 Index) const
{
	return BuildingPosts.IsValidIndex(Index) ? GetActorTransform().TransformPosition(BuildingPosts[Index].Location) : GetActorLocation();
}

float AFortGarrisonZone::GetBuildingPostYaw(int32 Index) const
{
	return static_cast<float>(FRotator::NormalizeAxis(GetActorRotation().Yaw + (BuildingPosts.IsValidIndex(Index) ? BuildingPosts[Index].Yaw : 0.0f)));
}

AFortGarrisonZone* AFortGarrisonZone::FindAt(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AFortGarrisonZone> It(World); It; ++It)
	{
		if (It->IsInside(Location))
		{
			return *It;
		}
	}
	return nullptr;
}

void AFortGarrisonZone::RaiseAlarm(AFortSoldier* Caller, AActor* Target, const FVector& From, float Radius)
{
	if (!HasAuthority() || !Target)
	{
		return;
	}
	int32 Warned = 0;
	for (const TPair<TWeakObjectPtr<AFortSoldier>, FVector>& Entry : Soldiers)
	{
		AFortSoldier* Soldier = Entry.Key.Get();
		if (Soldier && Soldier != Caller && !Soldier->IsDead() && FVector::Dist(Soldier->GetActorLocation(), From) <= Radius
			&& Soldier->HearAlarm(Target, From))
		{
			++Warned;
		}
	}
	if (Warned > 0)
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: alarm, %d soldier(s) hunt %s"), *GetName(), Warned, *Target->GetName());
	}
}

void AFortGarrisonZone::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	if (Outline.Num() < 3)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no outline, the garrison is not held"), *GetName());
		return;
	}
	// the difficulty of the campaign day (nothing on a test map, day 0)
	const FRunDayDifficulty Difficulty = URunSubsystem::DifficultyOf(this);
	HealthScale = Difficulty.MobHealthScale;
	HitChanceScale = Difficulty.SoldierHitChanceScale;
	WantedSoldiers = Difficulty.SoldiersPerFort;
	CheckSoldiers();
	GetWorldTimerManager().SetTimer(CheckTimer, this, &AFortGarrisonZone::CheckSoldiers, CheckInterval, true);
	GetWorldTimerManager().SetTimer(SettleTimer, this, &AFortGarrisonZone::AdjustGarrison, FMath::Max(GarrisonSettleSeconds, CheckInterval), false);
	UE_LOG(Logmonke_island, Log, TEXT("%s: fort of %d corners, %d posts, %d soldier(s) in garrison so far (day: %d wanted, health x%.2f, hit chance x%.2f)"),
		*GetName(), Outline.Num(), PatrolPoints.Num(), Soldiers.Num(), WantedSoldiers, HealthScale, HitChanceScale);
}

void AFortGarrisonZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
	GetWorldTimerManager().ClearTimer(SettleTimer);
	Soldiers.Reset();
	Strangers.Reset();
	Super::EndPlay(EndPlayReason);
}

void AFortGarrisonZone::AdjustGarrison()
{
	// whoever began since the last check joins first
	CheckSoldiers();
	TArray<AFortSoldier*> Alive;
	for (const TPair<TWeakObjectPtr<AFortSoldier>, FVector>& Entry : Soldiers)
	{
		AFortSoldier* Soldier = Entry.Key.Get();
		if (Soldier && !Soldier->IsDead() && !Soldier->IsFiringSquad())
		{
			Alive.Add(Soldier);
		}
	}
	const int32 Change = FortGarrisonMath::GarrisonChange(Alive.Num(), WantedSoldiers);
	int32 Garrison = Alive.Num();
	if (Change < 0)
	{
		// the surplus leaves: the last ones seen (the first placed in the level stay)
		for (int32 Index = 0; Index < -Change; ++Index)
		{
			AFortSoldier* Gone = Alive[Alive.Num() - 1 - Index];
			Soldiers.Remove(Gone);
			Gone->Destroy();
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s: garrison of %d, the day wants %d: %d soldier(s) sent away"), *GetName(), Alive.Num(), WantedSoldiers, -Change);
		Alive.SetNum(Alive.Num() + Change);
		Garrison = Alive.Num();
	}
	else
	{
		int32 Spawned = 0;
		for (int32 Index = 0; Index < Change; ++Index)
		{
			// the uniforms of those placed, in turn
			const AFortSoldier* Template = Alive[Index % Alive.Num()];
			if (SpawnSoldier(Template, FortGarrisonMath::SpawnPost(Index, Alive.Num(), PatrolPoints.Num())))
			{
				++Spawned;
			}
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s: garrison of %d, the day wants %d: %d soldier(s) added at the posts (health x%.2f, hit chance x%.2f)"),
			*GetName(), Alive.Num(), WantedSoldiers > 0 ? WantedSoldiers : Alive.Num(), Spawned, HealthScale, HitChanceScale);
		Garrison += Spawned;
	}
	// then the guards of the buildings (T-332, owner 2026-10-11: "je veux plus de soldats dans les bâtiments")
	PostBuildingGuards(Alive, Garrison);
}

int32 AFortGarrisonZone::PostBuildingGuards(const TArray<AFortSoldier*>& Templates, int32 Garrison)
{
	const UFortSoldierRules* SoldierRules = nullptr;
	for (const AFortSoldier* Template : Templates)
	{
		if (Template && Template->Rules)
		{
			SoldierRules = Template->Rules;
			break;
		}
	}
	if (!SoldierRules)
	{
		return 0;
	}
	// the day of the campaign (0 without one: a test map)
	const int32 Day = URunSubsystem::CampaignDayOf(this);
	const int32 Wanted = FortGarrisonMath::BuildingGuardsOfDay(SoldierRules->BuildingGuardsByDay, Day, SoldierRules->BuildingGuardsWithoutRun);
	if (Wanted > 0 && BuildingPosts.IsEmpty())
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: the day wants %d guard(s) in the buildings, the fort has no building post (scripts/editor/fort_building_posts_ue.py)"),
			*GetName(), Wanted);
		return 0;
	}
	const int32 Count = FortGarrisonMath::BuildingGuardsToPost(Wanted, Garrison, SoldierRules->MaxSoldiersPerFort, BuildingPosts.Num());
	TArray<FName> Buildings;
	Buildings.Reserve(BuildingPosts.Num());
	for (const FFortBuildingPost& Post : BuildingPosts)
	{
		Buildings.Add(Post.Building);
	}
	// spread over the buildings, from a building drawn for each fort
	const TArray<int32> Posts = FortGarrisonMath::SpreadOverBuildings(Buildings, Count, FMath::RandRange(0, FMath::Max(BuildingPosts.Num() - 1, 0)));
	int32 Posted = 0;
	TMap<FName, int32> PerBuilding;
	for (int32 Index = 0; Index < Posts.Num(); ++Index)
	{
		// the uniforms of those placed, in turn after the clones of the court
		if (SpawnSoldier(Templates[(Garrison + Index) % Templates.Num()], INDEX_NONE, Posts[Index]))
		{
			++Posted;
			++PerBuilding.FindOrAdd(BuildingPosts[Posts[Index]].Building);
		}
	}
	FString Where;
	for (const TPair<FName, int32>& Entry : PerBuilding)
	{
		Where += FString::Printf(TEXT("%s%s %d"), Where.IsEmpty() ? TEXT("") : TEXT(", "), *Entry.Key.ToString(), Entry.Value);
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: day %d wants %d guard(s) in the buildings: %d posted on %d post(s) (%s), %d soldier(s) in the fort (at most %d)"),
		*GetName(), Day, Wanted, Posted, BuildingPosts.Num(), Where.IsEmpty() ? TEXT("none") : *Where, Garrison + Posted, SoldierRules->MaxSoldiersPerFort);
	return Posted;
}

AFortSoldier* AFortGarrisonZone::SpawnSoldier(const AFortSoldier* Template, int32 PostIndex, int32 BuildingPostIndex)
{
	UWorld* World = GetWorld();
	if (!World || !Template)
	{
		return nullptr;
	}
	// at a post of the patrol (or by the template without any), or at a post of a building facing its door, inside
	// the walls, a little in the air
	const bool bGuard = BuildingPosts.IsValidIndex(BuildingPostIndex);
	const FVector Post = bGuard ? GetBuildingPostLocation(BuildingPostIndex) : (PostIndex != INDEX_NONE ? GetPatrolPoint(PostIndex) : Template->GetActorLocation());
	const FVector Where = ClampInside(Post) + FVector(0.0f, 0.0f, SpawnLiftCm);
	const float Yaw = bGuard ? GetBuildingPostYaw(BuildingPostIndex) : FMath::FRandRange(-180.0f, 180.0f);
	const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Where);
	AFortSoldier* Soldier = World->SpawnActorDeferred<AFortSoldier>(Template->GetClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Soldier)
	{
		return nullptr;
	}
	Soldier->Rules = Template->Rules;
	Soldier->Kind = Template->Kind;
	Soldier->Skin = Template->Skin;
	Soldier->HatMesh = Template->HatMesh;
	Soldier->HeadMesh = Template->HeadMesh;
	Soldier->FirstPost = bGuard ? INDEX_NONE : PostIndex;
	if (bGuard)
	{
		Soldier->AssignBuildingPost(BuildingPostIndex, Post, Yaw);
	}
	Soldier->JoinGarrison(this);
	Soldier->FinishSpawning(Transform);
	// it stands inside: the next check takes it into the garrison and gives it the health of the day
	UE_LOG(Logmonke_island, Log, TEXT("%s: %s added (%s of %s) at %s"), *GetName(), *Soldier->GetName(), *Template->GetClass()->GetName(),
		*Template->GetName(), bGuard ? *FString::Printf(TEXT("building post %d (%s)"), BuildingPostIndex, *BuildingPosts[BuildingPostIndex].Building.ToString())
		: *FString::Printf(TEXT("post %d"), PostIndex));
	return Soldier;
}

void AFortGarrisonZone::CheckSoldiers()
{
	for (TActorIterator<AFortSoldier> It(GetWorld()); It; ++It)
	{
		AFortSoldier* Soldier = *It;
		const TWeakObjectPtr<AFortSoldier> Key(Soldier);
		if (Strangers.Contains(Key))
		{
			continue;
		}
		FVector* LastInside = Soldiers.Find(Key);
		if (!LastInside)
		{
			// a soldier that has not begun yet still sets its health from its rules in its BeginPlay: it is looked at
			// again next time, once its health is there to scale
			if (!Soldier->HasActorBegunPlay())
			{
				continue;
			}
			// first sight: a soldier that starts in the fort belongs to it
			if (IsInside(Soldier->GetActorLocation()))
			{
				Soldiers.Add(Key, Soldier->GetActorLocation());
				Soldier->JoinGarrison(this);
				// the difficulty of the day (owner 2026-10-09): a sturdier garrison
				if (UIslanderHealthComponent* Health = Soldier->GetHealth(); Health && !Soldier->IsDead() && !FMath::IsNearlyEqual(HealthScale, 1.0f))
				{
					Health->ScaleMaxHealth(HealthScale);
				}
			}
			else
			{
				Strangers.Add(Key);
			}
			continue;
		}
		if (Soldier->IsDead())
		{
			continue;
		}
		const UCharacterMovementComponent* Movement = Soldier->GetCharacterMovement();
		const FVector Here = Soldier->GetActorLocation();
		if (IsInside(Here))
		{
			// a place to put it back on: only where it stands (not in the air over the ditch)
			if (!Movement || Movement->IsMovingOnGround())
			{
				*LastInside = Here;
			}
			continue;
		}
		Soldier->SetActorLocation(*LastInside, false, nullptr, ETeleportType::TeleportPhysics);
		if (UCharacterMovementComponent* Moving = Soldier->GetCharacterMovement())
		{
			Moving->StopMovementImmediately();
		}
		Soldier->OnPutBackInside();
		UE_LOG(Logmonke_island, Verbose, TEXT("%s: %s put back inside"), *GetName(), *Soldier->GetName());
	}
}
