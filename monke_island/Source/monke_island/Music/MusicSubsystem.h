// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Music/MusicMath.h"
#include "MusicSubsystem.generated.h"

class UAudioComponent;
class UMusicRules;
class USoundBase;

/**
 *  The music of the local player (T-220): one 2D sound per track, kept across the maps (menus, galleon, islands), whose
 *  volumes fade toward the track MusicMath::PickTrack chooses from the world: the day / night cycle of the island, the
 *  boat or the galleon the player is aboard (their own tracks since T-259, on every map), a flag of a village rising
 *  nearby. A track silent for good is paused (it goes on from there), the British song starts over at each flag. Values
 *  in DA_MusicRules. Not on a dedicated server.
 */
UCLASS(Config=Game)
class MONKE_ISLAND_API UMusicSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:

	UMusicSubsystem();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual UWorld* GetTickableGameObjectWorld() const override;

	/** Track chosen now */
	EMusicTrack GetTrack() const { return Wanted; }

private:

	/** Looks at the world: the song wanted, and the daylight factor of the Hawaiian song */
	void ReadWorld(UWorld* World, double Now);

	/** The sound of a song, started (paused when silent) */
	UAudioComponent* GetComponent(EMusicTrack Track, UWorld* World);

	/** DA_MusicRules (set in DefaultGame.ini or here) */
	UPROPERTY(Config)
	FSoftObjectPath MusicRulesPath;

	UPROPERTY(Transient)
	TObjectPtr<UMusicRules> Rules;

	/** The sound of each song (kept across the maps) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Components;

	/** Volume of each track now (0-1, before its full volume) */
	float Volumes[static_cast<int32>(EMusicTrack::Count)] = {};

	/** Song wanted, the daylight factor of the Hawaiian song, the British song's start (world time, -1 none) */
	EMusicTrack Wanted = EMusicTrack::Hawaiian;

	/** The local player set foot on the island since it last left the galleon or the boat (expedition maps) */
	bool bLanded = false;

	/** World of bLanded: a new map starts unlanded */
	TWeakObjectPtr<UWorld> LandedWorld;
	float DaylightFactor = 1.0f;
	double CrownStart = -1.0;
	bool bCeremonyWasNear = false;

	double NextCheck = 0.0;
	bool bReady = false;
};
