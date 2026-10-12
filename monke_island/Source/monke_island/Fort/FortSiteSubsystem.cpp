// monke_island

#include "Fort/FortSiteSubsystem.h"
#include "Fort/FortSite.h"
#include "Fort/FortSiteMath.h"
#include "Fort/FortSiteRules.h"
#include "Camps/SiteClearing.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Expedition/Islands/ExpeditionIslandValidation.h"
#include "Run/RunCampaign.h"
#include "Run/RunDifficulty.h"
#include "Run/RunSubsystem.h"
#include "monke_island.h"
#include "UObject/Package.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

namespace
{
	/** A PIE opened on an expedition level waits this long for its players before it builds the forts anyway (s) */
	constexpr double ResolveTimeout = 15.0;
}

bool UFortSiteSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld();
}

void UFortSiteSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}
	// a game started from L_Boot names its fragment in the travel URL: the forts stand before anything looks at the land
	const FString OptionMatch = FString::Printf(TEXT("%s="), ExpeditionIslands::FragmentOptionKey);
	const FName FromUrl = ExpeditionIslands::ParseFragmentOption(InWorld.URL.GetOption(*OptionMatch, nullptr));
	const UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(&InWorld);
	const bool bExpeditionMap = !FromUrl.IsNone() || (Islands && !Islands->ResolveFragmentIdForWorld(&InWorld).IsNone());
	Rules = Cast<UFortSiteRules>(RulesPath.TryLoad());
	if (!Rules)
	{
		if (bExpeditionMap)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Forts: no DA_FortSiteRules (%s), no fort in this expedition"), *RulesPath.ToString());
		}
		return;
	}
	if (!FromUrl.IsNone())
	{
		BuildForts(FromUrl);
		return;
	}
	if (!bExpeditionMap)
	{
		// a test map with sites of its own (L_Test_Flat and its section of Raraka)
		const FString Package = UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName());
		for (const FFortFragmentSites& Entry : Rules->Fragments)
		{
			if (!Entry.TestMap.IsNull() && Entry.TestMap.ToSoftObjectPath().GetLongPackageName() == Package)
			{
				BuildForts(Entry.FragmentId);
				return;
			}
		}
		Rules = nullptr;
		return;
	}
	// a PIE opened on the level: the fragment is where the players are, once the boat start has placed them
	ResolveStart = InWorld.GetTimeSeconds();
	InWorld.GetTimerManager().SetTimer(ResolveTimer, this, &UFortSiteSubsystem::ResolveFromPlayers, 0.5f, true, 1.5f);
}

void UFortSiteSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResolveTimer);
	}
	Forts.Reset();
	Super::Deinitialize();
}

TArray<AFortSite*> UFortSiteSubsystem::GetForts() const
{
	TArray<AFortSite*> Result;
	for (const TWeakObjectPtr<AFortSite>& Fort : Forts)
	{
		if (AFortSite* Alive = Fort.Get())
		{
			Result.Add(Alive);
		}
	}
	return Result;
}

void UFortSiteSubsystem::ResolveFromPlayers()
{
	UWorld* World = GetWorld();
	if (!World || !Rules)
	{
		return;
	}
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Players.Add(Pawn->GetActorLocation());
		}
	}
	if (Players.IsEmpty() && World->GetTimeSeconds() - ResolveStart < ResolveTimeout)
	{
		return;
	}
	World->GetTimerManager().ClearTimer(ResolveTimer);
	FName Nearest = NAME_None;
	double BestDistSq = TNumericLimits<double>::Max();
	for (const FFortFragmentSites& Entry : Rules->Fragments)
	{
		for (const FFortSiteSpot& Spot : Entry.Sites)
		{
			for (const FVector& Player : Players)
			{
				const double DistSq = FVector::DistSquared2D(Spot.Location, Player);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Nearest = Entry.FragmentId;
				}
			}
		}
	}
	if (Nearest.IsNone())
	{
		const UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(World);
		Nearest = Islands ? Islands->ResolveFragmentIdForWorld(World) : NAME_None;
	}
	BuildForts(Nearest);
}

void UFortSiteSubsystem::BuildForts(FName FragmentId)
{
	UWorld* World = GetWorld();
	const FFortFragmentSites* Fragment = Rules ? Rules->FindFragment(FragmentId) : nullptr;
	if (!World || !Fragment || Fragment->Sites.IsEmpty())
	{
		UE_LOG(Logmonke_island, Log, TEXT("Forts: no fort site in %s"), *FragmentId.ToString());
		return;
	}
	// the day of the campaign decides (no run: always, as on the test maps)
	const URunSubsystem* Run = URunSubsystem::Get(World);
	const int32 Day = Run ? Run->GetCampaignDay() : 0;
	if (Day > 0 && !Run->IsUnlocked(ERunUnlock::Forts))
	{
		UE_LOG(Logmonke_island, Log, TEXT("Forts: none in %s on day %d (from day %d)"), *FragmentId.ToString(), Day,
			RunCampaign::UnlockDay(Run->GetCampaignUnlocks(), ERunUnlock::Forts));
		return;
	}
	const int32 Draw = Rules->Seed != 0 ? Rules->Seed : FMath::Rand();
	FRandomStream Stream(Draw);
	const FString LevelShortName = FPackageName::GetShortName(Rules->FortLevel.ToSoftObjectPath().GetLongPackageName());
	const AActor* Probe = World->GetWorldSettings();
	// the forts of the day (the difficulty curve, owner 2026-10-09: two from day 6, three from day 9), as many as the
	// fragment has sites; the rules' count without a run or when the day sets none
	const int32 FortsWanted = RunDifficulty::CountOr(Run ? Run->GetDayDifficulty().FortsPerExpedition : 0, Rules->FortsPerExpedition);
	if (FortsWanted > Fragment->Sites.Num())
	{
		UE_LOG(Logmonke_island, Log, TEXT("Forts: day %d asks %d forts, %s has %d site(s)"), Day, FortsWanted, *FragmentId.ToString(), Fragment->Sites.Num());
	}
	for (const int32 SiteIndex : FortSiteMath::PickSites(Fragment->Sites.Num(), FortsWanted, Stream))
	{
		const FFortSiteSpot& Spot = Fragment->Sites[SiteIndex];
		// the ground under the fort's plateau (the vegetation skipped), its highest points set under the vault and the moat
		TArray<double> Heights;
		for (const FVector& Point : FortSiteMath::GroundProbes(Spot.Location, Rules->GroundRingsCm))
		{
			const FVector Ground = FSiteClearing::GroundAt(Probe, Point, {});
			if (!Ground.Equals(Point))
			{
				Heights.Add(Ground.Z);
			}
		}
		const double GroundZ = FortSiteMath::HeightAtQuantile(Heights, Rules->GroundQuantile, Spot.Location.Z);
		const FTransform Transform(FRotator(0.0f, Spot.Yaw, 0.0f), FVector(Spot.Location.X, Spot.Location.Y, GroundZ));
		AFortSite* Site = World->SpawnActorDeferred<AFortSite>(AFortSite::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Site)
		{
			continue;
		}
		Site->Rules = Rules;
		Site->LevelTransform = FortSiteMath::LevelTransform(Spot.Location, Spot.Yaw, GroundZ, Rules->TopGroundLocalCm);
		Site->LevelName = FortSiteMath::LevelName(LevelShortName, FragmentId, SiteIndex, Draw);
		Site->FinishSpawning(Transform);
		Forts.Add(Site);
		UE_LOG(Logmonke_island, Log, TEXT("Forts: %s site %d of %d at %s, yaw %.0f, ground %.0f cm (%d of %d probes hit, day %d)"),
			*FragmentId.ToString(), SiteIndex + 1, Fragment->Sites.Num(), *Spot.Location.ToCompactString(), Spot.Yaw, GroundZ,
			Heights.Num(), 1 + 8 * Rules->GroundRingsCm.Num(), Day);
	}
}
