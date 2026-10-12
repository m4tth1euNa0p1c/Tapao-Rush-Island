// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TapaoDeathUISubsystem.generated.h"

class FTapaoStyle;
class IInputProcessor;
class STapaoDeath;
class UDeathSpectatorComponent;
class UTapaoUISkin;

/**
 *  Death screen and spectator of the local player (T-135, docs/features/death-spectator.md): shows STapaoDeath while
 *  the local pawn is dead and the crew still lives ("TU ES MORT" and the cause, then the bar of the teammate followed),
 *  never over NAUFRAGÉ, the end of a day or a TAPAO question (UTapaoUISubsystem shows NAUFRAGÉ once the whole crew is
 *  dead). Its input processor turns the left and right clicks (and the arrows, the shoulder buttons) of a dead player
 *  into a change of the teammate followed (UDeathSpectatorComponent::RequestCycle), only while this screen shows and
 *  the game viewport has the focus. Values from the replicated state, read a few times a second (no binding to keep
 *  across the maps), as UTapaoDayUISubsystem.
 */
UCLASS()
class MONKE_ISLAND_API UTapaoDeathUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The death screen or the spectator bar is shown */
	bool IsShown() const { return Screen.IsValid(); }

	/** A click or key of the dead player: follow the next (1) or the previous (-1) teammate; false when it is not his
	 *  (the screen is not shown, a menu is open, the game viewport has not the focus) */
	bool HandleCycleInput(int32 Direction);

private:

	bool Poll(float DeltaTime);
	void Show();
	void Hide();

	/** Spectator component of the local player controller, or nullptr */
	UDeathSpectatorComponent* GetLocalSpectator() const;

	FSoftObjectPath SkinPath = FSoftObjectPath(TEXT("/Game/MonkeIsland/UI/Tapao/DA_TapaoUISkin.DA_TapaoUISkin"));

	UPROPERTY(Transient)
	TObjectPtr<UTapaoUISkin> Skin;

	TSharedPtr<FTapaoStyle> Style;
	TSharedPtr<STapaoDeath> Screen;
	TSharedPtr<IInputProcessor> InputProcessor;
	FTSTicker::FDelegateHandle TickerHandle;
};
