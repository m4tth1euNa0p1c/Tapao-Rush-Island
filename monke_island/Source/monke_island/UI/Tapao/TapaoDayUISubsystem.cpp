// monke_island

#include "UI/Tapao/TapaoDayUISubsystem.h"
#include "UI/Tapao/STapaoDayEnd.h"
#include "UI/Tapao/TapaoDayEndText.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoShopUISubsystem.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "UI/Tapao/TapaoUISubsystem.h"
#include "Death/DeathSpectatorComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Run/RunRules.h"
#include "Run/RunStateComponent.h"
#include "Run/RunSubsystem.h"
#include "monke_island.h"

namespace
{
	/** Over the HUD (100), the death screen (600) and the inventory (850); under the pause (900), the end screens (920)
	 *  and the questions (950) */
	constexpr int32 DayEndZOrder = 870;

	/** DA_RunRules of the game instance, else the class defaults (the clients read their own copy of the asset) */
	const URunRules* RunRulesOf(const UWorld* World)
	{
		const URunSubsystem* Subsystem = URunSubsystem::Get(World);
		const URunRules* Rules = Subsystem ? Subsystem->GetRules() : nullptr;
		return Rules ? Rules : GetDefault<URunRules>();
	}

#if !UE_BUILD_SHIPPING
	FAutoConsoleCommandWithWorldAndArgs GDayEndPreviewCommand(
		TEXT("monke.UI.DayEnd"),
		TEXT("Shows a made-up end of day on this machine only (tests of the screens, nothing changes on the server): ")
		TEXT("monke.UI.DayEnd Next|Choose|Complete|Victory|Failed|Executed [Lost], or monke.UI.DayEnd Hide"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			UTapaoDayUISubsystem* Screens = GameInstance ? GameInstance->GetSubsystem<UTapaoDayUISubsystem>() : nullptr;
			if (!Screens)
			{
				return;
			}
			const FString Kind = Args.Num() > 0 ? Args[0] : FString(TEXT("Next"));
			if (Kind.Equals(TEXT("Hide"), ESearchCase::IgnoreCase))
			{
				Screens->DebugHide();
				if (UTapaoUISubsystem* Tapao = GameInstance->GetSubsystem<UTapaoUISubsystem>())
				{
					Tapao->DebugCloseEndScreen();
				}
				return;
			}
			const bool bLost = Args.Num() > 1 && Args[1].Equals(TEXT("Lost"), ESearchCase::IgnoreCase);
			ERunDayVerdict Verdict = ERunDayVerdict::NextDay;
			if (Kind.Equals(TEXT("Choose"), ESearchCase::IgnoreCase))
			{
				Verdict = ERunDayVerdict::QuotaMetChoose;
			}
			else if (Kind.Equals(TEXT("Complete"), ESearchCase::IgnoreCase))
			{
				Verdict = ERunDayVerdict::ContractComplete;
			}
			else if (Kind.Equals(TEXT("Victory"), ESearchCase::IgnoreCase))
			{
				Verdict = ERunDayVerdict::Victory;
			}
			else if (Kind.Equals(TEXT("Failed"), ESearchCase::IgnoreCase))
			{
				Verdict = ERunDayVerdict::ContractFailed;
			}
			else if (Kind.Equals(TEXT("Executed"), ESearchCase::IgnoreCase))
			{
				Verdict = ERunDayVerdict::Executed;
			}
			Screens->DebugPreview(Verdict, bLost);
		}));
#endif
}

bool UTapaoDayUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

void UTapaoDayUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Skin = Cast<UTapaoUISkin>(SkinPath.TryLoad());
	if (!Skin)
	{
		return;
	}
	Style = MakeShared<FTapaoStyle>(Skin);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UTapaoDayUISubsystem::Poll), 0.2f);
}

void UTapaoDayUISubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	Hide();
	Super::Deinitialize();
}

bool UTapaoDayUISubsystem::Poll(float DeltaTime)
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const URunStateComponent* Run = URunStateComponent::Get(World);
	if (!Run)
	{
		if (SeenWorld.Get() != World)
		{
			Hide();
		}
		return true;
	}
	// a new map: what was there already is not news (the day ended on the island before)
	if (SeenWorld.Get() != World)
	{
		Hide();
		SeenWorld = World;
		SeenDays = Run->GetDaysPlayed();
		bSeenChoice = Run->IsAwaitingChoice();
		return true;
	}
	const bool bNewDay = Run->GetDaysPlayed() > SeenDays && Run->GetLastDayReport().Verdict != ERunDayVerdict::Invalid;
	const bool bChoiceGone = bSeenChoice && !Run->IsAwaitingChoice();
	if (bNewDay || bChoiceGone)
	{
		SeenDays = Run->GetDaysPlayed();
		bSeenChoice = Run->IsAwaitingChoice();
		Show(World);
		return true;
	}
	// a day survived: the summary leaves, the departure scene goes on; a lost day keeps its notice until the travel, the
	// greed question until it is answered
	if (Screen.IsValid() && !bScreenStays && !Run->IsAwaitingChoice() && FPlatformTime::Seconds() - ShownTime > SummarySeconds)
	{
		Hide();
	}
	return true;
}

bool UTapaoDayUISubsystem::MakeInput(UWorld* World, FTapaoDayEndInput& OutInput) const
{
	const URunStateComponent* Run = URunStateComponent::Get(World);
	if (!Run)
	{
		return false;
	}
	const URunRules* Rules = RunRulesOf(World);
	const FRunDayReport& Report = Run->GetLastDayReport();
	OutInput.Report = Report;
	OutInput.CampaignDay = Run->GetDaysPlayed();
	OutInput.bAwaitingChoice = Run->IsAwaitingChoice();
	OutInput.Outcome = Run->GetOutcome();
	OutInput.bGreedDay = Run->IsGreedDay();
	OutInput.ZoneName = Rules->Zones.IsValidIndex(Report.ZoneIndex) ? Rules->Zones[Report.ZoneIndex].DisplayName : FText::GetEmpty();
	OutInput.NextZoneName = Run->GetZoneName();
	// the contract to come, in days of the campaign (the state already moved to the next day)
	OutInput.DeadlineDay = TapaoDayEndText::ContractDeadlineDay(Run->GetDaysPlayed(), Run->GetDayIndex(), Run->GetDaysPerPeriod());
	OutInput.Treasury = Run->GetTreasury();
	// the cause of the local player's death, on a lost day (T-135: NAUFRAGÉ used to carry it)
	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController(World) : nullptr;
	const UDeathSpectatorComponent* Spectator = Controller ? Controller->FindComponentByClass<UDeathSpectatorComponent>() : nullptr;
	OutInput.DeathCause = Spectator && Report.bWiped ? Spectator->DescribeOwnerDeath() : FText::GetEmpty();
	OutInput.FailedTreasuryLoss = Rules->FailedTreasuryLoss;
	OutInput.GreedLootScale = Rules->GreedLootScale;
	OutInput.GreedDangerScale = Rules->GreedDangerScale;
	OutInput.bGreedDefaultStay = Rules->bGreedDefaultStay;
	return true;
}

void UTapaoDayUISubsystem::Show(UWorld* World)
{
	FTapaoDayEndInput Input;
	if (!MakeInput(World, Input))
	{
		Hide();
		return;
	}
	ShowNotice(World, Input, false);
}

void UTapaoDayUISubsystem::ShowNotice(UWorld* World, const FTapaoDayEndInput& Input, bool bPreview)
{
	Hide();
	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!World || !Viewport || !Style.IsValid())
	{
		return;
	}
	const FTapaoDayEndView View = TapaoDayEndText::Build(Input);
	// the execution of a crew alive has its own scene and titles, then the end screen of the lost campaign: no notice
	if (View.bCampaignLost && !View.bDayLost)
	{
		return;
	}
	const bool bHost = World->GetNetMode() != NM_Client;
	// a lost day: no departure scene to watch, the notice stays in the middle until the travel (a test leaves anyway)
	bScreenStays = View.bDayLost && !bPreview;

	Screen = SNew(STapaoDayEnd)
		.Style(Style)
		.View(View)
		.bCanChoose(bHost)
		.bCentered(View.bDayLost)
		.Countdown(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(this, &UTapaoDayUISubsystem::GetCountdownText)))
		.OnChoice(FOnTapaoGreedChoice::CreateUObject(this, &UTapaoDayUISubsystem::HandleChoice))
		.OnNextDay(FSimpleDelegate::CreateUObject(this, &UTapaoDayUISubsystem::HandleNextDay));
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), DayEndZOrder);
	ShownTime = FPlatformTime::Seconds();
	UE_LOG(Logmonke_island, Log, TEXT("TapaoUI: end of day shown (%s, %s%s)%s"), *UEnum::GetValueAsString(View.Verdict),
		View.bDayLost ? TEXT("day lost") : TEXT("day survived"), View.bAsksGreed ? TEXT(", greed question") : TEXT(""),
		bPreview ? TEXT(" [preview]") : TEXT(""));

	// the host's buttons need the mouse (the greed question, JOUR SUIVANT); the others keep the game's input
	APlayerController* Controller = GameInstance->GetFirstLocalPlayerController(World);
	if (bHost && (View.bAsksGreed || View.bOffersNextDay) && Controller)
	{
		FInputModeGameAndUI Mode;
		Mode.SetWidgetToFocus(Screen);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		Controller->SetInputMode(Mode);
		Controller->SetShowMouseCursor(true);
		bScreenTakesMouse = true;
	}
}

void UTapaoDayUISubsystem::Hide()
{
	bScreenStays = false;
	if (!Screen.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	}
	Screen.Reset();
	const UTapaoShopUISubsystem* Shop = GameInstance ? GameInstance->GetSubsystem<UTapaoShopUISubsystem>() : nullptr;
	if (bScreenTakesMouse && !(Shop && Shop->IsShopOpen()))
	{
		bScreenTakesMouse = false;
		if (APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr)
		{
			Controller->SetInputMode(FInputModeGameOnly());
			Controller->SetShowMouseCursor(false);
		}
	}
}

FText UTapaoDayUISubsystem::GetCountdownText() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	const AExpeditionGameState* Expedition = AExpeditionGameState::Get(World);
	const URunStateComponent* Run = URunStateComponent::Get(World);
	if (!Expedition || !Run)
	{
		return FText::GetEmpty();
	}
	// the server's clock and travel time, replicated: every machine counts the same
	const double Travel = Expedition->GetNextTravelTime();
	if (Travel < 0.0)
	{
		return FText::GetEmpty();
	}
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt32(Travel - Expedition->GetServerWorldTimeSeconds()));
	return TapaoDayEndText::Countdown(Run->IsAwaitingChoice(), Expedition->IsTeamWiped(), Seconds);
}

void UTapaoDayUISubsystem::HandleChoice(bool bStay)
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	URunSubsystem* Run = URunSubsystem::Get(World);
	if (!Run || !World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// the server decides (the host's machine is the server): the answer reaches the others with the run state
	if (Run->Choose(World, bStay))
	{
		if (AExpeditionGameState* GameState = AExpeditionGameState::Get(World))
		{
			GameState->HurryTravel(TravelAfterChoiceSeconds);
		}
	}
	// the next poll sees the answer and shows what follows
}

void UTapaoDayUISubsystem::HandleNextDay()
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// the host sends the galleon on (T-173): the travel takes everybody to the next island, the day lost
	if (AExpeditionGameState* GameState = AExpeditionGameState::Get(World))
	{
		GameState->HurryTravel(TravelAfterChoiceSeconds);
	}
}

void UTapaoDayUISubsystem::DebugPreview(ERunDayVerdict Verdict, bool bLost)
{
	UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	FTapaoDayEndInput Input;
	if (!MakeInput(World, Input))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("monke.UI.DayEnd: no run state in this world (an island of the campaign is needed)"));
		return;
	}
	// a made-up day on the run's zone, quota and treasury (5 500 when the run has none yet)
	const URunStateComponent* Run = URunStateComponent::Get(World);
	FRunDayReport& R = Input.Report;
	R = FRunDayReport();
	R.Verdict = Verdict;
	R.ZoneIndex = FMath::Max(0, Run->GetPeriodIndex() - 1);
	R.Day = Run->GetDayIndex();
	R.Quota = Run->GetQuota() > 0 ? Run->GetQuota() : 5500;
	const bool bMet = Verdict == ERunDayVerdict::QuotaMetChoose || Verdict == ERunDayVerdict::ContractComplete || Verdict == ERunDayVerdict::Victory;
	R.ContractValue = bMet ? R.Quota + R.Quota / 4 : R.Quota * 3 / 5;
	R.bWiped = bLost;
	R.DayValue = bLost ? 0 : R.ContractValue / 3;
	R.Deaths = bLost ? FMath::Max(1, World->GetGameState() ? World->GetGameState()->PlayerArray.Num() : 1) : 0;
	R.MedicalFees = R.Deaths * 50;
	R.Payout = bMet ? R.ContractValue - R.Quota : 0;
	R.Penalty = Verdict == ERunDayVerdict::ContractFailed ? Input.Treasury / 2 : 0;
	Input.CampaignDay = Run->GetDaysPlayed() + 1;
	Input.bAwaitingChoice = Verdict == ERunDayVerdict::QuotaMetChoose;
	Input.bGreedDay = false;
	Input.Outcome = Verdict == ERunDayVerdict::Victory ? ERunOutcome::Victory : (Verdict == ERunDayVerdict::Executed ? ERunOutcome::Defeat : ERunOutcome::InProgress);
	Input.ZoneName = RunRulesOf(World)->Zones.IsValidIndex(R.ZoneIndex) ? RunRulesOf(World)->Zones[R.ZoneIndex].DisplayName : Run->GetZoneName();
	Input.DeadlineDay = TapaoDayEndText::ContractDeadlineDay(Input.CampaignDay, FMath::Min(R.Day + 1, Run->GetDaysPerPeriod()), Run->GetDaysPerPeriod());
	if (Verdict == ERunDayVerdict::Executed && !bLost)
	{
		// the execution has no notice: the end screen of the lost campaign after the scene
		if (UTapaoUISubsystem* Tapao = GameInstance->GetSubsystem<UTapaoUISubsystem>())
		{
			Tapao->DebugShowLostCampaign(R, Input.ZoneName, Input.CampaignDay);
		}
		return;
	}
	ShowNotice(World, Input, true);
}
