// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TapaoDayUISubsystem.generated.h"

class FTapaoStyle;
class SWidget;
class UTapaoUISkin;
class UWorld;
enum class ERunDayVerdict : uint8;
struct FTapaoDayEndInput;

/**
 *  Shows the end of each day (STapaoDayEnd, ADR-007 §7; T-173) on every machine, from the replicated run state
 *  (URunStateComponent::GetLastDayReport): a new report appears when the days played grow, again when the greed answer
 *  comes. A day survived shows on the left for SummarySeconds while the galleon sails off; a lost day (the whole crew
 *  dead) shows in the middle until the travel ("JOUR N PERDU", the countdown, JOUR SUIVANT for the host); the contract
 *  notices (CONTRAT TERMINÉ, CAMPAGNE HONORÉE, CONTRAT ÉCHOUÉ) take the place of the day's. The execution of the crew
 *  has its own scene, then the end screen of the lost campaign (UTapaoUISubsystem): no notice over it.
 *  On the host (or solo) the greed question has its two buttons, answered through URunSubsystem::Choose (server, then
 *  replicated to the others, who read "the captain decides"), then the travel is hurried.
 *  Looks at the run state a few times a second (no binding to keep across the maps).
 *  Tests (not in Shipping): monke.UI.DayEnd Next|Choose|Complete|Victory|Failed|Executed [Lost] | Hide.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UTapaoDayUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Time the summary of a day survived stays when there is nothing to answer (s); a lost day stays until the travel */
	UPROPERTY(Config)
	float SummarySeconds = 9.0f;

	/** Travel left after the host answered the greed question, or sent the galleon on after a lost day (s) */
	UPROPERTY(Config)
	float TravelAfterChoiceSeconds = 3.0f;

	/** Tests (monke.UI.DayEnd): the notice of a made-up end of day on this machine only, with the zone and the quota of
	 *  the run (nothing changes on the server); an Executed verdict of a crew alive shows the end screen of the lost
	 *  campaign instead (UTapaoUISubsystem) */
	void DebugPreview(ERunDayVerdict Verdict, bool bLost);

	/** Tests (monke.UI.DayEnd Hide): hides a notice shown by DebugPreview */
	void DebugHide() { Hide(); }

private:

	bool Poll(float DeltaTime);
	void Show(UWorld* World);
	void Hide();
	void HandleChoice(bool bStay);

	/** The host sends the galleon to the next island now (a lost day) */
	void HandleNextDay();

	/** The notice of Input on this machine; bPreview: a test, the summary leaves after SummarySeconds */
	void ShowNotice(UWorld* World, const FTapaoDayEndInput& Input, bool bPreview);

	/** What the notice is built from, read from the run state of the world and DA_RunRules; false without a run state */
	bool MakeInput(UWorld* World, FTapaoDayEndInput& OutInput) const;

	/** The live line under the notice: the time left to answer the greed question, or to the next day */
	FText GetCountdownText() const;

	FSoftObjectPath SkinPath = FSoftObjectPath(TEXT("/Game/MonkeIsland/UI/Tapao/DA_TapaoUISkin.DA_TapaoUISkin"));

	UPROPERTY(Transient)
	TObjectPtr<UTapaoUISkin> Skin;

	TSharedPtr<FTapaoStyle> Style;
	TSharedPtr<SWidget> Screen;
	FTSTicker::FDelegateHandle TickerHandle;

	/** World the baseline was taken in, and the days played already shown there */
	TWeakObjectPtr<UWorld> SeenWorld;
	int32 SeenDays = -1;
	bool bSeenChoice = false;
	bool bScreenTakesMouse = false;

	/** The notice stays until the travel (a lost day), not SummarySeconds */
	bool bScreenStays = false;
	double ShownTime = 0.0;
};
