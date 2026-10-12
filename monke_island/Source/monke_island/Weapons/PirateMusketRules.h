// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"
#include "PirateMusketRules.generated.h"

/**
 *  Tunable rules of the redesigned flintlock musket (DA_PirateMusketRules; T-170, second PC: bean-musket v0.2 and
 *  bean-musket-anim v0.2, 24 gestures in first and third person). Same gameplay as the old musket (UMusketRules):
 *  one ball in the barrel, the rest in the pouch, a full reload butt on the ground after each shot, stretched over
 *  ReloadSeconds (the GroundReload clip lasts 6 s at 24 fps).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UPirateMusketRules : public UFirearmRules
{
	GENERATED_BODY()

public:

	UPirateMusketRules();

	/** Balls a player carries, the one in the barrel included */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
	int32 MaxBalls = 15;

	/** The musket starts loaded (that ball counts in MaxBalls) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo")
	bool bStartLoaded = true;

	/** Full reload time: the ReloadGesture clip (all its phases) is stretched over it (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload", meta=(ClampMin="0.1", Units="s"))
	float ReloadSeconds = 1.5f;

	/** Gesture of the reload (the whole sequence, half cock to full cock) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	EFirearmGesture ReloadGesture = EFirearmGesture::GroundReload;

	/** Pressing fire with an empty barrel starts the reload */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoReloadOnEmptyFire = true;

	/** Event of the shot clip where the trigger is pulled: the clip starts there, the ball leaves at FireEvent (the
	 *  lock time between them, about 0.12 s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shot")
	FName TriggerEvent = TEXT("Trigger");
};
