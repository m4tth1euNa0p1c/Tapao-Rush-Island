// monke_island

#pragma once

#include "CoreMinimal.h"
#include "RunCampaign.generated.h"

/** Content of the islands that opens with the days of the campaign (docs/features/campagne-dix-jours.md) */
UENUM(BlueprintType)
enum class ERunUnlock : uint8
{
	/** The reliquary temples, one artifact on the pedestal (owner: "le 3e jour le premier temple", then from day 1:
	 *  "fais apparaître le temple artefact dès le jour 1", 2026-10-08) */
	Temples,
	/** The French forts and their garrison, built on the land of the fragment ("le 6e jour les forts français
	 *  apparaissent", then from day 1 on 2026-10-08, then from day 3: "ajoute au 3e jour les forts français sur
	 *  Raraka", 2026-10-09) */
	Forts,
	/** Every building of the fragments: fishermen camps... ("maintenant tous les bâtiments apparaissent") */
	AllBuildings,
	/** The rest of the artifact temples ("le reste des temples artefacts aussi", then from day 3 with the forts:
	 *  "ajoute au 3e jour ... les temples artefacts", 2026-10-09) */
	AllArtifacts,
	/** Giant crabs coming out of the sand ("des crabes géants peuvent sortir du sable"; from day 4, owner 2026-10-07,
	 *  then from day 1, 2026-10-08) */
	GiantCrabs,
	/** More tiki in the villages ("les surplus de mob apparaissent au 2e jour", owner 2026-10-07) */
	MobSurplus,
	/** Tiki of the villages coming out with a musket ("au bout de la 6e vague les mobs utilisent des fusils quand ils
	 *  pop, dans les villages", owner 2026-10-07; the 6th wave read as the 6th day) */
	ArmedMobs,
	/** The night falls early: the expedition starts late in the day ("à partir du 3e jour la nuit tombe plus tôt",
	 *  owner 2026-10-07) */
	EarlyNight,
	/** More tiki out of the bushes by day ("plus de mobs sortent des bush le jour, le 2e jour", owner 2026-10-07) */
	DayBushMobs
};

/**
 *  Days the content of the islands opens in the campaign of the first ten days (owner requests 2026-10-07, 2026-10-08
 *  and 2026-10-09): villages, marae, the first reliquary temple (one of the six artifacts on its pedestal) and the
 *  giant crabs from day 1, the French forts and every artifact temple from day 3, then from day 7 every building. Days
 *  counted from the start of the run (URunSubsystem: days played + 1), whatever the contract. Values in DA_RunRules.
 *  What each day changes in the danger itself (counts, health, damage...) is the difficulty curve, FRunDayDifficulty.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunCampaignUnlocks
{
	GENERATED_BODY()

	/** First day of the reliquary temples (owner 2026-10-08: "dès le jour 1") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 TemplesFromDay = 1;

	/** First day of the French forts (owner 2026-10-09: "ajoute au 3e jour les forts français sur Raraka") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 FortsFromDay = 3;

	/** First day of every building (fishermen camps; villages and marae are there from day 1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 AllBuildingsFromDay = 7;

	/** First day of the other artifact temples (owner 2026-10-09: with the forts, "et les temples artefacts") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 AllArtifactsFromDay = 3;

	/** First day of the giant crabs (owner 2026-10-07: "les crabes apparaissent au 4e jour", then 2026-10-08: "dès le
	 *  jour 1") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 GiantCrabsFromDay = 1;

	/** First day of the extra tiki, and how many more (multiplies the danger of the run: the hostiles of the villages,
	 *  the defenders, the ambushes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 MobSurplusFromDay = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	float MobSurplusScale = 1.5f;

	/** First day the guardians and defenders of the villages may come out with a musket (DA_TikiRules.Musket) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 ArmedMobsFromDay = 6;

	/** First day the night falls early, and the solar hours the expedition then starts at (drawn): the night (18 h 30)
	 *  falls 2.5 to 5 min after the landing instead of anywhere from 0.5 to 12 min (DA_DayNightRules draws 6 h 30-18 h) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 EarlyNightFromDay = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign")
	FFloatInterval EarlyNightStartHours = FFloatInterval(13.0f, 15.5f);

	/** First day more tiki come out of the bushes by day, and how many more: multiplies the day's spawn scale of
	 *  DA_TikiRules (DaySpawnScale: the ambushes, the defenders of the villages) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="1"))
	int32 DayBushMobsFromDay = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0"))
	float DayBushMobScale = 2.5f;

	/** Temples of a fragment, before and from AllArtifactsFromDay */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0"))
	int32 TemplesPerFragmentFirst = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0"))
	int32 TemplesPerFragmentAll = 3;

	/** Artifacts on the pedestal of each temple, drawn without repeat from the temple's pool (DA_TempleReliquaryRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Campaign", meta=(ClampMin="0"))
	int32 ArtifactsPerTemple = 1;
};

/** Pure rules of the campaign's unlocks (tested by monke_island.Run.Campaign) */
namespace RunCampaign
{
	/** First day of a content */
	inline int32 UnlockDay(const FRunCampaignUnlocks& Unlocks, ERunUnlock Content)
	{
		switch (Content)
		{
		case ERunUnlock::Temples:
			return Unlocks.TemplesFromDay;
		case ERunUnlock::Forts:
			return Unlocks.FortsFromDay;
		case ERunUnlock::AllBuildings:
			return Unlocks.AllBuildingsFromDay;
		case ERunUnlock::AllArtifacts:
			return Unlocks.AllArtifactsFromDay;
		case ERunUnlock::GiantCrabs:
			return Unlocks.GiantCrabsFromDay;
		case ERunUnlock::MobSurplus:
			return Unlocks.MobSurplusFromDay;
		case ERunUnlock::ArmedMobs:
			return Unlocks.ArmedMobsFromDay;
		case ERunUnlock::EarlyNight:
			return Unlocks.EarlyNightFromDay;
		case ERunUnlock::DayBushMobs:
			return Unlocks.DayBushMobsFromDay;
		default:
			return 1;
		}
	}

	/** True when the content is open on Day (from 1); Day 0 = no campaign (test maps, PIE): everything is open */
	inline bool IsUnlocked(const FRunCampaignUnlocks& Unlocks, ERunUnlock Content, int32 Day)
	{
		return Day <= 0 || Day >= UnlockDay(Unlocks, Content);
	}

	/** Multiplier of the tiki of Day from the campaign (MobSurplusScale from MobSurplusFromDay; 1 without a campaign) */
	inline float MobScale(const FRunCampaignUnlocks& Unlocks, int32 Day)
	{
		return Day > 0 && IsUnlocked(Unlocks, ERunUnlock::MobSurplus, Day) ? FMath::Max(Unlocks.MobSurplusScale, 0.0f) : 1.0f;
	}

	/** Multiplier of the tiki out of the bushes by day on Day (DayBushMobScale from DayBushMobsFromDay; 1 without a
	 *  campaign, Day 0) */
	inline float DayBushScale(const FRunCampaignUnlocks& Unlocks, int32 Day)
	{
		return Day > 0 && IsUnlocked(Unlocks, ERunUnlock::DayBushMobs, Day) ? FMath::Max(Unlocks.DayBushMobScale, 0.0f) : 1.0f;
	}

	/** True when the expedition of Day starts late (from EarlyNightFromDay; never without a campaign, Day 0), with the
	 *  hours to draw the start from */
	inline bool EarlyNightStartHours(const FRunCampaignUnlocks& Unlocks, int32 Day, FFloatInterval& OutHours)
	{
		if (Day <= 0 || !IsUnlocked(Unlocks, ERunUnlock::EarlyNight, Day))
		{
			return false;
		}
		OutHours = FFloatInterval(Unlocks.EarlyNightStartHours.Min, FMath::Max(Unlocks.EarlyNightStartHours.Min, Unlocks.EarlyNightStartHours.Max));
		return true;
	}

	/** Temples of a fragment on Day (0 before the temples) */
	inline int32 TemplesPerFragment(const FRunCampaignUnlocks& Unlocks, int32 Day)
	{
		if (!IsUnlocked(Unlocks, ERunUnlock::Temples, Day))
		{
			return 0;
		}
		return FMath::Max(0, IsUnlocked(Unlocks, ERunUnlock::AllArtifacts, Day) ? Unlocks.TemplesPerFragmentAll : Unlocks.TemplesPerFragmentFirst);
	}

	/** Deals the artifacts of a pool to the temples: PerTemple each, without repeat while the pool lasts, the pool order
	 *  shuffled by Stream; returns, per temple, the indices into the pool */
	inline TArray<TArray<int32>> DealArtifacts(int32 PoolSize, int32 Temples, int32 PerTemple, FRandomStream& Stream)
	{
		TArray<int32> Order;
		for (int32 Index = 0; Index < PoolSize; ++Index)
		{
			Order.Add(Index);
		}
		for (int32 Index = Order.Num() - 1; Index > 0; --Index)
		{
			Order.Swap(Index, Stream.RandRange(0, Index));
		}
		TArray<TArray<int32>> Dealt;
		int32 Next = 0;
		for (int32 Temple = 0; Temple < Temples; ++Temple)
		{
			TArray<int32>& Hand = Dealt.AddDefaulted_GetRef();
			for (int32 Card = 0; Card < PerTemple && Next < Order.Num(); ++Card)
			{
				Hand.Add(Order[Next++]);
			}
		}
		return Dealt;
	}
}
