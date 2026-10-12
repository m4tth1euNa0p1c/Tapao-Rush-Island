// monke_island

#include "UI/Tapao/TapaoDayEndText.h"

#define LOCTEXT_NAMESPACE "TapaoDayEnd"

namespace
{
	/** "+50 %" for a multiplier of 1.5, "-20 %" for 0.8 (DA_RunRules values, never written in the texts) */
	FText SignedPercent(float Scale)
	{
		const int32 Percent = FMath::RoundToInt((Scale - 1.0f) * 100.0f);
		return FText::Format(LOCTEXT("SignedPercent", "{0}{1} %"), FText::AsCultureInvariant(FString(Percent < 0 ? TEXT("-") : TEXT("+"))),
			FText::AsNumber(FMath::Abs(Percent)));
	}

	/** "Il manque 2 300 couronnes au contrat." */
	FText MissingLine(const FRunDayReport& Report)
	{
		return FText::Format(LOCTEXT("Missing", "Il manque {0} couronnes au contrat."), FText::AsNumber(FMath::Max(0, Report.Quota - Report.ContractValue)));
	}
}

ERunDayVerdict TapaoDayEndText::EffectiveVerdict(ERunDayVerdict Reported, bool bAwaitingChoice, ERunOutcome Outcome, bool bGreedDay)
{
	if (Reported != ERunDayVerdict::QuotaMetChoose || bAwaitingChoice)
	{
		return Reported;
	}
	// the greed answer came: what it led to (URunSubsystem::Choose also writes it into the report)
	if (Outcome == ERunOutcome::Victory)
	{
		return ERunDayVerdict::Victory;
	}
	return bGreedDay ? ERunDayVerdict::NextDay : ERunDayVerdict::ContractComplete;
}

FTapaoDayEndView TapaoDayEndText::Build(const FTapaoDayEndInput& In)
{
	FTapaoDayEndView View;
	const FRunDayReport& R = In.Report;
	View.Verdict = EffectiveVerdict(R.Verdict, In.bAwaitingChoice, In.Outcome, In.bGreedDay);
	View.bDayLost = R.bWiped;
	View.bAsksGreed = View.Verdict == ERunDayVerdict::QuotaMetChoose;
	View.bCampaignLost = View.Verdict == ERunDayVerdict::Executed;
	View.bOffersNextDay = R.bWiped && !View.bAsksGreed && !View.bCampaignLost && View.Verdict != ERunDayVerdict::Invalid;
	View.bQuotaMet = R.Quota > 0 && R.ContractValue >= R.Quota;

	// one count of the days (T-283): the day of the campaign, as the HUD and the arrival title say it
	const int32 Day = In.CampaignDay > 0 ? In.CampaignDay : R.Day;

	switch (View.Verdict)
	{
	case ERunDayVerdict::ContractComplete:
		View.Kind = ETapaoDayEndKind::ContractDone;
		break;
	case ERunDayVerdict::Victory:
		View.Kind = ETapaoDayEndKind::CampaignWon;
		break;
	case ERunDayVerdict::ContractFailed:
		View.Kind = ETapaoDayEndKind::ContractFailed;
		break;
	case ERunDayVerdict::Executed:
		View.Kind = ETapaoDayEndKind::CampaignLost;
		break;
	default:
		View.Kind = R.bWiped ? ETapaoDayEndKind::DayLost : ETapaoDayEndKind::DayDone;
		break;
	}
	const bool bContract = View.Kind != ETapaoDayEndKind::DayDone && View.Kind != ETapaoDayEndKind::DayLost;
	View.bAlarm = View.Kind == ETapaoDayEndKind::DayLost || View.Kind == ETapaoDayEndKind::ContractFailed || View.Kind == ETapaoDayEndKind::CampaignLost;

	// a contract notice is the Bureau's (docs/narrative/BIBLE.md: impersonal, exact figures, no "!")
	View.Header = bContract ? LOCTEXT("BureauHeader", "BUREAU DES POSSESSIONS LOINTAINES") : FText::GetEmpty();
	switch (View.Kind)
	{
	case ETapaoDayEndKind::DayDone:
		View.Title = FText::Format(LOCTEXT("Title", "JOUR {0} TERMINÉ"), Day);
		break;
	case ETapaoDayEndKind::DayLost:
		View.Title = FText::Format(LOCTEXT("TitleWiped", "JOUR {0} PERDU"), Day);
		break;
	case ETapaoDayEndKind::ContractDone:
		View.Title = LOCTEXT("TitleContractDone", "CONTRAT TERMINÉ");
		break;
	case ETapaoDayEndKind::CampaignWon:
		View.Title = LOCTEXT("TitleCampaignWon", "CAMPAGNE HONORÉE");
		break;
	default:
		View.Title = LOCTEXT("TitleContractFailed", "CONTRAT ÉCHOUÉ");
		break;
	}
	if (!bContract)
	{
		View.Subtitle = In.ZoneName;
	}
	else if (In.ZoneName.IsEmpty())
	{
		View.Subtitle = FText::Format(R.bWiped ? LOCTEXT("SubtitleLostAlone", "Jour {0} perdu") : LOCTEXT("SubtitleDoneAlone", "Jour {0} terminé"), Day);
	}
	else
	{
		View.Subtitle = FText::Format(R.bWiped ? LOCTEXT("SubtitleLost", "{0} · jour {1} perdu") : LOCTEXT("SubtitleDone", "{0} · jour {1} terminé"),
			In.ZoneName, Day);
	}

	// the quota of the contract, big (owner 2026-10-07: "affiche les quotas en fin de partie")
	View.Quota = FText::Format(LOCTEXT("QuotaBig", "QUOTA : {0} / {1} couronnes"), FText::AsNumber(R.ContractValue), FText::AsNumber(R.Quota));

	View.Lines.Add(R.bWiped ? LOCTEXT("Lost", "Tout l'équipage est mort : le butin reste sur l'île.")
		: FText::Format(LOCTEXT("Value", "Valeur ramenée au galion : {0} couronnes"), FText::AsNumber(R.DayValue)));
	if (R.bWiped && !In.DeathCause.IsEmpty())
	{
		// the cause of the local player's death stays readable once the whole crew is dead (cahier §19, T-135)
		View.Lines.Add(FText::Format(LOCTEXT("DeathCause", "Tu es mort : {0}"), In.DeathCause));
	}
	View.Lines.Add(FText::Format(LOCTEXT("Dead", "Morts : {0}   ·   frais médicaux : {1} couronnes"), R.Deaths, FText::AsNumber(R.MedicalFees)));
	switch (View.Kind)
	{
	case ETapaoDayEndKind::ContractDone:
	case ETapaoDayEndKind::CampaignWon:
		if (R.Payout > 0)
		{
			View.Lines.Add(FText::Format(LOCTEXT("Payout", "Excédent versé au Trésor : +{0} couronnes"), FText::AsNumber(R.Payout)));
		}
		break;
	case ETapaoDayEndKind::ContractFailed:
		View.Lines.Add(MissingLine(R));
		if (R.Penalty > 0)
		{
			const int32 Share = FMath::RoundToInt(FMath::Clamp(In.FailedTreasuryLoss, 0.0f, 1.0f) * 100.0f);
			View.Lines.Add(FText::Format(LOCTEXT("Penalty", "Retenue sur la trésorerie : {0} couronnes ({1} %)"), FText::AsNumber(R.Penalty), Share));
		}
		break;
	case ETapaoDayEndKind::CampaignLost:
		View.Lines.Add(MissingLine(R));
		break;
	default:
		break;
	}
	View.Lines.Add(FText::Format(LOCTEXT("Treasury", "Trésorerie : {0} couronnes"), FText::AsNumber(In.Treasury)));

	switch (View.Verdict)
	{
	case ERunDayVerdict::NextDay:
		if (In.bGreedDay)
		{
			View.Outcome = FText::Format(LOCTEXT("NextGreed", "Un jour de plus dans la région : butin {0}, danger {1}."),
				SignedPercent(In.GreedLootScale), SignedPercent(In.GreedDangerScale));
		}
		else if (In.DeadlineDay > 0)
		{
			View.Outcome = FText::Format(LOCTEXT("Next", "Jour suivant : {0}. Échéance du contrat : jour {1}."), In.NextZoneName, In.DeadlineDay);
		}
		else
		{
			View.Outcome = FText::Format(LOCTEXT("NextNoDeadline", "Jour suivant : {0}."), In.NextZoneName);
		}
		break;
	case ERunDayVerdict::QuotaMetChoose:
		View.Outcome = LOCTEXT("Choose", "QUOTA ATTEINT : quitter la région, ou tenter un jour de plus ?");
		break;
	case ERunDayVerdict::ContractComplete:
		View.Outcome = FText::Format(LOCTEXT("Complete", "Le Bureau constate que le contrat est rempli. Nouvelle destination : {0}."), In.NextZoneName);
		break;
	case ERunDayVerdict::Victory:
		View.Outcome = LOCTEXT("Victory", "Le Bureau constate que la campagne est honorée. La Couronne est satisfaite. Provisoirement.");
		break;
	case ERunDayVerdict::ContractFailed:
		View.Outcome = FText::Format(LOCTEXT("Failed", "Le Bureau constate le défaut. Retour vers : {0}. Les améliorations du galion restent acquises."),
			In.NextZoneName);
		break;
	case ERunDayVerdict::Executed:
		View.Outcome = R.bWiped ? LOCTEXT("ExecutedPosthumous", "Avis de déchéance : l'équipage est déclaré traître, à titre posthume.")
			: LOCTEXT("Executed", "Avis de déchéance : l'équipage est déclaré traître et remis au peloton français.");
		break;
	default:
		break;
	}
	View.GreedHint = View.bAsksGreed ? GreedHint(In.GreedLootScale, In.GreedDangerScale, In.bGreedDefaultStay) : FText::GetEmpty();
	return View;
}

FText TapaoDayEndText::GreedHint(float LootScale, float DangerScale, bool bDefaultStay)
{
	return FText::Format(LOCTEXT("GreedHint", "Un jour de plus : butin {0}, danger {1}. Sans réponse, {2}"), SignedPercent(LootScale), SignedPercent(DangerScale),
		bDefaultStay ? LOCTEXT("GreedDefaultStay", "l'équipage reste un jour de plus.") : LOCTEXT("GreedDefaultLeave", "l'équipage quitte la région."));
}

FText TapaoDayEndText::Countdown(bool bAwaitingChoice, bool bDayLost, int32 SecondsLeft)
{
	if (SecondsLeft < 0)
	{
		return FText::GetEmpty();
	}
	if (bAwaitingChoice)
	{
		return FText::Format(LOCTEXT("GreedCountdown", "Réponse attendue dans {0} s"), SecondsLeft);
	}
	if (bDayLost)
	{
		return FText::Format(LOCTEXT("NextDayCountdown", "Jour suivant dans {0} s"), SecondsLeft);
	}
	return FText::GetEmpty();
}

TArray<FText> TapaoDayEndText::LostCampaignLines(const FRunDayReport& Report, const FText& ZoneName, int32 CampaignDay)
{
	TArray<FText> Lines;
	if (Report.Verdict != ERunDayVerdict::Executed)
	{
		// no report of the failed day (a test of the scene, monke.Execution.Start): the short line
		Lines.Add(LOCTEXT("ExecutedLine", "Quota non atteint : la campagne est perdue."));
		return Lines;
	}
	Lines.Add(ZoneName.IsEmpty() ? LOCTEXT("LostNoticeAlone", "Avis de déchéance") : FText::Format(LOCTEXT("LostNotice", "Avis de déchéance : {0}"), ZoneName));
	Lines.Add(FText::Format(LOCTEXT("LostValue", "{0} / {1} couronnes : il manque {2} couronnes."), FText::AsNumber(Report.ContractValue),
		FText::AsNumber(Report.Quota), FText::AsNumber(FMath::Max(0, Report.Quota - Report.ContractValue))));
	Lines.Add(Report.bWiped ? LOCTEXT("LostPosthumous", "L'équipage est déclaré traître, à titre posthume.")
		: LOCTEXT("LostSquad", "L'équipage est déclaré traître et remis au peloton français."));
	if (CampaignDay > 0)
	{
		Lines.Add(FText::Format(LOCTEXT("LostDay", "La campagne s'achève au jour {0}."), CampaignDay));
	}
	return Lines;
}

int32 TapaoDayEndText::ContractDeadlineDay(int32 DaysPlayed, int32 DayInContract, int32 DaysPerContract)
{
	if (DaysPlayed < 0 || DayInContract < 1 || DaysPerContract < 1)
	{
		return 0;
	}
	return DaysPlayed + 1 + FMath::Max(0, DaysPerContract - DayInContract);
}

FText TapaoDayEndText::ContractLine(int32 DaysPlayed, int32 DayInContract, int32 DaysPerContract, bool bGreedDay)
{
	const int32 Deadline = ContractDeadlineDay(DaysPlayed, DayInContract, DaysPerContract);
	if (Deadline <= 0)
	{
		return FText::GetEmpty();
	}
	const FText Line = FText::Format(LOCTEXT("ContractDeadline", "Contrat : jour {0}"), Deadline);
	return bGreedDay ? FText::Format(LOCTEXT("ContractGreedDay", "{0} · jour de cupidité"), Line) : Line;
}

bool TapaoDayEndText::IsCampaignUnderWay(int32 Quota, int32 DaysPlayed, ERunDayVerdict LastVerdict)
{
	return Quota > 0 || DaysPlayed > 0 || LastVerdict != ERunDayVerdict::Invalid;
}

#undef LOCTEXT_NAMESPACE
