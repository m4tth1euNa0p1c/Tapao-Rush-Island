// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WeatherSubsystem.generated.h"

class AWeatherDirector;

/**
 *  Weather of a game world (T-203): on the server (listen server or standalone) it spawns the replicated
 *  AWeatherDirector with DA_WeatherRules when the world begins play; on every machine it knows the director (which
 *  registers itself, a client's when it arrives by replication) and answers what the turned weather gives the wind and
 *  the sky, as pure functions of the replicated state and the server clock.
 *  Debug (not in Shipping, server): monke.Weather.Turn turns the weather as a seized artifact would, monke.Weather.Clear
 *  clears it.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UWeatherSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	UWeatherSubsystem();

	/** The subsystem of the world of an object, or nullptr */
	static UWeatherSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Called by the director when it begins play on this machine */
	void RegisterDirector(AWeatherDirector* InDirector);

	/** Called by the director when it ends play */
	void UnregisterDirector(AWeatherDirector* InDirector);

	/** Director of this world (nullptr before it spawned or arrived) */
	AWeatherDirector* GetDirector() const { return Director.Get(); }

	/** Weight of the turned weather now (0 clear, 1 turned) */
	float GetTurnWeight() const;

	/** Mean wind strength the turned weather keeps the wind over now (0 when clear) */
	float GetWindFloor() const;

	/** Storm weight the turned weather gives the sky now (0-1) */
	float GetSkyFactor() const;

private:

	/** DA_WeatherRules (DefaultGame.ini [/Script/monke_island.WeatherSubsystem] RulesPath; this default otherwise) */
	UPROPERTY(Config)
	FSoftObjectPath RulesPath;

	/** Director of this world */
	TWeakObjectPtr<AWeatherDirector> Director;
};
