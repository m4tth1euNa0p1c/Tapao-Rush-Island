// monke_island

#include "Cargo/CargoLedger.h"

#define LOCTEXT_NAMESPACE "Cargo"

int32 FCargoLedger::Configure(int32 SlotCount, float InMaxMassKg)
{
	const int32 Wanted = FMath::Clamp(SlotCount, 0, MaxSlotCount);
	MaxMassKg = FMath::Max(0.0f, InMaxMassKg);

	if (Slots.Num() < Wanted)
	{
		Slots.SetNum(Wanted);
	}
	else
	{
		while (Slots.Num() > Wanted && !Slots.Last().bOccupied)
		{
			Slots.Pop();
		}
	}
	return Slots.Num();
}

ECargoRefusal FCargoLedger::CanStow(const FCargoEntry& Entry, EResourceState State, int32 SlotIndex, int32& OutSlot) const
{
	OutSlot = INDEX_NONE;

	if (bLocked)
	{
		return ECargoRefusal::Locked;
	}
	if (!Entry.Id.IsValid())
	{
		return ECargoRefusal::InvalidResource;
	}
	if (State == EResourceState::Stowed || FindSlotOf(Entry.Id) != INDEX_NONE)
	{
		return ECargoRefusal::AlreadyStowed;
	}
	if (State == EResourceState::Lost)
	{
		return ECargoRefusal::ResourceLost;
	}
	if (Entry.MassKg < 0.0f || Entry.Quantity <= 0 || Entry.Value < 0)
	{
		return ECargoRefusal::InvalidResource;
	}

	int32 Slot = SlotIndex;
	if (Slot == INDEX_NONE)
	{
		Slot = FindFreeSlot();
		if (Slot == INDEX_NONE)
		{
			return ECargoRefusal::NoFreeSlot;
		}
	}
	else if (!Slots.IsValidIndex(Slot))
	{
		return ECargoRefusal::InvalidSlot;
	}
	else if (Slots[Slot].bOccupied)
	{
		return FindFreeSlot() == INDEX_NONE ? ECargoRefusal::NoFreeSlot : ECargoRefusal::SlotTaken;
	}

	if (GetMassKg() + Entry.MassKg > MaxMassKg + KINDA_SMALL_NUMBER)
	{
		return ECargoRefusal::TooHeavy;
	}

	OutSlot = Slot;
	return ECargoRefusal::None;
}

ECargoRefusal FCargoLedger::Stow(const FCargoEntry& Entry, EResourceState State, int32 SlotIndex, AActor* Actor, int32& OutSlot)
{
	const ECargoRefusal Refusal = CanStow(Entry, State, SlotIndex, OutSlot);
	if (Refusal != ECargoRefusal::None)
	{
		return Refusal;
	}

	FCargoSlot& Slot = Slots[OutSlot];
	Slot.bOccupied = true;
	Slot.Entry = Entry;
	Slot.Actor = Actor;
	return ECargoRefusal::None;
}

ECargoRefusal FCargoLedger::Release(int32 SlotIndex, bool bIgnoreLock, FCargoSlot& OutSlot)
{
	OutSlot = FCargoSlot();
	if (bLocked && !bIgnoreLock)
	{
		return ECargoRefusal::Locked;
	}
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return ECargoRefusal::InvalidSlot;
	}
	if (!Slots[SlotIndex].bOccupied)
	{
		return ECargoRefusal::SlotEmpty;
	}

	OutSlot = Slots[SlotIndex];
	Slots[SlotIndex] = FCargoSlot();
	return ECargoRefusal::None;
}

TArray<FCargoSlot> FCargoLedger::RemoveAll()
{
	TArray<FCargoSlot> Removed;
	for (FCargoSlot& Slot : Slots)
	{
		if (Slot.bOccupied)
		{
			Removed.Add(Slot);
			Slot = FCargoSlot();
		}
	}
	return Removed;
}

int32 FCargoLedger::FindSlotOf(const FResourceId& Id) const
{
	if (!Id.IsValid())
	{
		return INDEX_NONE;
	}
	return Slots.IndexOfByPredicate([&Id](const FCargoSlot& Slot) { return Slot.bOccupied && Slot.Entry.Id == Id; });
}

int32 FCargoLedger::FindSlotOfActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return INDEX_NONE;
	}
	return Slots.IndexOfByPredicate([Actor](const FCargoSlot& Slot) { return Slot.bOccupied && Slot.Actor == Actor; });
}

int32 FCargoLedger::FindFreeSlot() const
{
	return Slots.IndexOfByPredicate([](const FCargoSlot& Slot) { return !Slot.bOccupied; });
}

int32 FCargoLedger::GetFreeSlotCount() const
{
	int32 Count = 0;
	for (const FCargoSlot& Slot : Slots)
	{
		Count += Slot.bOccupied ? 0 : 1;
	}
	return Count;
}

float FCargoLedger::GetMassKg() const
{
	float Mass = 0.0f;
	for (const FCargoSlot& Slot : Slots)
	{
		if (Slot.bOccupied)
		{
			Mass += Slot.Entry.MassKg;
		}
	}
	return Mass;
}

FCargoTotals FCargoLedger::ComputeTotals() const
{
	FCargoTotals Totals;
	for (const FCargoSlot& Slot : Slots)
	{
		if (Slot.bOccupied)
		{
			Totals.Add(Slot.Entry);
		}
	}
	return Totals;
}

FCargoSnapshot FCargoLedger::MakeSnapshot() const
{
	FCargoSnapshot Snapshot;
	for (const FCargoSlot& Slot : Slots)
	{
		if (Slot.bOccupied)
		{
			Snapshot.Entries.Add(Slot.Entry);
			Snapshot.Totals.Add(Slot.Entry);
		}
	}
	return Snapshot;
}

FText FCargoLedger::GetRefusalText(ECargoRefusal Refusal)
{
	// TODO(String Table, ERR-08): move to ST_Cargo once Content is available in this branch
	switch (Refusal)
	{
	case ECargoRefusal::None:				return FText::GetEmpty();
	case ECargoRefusal::NotAuthority:		return LOCTEXT("NotAuthority", "Action refusée par l'hôte.");
	case ECargoRefusal::Locked:				return LOCTEXT("Locked", "Cargaison verrouillée : la barque est en mouvement.");
	case ECargoRefusal::InvalidResource:	return LOCTEXT("InvalidResource", "Cet objet ne peut pas être arrimé.");
	case ECargoRefusal::InvalidSlot:		return LOCTEXT("InvalidSlot", "Aucun emplacement ici.");
	case ECargoRefusal::SlotTaken:			return LOCTEXT("SlotTaken", "Emplacement occupé : visez un emplacement libre.");
	case ECargoRefusal::SlotEmpty:			return LOCTEXT("SlotEmpty", "Rien à désarrimer ici.");
	case ECargoRefusal::NoFreeSlot:			return LOCTEXT("NoFreeSlot", "Barque pleine : plus aucun emplacement libre.");
	case ECargoRefusal::TooHeavy:			return LOCTEXT("TooHeavy", "Barque trop chargée : cet objet dépasse la masse maximale.");
	case ECargoRefusal::AlreadyStowed:		return LOCTEXT("AlreadyStowed", "Cet objet est déjà arrimé.");
	case ECargoRefusal::ResourceLost:		return LOCTEXT("ResourceLost", "Cet objet est perdu.");
	case ECargoRefusal::TooFar:				return LOCTEXT("TooFar", "Trop loin de l'emplacement.");
	}
	return LOCTEXT("Unknown", "Arrimage impossible.");
}

FVector CargoLayout::ComputeSlotOffset(int32 SlotIndex, int32 SlotCount, int32 Columns, const FVector2D& Spacing, const FVector& Origin)
{
	const int32 SafeColumns = FMath::Max(1, Columns);
	const int32 SafeCount = FMath::Max(1, SlotCount);
	const int32 Index = FMath::Clamp(SlotIndex, 0, SafeCount - 1);
	const int32 Rows = FMath::DivideAndRoundUp(SafeCount, SafeColumns);
	const int32 Row = Index / SafeColumns;
	const int32 Column = Index % SafeColumns;

	const double X = (static_cast<double>(Row) - (Rows - 1) * 0.5) * Spacing.X;
	const double Y = (static_cast<double>(Column) - (SafeColumns - 1) * 0.5) * Spacing.Y;
	return Origin + FVector(X, Y, 0.0);
}

#undef LOCTEXT_NAMESPACE
