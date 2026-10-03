// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UI/FloodTimerSource.h"
#include "FloodTimerSubsystem.generated.h"

class UFloodTimerWidget;
class UHudTimerRules;

/**
 *  Puts the flood timer on the screen of each local player once a flood clock is active in the world
 *  (expedition started, or a standalone flood actor). No flood source = no timer and no error.
 *  A world subsystem rather than a hook in the player controller or the HUD: it works with any controller
 *  (Lvl_Raraka runs BP_FirstPersonPlayerController) and keeps AIslanderHUD, owned by another task, untouched.
 *  The widget is owned by the local player controller; the dedicated server never creates one.
 *  Config (DefaultGame.ini, [/Script/monke_island.FloodTimerSubsystem]): RulesAsset, WidgetClass.
 */
UCLASS(config=Game)
class MONKE_ISLAND_API UFloodTimerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Rules of the timer (DA_HudTimerRules); empty = class defaults, silent */
	UPROPERTY(Config, EditAnywhere, Category="HUD|Flood Timer")
	TSoftObjectPtr<UHudTimerRules> RulesAsset;

	/** Widget class (a future WBP_FloodTimer / part of WBP_HUD); empty = UFloodTimerWidget and its Slate view */
	UPROPERTY(Config, EditAnywhere, Category="HUD|Flood Timer")
	TSoftClassPtr<UFloodTimerWidget> WidgetClass;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Timer widgets created in this world */
	const TArray<TObjectPtr<UFloodTimerWidget>>& GetWidgets() const { return Widgets; }

protected:

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:

	/** Looks for an active flood source and a local player without a timer */
	void Discover();

	/** Rules loaded at begin play */
	UPROPERTY(Transient)
	TObjectPtr<UHudTimerRules> Rules;

	/** Widget class resolved at begin play */
	UPROPERTY(Transient)
	TSubclassOf<UFloodTimerWidget> ResolvedWidgetClass;

	/** Timer widgets created in this world */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFloodTimerWidget>> Widgets;

	/** Source probe used by the discovery */
	FFloodTimerSource Probe;

	/** Discovery period */
	FTimerHandle DiscoveryTimer;
};
