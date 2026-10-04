// monke_island

#include "Cargo/CargoComponent.h"
#include "Cargo/CargoRules.h"
#include "Cargo/ResourceComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "monke_island.h"

UCargoComponent::UCargoComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCargoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCargoComponent, Ledger);
}

void UCargoComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (!Rules)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Cargo %s: no DA_CargoRules, using the UCargoRules defaults"), *GetNameSafe(GetOwner()));
		}
		const UCargoRules& R = GetRules();
		Ledger.Configure(R.SlotCount, R.MaxMassKg);
		OnCargoChanged.Broadcast(this);
	}
}

void UCargoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// no Stowed orphan frozen on a vanished hull: everything aboard is lost
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		LoseAll();
	}
	for (const FCargoSlot& Slot : Ledger.Slots)
	{
		if (Slot.bOccupied && IsValid(Slot.Actor))
		{
			Slot.Actor->OnDestroyed.RemoveDynamic(this, &UCargoComponent::HandleStowedActorDestroyed);
		}
	}
	Super::EndPlay(EndPlayReason);
}

const UCargoRules& UCargoComponent::GetRules() const
{
	return Rules ? *Rules : *GetDefault<UCargoRules>();
}

bool UCargoComponent::Refuse(ECargoRefusal Refusal, const AActor* Resource, int32 SlotIndex, FText& OutRefusal) const
{
	OutRefusal = FCargoLedger::GetRefusalText(Refusal);
	UE_LOG(Logmonke_island, Verbose, TEXT("Cargo %s: refused %s in slot %d (%s)"), *GetNameSafe(GetOwner()), *GetNameSafe(Resource), SlotIndex,
		*UEnum::GetValueAsString(Refusal));
	return false;
}

bool UCargoComponent::TryStow(AActor* Resource, int32 SlotIndex, FText& OutRefusal)
{
	OutRefusal = FText::GetEmpty();
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return Refuse(ECargoRefusal::NotAuthority, Resource, SlotIndex, OutRefusal);
	}

	UResourceComponent* ResourceComp = IsValid(Resource) && Resource != Owner ? Resource->FindComponentByClass<UResourceComponent>() : nullptr;
	if (!ResourceComp)
	{
		return Refuse(ECargoRefusal::InvalidResource, Resource, SlotIndex, OutRefusal);
	}

	if (!Resource->GetIsReplicated() || !Resource->IsReplicatingMovement())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Cargo %s: %s does not replicate (or not its movement), clients will not see it stowed"),
			*GetNameSafe(Owner), *GetNameSafe(Resource));
	}

	// checks without changing anything, then the distance to the resolved slot
	const FCargoEntry Entry = ResourceComp->MakeEntry();
	int32 Slot = INDEX_NONE;
	const ECargoRefusal Check = Ledger.CanStow(Entry, ResourceComp->GetState(), SlotIndex, Slot);
	if (Check != ECargoRefusal::None)
	{
		return Refuse(Check, Resource, SlotIndex, OutRefusal);
	}

	const UCargoRules& R = GetRules();
	const FTransform SlotTransform = GetSlotWorldTransform(Slot);
	if (R.MaxStowDistanceCm > 0.0f
		&& FVector::DistSquared(SlotTransform.GetLocation(), Resource->GetActorLocation()) > FMath::Square(R.MaxStowDistanceCm))
	{
		return Refuse(ECargoRefusal::TooFar, Resource, Slot, OutRefusal);
	}

	FName Socket = NAME_None;
	USceneComponent* Parent = GetSlotParent(Slot, Socket);
	if (!Parent)
	{
		return Refuse(ECargoRefusal::InvalidSlot, Resource, Slot, OutRefusal);
	}

	int32 UsedSlot = INDEX_NONE;
	const ECargoRefusal Result = Ledger.Stow(Entry, ResourceComp->GetState(), Slot, Resource, UsedSlot);
	if (!ensure(Result == ECargoRefusal::None))
	{
		return Refuse(Result, Resource, Slot, OutRefusal);
	}

	// physics off first (state Stowed), then attach to the slot
	ResourceComp->SetQueryOnlyWhenStowed(R.bQueryOnlyWhenStowed);
	ResourceComp->SetStateInternal(EResourceState::Stowed);
	ResourceComp->SetStowedIn(this);
	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, false);
	Resource->AttachToComponent(Parent, AttachRules, Socket);
	if (Socket.IsNone())
	{
		Resource->SetActorRelativeLocation(CargoLayout::ComputeSlotOffset(UsedSlot, Ledger.Slots.Num(), R.SlotColumns, R.SlotSpacingCm, R.SlotGridOrigin));
		Resource->SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	Resource->OnDestroyed.AddUniqueDynamic(this, &UCargoComponent::HandleStowedActorDestroyed);

	UE_LOG(Logmonke_island, Log, TEXT("Cargo %s: stowed %s [%s] in slot %d (%.1f / %.1f kg)"), *GetNameSafe(Owner), *GetNameSafe(Resource),
		*Entry.Id.ToString(), UsedSlot, Ledger.GetMassKg(), Ledger.MaxMassKg);
	OnCargoChanged.Broadcast(this);
	return true;
}

bool UCargoComponent::TryUnstow(int32 SlotIndex, FText& OutRefusal)
{
	OutRefusal = FText::GetEmpty();
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return Refuse(ECargoRefusal::NotAuthority, nullptr, SlotIndex, OutRefusal);
	}

	FCargoSlot Removed;
	const ECargoRefusal Result = Ledger.Unstow(SlotIndex, Removed);
	if (Result != ECargoRefusal::None)
	{
		return Refuse(Result, nullptr, SlotIndex, OutRefusal);
	}

	DetachRemoved(Removed.Actor, EResourceState::Free);

	UE_LOG(Logmonke_island, Log, TEXT("Cargo %s: unstowed %s [%s] from slot %d"), *GetNameSafe(Owner), *GetNameSafe(Removed.Actor),
		*Removed.Entry.Id.ToString(), SlotIndex);
	OnCargoChanged.Broadcast(this);
	return true;
}

void UCargoComponent::SetLocked(bool bInLocked)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || Ledger.bLocked == bInLocked)
	{
		return;
	}
	Ledger.bLocked = bInLocked;
	OnCargoChanged.Broadcast(this);
}

void UCargoComponent::SetCapacity(int32 SlotCount, float MaxMassKg)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const int32 Kept = Ledger.Configure(SlotCount, MaxMassKg);
	if (Kept != SlotCount)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Cargo %s: asked %d slots, kept %d (bounds or stowed objects)"), *GetNameSafe(GetOwner()), SlotCount, Kept);
	}
	RefreshGridPlacement();
	OnCargoChanged.Broadcast(this);
}

FCargoSnapshot UCargoComponent::FreezeForExtraction()
{
	SetLocked(true);
	return Ledger.MakeSnapshot();
}

void UCargoComponent::LoseAll()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	const TArray<FCargoSlot> Removed = Ledger.RemoveAll();
	if (Removed.IsEmpty())
	{
		return;
	}
	for (const FCargoSlot& Slot : Removed)
	{
		DetachRemoved(Slot.Actor, EResourceState::Lost);
	}
	UE_LOG(Logmonke_island, Log, TEXT("Cargo %s: %d stowed objects lost"), *GetNameSafe(GetOwner()), Removed.Num());
	OnCargoChanged.Broadcast(this);
}

bool UCargoComponent::ReleaseResource(AActor* Resource, EResourceState NewState)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || NewState == EResourceState::Stowed)
	{
		return false;
	}
	const int32 Slot = Ledger.FindSlotOfActor(Resource);
	if (Slot == INDEX_NONE)
	{
		// not (or no longer) counted here: only fix the actor
		DetachRemoved(Resource, NewState);
		return true;
	}

	FCargoSlot Removed;
	const ECargoRefusal Result = Ledger.Release(Slot, NewState == EResourceState::Lost, Removed);
	if (Result != ECargoRefusal::None)
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Cargo %s: release of %s refused (%s)"), *GetNameSafe(GetOwner()), *GetNameSafe(Resource),
			*UEnum::GetValueAsString(Result));
		return false;
	}

	DetachRemoved(Resource, NewState);
	UE_LOG(Logmonke_island, Log, TEXT("Cargo %s: released %s [%s] from slot %d as %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Resource),
		*Removed.Entry.Id.ToString(), Slot, *UEnum::GetValueAsString(NewState));
	OnCargoChanged.Broadcast(this);
	return true;
}

void UCargoComponent::DetachRemoved(AActor* Resource, EResourceState NewState)
{
	if (!IsValid(Resource))
	{
		return;
	}
	Resource->OnDestroyed.RemoveDynamic(this, &UCargoComponent::HandleStowedActorDestroyed);
	if (Resource->GetAttachParentActor() == GetOwner())
	{
		Resource->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		if (NewState != EResourceState::Lost)
		{
			Resource->AddActorWorldOffset(FVector(0.0, 0.0, GetRules().UnstowLiftCm));
		}
	}
	if (UResourceComponent* ResourceComp = Resource->FindComponentByClass<UResourceComponent>())
	{
		ResourceComp->SetStateInternal(NewState);
	}
}

void UCargoComponent::RefreshGridPlacement()
{
	const UCargoRules& R = GetRules();
	for (int32 Index = 0; Index < Ledger.Slots.Num(); ++Index)
	{
		const FCargoSlot& Slot = Ledger.Slots[Index];
		FName Socket = NAME_None;
		if (Slot.bOccupied && IsValid(Slot.Actor) && GetSlotParent(Index, Socket) && Socket.IsNone())
		{
			Slot.Actor->SetActorRelativeLocation(CargoLayout::ComputeSlotOffset(Index, Ledger.Slots.Num(), R.SlotColumns, R.SlotSpacingCm, R.SlotGridOrigin));
		}
	}
}

void UCargoComponent::HandleStowedActorDestroyed(AActor* DestroyedActor)
{
	const int32 Slot = Ledger.FindSlotOfActor(DestroyedActor);
	if (Slot == INDEX_NONE || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	UE_LOG(Logmonke_island, Warning, TEXT("Cargo %s: stowed %s destroyed, slot %d freed"), *GetNameSafe(GetOwner()), *GetNameSafe(DestroyedActor), Slot);
	Ledger.Slots[Slot] = FCargoSlot();
	OnCargoChanged.Broadcast(this);
}

void UCargoComponent::OnRep_Ledger()
{
	OnCargoChanged.Broadcast(this);
}

USceneComponent* UCargoComponent::GetSlotParent(int32 SlotIndex, FName& OutSocket) const
{
	OutSocket = NAME_None;
	const AActor* Owner = GetOwner();
	USceneComponent* Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		return nullptr;
	}
	const FName Socket(*FString::Printf(TEXT("%s%d"), *GetRules().SlotSocketPrefix, SlotIndex));
	if (Root->DoesSocketExist(Socket))
	{
		OutSocket = Socket;
	}
	return Root;
}

FTransform UCargoComponent::GetSlotWorldTransform(int32 SlotIndex) const
{
	FName Socket = NAME_None;
	const USceneComponent* Parent = GetSlotParent(SlotIndex, Socket);
	if (!Parent)
	{
		return FTransform::Identity;
	}
	if (!Socket.IsNone())
	{
		return Parent->GetSocketTransform(Socket);
	}
	const UCargoRules& R = GetRules();
	const FVector Local = CargoLayout::ComputeSlotOffset(SlotIndex, Ledger.Slots.Num(), R.SlotColumns, R.SlotSpacingCm, R.SlotGridOrigin);
	return FTransform(Parent->GetComponentQuat(), Parent->GetComponentTransform().TransformPosition(Local));
}

int32 UCargoComponent::FindClosestSlot(const FVector& WorldLocation, float MaxDistance, bool bFreeOnly) const
{
	int32 Best = INDEX_NONE;
	double BestDistSq = FMath::Square(static_cast<double>(MaxDistance));
	for (int32 Index = 0; Index < Ledger.Slots.Num(); ++Index)
	{
		if (bFreeOnly && Ledger.Slots[Index].bOccupied)
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(GetSlotWorldTransform(Index).GetLocation(), WorldLocation);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Index;
		}
	}
	return Best;
}
