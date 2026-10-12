// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Core/DeathCause.h"
#include "Engine/DataAsset.h"
#include "DeathRules.generated.h"

class APlayerState;

/** A short cause of death told for the blows of an actor class (cahier §19) */
USTRUCT(BlueprintType)
struct FDeathSourceText
{
	GENERATED_BODY()

	/** Class of the pawn or actor behind the blow (a creature, a soldier, a hazard), its subclasses included: the most
	 *  derived entry that matches wins, so a Blueprint entry is more precise than its native class */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death")
	TSoftClassPtr<AActor> Source;

	/** Only for this damage type and its subclasses; any damage when empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death")
	TSoftClassPtr<UDamageType> DamageType;

	/** The cause, e.g. "Écrasé par Tupa Nui" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Death")
	FText Text;
};

/**
 *  Death of a player and spectator (T-135, docs/features/death-spectator.md): DA_DeathRules in
 *  /Game/MonkeIsland/UI/Tapao, read by UDeathSpectatorComponent (the class defaults when the asset is missing).
 *  How long "TU ES MORT" stays, how the camera follows a living teammate, and the short causes of death told to the
 *  dead player (cahier §19), by the cause the blow gave, then by the class behind the blow.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UDeathRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UDeathRules();

	/** Time "TU ES MORT" and the cause stay before the camera follows a living teammate (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flow", meta=(ClampMin="0", Units="s"))
	float DeathScreenSeconds = 4.0f;

	/** A click of the dead player skips the death screen only after this time, so the cause stays readable (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flow", meta=(ClampMin="0", Units="s"))
	float SkipDeathScreenAfter = 1.5f;

	/** Time the camera stays on a followed teammate who just died, before it goes to the next one (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flow", meta=(ClampMin="0", Units="s"))
	float FollowedDeathSeconds = 2.5f;

	/** Shortest time between two changes of the followed teammate asked by a dead player (s, checked by the server) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flow", meta=(ClampMin="0.05", Units="s"))
	float MinCycleInterval = 0.25f;

	/** Period of the server's look at the living teammates while its player is dead (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flow", meta=(ClampMin="0.1", Units="s"))
	float RefreshSeconds = 0.5f;

	/** Follow camera: distance behind the followed teammate (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin="50", Units="cm"))
	float FollowDistance = 380.0f;

	/** Follow camera: height of the point it looks at, above the teammate's origin (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(Units="cm"))
	float FollowHeight = 60.0f;

	/** Follow camera: pitch (deg, negative looks down on the teammate) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin="-80", ClampMax="30", Units="deg"))
	float FollowPitch = -15.0f;

	/** Follow camera: radius of the probe that keeps it out of walls and rocks (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0", Units="cm"))
	float FollowProbeRadius = 12.0f;

	/** Follow camera: speed at which it turns with the teammate's view (higher follows faster, 0 turns at once) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin="0"))
	float FollowRotationSpeed = 6.0f;

	/** Follow camera: field of view (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera", meta=(ClampMin="30", ClampMax="120", Units="deg"))
	float FollowFieldOfView = 85.0f;

	/** Short causes by the cause the blow gave (a fall, drowning, the deluge, a coconut...) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Texts")
	TMap<EDeathCause, FText> CauseTexts;

	/** Short causes by the class behind the blow (creatures, soldiers, hazards), when the blow gave no cause */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Texts")
	TArray<FDeathSourceText> SourceTexts;

	/** A teammate's blow; {Killer} is his name */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Texts")
	FText PlayerKillText;

	/** {Killer} when the teammate who struck has left the game */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Texts")
	FText TeammateName;

	/** One's own blow (a grenade, the torch) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Texts")
	FText SelfKillText;

	/** Nothing known of the blow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Texts")
	FText UnknownText;

	/** Short cause of a death for the dead player Victim (cahier §19); pure: the record and these texts only */
	FText DescribeDeath(const FDeathRecord& Record, const APlayerState* Victim) const;

	/** Entry of SourceTexts whose class is the nearest parent of Class (an entry of the damage type before one for any
	 *  damage); nullptr when none, or when Class is none */
	const FDeathSourceText* FindSourceText(const UClass* Class, const UClass* InDamageType) const;

	/** Point the follow camera looks at, above a teammate standing at PawnLocation */
	FVector FollowPivot(const FVector& PawnLocation) const;

	/** Place of the follow camera behind a teammate standing at PawnLocation and looking along Yaw (before the probe) */
	FVector FollowCameraLocation(const FVector& PawnLocation, float Yaw) const;
};
