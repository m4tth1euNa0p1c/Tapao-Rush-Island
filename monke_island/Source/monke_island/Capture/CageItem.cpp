// monke_island

#include "Capture/CageItem.h"
#include "Capture/CaptureCage.h"
#include "Capture/CaptureMath.h"
#include "Capture/CaptureRules.h"
#include "Capture/CaptureSubsystem.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Water/IslandWaterSubsystem.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** The probes of the sea start this high above the highest beach and go this deep under the sea level (cm) */
	constexpr float ShoreProbeAboveCm = 200.0f;
	constexpr float ShoreProbeBelowCm = 2000.0f;
}

ACageItem::ACageItem()
{
}

void ACageItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACageItem, bDeployed);
}

UStaticMesh* ACageItem::GetHeldMesh() const
{
	return Rules ? Rules->FoldedCageMesh.Get() : nullptr;
}

FTransform ACageItem::GetHeldGrip() const
{
	return Rules ? Rules->FoldedCageInHand : FTransform::Identity;
}

bool ACageItem::ShouldShowHeldMesh() const
{
	// out of the hand once it stands on the beach
	return IsEquipped() && !bDeployed;
}

bool ACageItem::GetHeldOverride(FTransform& OutWorld) const
{
	// from CrateDown to CagePlaced the folded cage lies on the sand in front of the player (T-337 cage_place.crate_down)
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!Rules || !Carrier || GetGesture() != ECaptureGesture::PlaceCage || GetGestureElapsed() < Rules->PlaceCrateDownSeconds)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Carrier->GetCapsuleComponent();
	const FVector Feet = Carrier->GetActorLocation() - FVector(0.0, 0.0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 55.0f);
	OutWorld = Rules->CrateDownSpot * FTransform(FRotator(0.0f, Carrier->GetActorRotation().Yaw, 0.0f), Feet);
	return true;
}

// ---------------------------------------------------------------- the spot

ECaptureRefusal ACageItem::CheckRoom(const FTransform& Spot) const
{
	const UWorld* World = GetWorld();
	if (!Rules || !World)
	{
		return ECaptureRefusal::NoRoom;
	}
	const UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	if (Capture && Capture->FindCageNear(Spot.GetLocation(), Rules->MinCageSpacingCm))
	{
		return ECaptureRefusal::CageTooClose;
	}
	// a box of the cage lifted off its floor (a slope never fills it): walls, trees, rocks, boats and bodies stop it, the
	// small loose objects are pushed aside
	const FVector Half(Rules->CageHalfExtentCm.X, Rules->CageHalfExtentCm.Y,
		FMath::Max(static_cast<float>(Rules->CageHalfExtentCm.Z) - 0.5f * Rules->ClearanceLiftCm, 10.0f));
	const FVector Center = Spot.GetLocation() + FVector(0.0, 0.0, Rules->ClearanceLiftCm + Half.Z);
	FCollisionObjectQueryParams Blockers;
	Blockers.AddObjectTypesToQuery(ECC_WorldStatic);
	Blockers.AddObjectTypesToQuery(ECC_WorldDynamic);
	Blockers.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CageRoom), false, GetCarrier());
	Params.AddIgnoredActor(this);
	return World->OverlapAnyTestByObjectType(Center, Spot.GetRotation(), Blockers, FCollisionShape::MakeBox(Half), Params)
		? ECaptureRefusal::NoRoom : ECaptureRefusal::None;
}

ECaptureRefusal ACageItem::FindSpot(FTransform& OutSpot) const
{
	OutSpot = FTransform::Identity;
	const AIslanderCharacter* Carrier = GetCarrier();
	const UWorld* World = GetWorld();
	if (!Rules || !Carrier || !World)
	{
		return ECaptureRefusal::CarrierUnable;
	}
	CaptureMath::FCageSpotFacts Facts;
	Facts.bCarrierAble = IsUsable();

	// in front of the player (its body follows the view in first person), on the ground
	const float Yaw = Carrier->GetActorRotation().Yaw;
	const UCapsuleComponent* Capsule = Carrier->GetCapsuleComponent();
	const FVector Feet = Carrier->GetActorLocation() - FVector(0.0, 0.0, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 55.0f);
	const FVector Ahead = Feet + FRotator(0.0f, Yaw, 0.0f).Vector() * Rules->PlaceDistanceCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CageSpot), false, Carrier);
	Params.AddIgnoredActor(this);
	// the land itself (the beach, the terrain): no boat, deck, crate or body
	const FCollisionObjectQueryParams Ground(ECC_WorldStatic);
	FHitResult Floor;
	if (World->LineTraceSingleByObjectType(Floor, Ahead + FVector(0.0, 0.0, Rules->FloorTraceUpCm), Ahead - FVector(0.0, 0.0, Rules->FloorTraceDownCm),
		Ground, Params))
	{
		const UPrimitiveComponent* Component = Floor.GetComponent();
		Facts.bFloor = Component && !Component->IsSimulatingPhysics() && !Cast<APawn>(Floor.GetActor());
		Facts.FloorZ = static_cast<float>(Floor.ImpactPoint.Z);
		Facts.FloorNormalZ = static_cast<float>(Floor.ImpactNormal.Z);
	}

	// the sea near: a seabed under the sea level somewhere around the spot (never a map without sea)
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	Facts.SeaLevel = Water ? Water->GetSeaLevel() : 0.0f;
	if (Water && Facts.bFloor && CaptureMath::IsBeachHeight(Facts.FloorZ, Facts.SeaLevel, Rules->MinHeightAboveSeaCm, Rules->MaxHeightAboveSeaCm))
	{
		const int32 Count = CaptureMath::ShoreProbeCount(Rules->ShoreProbeDirections, Rules->ShoreProbeRings);
		const float Top = Facts.SeaLevel + Rules->MaxHeightAboveSeaCm + ShoreProbeAboveCm;
		for (int32 Index = 0; Index < Count && !Facts.bShoreNear; ++Index)
		{
			const FVector2D Offset = CaptureMath::ShoreProbeOffset(Index, Rules->ShoreProbeDirections, Rules->ShoreProbeRings, Rules->MaxShoreDistanceCm);
			const FVector Probe(Ahead.X + Offset.X, Ahead.Y + Offset.Y, Top);
			FHitResult Bed;
			if (World->LineTraceSingleByObjectType(Bed, Probe, FVector(Probe.X, Probe.Y, Facts.SeaLevel - ShoreProbeBelowCm), Ground, Params))
			{
				Facts.bShoreNear = CaptureMath::IsUnderSea(static_cast<float>(Bed.ImpactPoint.Z), Facts.SeaLevel, Rules->ShoreMinDepthCm);
			}
		}
	}

	// the cage turned its door toward the player, on its floor
	OutSpot = FTransform(FRotator(0.0f, Yaw + Rules->CageYawOffsetDeg, 0.0f), FVector(Ahead.X, Ahead.Y, Facts.FloorZ));
	if (Facts.bFloor)
	{
		const ECaptureRefusal Room = CheckRoom(OutSpot);
		Facts.bCageNear = Room == ECaptureRefusal::CageTooClose;
		Facts.bClear = Room == ECaptureRefusal::None;
	}
	return CaptureMath::JudgeCageSpot(Facts, Rules->MinHeightAboveSeaCm, Rules->MaxHeightAboveSeaCm, Rules->MaxSlopeDeg);
}

// ---------------------------------------------------------------- owner

void ACageItem::RefreshLocal()
{
	FTransform Spot;
	LocalRefusal = FindSpot(Spot);
	// the crosshair on something to use (a pickup, the boat, a cage): the interaction keeps the key
	const AIslanderCharacter* Carrier = GetCarrier();
	const UIslanderGrabComponent* Grab = Carrier ? Carrier->GetGrabComponent() : nullptr;
	AActor* Interactable = nullptr;
	UPrimitiveComponent* Physics = nullptr;
	FVector Point;
	bInteractableInView = Grab && (Grab->IsHolding() || (Grab->FindInteractionTarget(Interactable, Physics, Point) && Interactable));
}

bool ACageItem::WantsUseKey() const
{
	return IsUsable() && LocalRefusal == ECaptureRefusal::None && !bInteractableInView && !bDeployed && GetGesture() == ECaptureGesture::None;
}

bool ACageItem::GetPrompt(FCaptureHudLine& Out) const
{
	if (!Rules || !IsEquipped())
	{
		return false;
	}
	if (GetGesture() == ECaptureGesture::PlaceCage)
	{
		Out.Text = Rules->PlacingText;
		Out.Progress = GetGestureProgress();
		return true;
	}
	// the interaction's own prompt (the crosshair on something to use)
	if (bInteractableInView || bDeployed)
	{
		return false;
	}
	if (LocalRefusal == ECaptureRefusal::None)
	{
		Out.Text = FText::Format(Rules->PlacePromptText, Rules->GetUseKeyText());
		return true;
	}
	Out.Text = Rules->GetRefusalText(LocalRefusal);
	Out.bWarning = true;
	return !Out.Text.IsEmpty();
}

void ACageItem::OnUsePressed()
{
	if (!WantsUseKey())
	{
		return;
	}
	if (HasAuthority())
	{
		ServerPlaceCage_Implementation();
	}
	else
	{
		ServerPlaceCage();
	}
}

// ---------------------------------------------------------------- server

void ACageItem::ServerPlaceCage_Implementation()
{
	if (!Rules || !ConsumeRequest(TEXT("place a cage")))
	{
		return;
	}
	if (bPlacing || bDeployed || GetGesture() != ECaptureGesture::None)
	{
		Refuse(ECaptureRefusal::Busy);
		return;
	}
	// the server's own spot, from where its carrier stands (the client only asked)
	FTransform Spot;
	const ECaptureRefusal Refusal = FindSpot(Spot);
	if (Refusal != ECaptureRefusal::None)
	{
		Refuse(Refusal);
		return;
	}
	PendingSpot = Spot;
	bPlacing = true;
	StartGesture(ECaptureGesture::PlaceCage, Rules->PlaceGestureSeconds);
	const float Seconds = FMath::Max(Rules->PlaceGestureSeconds, 0.05f);
	GetWorldTimerManager().SetTimer(DeployTimer, this, &ACageItem::DeployCage, FMath::Clamp(Rules->PlaceEventSeconds, 0.01f, Seconds), false);
	GetWorldTimerManager().SetTimer(FinishTimer, this, &ACageItem::FinishPlacing, Seconds, false);
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: sets a cage down at %s"), *GetNameSafe(GetCarrier()), *Spot.GetLocation().ToCompactString());
}

void ACageItem::DeployCage()
{
	if (!bPlacing || !Rules)
	{
		return;
	}
	// still able, and the spot still free (another cage set meanwhile, a player stepped in): the first one wins
	ECaptureRefusal Refusal = IsUsable() ? CheckRoom(PendingSpot) : ECaptureRefusal::CarrierUnable;
	ACaptureCage* Cage = nullptr;
	if (Refusal == ECaptureRefusal::None)
	{
		Cage = ACaptureCage::PlaceCage(GetWorld(), Rules, PendingSpot, GetCarrier(), GetDefinition());
		Refusal = Cage ? ECaptureRefusal::None : ECaptureRefusal::NoRoom;
	}
	if (!Cage)
	{
		Refuse(Refusal);
		CancelPlacing();
		return;
	}
	bDeployed = true;
	ForceNetUpdate();
}

void ACageItem::FinishPlacing()
{
	GetWorldTimerManager().ClearTimer(DeployTimer);
	GetWorldTimerManager().ClearTimer(FinishTimer);
	bPlacing = false;
	StopGesture();
	if (!bDeployed)
	{
		return;
	}
	bDeployed = false;
	ForceNetUpdate();
	// the cage stands on the beach: the item leaves the inventory (it comes back when the cage goes); last: it may destroy
	// this actor
	AIslanderCharacter* Carrier = GetCarrier();
	UIslanderInventoryComponent* Inventory = Carrier ? Carrier->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	if (!Inventory || !Inventory->RemoveItem(this))
	{
		UE_LOG(LogMonkeCapture, Warning, TEXT("%s: the cage stands but the item was in no inventory, it goes"), *GetName());
		Destroy();
	}
}

void ACageItem::CancelPlacing()
{
	if (!bPlacing)
	{
		return;
	}
	// the cage already stands: the gesture ends now, the item goes
	if (bDeployed)
	{
		FinishPlacing();
		return;
	}
	GetWorldTimerManager().ClearTimer(DeployTimer);
	GetWorldTimerManager().ClearTimer(FinishTimer);
	bPlacing = false;
	StopGesture();
}

void ACageItem::OnEquippedChanged()
{
	Super::OnEquippedChanged();
	// put away or dropped while setting the cage down
	if (HasAuthority() && !IsEquipped() && bPlacing)
	{
		CancelPlacing();
	}
}
