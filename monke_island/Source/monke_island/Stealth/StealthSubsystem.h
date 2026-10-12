// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Stealth/StealthMath.h"
#include "Stealth/StealthTypes.h"
#include "StealthSubsystem.generated.h"

class ACharacter;
class ADayNightCycleActor;
class UStealthRules;

/** Answers whether there is a bush of the island within RadiusCm of a location (the bush grid of ATikiDirector) */
DECLARE_DELEGATE_RetVal_TwoParams(bool, FStealthCoverQuery, const FVector& /*Location*/, float /*RadiusCm*/);

/**
 *  Stealth of the players (T-262, owner request 2026-10-09; docs/features/furtivite.md). Answers the sight of the mobs
 *  and the villagers (CanSee: a crouched player, or any player at night, is seen only at contact; crouched in a bush
 *  the contact is closer still, T-207) and keeps the noises the players make (ReportNoise: running steps, voice,
 *  shots, explosions) for the mobs and villagers that listen (FindHeardNoise). Values in DA_StealthRules, loaded by
 *  path. Server authoritative: the noises live on the server only; every machine can ask CanSee and IsNight (the
 *  crouch and the clock replicate), the HUD reads the exposure of its own player.
 *  Debug (non shipping): monke.Stealth.Debug 1 logs and draws the noises, monke.Stealth.Night -1/0/1 forces the night.
 */
UCLASS()
class MONKE_ISLAND_API UStealthSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Rules loaded by default */
	static const TCHAR* DefaultRulesPath;

	/** Subsystem of the world of an object, nullptr without a game world */
	static UStealthSubsystem* Get(const UObject* WorldContext);

	/** Rules of the world of an object: its subsystem's, the class defaults without one */
	static const UStealthRules& RulesOf(const UObject* WorldContext);

	/** True when Observer sees Target within SightRangeCm (0 or less: no limit). Only the stealth rules: a crouched
	 *  player is seen only at contact, day and night, carrying an object or not; at night any player only at contact;
	 *  crouched in a bush the contact is closer still (T-207). Any other target is seen in range. No line of sight:
	 *  the caller keeps its own (walls, view cone) */
	bool CanSee(const AActor* Observer, const AActor* Target, float SightRangeCm) const;

	/** Server: a noise made by Instigator at Location, heard up to RadiusCm (clamped to the rules' maximum) by a
	 *  listener of hearing scale 1. Ignored on a client and for a radius or a location that is not a number */
	void ReportNoise(AActor* Instigator, const FVector& Location, float RadiusCm, EStealthNoise Kind);

	/** Server: the noise of a player Listener hears now (its radius times HearingScale, still fresh, never its own):
	 *  the newest, the nearest of the same age; false when none */
	bool FindHeardNoise(const AActor* Listener, float HearingScale, FStealthNoise& OutNoise) const;

	/** True at night on the island (EDayPhase::Night of ADayNightCycleActor), on every machine */
	bool IsNight() const;

	/** Rules in use (DA_StealthRules, or the class defaults when it is missing) */
	const UStealthRules& GetRules() const;

	/** Radius of a kind of noise from the rules (cm) */
	float GetNoiseRadius(EStealthNoise Kind) const;

	/** Server: ReportNoise with the radius of its kind from the rules */
	void ReportNoiseOfKind(AActor* Instigator, const FVector& Location, EStealthNoise Kind);

	/** Server: one line for a weapon or a blast: the noise of its kind at Location, when the world has the subsystem */
	static void Report(const UObject* WorldContext, AActor* Instigator, const FVector& Location, EStealthNoise Kind);

	/** CanSee from a place rather than an actor (a monster hidden in a bush): Eyes, its body radius ObserverRadiusCm */
	bool CanSeeFrom(const FVector& Eyes, float ObserverRadiusCm, const AActor* Target, float SightRangeCm) const;

	/** Server: FindHeardNoise for a listener standing at ListenerLocation (ListenerActor may be null) */
	bool FindHeardNoiseAt(const FVector& ListenerLocation, const AActor* ListenerActor, float HearingScale, FStealthNoise& OutNoise) const;

	/** Exposure of a player for its HUD: Noisy while bRunning, Hidden crouched or at night, Visible otherwise */
	EStealthExposure GetExposure(const ACharacter* Player, bool bRunning) const;

	/** The bush query of the island (ATikiDirector sets it at begin play, clears it at end play) */
	void SetCoverQuery(FStealthCoverQuery InQuery) { CoverQuery = MoveTemp(InQuery); }

	/** Unbinds the bush query when Owner bound it */
	void ClearCoverQuery(const UObject* Owner);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Deinitialize() override;

private:

	/** CanSee once the observer's place and body radius are known */
	bool CanSeeFromPlace(const FVector& Eyes, float ObserverRadiusCm, const AActor* Target, float SightRangeCm) const;

	/** True when Location is within RadiusCm of a bush (the bound query; false without one) */
	bool IsInBush(const FVector& Location, float RadiusCm) const;

	/** True when the noise maker is a player: a player pawn, or the weapon or projectile of one */
	static bool IsPlayerNoise(const AActor* Instigator);

	/** True when Actor is a pawn of a player */
	static bool IsPlayerPawn(const AActor* Actor);

	/** Rules (DA_StealthRules or the class defaults) */
	UPROPERTY(Transient)
	TObjectPtr<UStealthRules> Rules;

	/** Noises still audible, oldest first (server) */
	TArray<FStealthNoiseRecord> Noises;

	/** Bush query of the island */
	FStealthCoverQuery CoverQuery;

	/** Day / night cycle of the world, looked up again at most every two seconds while missing */
	mutable TWeakObjectPtr<ADayNightCycleActor> DayNight;
	mutable double NextDayNightLookup = 0.0;
};
