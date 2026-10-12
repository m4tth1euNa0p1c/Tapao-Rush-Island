// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FirearmTracer.generated.h"

class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Look of the tracer rounds of a weapon (owner 2026-10-10: "crée et ajoute les projectiles des balles des différentes
 *  armes, les traces des balles comme des munitions traçantes"): the round itself and a glowing streak behind it, flying
 *  from the visible muzzle to where the shot ends. The shots stay instant on the server (hitscan): the tracer is only
 *  what every machine draws, slower than a real ball so that the eye follows it.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FFirearmTracerLook
{
	GENERATED_BODY()

	/** Tracers drawn for this weapon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer")
	bool bEnabled = true;

	/** One tracer every N rounds (1: every round; the pellets of a shot count one by one) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer", meta=(ClampMin="1"))
	int32 EveryNth = 1;

	/** Speed of the drawn round (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer", meta=(ClampMin="1000", Units="cm/s"))
	float SpeedCmps = 22000.0f;

	/** Length of the glowing streak behind the round (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer", meta=(ClampMin="1", Units="cm"))
	float LengthCm = 120.0f;

	/** Thickness of the streak (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer", meta=(ClampMin="0.1", Units="cm"))
	float WidthCm = 2.2f;

	/** Colour of the glow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer")
	FLinearColor Color = FLinearColor(1.0f, 0.55f, 0.18f);

	/** Brightness of the glow (emissive multiplier) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer", meta=(ClampMin="0"))
	float Intensity = 30.0f;

	/** Longest flight drawn (cm): a shot into the sky stops there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer", meta=(ClampMin="100", Units="cm"))
	float MaxDistanceCm = 25000.0f;

	/** The streak: a mesh standing along +Z, 100 cm high and 100 cm wide, centred (/Engine/BasicShapes/Cylinder) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer")
	TObjectPtr<UStaticMesh> StreakMesh;

	/** Unlit additive material of the streak and the round: colour from custom primitive data 0-2, brightness from 3
	 *  (M_Tracer) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer")
	TObjectPtr<UMaterialInterface> StreakMaterial;

	/** The round at the head of the streak (the lead ball); none: the streak only */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer")
	TObjectPtr<UStaticMesh> RoundMesh;

	/** Size of the drawn round (cm, across): the round mesh is scaled to it whatever its own size */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tracer", meta=(ClampMin="0.1", Units="cm"))
	float RoundSizeCm = 3.0f;
};

/** Pure rules of the tracers (tested by monke_island.Weapons.Tracer) */
namespace FirearmTracerMath
{
	/** True for the rounds that carry a tracer: the 1st, then every Nth (Counter counts the rounds fired so far) */
	inline bool IsTracerRound(int32 Counter, int32 EveryNth)
	{
		return EveryNth <= 1 || (Counter % EveryNth) == 0;
	}

	/** Where a drawn round ends: the hit, or the end of the shot cut to the longest flight drawn */
	inline FVector FlightEnd(const FVector& From, const FVector& To, float MaxDistanceCm)
	{
		const FVector Delta = To - From;
		const float Length = Delta.Size();
		return Length > MaxDistanceCm && Length > UE_KINDA_SMALL_NUMBER ? From + Delta * (MaxDistanceCm / Length) : To;
	}

	/** Length of the streak behind the head after Travelled cm: it grows out of the muzzle, then keeps its length */
	inline float StreakLength(float Travelled, float LengthCm)
	{
		return FMath::Clamp(Travelled, 0.0f, LengthCm);
	}
}

/**
 *  One tracer round in flight (every machine that draws, never a dedicated server; not replicated): a glowing streak
 *  and the round, moved from the muzzle to the end of the shot, then gone. Spawned by AFirearmTracer::Launch from the
 *  shot multicast of the firearms and the French soldiers' muskets.
 */
UCLASS(NotBlueprintable, Transient)
class MONKE_ISLAND_API AFirearmTracer : public AActor
{
	GENERATED_BODY()

public:

	AFirearmTracer();

	/** Spawns a tracer from From to To with the look of a weapon; nothing on a dedicated server, nor without a mesh */
	static AFirearmTracer* Launch(UWorld* World, const FFirearmTracerLook& Look, const FVector& From, const FVector& To);

	virtual void Tick(float DeltaSeconds) override;

private:

	/** Places the streak and the round for the distance flown */
	void Place();

	UPROPERTY()
	TObjectPtr<USceneComponent> Root;

	/** Glowing streak */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Streak;

	/** The round at its head */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Round;

	FVector Start = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	float Distance = 0.0f;
	float Travelled = 0.0f;
	float Speed = 22000.0f;
	float StreakLengthCm = 120.0f;
	float StreakWidthCm = 2.2f;
};
