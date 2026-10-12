// monke_island

#include "Player/IslanderHealthComponent.h"
#include "Run/RunDifficulty.h"
#include "Run/RunSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"

UIslanderHealthComponent::UIslanderHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UIslanderHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Health = MaxHealth;
		Owner->OnTakeAnyDamage.AddDynamic(this, &UIslanderHealthComponent::HandleTakeAnyDamage);
		return;
	}

	// a client keeps the health the server replicated before this BeginPlay: refilled here, a wounded creature came back
	// into relevance whole and a dead one stood up again (network review R-26)
	if (!bDead && Health > 0.0f)
	{
		return;
	}
	// received dead: OnRep_Health ran before the owner bound OnDeath in its own BeginPlay, which follows this one. The
	// death is told on the next frame, once, so the body settles (a capsule that no longer blocks the way)
	bDeadOnArrival = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (bDead || Health <= 0.0f)
			{
				OnDeath.Broadcast(LastDamageCauser);
			}
		}));
	}
}

void UIslanderHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UIslanderHealthComponent, MaxHealth);
	DOREPLIFETIME(UIslanderHealthComponent, Health);
	DOREPLIFETIME(UIslanderHealthComponent, bDead);
	DOREPLIFETIME(UIslanderHealthComponent, LastDamageCauser);
	DOREPLIFETIME(UIslanderHealthComponent, DeathRecord);
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
	// the item in hand may take part of the hit (cutlass parry, T-070)
	if (const UIslanderInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UIslanderInventoryComponent>() : nullptr)
	{
		Damage = Inventory->ModifyIncomingDamage(Damage, DamageCauser);
	}
	// the difficulty of the day (owner 2026-10-09): the mobs, the soldiers and the crab hurt the players more; nothing on
	// a test map (day 0) nor between creatures
	const bool bOwnerIsPlayer = IsPlayerPawn(GetOwner());
	if (bOwnerIsPlayer)
	{
		Damage = ScaleMobDamage(Damage, URunSubsystem::DifficultyOf(this).MobDamageScale, bOwnerIsPlayer, IsMobCauser(InstigatedBy, DamageCauser));
	}
	// who struck and with what: the cause of a death (T-135)
	ApplyDamageFrom(Damage, DamageCauser, InstigatedBy, DamageType, EDeathCause::Unknown);
}

float UIslanderHealthComponent::ScaleMobDamage(float Damage, float MobDamageScale, bool bOwnerIsPlayer, bool bFromMob)
{
	return bOwnerIsPlayer && bFromMob ? Damage * FMath::Max(MobDamageScale, 0.0f) : Damage;
}

bool UIslanderHealthComponent::IsPlayerPawn(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

bool UIslanderHealthComponent::IsMobCauser(const AController* InstigatedBy, const AActor* DamageCauser)
{
	if (InstigatedBy && InstigatedBy->IsPlayerController())
	{
		return false;
	}
	// the creature itself, or the creature behind its weapon or projectile
	const APawn* Pawn = Cast<APawn>(DamageCauser);
	if (!Pawn && DamageCauser)
	{
		Pawn = DamageCauser->GetInstigator();
		if (!Pawn)
		{
			Pawn = Cast<APawn>(DamageCauser->GetOwner());
		}
	}
	return Pawn && !Pawn->IsPlayerControlled();
}

FDeathRecord UIslanderHealthComponent::MakeDeathRecord(EDeathCause Cause, const AActor* DamageCauser, const AController* InstigatedBy, const UDamageType* DamageType, float Time)
{
	FDeathRecord Record;
	Record.Time = FMath::Max(Time, 0.0f);
	Record.CauserClass = DamageCauser ? DamageCauser->GetClass() : nullptr;
	Record.DamageType = DamageType ? DamageType->GetClass() : nullptr;

	// the pawn behind the blow: the instigator's, the causer itself, or the pawn that fired or holds the causer
	const APawn* Killer = InstigatedBy ? InstigatedBy->GetPawn() : nullptr;
	if (!Killer && DamageCauser)
	{
		Killer = Cast<APawn>(DamageCauser);
		if (!Killer)
		{
			Killer = DamageCauser->GetInstigator();
		}
		if (!Killer)
		{
			Killer = Cast<APawn>(DamageCauser->GetOwner());
		}
	}
	Record.KillerClass = Killer ? Killer->GetClass() : Record.CauserClass.Get();

	// a player's blow: a teammate's weapon, one's own grenade
	if (InstigatedBy && InstigatedBy->IsPlayerController())
	{
		Record.KillerPlayer = InstigatedBy->PlayerState;
	}
	else if (Killer && Killer->IsPlayerControlled())
	{
		Record.KillerPlayer = Killer->GetPlayerState();
	}
	Record.Cause = Cause != EDeathCause::Unknown ? Cause : (Record.KillerPlayer ? EDeathCause::Player : EDeathCause::Unknown);
	return Record;
}

float UIslanderHealthComponent::ApplyHealthDamage(float Amount, AActor* DamageCauser, EDeathCause Cause)
{
	return ApplyDamageFrom(Amount, DamageCauser, nullptr, nullptr, Cause);
}

float UIslanderHealthComponent::ApplyDamageFrom(float Amount, AActor* DamageCauser, const AController* InstigatedBy, const UDamageType* DamageType, EDeathCause Cause)
{
	// CanBeDamaged off (god mode of the test map, cinematics) also stops the direct damage: falls, coconuts, drowning
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetOwner()->CanBeDamaged() || bDead || !FMath::IsFinite(Amount) || Amount <= 0.0f)
	{
		return 0.0f;
	}

	const float OldHealth = Health;
	Health = FMath::Max(0.0f, Health - Amount);
	LastDamageCauser = DamageCauser;
	if (Health <= 0.0f)
	{
		// recorded before any notification: the death screens read it with the death (T-135)
		const UWorld* World = GetWorld();
		DeathRecord = MakeDeathRecord(Cause, DamageCauser, InstigatedBy, DamageType, World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f);
	}
	OnHealthChanged.Broadcast(Health, Health - OldHealth);

	if (Health <= 0.0f)
	{
		bDead = true;
		UE_LOG(Logmonke_island, Log, TEXT("%s died (killer: %s, cause: %s, killer class: %s, by player: %s)"), *GetNameSafe(GetOwner()),
			*GetNameSafe(DamageCauser), *UEnum::GetValueAsString(DeathRecord.Cause), *GetNameSafe(DeathRecord.KillerClass.Get()),
			DeathRecord.KillerPlayer ? *DeathRecord.KillerPlayer->GetPlayerName() : TEXT("none"));
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
	LastDamageCauser = nullptr;
	DeathRecord = FDeathRecord();
	OnHealthChanged.Broadcast(Health, Health - OldHealth);
}

void UIslanderHealthComponent::ScaleMaxHealth(float Scale)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || FMath::IsNearlyEqual(Scale, 1.0f))
	{
		return;
	}
	MaxHealth = RunDifficulty::ScaledMaxHealth(MaxHealth, Scale);
	ResetHealth();
}

void UIslanderHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, Health - OldHealth);
	// received with the owner, before BeginPlay: nobody of the owner listens yet, BeginPlay tells the death (R-26)
	if (Health <= 0.0f && OldHealth > 0.0f && HasBegunPlay())
	{
		OnDeath.Broadcast(LastDamageCauser);
	}
}
