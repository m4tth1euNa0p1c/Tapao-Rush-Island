// monke_island

#include "Run/RunRules.h"
#include "Economy/EconomyRules.h"

#define LOCTEXT_NAMESPACE "RunRules"

URunRules::URunRules()
{
	// the ten-day campaign (owner request 2026-10-07, docs/features/campagne-dix-jours.md): three contracts, the quota
	// adding up inside each one (ADR-007), starting values to test. The zone table of ADR-007 (1 800 ... 18 000 over
	// five zones) stays in the ADR for the record. Since 2026-10-09 ("tout devient plus dur") the danger and the deluge
	// of the stages are neutral: the difficulty curve by day below carries them, day after day.
	auto AddZone = [this](const TCHAR* Id, const FText& Name, int32 Quota, int32 Days, float Spawn, float Loot, float Flood)
	{
		FRunZone& Zone = Zones.AddDefaulted_GetRef();
		Zone.ZoneId = Id;
		Zone.DisplayName = Name;
		Zone.Quota = Quota;
		Zone.Days = Days;
		Zone.SpawnScale = Spawn;
		Zone.LootValueScale = Loot;
		Zone.FloodDelayScale = Flood;
	};
	AddZone(TEXT("Villages"), LOCTEXT("Villages", "Villages et marae"), 5500, 3, 1.0f, 1.0f, 1.0f);
	AddZone(TEXT("Temples"), LOCTEXT("Temples", "Temples et forts"), 10500, 3, 1.0f, 1.5f, 1.0f);
	AddZone(TEXT("GreatAtoll"), LOCTEXT("GreatAtoll", "Grand atoll"), 20500, 4, 1.0f, 2.2f, 1.0f);

	// the difficulty curve of the ten days (owner 2026-10-09: "mets tout devient plus dur"): a gentle first day, the
	// forts and every artifact temple on day 3, two forts from day 6, three from day 9, the crab out earlier and the
	// deluge sooner at the end (day 10: 540 s x 0.78 = 7 min). The surplus of the campaign (x1.5 from day 2) and the
	// bushes by day (x2.5 from day 2) stay on top of MobCount. Starting values, to tune in play (DA_RunRules).
	auto AddDay = [this](float Mobs, float Health, float Damage, int32 Soldiers, float HitChance, int32 Forts, float CrabHealth, float CrabMinute,
		float Flood, float Night)
	{
		FRunDayDifficulty& Day = DifficultyByDay.AddDefaulted_GetRef();
		Day.MobCountScale = Mobs;
		Day.MobHealthScale = Health;
		Day.MobDamageScale = Damage;
		Day.SoldiersPerFort = Soldiers;
		Day.SoldierHitChanceScale = HitChance;
		Day.FortsPerExpedition = Forts;
		Day.CrabHealthScale = CrabHealth;
		Day.CrabEmergenceMinute = CrabMinute;
		Day.FloodDelayScale = Flood;
		Day.NightShare = Night;
	};
	//     mobs   health damage soldiers hit    forts crab   crab min flood  night
	AddDay(0.80f, 0.90f, 0.90f, 0,       1.00f, 0,    0.90f, 0.00f,   1.00f, 0.00f); // day 1: gentle
	AddDay(1.00f, 1.00f, 1.00f, 0,       1.00f, 0,    1.00f, 0.00f,   1.00f, 0.00f); // day 2: the rules, the surplus of the campaign
	AddDay(1.10f, 1.10f, 1.10f, 7,       1.00f, 1,    1.10f, 5.50f,   0.95f, 0.00f); // day 3: forts and every artifact temple
	AddDay(1.25f, 1.20f, 1.20f, 8,       1.10f, 1,    1.20f, 5.50f,   0.90f, 0.00f); // day 4
	AddDay(1.25f, 1.20f, 1.20f, 8,       1.10f, 1,    1.20f, 5.25f,   0.90f, 0.00f); // day 5
	AddDay(1.40f, 1.35f, 1.30f, 9,       1.20f, 2,    1.35f, 5.00f,   0.85f, 0.60f); // day 6: two forts, the night takes more of the day
	AddDay(1.60f, 1.50f, 1.40f, 10,      1.25f, 2,    1.50f, 5.00f,   0.85f, 0.65f); // day 7: everything open
	AddDay(1.80f, 1.70f, 1.50f, 11,      1.30f, 2,    1.70f, 4.50f,   0.80f, 0.70f); // day 8
	AddDay(1.80f, 1.70f, 1.50f, 11,      1.30f, 3,    1.70f, 4.50f,   0.80f, 0.70f); // day 9: three forts
	AddDay(2.00f, 2.00f, 1.60f, 12,      1.40f, 3,    2.00f, 4.00f,   0.78f, 0.80f); // day 10: the crab at 4:00, the deluge at 7 min
}

FRunDayDifficulty URunRules::GetDayDifficulty(int32 Day) const
{
	// an asset that emptied the table plays every day as the rules of the creatures say (no curve)
	return RunDifficulty::Of(DifficultyByDay, Day);
}

FRunContractRules URunRules::MakeContractRules() const
{
	FRunContractRules Rules;
	// an asset that emptied the zones falls back on the zones of ADR-007 (an empty list would win at once)
	Rules.Zones = Zones.IsEmpty() && this != GetDefault<URunRules>() ? GetDefault<URunRules>()->Zones : Zones;
	Rules.DaysPerContract = FMath::Max(1, DaysPerContract);
	Rules.GreedLootScale = FMath::Max(0.0f, GreedLootScale);
	Rules.GreedDangerScale = FMath::Max(0.0f, GreedDangerScale);
	Rules.ExcessPayoutFraction = FMath::Clamp(ExcessPayoutFraction, 0.0f, 1.0f);
	Rules.FailedTreasuryLoss = FMath::Clamp(FailedTreasuryLoss, 0.0f, 1.0f);
	Rules.FailedZoneDrop = FMath::Max(0, FailedZoneDrop);
	Rules.bExecuteOnFailedContract = bExecuteOnFailedContract;
	Rules.MedicalFee = FMath::Max(0, RecruitFee);
	return Rules;
}

#undef LOCTEXT_NAMESPACE

FRunRulesSnapshot URunRules::MakeSnapshot() const
{
	FRunRulesSnapshot Snapshot;
	Snapshot.QuotaBase = FMath::Max(0, QuotaBase);
	Snapshot.QuotaStep = FMath::Max(0, QuotaStep);
	Snapshot.QuotaAccel = FMath::Max(0, QuotaAccel);
	Snapshot.PlayerQuotaScale = FMath::Max(0.0f, PlayerQuotaScale);
	Snapshot.RecruitFee = FMath::Max(0, RecruitFee);
	Snapshot.DifficultyStep = FMath::Max(0.0f, DifficultyStep);
	Snapshot.DifficultyMax = FMath::Max(1.0f, DifficultyMax);
	// the tax and the bonus of cahier §7 live in one place
	if (Economy)
	{
		const FEconomyRulesSnapshot EconomySnapshot = Economy->MakeSnapshot();
		Snapshot.TaxRateBasisPoints = EconomySnapshot.TaxRateBasisPoints;
		Snapshot.ContractBonus = EconomySnapshot.ContractBonus;
		Snapshot.bTaxContractBonus = EconomySnapshot.bTaxContractBonus;
	}
	return Snapshot;
}
