// monke_island

#include "Capture/HandcuffsItem.h"
#include "Capture/CaptureMath.h"
#include "Capture/CapturePrisoner.h"
#include "Capture/CaptureRules.h"
#include "Capture/CaptureSubsystem.h"
#include "Capture/CaptureTargets.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** The pawns around the eyes are looked for this much beyond the reach (their capsule, not their middle, is near) */
	constexpr float TargetSearchMarginCm = 60.0f;
}

AHandcuffsItem::AHandcuffsItem()
{
}

void AHandcuffsItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AHandcuffsItem, Prisoner);
}

void AHandcuffsItem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		CancelCuff();
	}
	Super::EndPlay(EndPlayReason);
}

UStaticMesh* AHandcuffsItem::GetHeldMesh() const
{
	return Rules ? Rules->OpenHandcuffsMesh.Get() : nullptr;
}

FTransform AHandcuffsItem::GetHeldGrip() const
{
	return Rules ? Rules->HandcuffsInHand : FTransform::Identity;
}

bool AHandcuffsItem::ShouldShowHeldMesh() const
{
	// on the wrists of a prisoner, the hand holds its chain; closed on the wrists at CuffsClosed of the gesture
	const bool bClosed = Rules && GetGesture() == ECaptureGesture::Cuff && GetGestureProgress() >= Rules->CuffsClosedShare;
	return IsEquipped() && !IsInUse() && !bClosed;
}

// ---------------------------------------------------------------- targets

ECaptureRefusal AHandcuffsItem::JudgeTarget(const ACharacter* Target, bool bServer, float& OutHealthFraction) const
{
	OutHealthFraction = 1.0f;
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Rules || !Carrier)
	{
		return ECaptureRefusal::CarrierUnable;
	}
	const UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	CaptureMath::FCuffFacts Facts;
	Facts.bCarrierAble = IsUsable();
	Facts.bHandcuffsInUse = IsInUse();
	Facts.bLeading = Capture && Capture->FindPrisonerLedBy(Carrier);
	Facts.bHasTarget = IsValid(Target);
	if (Facts.bHasTarget)
	{
		Facts.bCapturable = CaptureTargets::IsCapturable(Target, *Rules);
		bool bAlive = false;
		Facts.HealthFraction = CaptureTargets::HealthFractionOf(Target, bAlive);
		OutHealthFraction = Facts.HealthFraction;
		Facts.bDown = CaptureTargets::IsDown(Target);
		// the target's body within reach of the eyes and nothing in between (the server adds the latency tolerance)
		const UIslanderGrabComponent* Grab = Carrier->GetGrabComponent();
		FVector Eyes = Carrier->GetPawnViewLocation();
		FVector Direction = FVector::ForwardVector;
		if (Grab)
		{
			Grab->GetEyes(Eyes, Direction);
		}
		const float Reach = Rules->CuffReachCm + (bServer ? Rules->ServerReachToleranceCm : 0.0f);
		Facts.bInReach = UIslanderGrabComponent::IsActorInReach(GetWorld(), Target, Carrier, this, Eyes, Reach);
		Facts.bClaimedByOther = bServer && Capture && Capture->IsClaimedByOther(Target, this);
	}
	return CaptureMath::JudgeCuff(Facts, Rules->SubduedHealthFraction, Rules->bDownIsSubdued);
}

ACharacter* AHandcuffsItem::FindTarget() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const UWorld* World = GetWorld();
	if (!Rules || !Carrier || !World)
	{
		return nullptr;
	}
	FVector Eyes = Carrier->GetPawnViewLocation();
	FVector Direction = Carrier->GetActorForwardVector();
	if (const UIslanderGrabComponent* Grab = Carrier->GetGrabComponent())
	{
		Grab->GetEyes(Eyes, Direction);
	}
	// along the active camera: what the crosshair shows, in first and third person
	if (const APlayerController* Controller = Cast<APlayerController>(Carrier->GetController()))
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		Direction = ViewRotation.Vector();
	}
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CuffTargets), false, Carrier);
	World->OverlapMultiByObjectType(Overlaps, Eyes, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Rules->CuffReachCm + TargetSearchMarginCm), Params);
	ACharacter* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACharacter* Candidate = Cast<ACharacter>(Overlap.GetActor());
		// the players and the prisoners already taken are no targets
		if (!Candidate || Candidate == Carrier || Candidate->IsPlayerControlled() || Candidate->IsA<ACapturePrisoner>())
		{
			continue;
		}
		const FVector ToTarget = Candidate->GetActorLocation() - Eyes;
		const float Angle = CaptureMath::AimAngleDeg(Direction, ToTarget);
		if (Angle > Rules->CuffAimAngleDeg)
		{
			continue;
		}
		const float Score = CaptureMath::TargetScore(Angle, static_cast<float>(ToTarget.Size()), Rules->CuffReachCm);
		if (Score < BestScore)
		{
			Best = Candidate;
			BestScore = Score;
		}
	}
	return Best;
}

// ---------------------------------------------------------------- owner

void AHandcuffsItem::RefreshLocal()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	// the crosshair on something to use (a pickup, the cage to sell in): the interaction keeps the key
	const UIslanderGrabComponent* Grab = Carrier ? Carrier->GetGrabComponent() : nullptr;
	AActor* Interactable = nullptr;
	UPrimitiveComponent* Physics = nullptr;
	FVector Point;
	bInteractableInView = Grab && (Grab->IsHolding() || (Grab->FindInteractionTarget(Interactable, Physics, Point) && Interactable));
	// a hold under way keeps its target
	if (bLocalHolding)
	{
		return;
	}
	ACharacter* Target = IsInUse() ? nullptr : FindTarget();
	LocalTarget = Target;
	LocalKind = CaptureTargets::KindOf(Target);
	LocalRefusal = JudgeTarget(Target, false, LocalHealth);
}

bool AHandcuffsItem::WantsUseKey() const
{
	// the key stays mapped as long as it is held (its release ends the hold)
	if (bLocalHolding || IsUseKeyDown())
	{
		return true;
	}
	return IsUsable() && !IsInUse() && LocalTarget.IsValid() && LocalRefusal == ECaptureRefusal::None && !bInteractableInView
		&& GetGesture() == ECaptureGesture::None;
}

bool AHandcuffsItem::GetPrompt(FCaptureHudLine& Out) const
{
	if (!Rules || !IsEquipped())
	{
		return false;
	}
	if (GetGesture() == ECaptureGesture::Cuff)
	{
		Out.Text = Rules->CuffingText;
		Out.Progress = GetGestureProgress();
		return true;
	}
	if (bLocalHolding)
	{
		// asked, the server's start on its way
		Out.Text = Rules->CuffingText;
		Out.Progress = CaptureMath::Progress(GetWorld()->GetTimeSeconds(), LocalHoldStart, Rules->CuffHoldSeconds);
		return true;
	}
	if (IsInUse())
	{
		// its own prisoner: the reminder of the subsystem says where to take it; someone else's: say so
		const UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
		if (Capture && Capture->FindPrisonerLedBy(GetCarrier()))
		{
			return false;
		}
		Out.Text = Rules->HandcuffsInUseText;
		return true;
	}
	if (bInteractableInView || !LocalTarget.IsValid())
	{
		return false;
	}
	if (LocalRefusal == ECaptureRefusal::None)
	{
		Out.Text = FText::Format(Rules->CuffPromptText, Rules->GetUseKeyText(), Rules->GetKindName(LocalKind));
		return true;
	}
	// a target in front that cannot be cuffed yet: why (not subdued: its health against the threshold)
	Out.Text = Rules->GetRefusalText(LocalRefusal, LocalHealth);
	Out.bWarning = true;
	return !Out.Text.IsEmpty();
}

void AHandcuffsItem::OnUsePressed()
{
	ACharacter* Target = LocalTarget.Get();
	if (bLocalHolding || !Target || !WantsUseKey())
	{
		return;
	}
	bLocalHolding = true;
	LocalHoldStart = GetWorld()->GetTimeSeconds();
	if (HasAuthority())
	{
		ServerBeginCuff_Implementation(Target);
	}
	else
	{
		ServerBeginCuff(Target);
	}
}

void AHandcuffsItem::OnUseReleased()
{
	if (!bLocalHolding)
	{
		return;
	}
	bLocalHolding = false;
	if (HasAuthority())
	{
		ServerCancelCuff_Implementation();
	}
	else
	{
		ServerCancelCuff();
	}
}

void AHandcuffsItem::OnEquippedChanged()
{
	Super::OnEquippedChanged();
	if (IsEquipped())
	{
		return;
	}
	bLocalHolding = false;
	// put away or dropped during the hold
	if (HasAuthority())
	{
		CancelCuff();
	}
}

// ---------------------------------------------------------------- server

void AHandcuffsItem::ServerBeginCuff_Implementation(ACharacter* Target)
{
	if (!Rules || !ConsumeRequest(TEXT("cuff")))
	{
		return;
	}
	if (CuffTarget.IsValid() || GetGesture() != ECaptureGesture::None)
	{
		Refuse(ECaptureRefusal::Busy);
		return;
	}
	// everything again from the server's own world (the client only proposed its target)
	float Health = 1.0f;
	const ECaptureRefusal Refusal = JudgeTarget(Target, true, Health);
	if (Refusal != ECaptureRefusal::None)
	{
		Refuse(Refusal, Health);
		return;
	}
	UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	if (Capture && !Capture->ClaimTarget(Target, this))
	{
		Refuse(ECaptureRefusal::Busy);
		return;
	}
	CuffTarget = Target;
	StartGesture(ECaptureGesture::Cuff, Rules->CuffHoldSeconds);
	GetWorldTimerManager().SetTimer(CuffTimer, this, &AHandcuffsItem::CompleteCuff, FMath::Max(Rules->CuffHoldSeconds, 0.05f), false);
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: cuffing %s (health %.0f %%)"), *GetNameSafe(GetCarrier()), *GetNameSafe(Target), Health * 100.0f);
}

void AHandcuffsItem::ServerCancelCuff_Implementation()
{
	// a release only stops what runs: never rate limited (a refused release would let the cuffing end anyway)
	CancelCuff();
}

void AHandcuffsItem::CompleteCuff()
{
	ACharacter* Target = CuffTarget.Get();
	// checked again at the end: still subdued, alive, near and in sight, the player still able
	float Health = 1.0f;
	const ECaptureRefusal Refusal = Target ? JudgeTarget(Target, true, Health) : ECaptureRefusal::NoTarget;
	ReleaseClaim();
	CuffTarget.Reset();
	StopGesture();
	if (Refusal != ECaptureRefusal::None)
	{
		Refuse(Refusal, Health);
		return;
	}
	ACapturePrisoner* NewPrisoner = ACapturePrisoner::CaptureFrom(Target, GetCarrier(), this, Rules);
	if (!NewPrisoner)
	{
		Refuse(ECaptureRefusal::NoTarget);
		return;
	}
	Prisoner = NewPrisoner;
	ForceNetUpdate();
	NotifyCarrier(FText::Format(Rules->CapturedText, Rules->GetKindName(NewPrisoner->GetKind())), false);
}

void AHandcuffsItem::CancelCuff()
{
	if (!CuffTarget.IsValid() && !GetWorldTimerManager().IsTimerActive(CuffTimer))
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(CuffTimer);
	ReleaseClaim();
	CuffTarget.Reset();
	StopGesture();
}

void AHandcuffsItem::ReleaseClaim()
{
	if (UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this))
	{
		Capture->ReleaseClaim(CuffTarget.Get(), this);
	}
}

void AHandcuffsItem::ReleasePrisoner(const ACapturePrisoner* InPrisoner)
{
	if (!HasAuthority() || !InPrisoner || Prisoner.Get() != InPrisoner)
	{
		return;
	}
	// free again: the handcuffs come back after the sale (owner 2026-10-11, reusable)
	Prisoner = nullptr;
	ForceNetUpdate();
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: the handcuffs of %s are free again"), *GetName(), *GetNameSafe(GetCarrier()));
}
