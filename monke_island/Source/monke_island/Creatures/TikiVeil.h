// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TikiVeil.generated.h"

class APlayerController;
struct FTikiAshBurstRules;

/**
 *  Veil of ash over the view of a blinded player (T-275: the cloud of the Veilleur des Cendres). Local and cosmetic: the
 *  server tells the machine of the blinded player (ATikiAshBurst::MulticastBlind), whose camera fades toward the colour
 *  of the ash (the HUD stays readable over it), holds, then fades back. Blinded again while veiled: the veil lasts
 *  longer. A fade owned by another system (the black of a day transition) is never touched. No tick: a timer runs only
 *  while a veil is on.
 */
UCLASS()
class MONKE_ISLAND_API UTikiVeilSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Subsystem of the world of an object, nullptr without a game world */
	static UTikiVeilSubsystem* Get(const UObject* WorldContext);

	/** This machine: veils the view of the local Player for Seconds (fade in, hold, fade out by Rules) */
	void Veil(APlayerController* Player, float Seconds, const FTikiAshBurstRules& Rules);

	/** True while Player's view is veiled */
	bool IsVeiled(const APlayerController* Player) const;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Deinitialize() override;

private:

	/** A veil on the view of a local player */
	struct FVeil
	{
		TWeakObjectPtr<APlayerController> Player;

		/** World time it starts to fade out (s) */
		double EndTime = 0.0;

		/** Length of the fade out (s) and colour of the veil (to know it is still ours) */
		float FadeOut = 1.0f;
		FLinearColor Color = FLinearColor::Black;
	};

	/** Veils on now */
	TArray<FVeil> Veils;

	/** Ends the veils whose time is over, while any is on */
	FTimerHandle Timer;

	void Update();
};
