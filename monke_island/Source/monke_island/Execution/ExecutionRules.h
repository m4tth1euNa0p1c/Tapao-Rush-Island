// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExecutionRules.generated.h"

class AFortSoldier;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;

/**
 *  The execution of a crew that missed the quota on the last day of its contract (owner request 2026-10-07: "si quota
 *  pas atteint au jour jalon, alors animation des beans mis sur une planche et soldats français les fusillent";
 *  DA_ExecutionRules, AExecutionScene). Places in the frame of the galleon's hull (cm: X across the ship, Y along it,
 *  Z up from the waterline); times in seconds from the end of the day.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UExecutionRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** "Quota non atteint" over the end of the day, before the scene (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float TitleSeconds = 3.5f;

	/** The crew is put on the plank and the squad comes on deck, in the black, at this time (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.5", Units="s"))
	float PlaceAtSeconds = 4.0f;

	/** Fades to and from black around the cuts (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.1", Units="s"))
	float FadeSeconds = 0.8f;

	/** The squad shoulders its muskets (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float AimAtSeconds = 7.0f;

	/** The volley (s), each musket a little after the other within VolleySpreadSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float FireAtSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float VolleySpreadSeconds = 0.3f;

	/** The picture fades to black and the end screen comes (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float EndAtSeconds = 15.5f;

	/** Drum roll from the place time to the volley (none: silence) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> DrumSound;

	/** The plank: its mesh (a 1 m cube is stretched to PlankSize when none) and material */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plank")
	TObjectPtr<UStaticMesh> PlankMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plank")
	TObjectPtr<UMaterialInterface> PlankMaterial;

	/** Inner end of the plank (top of the board resting on the bulwark: the main deck of the galleon is at 162 cm, its rail at
	 *  240 cm, its side at 330 cm), and the way it runs out over the sea (hull frame) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plank")
	FVector PlankStart = FVector(240.0, 0.0, 248.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plank")
	FVector PlankDirection = FVector(1.0, 0.0, 0.0);

	/** Length, width and thickness of the plank (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plank")
	FVector PlankSize = FVector(480.0, 45.0, 8.0);

	/** Where the first of the crew stands along the plank, and the room between two of them (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plank", meta=(ClampMin="0", Units="cm"))
	float FirstSpotCm = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plank", meta=(ClampMin="30", Units="cm"))
	float SpotSpacingCm = 80.0f;

	/** Uniforms of the squad, one soldier each (BP_BeanFR_*), in line abreast */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Squad")
	TArray<TSubclassOf<AFortSoldier>> SquadClasses;

	/** Middle of the line of soldiers on the deck (hull frame, feet), and the room between two of them (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Squad")
	FVector SquadCentre = FVector(-110.0, 0.0, 162.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Squad", meta=(ClampMin="40", Units="cm"))
	float SquadSpacingCm = 100.0f;

	/** The bodies thrown off the plank by the balls: speed along the shot and upward (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Squad", meta=(ClampMin="0"))
	float PushSpeed = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Squad", meta=(ClampMin="0"))
	float PushUpSpeed = 220.0f;

	/** The camera glides from CameraStart to CameraEnd between the place time and the end, looking at CameraLookAt
	 *  (hull frame; by default over the shoulders of the squad, the crew facing it with the sea behind) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
	FVector CameraStart = FVector(-560.0, -360.0, 480.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
	FVector CameraEnd = FVector(-420.0, -250.0, 430.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
	FVector CameraLookAt = FVector(430.0, 0.0, 290.0);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="20", ClampMax="120", Units="deg"))
	float CameraFieldOfView = 55.0f;
};
