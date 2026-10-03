// monke_island

#include "Cargo/ResourceComponent.h"
#include "Cargo/CargoComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Items/ValuableComponent.h"
#include "Net/UnrealNetwork.h"
#include "monke_island.h"

UResourceComponent::UResourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UResourceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UResourceComponent, ResourceId);
	DOREPLIFETIME(UResourceComponent, State);
}

void UResourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority() && !ResourceId.IsValid())
	{
		ResourceId = FResourceId::NewId();
	}
}

float UResourceComponent::GetEffectiveMassKg() const
{
	if (MassKg > 0.0f)
	{
		return MassKg;
	}
	const AActor* Owner = GetOwner();
	const UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	return Root && Root->GetBodyInstance() ? Root->GetBodyInstance()->GetBodyMass() : 0.0f;
}

int32 UResourceComponent::GetCurrentValue() const
{
	const AActor* Owner = GetOwner();
	if (const UValuableComponent* Valuable = Owner ? Owner->FindComponentByClass<UValuableComponent>() : nullptr)
	{
		return Valuable->GetValue();
	}
	return Value;
}

FCargoEntry UResourceComponent::MakeEntry()
{
	if (!ResourceId.IsValid() && GetOwner() && GetOwner()->HasAuthority())
	{
		ResourceId = FResourceId::NewId();
	}

	FCargoEntry Entry;
	Entry.Id = ResourceId;
	Entry.Category = Category;
	Entry.MassKg = GetEffectiveMassKg();
	Entry.Value = GetCurrentValue();
	Entry.Quantity = Quantity;
	return Entry;
}

void UResourceComponent::SetState(EResourceState NewState)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Resource %s: SetState refused off the server"), *GetNameSafe(GetOwner()));
		return;
	}
	if (State == NewState || State == EResourceState::Lost)
	{
		return;
	}

	const EResourceState OldState = State;
	State = NewState;
	if (NewState != EResourceState::Stowed)
	{
		StowedIn.Reset();
	}
	ApplyStatePhysics(OldState);
	OnStateChanged.Broadcast(State, OldState);
}

void UResourceComponent::OnRep_State(EResourceState OldState)
{
	ApplyStatePhysics(OldState);
	OnStateChanged.Broadcast(State, OldState);
}

void UResourceComponent::ApplyStatePhysics(EResourceState OldState)
{
	AActor* Owner = GetOwner();
	UPrimitiveComponent* Root = Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
	if (!Root)
	{
		return;
	}

	if (State == EResourceState::Stowed && !bPhysicsSuspended)
	{
		bSavedSimulate = Root->IsSimulatingPhysics();
		SavedCollision = Root->GetCollisionEnabled();
		bPhysicsSuspended = true;
		Root->SetSimulatePhysics(false);
		if (bQueryOnlyWhenStowed)
		{
			Root->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}
	else if (State != EResourceState::Stowed && bPhysicsSuspended)
	{
		bPhysicsSuspended = false;
		Root->SetCollisionEnabled(SavedCollision);
		if (State != EResourceState::Lost)
		{
			Root->SetSimulatePhysics(bSavedSimulate);
		}
	}
}

UCargoComponent* UResourceComponent::GetStowedIn() const
{
	return StowedIn.Get();
}

void UResourceComponent::SetStowedIn(UCargoComponent* Cargo)
{
	StowedIn = Cargo;
}
