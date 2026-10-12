// monke_island

// The characters around the giant crab (T-328). Its hit bodies follow the clip of each machine and, at head height
// (claws, legs, shell), they pressed the capsules down onto the sand: "is stuck and failed to move", normal Z -0.98, a
// push no move can undo. On every machine the moves of the characters around it now ignore those bodies; its upright
// pillars stop them instead (UGiantCrabBodyComponent). The server frees a character still pinned in its shapes. The
// hunter (targets, approach, blows, clips) is in GiantCrab.cpp.

#include "Creatures/GiantCrab.h"
#include "Creatures/GiantCrabBody.h"
#include "Creatures/GiantCrabInternal.h"
#include "Creatures/GiantCrabMath.h"
#include "Creatures/GiantCrabRules.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

void AGiantCrab::UpdatePawns(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!Rules || !Bodies || !World)
	{
		return;
	}
	const FGiantCrabPawnRules& PawnRules = Rules->Pawns;
	PawnClock += DeltaSeconds;
	if (PawnClock < FMath::Max(0.05f, PawnRules.CheckSeconds))
	{
		return;
	}
	const float Step = PawnClock;
	PawnClock = 0.0f;
	// nothing pins anyone while it rises through the sand, its collision off
	const bool bFree = HasAuthority() && PawnRules.bFreePinned && !bRising;
	if (!bFree)
	{
		PinnedTimes.Reset();
	}
	if (!PawnRules.bIgnoreAnimatedBodies && !bFree)
	{
		return;
	}

	// the characters around it, whatever their side (object type Pawn: their capsules and meshes; its own bodies left out)
	TArray<FOverlapResult> Overlaps;
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(GiantCrabPawns), false, this);
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(FMath::Max(100.0f, PawnRules.CheckRadiusCm)), Params);
	TArray<ACharacter*, TInlineAllocator<16>> Near;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACharacter* Character = Cast<ACharacter>(Overlap.GetActor());
		if (Character && IsValid(Character) && !Character->IsA<AGiantCrab>())
		{
			Near.AddUnique(Character);
		}
	}

	// every machine: their moves (predicted or not) ignore its animated bodies, its pillars stop them
	if (PawnRules.bIgnoreAnimatedBodies)
	{
		for (ACharacter* Character : Near)
		{
			if (UGiantCrabBodyComponent::IgnoreAnimatedBodies(*Character))
			{
				UE_LOG(Logmonke_island, Verbose, TEXT("GiantCrab %s: the moves of %s ignore its animated bodies"), *GetName(), *Character->GetName());
			}
		}
	}
	if (!bFree)
	{
		return;
	}

	// server: how long each one has been pressed into its shapes; the long-pinned ones are freed
	for (auto It = PinnedTimes.CreateIterator(); It; ++It)
	{
		ACharacter* Known = It.Key().Get();
		if (!Known || !Near.Contains(Known))
		{
			It.RemoveCurrent();
		}
	}
	const ACharacter* Held = HeldPrey;
	for (ACharacter* Character : Near)
	{
		// the one in its claw, a ragdoll, a body whose movement is off and the dead are left where they are
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		const UIslanderHealthComponent* Life = Character->FindComponentByClass<UIslanderHealthComponent>();
		const bool bMovable = Character != Held && Move && Move->MovementMode != MOVE_None && !IsKnockedOut(Character) && !(Life && Life->IsDead());
		FVector Away = FVector::ZeroVector;
		FName Shape = NAME_None;
		const float Depth = bMovable ? Bodies->FindPin(*Character, Away, Shape) : 0.0f;
		const float Before = PinnedTimes.FindRef(Character);
		const float Time = GiantCrabMath::PinnedTime(Before, Depth, PawnRules.PinDepthCm, Step);
		if (Time <= 0.0f)
		{
			PinnedTimes.Remove(Character);
		}
		else if (Time >= PawnRules.PinnedSeconds && FreePinnedPawn(Character, Away, Depth, Shape, Time, Before < PawnRules.PinnedSeconds))
		{
			PinnedTimes.Remove(Character);
		}
		else
		{
			// still counting, or no free spot yet: tried again at the next look
			PinnedTimes.Add(Character, Time);
		}
	}
}

bool AGiantCrab::FreePinnedPawn(ACharacter* Pawn, const FVector& Away, float Depth, FName Shape, float PinnedSeconds, bool bReport)
{
	UWorld* World = GetWorld();
	UCapsuleComponent* Capsule = Pawn ? Pawn->GetCapsuleComponent() : nullptr;
	if (!World || !Capsule || !Rules)
	{
		return false;
	}
	const FGiantCrabPawnRules& PawnRules = Rules->Pawns;
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector From = Pawn->GetActorLocation();
	const FVector Center = GetActorLocation();

	// the spots tried: just out of the shape when it pushes level (a pillar), then out of its footprint, straight away
	// from it, then turned each side in turn all the way round
	TArray<FVector2D, TInlineAllocator<16>> Spots;
	if (GiantCrabMath::IsLevelPush(Away, GiantCrabInternal::LevelPushShare))
	{
		Spots.Add(FVector2D(From) + FVector2D(Away).GetSafeNormal() * (Depth + PawnRules.NudgeMarginCm));
	}
	const float Distance = GiantCrabMath::FreeSpotDistance(static_cast<float>(FVector::Dist2D(From, Center)), PawnRules.ClearRadiusCm + Radius,
		PawnRules.NudgeMarginCm);
	const FVector2D Back(-GetActorForwardVector());
	const int32 Tries = GiantCrabMath::FreeSpotTries(PawnRules.AngleStepDegrees);
	for (int32 Try = 0; Try < Tries; ++Try)
	{
		Spots.Add(GiantCrabMath::FreeSpot(FVector2D(Center), FVector2D(From), Back, Distance, GiantCrabMath::FreeSpotAngle(Try, PawnRules.AngleStepDegrees)));
	}

	// a spot inside its ground, on walkable ground under the character's head, where its capsule touches nothing that
	// stops it (its own move rules: the bodies it ignores stay ignored)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GiantCrabFreeSpot), false, Pawn);
	FCollisionResponseParams Response;
	Capsule->InitSweepCollisionParams(Params, Response);
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(GiantCrabFreeGround), false, Pawn);
	GroundParams.AddIgnoredActor(this);
	const FCollisionShape Body = Capsule->GetCollisionShape(1.0f);
	const FQuat Upright = Capsule->GetComponentQuat();
	const UCharacterMovementComponent* Move = Pawn->GetCharacterMovement();
	const float WalkableZ = Move ? Move->GetWalkableFloorZ() : 0.7f;
	for (const FVector2D& Spot2D : Spots)
	{
		FVector Spot(Spot2D.X, Spot2D.Y, From.Z);
		if (!IsInTerritory(Spot))
		{
			continue;
		}
		FHitResult Ground;
		const FVector Top = Spot + FVector(0.0f, 0.0f, HalfHeight);
		const FVector Bottom = Spot - FVector(0.0f, 0.0f, HalfHeight + PawnRules.GroundSearchCm);
		if (!World->LineTraceSingleByObjectType(Ground, Top, Bottom, FCollisionObjectQueryParams(ECC_WorldStatic), GroundParams)
			|| Ground.bStartPenetrating || Ground.ImpactNormal.Z < WalkableZ)
		{
			continue;
		}
		Spot.Z = Ground.ImpactPoint.Z + HalfHeight + GiantCrabInternal::FreeSpotLiftCm;
		if (World->OverlapBlockingTestByChannel(Spot, Upright, Capsule->GetCollisionObjectType(), Body, Params, Response))
		{
			continue;
		}
		if (!Pawn->TeleportTo(Spot, Pawn->GetActorRotation(), false, true))
		{
			continue;
		}
		UE_LOG(Logmonke_island, Log, TEXT("GiantCrab %s: frees %s, pinned %.1f s in %s (%.0f cm deep), moved %.0f cm"), *GetName(), *Pawn->GetName(),
			PinnedSeconds, *Shape.ToString(), Depth, FVector::Dist2D(From, Spot));
		return true;
	}
	if (bReport)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("GiantCrab %s: no free spot around it for %s, pinned %.1f s in %s (%.0f cm deep), tried again"), *GetName(),
			*Pawn->GetName(), PinnedSeconds, *Shape.ToString(), Depth);
	}
	return false;
}
