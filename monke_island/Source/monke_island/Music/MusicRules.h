// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MusicRules.generated.h"

class USoundBase;

/**
 *  Music of the game (DA_MusicRules, T-220, owner request 2026-10-07; songs of OneDrive maquette-blender/audio cut and
 *  converted by scripts/audio/convert_music.py): the Hawaiian song by default (menus, islands), lowered with the
 *  daylight as the night falls until the night song takes over; the night song until dawn, or until the crew leaves the
 *  island; the British song for the minute the flag of a village rises to the trumpet. T-259 (owner 2026-10-08): the
 *  places have their own track, on every map: aboard the galleon the ship's creaking (galion_song), at a place of a
 *  boat the oar strokes (barque_song), both prepared by scripts/audio/owner_sounds_prepare.py.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UMusicRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Hawaiian song (SW_Music_Hawaiian, looping) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Songs")
	TObjectPtr<USoundBase> Hawaiian;

	/** Night song (SW_Music_Night, looping) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Songs")
	TObjectPtr<USoundBase> Night;

	/** British song of the flag (SW_Music_Crown) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Songs")
	TObjectPtr<USoundBase> Crown;

	/** Track aboard the galleon (SW_Music_Galleon, looping: the ship's creaking); none = silence aboard */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Songs")
	TObjectPtr<USoundBase> Galleon;

	/** Track at a place of a boat, seated or standing (SW_Music_Boat, looping: the oar strokes); none = silence */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Songs")
	TObjectPtr<USoundBase> Boat;

	/** Volume of the Hawaiian song at its full */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Volume", meta=(ClampMin="0", ClampMax="2"))
	float HawaiianVolume = 0.55f;

	/** Volume of the night song at its full */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Volume", meta=(ClampMin="0", ClampMax="2"))
	float NightVolume = 0.6f;

	/** Volume of the British song at its full */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Volume", meta=(ClampMin="0", ClampMax="2"))
	float CrownVolume = 0.75f;

	/** Volume of the galleon's track at its full */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Volume", meta=(ClampMin="0", ClampMax="2"))
	float GalleonVolume = 0.8f;

	/** Volume of the boat's track at its full */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Volume", meta=(ClampMin="0", ClampMax="2"))
	float BoatVolume = 0.7f;

	/** Time a song takes to come in or go out (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fades", meta=(ClampMin="0", Units="s"))
	float FadeSeconds = 2.5f;

	/** Time the night song takes to come in and to go at dawn (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fades", meta=(ClampMin="0", Units="s"))
	float NightFadeSeconds = 6.0f;

	/** The Hawaiian song is lowered with the daylight at dusk: silent at this daylight and under (0 night, 1 day) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0", ClampMax="1"))
	float DaylightSilent = 0.15f;

	/** ...and full from this daylight on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0", ClampMax="1"))
	float DaylightFull = 0.55f;

	/** The British song plays while a flag rises within this distance of the player (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flag", meta=(ClampMin="0", Units="cm"))
	float CeremonyRadius = 4000.0f;

	/** ...for at most this long (owner: one minute) (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flag", meta=(ClampMin="1", Units="s"))
	float CrownSeconds = 60.0f;

	/** Seconds between two looks at the world (night, boat, flag) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flag", meta=(ClampMin="0.05", Units="s"))
	float CheckInterval = 0.25f;
};
