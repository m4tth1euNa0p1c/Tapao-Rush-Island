#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "DeathCause.generated.h"

/** Cause of the fatal hit; Unknown also denotes a living character. */
UENUM(BlueprintType)
enum class EDeathCause : uint8
{
	Unknown,
	Fall,
	Drowning,
	Coconut,
	Tiki,
	Shark
};

/** Damage metadata shared by hazards and health, without concrete actor dependencies. */
UCLASS()
class MONKE_ISLAND_API UMonkeDamageType : public UDamageType
{
	GENERATED_BODY()
public:
	/** Source category, not a gameplay damage value. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Damage")
	EDeathCause Cause = EDeathCause::Unknown;
};

UCLASS()
class MONKE_ISLAND_API UTikiDamageType : public UMonkeDamageType
{
	GENERATED_BODY()
public:
	UTikiDamageType() { Cause = EDeathCause::Tiki; }
};

UCLASS()
class MONKE_ISLAND_API USharkDamageType : public UMonkeDamageType
{
	GENERATED_BODY()
public:
	USharkDamageType() { Cause = EDeathCause::Shark; }
};

/** Stable machine-readable name; unknown enum values never emit arbitrary text. */
inline const TCHAR* DeathCauseName(EDeathCause Cause)
{
	switch (Cause)
	{
	case EDeathCause::Fall: return TEXT("fall");
	case EDeathCause::Drowning: return TEXT("drowning");
	case EDeathCause::Coconut: return TEXT("coconut");
	case EDeathCause::Tiki: return TEXT("tiki");
	case EDeathCause::Shark: return TEXT("shark");
	default: return TEXT("unknown");
	}
}
