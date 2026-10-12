// monke_island

#include "Capture/CaptureSubsystem.h"
#include "Capture/CaptiveComponent.h"
#include "Capture/CaptureCage.h"
#include "Capture/CaptureItem.h"
#include "Capture/CapturePrisoner.h"
#include "Capture/CaptureRules.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY(LogMonkeCapture);

UCaptureSubsystem* UCaptureSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCaptureSubsystem>() : nullptr;
}

bool UCaptureSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// game worlds only (PIE and standalone), not the editor's preview worlds
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UCaptureSubsystem::Deinitialize()
{
	Cages.Reset();
	Prisoners.Reset();
	Claims.Reset();
	Super::Deinitialize();
}

void UCaptureSubsystem::RegisterCage(ACaptureCage* Cage)
{
	if (Cage)
	{
		Cages.AddUnique(Cage);
	}
}

void UCaptureSubsystem::UnregisterCage(ACaptureCage* Cage)
{
	Cages.RemoveAll([Cage](const TWeakObjectPtr<ACaptureCage>& Entry) { return !Entry.IsValid() || Entry.Get() == Cage; });
}

void UCaptureSubsystem::RegisterPrisoner(ACapturePrisoner* Prisoner)
{
	if (Prisoner)
	{
		Prisoners.AddUnique(Prisoner);
	}
}

void UCaptureSubsystem::UnregisterPrisoner(ACapturePrisoner* Prisoner)
{
	Prisoners.RemoveAll([Prisoner](const TWeakObjectPtr<ACapturePrisoner>& Entry) { return !Entry.IsValid() || Entry.Get() == Prisoner; });
}

ACaptureCage* UCaptureSubsystem::FindCageNear(const FVector& Location, float Radius, const ACaptureCage* Ignore) const
{
	ACaptureCage* Best = nullptr;
	double BestDistance = FMath::Square(static_cast<double>(FMath::Max(Radius, 0.0f)));
	for (const TWeakObjectPtr<ACaptureCage>& Entry : Cages)
	{
		ACaptureCage* Cage = Entry.Get();
		if (!IsValid(Cage) || Cage == Ignore || Cage->IsGoing())
		{
			continue;
		}
		const double Distance = FVector::DistSquared2D(Cage->GetActorLocation(), Location);
		if (Distance <= BestDistance)
		{
			Best = Cage;
			BestDistance = Distance;
		}
	}
	return Best;
}

ACapturePrisoner* UCaptureSubsystem::FindPrisonerLedBy(const APawn* Jailer) const
{
	if (!Jailer)
	{
		return nullptr;
	}
	for (const TWeakObjectPtr<ACapturePrisoner>& Entry : Prisoners)
	{
		ACapturePrisoner* Prisoner = Entry.Get();
		const UCaptiveComponent* Captive = IsValid(Prisoner) ? Prisoner->GetCaptive() : nullptr;
		if (Captive && Captive->IsLedBy(Jailer))
		{
			return Prisoner;
		}
	}
	return nullptr;
}

ACapturePrisoner* UCaptureSubsystem::FindLedPrisonerNear(const FVector& Location, float Radius) const
{
	const double MaxDistance = FMath::Square(static_cast<double>(FMath::Max(Radius, 0.0f)));
	for (const TWeakObjectPtr<ACapturePrisoner>& Entry : Prisoners)
	{
		ACapturePrisoner* Prisoner = Entry.Get();
		const UCaptiveComponent* Captive = IsValid(Prisoner) ? Prisoner->GetCaptive() : nullptr;
		if (Captive && Captive->GetState() == ECaptiveState::Led && FVector::DistSquared2D(Prisoner->GetActorLocation(), Location) <= MaxDistance)
		{
			return Prisoner;
		}
	}
	return nullptr;
}

bool UCaptureSubsystem::ClaimTarget(const APawn* Target, const AActor* Claimer)
{
	if (!Target || !Claimer)
	{
		return false;
	}
	if (IsClaimedByOther(Target, Claimer))
	{
		return false;
	}
	Claims.Add(Target, Claimer);
	return true;
}

void UCaptureSubsystem::ReleaseClaim(const APawn* Target, const AActor* Claimer)
{
	// stale entries (a target or handcuffs gone) leave with the claim
	for (auto It = Claims.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid() || (It.Key().Get() == Target && It.Value().Get() == Claimer))
		{
			It.RemoveCurrent();
		}
	}
}

bool UCaptureSubsystem::IsClaimedByOther(const APawn* Target, const AActor* Claimer) const
{
	const TWeakObjectPtr<const AActor>* Holder = Target ? Claims.Find(Target) : nullptr;
	return Holder && Holder->IsValid() && Holder->Get() != Claimer;
}

void UCaptureSubsystem::PostNotice(const FText& Text, float Seconds, bool bWarning)
{
	const UWorld* World = GetWorld();
	if (Text.IsEmpty() || !World)
	{
		return;
	}
	NoticeText = Text;
	NoticeEndTime = World->GetTimeSeconds() + FMath::Max(Seconds, 0.1f);
	bNoticeWarning = bWarning;
	// the HUD reads it from its next look
	CachedFrame = MAX_uint64;
	UE_LOG(LogMonkeCapture, Log, TEXT("Notice: %s"), *Text.ToString());
}

bool UCaptureSubsystem::GetHudLine(const APawn* LocalPawn, FCaptureHudLine& Out) const
{
	// the widgets of the HUD read the line several times a frame: it is made once
	if (CachedFrame != GFrameCounter || CachedPawn.Get() != LocalPawn)
	{
		CachedFrame = GFrameCounter;
		CachedPawn = LocalPawn;
		bCachedLine = MakeHudLine(LocalPawn, CachedLine);
	}
	Out = CachedLine;
	return bCachedLine;
}

bool UCaptureSubsystem::MakeHudLine(const APawn* LocalPawn, FCaptureHudLine& Out) const
{
	Out = FCaptureHudLine();
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	// 1. a message of the capture, for a few seconds
	if (World->GetTimeSeconds() < NoticeEndTime && !NoticeText.IsEmpty())
	{
		Out.Text = NoticeText;
		Out.bWarning = bNoticeWarning;
		return true;
	}
	if (!LocalPawn)
	{
		return false;
	}
	// 2. the prompt of the capture item in hand (key, hold, reason)
	const UIslanderInventoryComponent* Inventory = LocalPawn->FindComponentByClass<UIslanderInventoryComponent>();
	if (const ACaptureItem* Item = Inventory ? Cast<ACaptureItem>(Inventory->GetEquippedItem()) : nullptr)
	{
		if (Item->GetPrompt(Out))
		{
			return true;
		}
	}
	// 3. a prisoner on the chain: where to take it
	if (const ACapturePrisoner* Prisoner = FindPrisonerLedBy(LocalPawn); Prisoner && Prisoner->Rules)
	{
		Out.Text = FText::Format(Prisoner->Rules->LeadingText, Prisoner->Rules->GetKindName(Prisoner->GetKind()));
		return true;
	}
	return false;
}
