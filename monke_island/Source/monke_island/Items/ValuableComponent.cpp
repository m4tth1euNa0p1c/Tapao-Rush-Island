// monke_island

#include "Items/ValuableComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
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

	Value = BaseValue;
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
		{
			Root->SetNotifyRigidBodyCollision(true);
			Root->OnComponentHit.AddDynamic(this, &UValuableComponent::HandleHit);
		}
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
