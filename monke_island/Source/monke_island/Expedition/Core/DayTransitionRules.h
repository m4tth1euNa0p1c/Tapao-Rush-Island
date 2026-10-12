// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DayTransitionRules.generated.h"

/**
 *  The end and the start of a day of the campaign (DA_DayTransitionRules, T-214, owner request 2026-10-07: "à la fin
 *  d'un jour, dès que le compteur de jour est fini, un décompteur final et une animation, on va partir, le bateau au
 *  loin, ensuite jour 2 animation arrivée vers une nouvelle zone, et début du jour"). Local presentation only: the
 *  server keeps the day, the travel and its delay (DA_ExpeditionRules.DayEndTravelDelay).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UDayTransitionRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Departure: the galleon sails off at this speed (cm/s), reached at this acceleration (cm/s²) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0"))
	float SailSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0"))
	float SailAcceleration = 180.0f;

	/** Where the camera watches it sail from, in the galleon's frame at the departure (cm: X ahead, Y right, Z up) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure")
	FVector DepartureCamera = FVector(-3500.0, 6500.0, 1800.0);

	/** Blend of the view to the camera (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float DepartureBlendSeconds = 1.5f;

	/** Fade to black before the travel (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float FadeSeconds = 1.5f;

	/**
	 *  Arrival: the crew starts on the deck of the galleon at anchor; the camera comes in from the open sea, from there in
	 *  the galleon's frame (cm: X out to sea, the island behind at -X; Y right; Z up), to the player's eyes over
	 *  ArrivalSeconds
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival")
	FVector ArrivalCamera = FVector(6500.0, 2500.0, 2400.0);

	/** Where the glide stops, outside the hull and the rigging (galleon frame, cm); the view then cuts to the player's eyes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival")
	FVector ArrivalCameraEnd = FVector(2600.0, 1500.0, 1300.0);

	/** Without a galleon in the map: the camera starts there in the frame of the boat the player sits in (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival")
	FVector ArrivalBoatCamera = FVector(-5000.0, 0.0, 2800.0);

	/** ... and its glide stops there (boat frame, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival")
	FVector ArrivalBoatCameraEnd = FVector(-900.0, 0.0, 450.0);

	/** The cut from the glide to the player's eyes: to black over this time, and back from it over 1.5 times it (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival", meta=(ClampMin="0.05", Units="s"))
	float ArrivalCutSeconds = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival", meta=(ClampMin="0.5", Units="s"))
	float ArrivalSeconds = 5.0f;

	/** "Jour N" in the middle of the screen at the arrival (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival", meta=(ClampMin="0", Units="s"))
	float TitleSeconds = 4.0f;

	/**
	 *  No arrival scene when the player is neither on the deck nor in the boat by then (s). While a galleon is awaited
	 *  (a galleon spawner in the map) the screen stays black, so the boat start and the move to the deck are not seen
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival", meta=(ClampMin="0", Units="s"))
	float ArrivalWaitSeconds = 12.0f;

	/** The first frames of an island are heavy (villages built, villagers coming out, navigation built): the arrival
	 *  waits in the black until the frames come faster than ArrivalSettleFrameMs, at least ArrivalSettleMinSeconds and
	 *  at most ArrivalSettleMaxSeconds after the loading screen (owner 2026-10-07: "le jeu lag lors des cinématiques") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival", meta=(ClampMin="1", Units="ms"))
	float ArrivalSettleFrameMs = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival", meta=(ClampMin="0", Units="s"))
	float ArrivalSettleMinSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival", meta=(ClampMin="0", Units="s"))
	float ArrivalSettleMaxSeconds = 6.0f;

	/** The screen comes back from black over this time when the arrival starts (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arrival", meta=(ClampMin="0", Units="s"))
	float ArrivalFadeInSeconds = 1.2f;
};
