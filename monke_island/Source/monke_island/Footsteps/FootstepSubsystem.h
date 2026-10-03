// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Footsteps/FootstepRules.h"
#include "FootstepSubsystem.generated.h"

class ACharacter;

/**
 *  Footsteps of every character near the local listener (players, tiki, villagers), on every machine that plays
 *  sound: never on a dedicated server, never replicated (each machine hears the movement it already sees).
 *  Steps follow the ground speed (cadence of DA_FootstepRules); the surface comes from the water depth at the feet,
 *  the floor's name keywords, then the height of the terrain above the water. A fall plays a heavy landing.
 *  Console: monke.Footsteps 0 turns it off (read when a world starts playing).
 */
UCLASS()
class MONKE_ISLAND_API UFootstepSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Rules loaded by default */
	static const TCHAR* DefaultRulesPath;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Tick(float DeltaTime) override;

	virtual TStatId GetStatId() const override;

	virtual bool IsTickable() const override { return Rules != nullptr; }

	/** Surface under a character's feet (also used by the tests) */
	EFootstepSurface FindSurface(const ACharacter* Character) const;

private:

	/** Step state of one character */
	struct FStepState
	{
		/** Fraction of the next step done (0..1) */
		float Phase = 0.0f;
		/** Whether it was on the ground last frame */
		bool bWasOnGround = true;
		/** Falling speed of the last airborne frame (cm/s, positive down) */
		float FallSpeed = 0.0f;
	};

	/** Plays one step of a surface at the feet */
	void PlayStep(const ACharacter* Character, EFootstepSurface Surface, float Volume) const;

	/** Rules (DA_FootstepRules), nothing plays when missing */
	UPROPERTY(Transient)
	TObjectPtr<UFootstepRules> Rules;

	/** Per character state */
	TMap<TWeakObjectPtr<const ACharacter>, FStepState> States;

	/** Time left before the dead characters are purged from States (s) */
	float PurgeTimer = 0.0f;
};
