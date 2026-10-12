// monke_island

#include "Items/ValuableComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "Run/RunSubsystem.h"
#include "monke_island.h"

UValuableComponent::UValuableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UValuableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UValuableComponent, Value);
}

void UValuableComponent::BeginPlay()
{
	Super::BeginPlay();

	// a client keeps the value the server replicated, received before this BeginPlay: its BaseValue lacks the zone
	// multiplier and the losses, and the server sends Value again only when it changes (network review R-07)
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}
	// ADR-007 §11: the loot of a further zone, or of a greed day, is worth more; a value restored (T-309) already is
	if (!bValueRestored)
	{
		if (const URunSubsystem* Run = URunSubsystem::Get(this))
		{
			BaseValue = FMath::Max(0, FMath::RoundToInt(BaseValue * Run->GetLootValueScale()));
		}
		Value = BaseValue;
	}
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent()))
	{
		Root->SetNotifyRigidBodyCollision(true);
		Root->OnComponentHit.AddDynamic(this, &UValuableComponent::HandleHit);
	}
}

void UValuableComponent::RestoreValue(int32 InBaseValue, int32 InValue)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const int32 OldValue = Value;
	BaseValue = FMath::Max(0, InBaseValue);
	Value = FMath::Clamp(InValue, 0, BaseValue);
	bValueRestored = true;
	// before BeginPlay nobody listens yet; the clients get Value with the object
	if (HasBegunPlay() && Value != OldValue)
	{
		OnValueChanged.Broadcast(Value, OldValue - Value);
	}
}

int32 UValuableComponent::ComputeImpactLoss(float ImpactSpeed, float SafeSpeed, float HeavySpeed, float MaxLossFraction, int32 InBaseValue)
{
	if (ImpactSpeed <= SafeSpeed || InBaseValue <= 0)
	{
		return 0;
	}
	const float Alpha = HeavySpeed > SafeSpeed ? FMath::Clamp((ImpactSpeed - SafeSpeed) / (HeavySpeed - SafeSpeed), 0.0f, 1.0f) : 1.0f;
	return FMath::Max(1, FMath::RoundToInt(InBaseValue * MaxLossFraction * Alpha));
}

void UValuableComponent::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Value <= 0 || Now - LastHitTime < HitCooldown || !HitComponent)
	{
		return;
	}

	// relative speed along the contact normal
	FVector RelativeVelocity = HitComponent->GetPhysicsLinearVelocity();
	if (OtherComp && OtherComp->IsSimulatingPhysics())
	{
		RelativeVelocity -= OtherComp->GetPhysicsLinearVelocity();
	}
	const float ImpactSpeed = static_cast<float>(FMath::Abs(FVector::DotProduct(RelativeVelocity, Hit.ImpactNormal)));
	const int32 Loss = ComputeImpactLoss(ImpactSpeed, SafeImpactSpeed, HeavyImpactSpeed, MaxLossPerHit, BaseValue);
	if (Loss > 0)
	{
		LastHitTime = Now;
		ApplyValueLoss(Loss);
	}
}

int32 UValuableComponent::ApplyValueLoss(int32 Loss)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Loss <= 0 || Value <= 0)
	{
		return 0;
	}

	const int32 OldValue = Value;
	Value = FMath::Max(0, Value - Loss);
	OnValueChanged.Broadcast(Value, OldValue - Value);
	UE_LOG(Logmonke_island, Log, TEXT("%s loses %d (value %d)"), *GetNameSafe(GetOwner()), OldValue - Value, Value);

	if (Value == 0 && bBreakAtZero)
	{
		OnBroken.Broadcast();
		GetOwner()->SetLifeSpan(0.05f);
	}
	return OldValue - Value;
}

void UValuableComponent::OnRep_Value(int32 OldValue)
{
	OnValueChanged.Broadcast(Value, OldValue - Value);
}
