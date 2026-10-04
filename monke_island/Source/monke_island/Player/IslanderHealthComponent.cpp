// monke_island

#include "Player/IslanderHealthComponent.h"
#include "Telemetry/ExpeditionEvents.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "monke_island.h"

UIslanderHealthComponent::UIslanderHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UIslanderHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UIslanderHealthComponent::HandleTakeAnyDamage);
	}
}

void UIslanderHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UIslanderHealthComponent, Health);
	DOREPLIFETIME(UIslanderHealthComponent, DeathCause);
	DOREPLIFETIME(UIslanderHealthComponent, bDead);
	DOREPLIFETIME(UIslanderHealthComponent, LastDamageCauser);
}

float UIslanderHealthComponent::ComputeFallDamage(float LandingSpeed, float SafeSpeed, float LethalSpeed, float MaxDamage)
{
	if (LandingSpeed <= SafeSpeed)
	{
		return 0.0f;
	}
	if (LandingSpeed >= LethalSpeed || LethalSpeed <= SafeSpeed)
	{
		return MaxDamage;
	}
	return MaxDamage * (LandingSpeed - SafeSpeed) / (LethalSpeed - SafeSpeed);
}

void UIslanderHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	const UMonkeDamageType* MonkeDamage = Cast<UMonkeDamageType>(DamageType);
	ApplyHealthDamage(Damage, DamageCauser, MonkeDamage ? MonkeDamage->Cause : EDeathCause::Unknown);
}

float UIslanderHealthComponent::ApplyHealthDamage(float Amount, AActor* DamageCauser, EDeathCause Cause)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead || !FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float OldHealth = Health;
	Health = FMath::Max(0.0f, Health - Amount);
	LastDamageCauser = DamageCauser;
	if (Health <= 0.0f)
	{
		DeathCause = Cause;
	}
	OnHealthChanged.Broadcast(Health, Health - OldHealth);

	if (Health <= 0.0f)
	{
		bDead = true;
		ExpeditionEvents::LogDeath(*this, DeathCause);
		OnDeath.Broadcast(DamageCauser);
	}
	return OldHealth - Health;
}

float UIslanderHealthComponent::ComputeHeal(float CurrentHealth, float MaxHealth, float Amount)
{
	if (Amount <= 0.0f || CurrentHealth >= MaxHealth)
	{
		return 0.0f;
	}
	return FMath::Min(MaxHealth, CurrentHealth + Amount) - CurrentHealth;
}

float UIslanderHealthComponent::ApplyHeal(float Amount, AActor* Source)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || bDead)
	{
		return 0.0f;
	}

	const float Healed = ComputeHeal(Health, MaxHealth, Amount);
	if (Healed > 0.0f)
	{
		Health += Healed;
		UE_LOG(Logmonke_island, Log, TEXT("%s healed %.0f by %s"), *GetNameSafe(GetOwner()), Healed, *GetNameSafe(Source));
		OnHealthChanged.Broadcast(Health, Healed);
	}
	return Healed;
}

float UIslanderHealthComponent::ApplyFallDamage(float LandingSpeed)
{
	const float Damage = ComputeFallDamage(LandingSpeed, SafeFallSpeed, LethalFallSpeed, MaxHealth);
	if (Damage > 0.0f)
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s fall at %.0f cm/s: %.0f damage"), *GetNameSafe(GetOwner()), LandingSpeed, Damage);
	}
	return ApplyHealthDamage(Damage, nullptr, EDeathCause::Fall);
}

void UIslanderHealthComponent::ResetHealth()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	const float OldHealth = Health;
	Health = MaxHealth;
	bDead = false;
	DeathCause = EDeathCause::Unknown;
	LastDamageCauser = nullptr;
	OnHealthChanged.Broadcast(Health, Health - OldHealth);
}

void UIslanderHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, Health - OldHealth);
	if (Health <= 0.0f && OldHealth > 0.0f)
	{
		OnDeath.Broadcast(LastDamageCauser);
	}
}
