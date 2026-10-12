// monke_island

// Network of the giant crab (T-304, T-305; review T-268 R-23 to R-25, R-31, R-33): the one held at its claw, its rise
// out of the sand, the fixed shapes that stop the pawns, the age of its actions, the latency tolerance of its weak
// points. The hunter (targets, approach, blows, clips) is in GiantCrab.cpp, the boss moves and the damage in
// GiantCrabMoves.cpp.

#include "Creatures/GiantCrab.h"
#include "Creatures/GiantCrabBody.h"
#include "Creatures/GiantCrabInternal.h"
#include "Creatures/GiantCrabLag.h"
#include "Creatures/GiantCrabRules.h"
#include "Player/IslanderCharacter.h"
#include "monke_island.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

// ---------------------------------------------------------------- the one held at the claw (T-304, R-23, R-33)

bool AGiantCrab::IsKnockedOut(const AActor* Actor)
{
	const AIslanderCharacter* Player = Cast<AIslanderCharacter>(Actor);
	return Player && Player->IsRagdoll();
}

void AGiantCrab::HoldAtClaw(ACharacter* Held) const
{
	USkeletalMeshComponent* CrabMesh = GetMesh();
	if (!Held || !CrabMesh)
	{
		return;
	}
	const FVector Claw = CrabMesh->DoesSocketExist(GiantCrabInternal::GrabSocket) ? CrabMesh->GetSocketLocation(GiantCrabInternal::GrabSocket)
		: GetActorLocation() + GetActorForwardVector() * 200.0f + FVector(0.0f, 0.0f, 100.0f);
	Held->SetActorLocation(Claw - FVector(0.0f, 0.0f, GiantCrabInternal::CaughtDrop), false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* Move = Held->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
}

void AGiantCrab::CatchPrey(ACharacter* Prey)
{
	// never two at once
	ReleaseHeldPrey(true);
	UCharacterMovementComponent* Move = Prey ? Prey->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}
	HeldPrey = Prey;
	LocalHeld = Prey;
	Move->DisableMovement();
	Move->StopMovementImmediately();
	// the machine of a player held keeps its pawn at the claw it sees (OnRep_HeldPrey): the server corrects none of the
	// moves it sends meanwhile, which only pulled it toward the server's claw by jolts (R-23); restored at the release
	bHeldPreyIgnoredErrors = Move->bIgnoreClientMovementErrorChecksAndCorrection;
	Move->bIgnoreClientMovementErrorChecksAndCorrection = true;
	HoldAtClaw(Prey);
	ForceNetUpdate();
}

void AGiantCrab::ReleaseHeldPrey(bool bFree)
{
	ACharacter* Held = HeldPrey;
	if (!Held && !LocalHeld.IsValid())
	{
		return;
	}
	HeldPrey = nullptr;
	LocalHeld = nullptr;
	ForceNetUpdate();
	UCharacterMovementComponent* Move = IsValid(Held) ? Held->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}
	Move->bIgnoreClientMovementErrorChecksAndCorrection = bHeldPreyIgnoredErrors;
	bHeldPreyIgnoredErrors = false;
	// a dead one, or one knocked out meanwhile, keeps its movement off: its ragdoll owns it (R-33: a corpse dropped
	// back to falling with a capsule that collides with nothing went through the ground)
	if (bFree && IsPrey(Held) && !IsKnockedOut(Held) && Move->MovementMode == MOVE_None)
	{
		Move->SetMovementMode(MOVE_Falling);
	}
	UE_LOG(Logmonke_island, Verbose, TEXT("GiantCrab %s: lets go of %s"), *GetName(), *Held->GetName());
}

void AGiantCrab::UpdateHeldPrey()
{
	ACharacter* Held = HeldPrey;
	if (!Held)
	{
		return;
	}
	// a dead one, one knocked out by something else, or the throw over: no longer carried (R-33)
	if (!IsValid(Held) || !IsPrey(Held) || IsKnockedOut(Held) || Action.State != EGiantCrabState::Throw || bThrowDone)
	{
		ReleaseHeldPrey(true);
		return;
	}
	HoldAtClaw(Held);
}

void AGiantCrab::OnRep_HeldPrey()
{
	SyncLocalHeld();
}

void AGiantCrab::SyncLocalHeld()
{
	if (HasAuthority())
	{
		return;
	}
	// only the machine of the player held does something: the others see its pawn through its replicated movement
	ACharacter* Wanted = IsValid(HeldPrey) && HeldPrey->IsLocallyControlled() ? HeldPrey.Get() : nullptr;
	ACharacter* Current = LocalHeld.Get();
	if (Wanted == Current)
	{
		return;
	}
	if (Current)
	{
		// let go here as on the server: it falls free, unless the knockout of the throw or its death owns its movement
		UCharacterMovementComponent* Move = Current->GetCharacterMovement();
		if (Move && IsPrey(Current) && !IsKnockedOut(Current) && Move->MovementMode == MOVE_None)
		{
			Move->SetMovementMode(MOVE_Falling);
		}
	}
	LocalHeld = Wanted;
	if (Wanted && IsPrey(Wanted) && !IsKnockedOut(Wanted))
	{
		// its movement off at once, at the claw this machine shows, rather than walking on until a correction
		Wanted->GetCharacterMovement()->DisableMovement();
		HoldAtClaw(Wanted);
	}
}

// ---------------------------------------------------------------- rise and fixed shapes (T-305, R-24, R-31)

void AGiantCrab::SetRising(bool bInRising)
{
	if (!HasAuthority())
	{
		return;
	}
	bRising = bInRising;
	OnRep_Rising();
	ForceNetUpdate();
}

void AGiantCrab::OnRep_Rising()
{
	// every machine: it rises through the sand with no collision at all (the server alone switched it off before: the
	// shapes of a client stopped its own pawn, R-31)
	SetActorEnableCollision(!bRising);
	ApplyBodyBlocking();
}

void AGiantCrab::ApplyBodyBlocking()
{
	if (Bodies)
	{
		// the charge runs through its prey faster than any prediction follows: its strikes do the work, not its shapes
		Bodies->SetBlockersEnabled(!bRising && Action.State != EGiantCrabState::Charge);
	}
}

// ---------------------------------------------------------------- age of the actions (T-305, R-24, R-32)

float AGiantCrab::ActionAge() const
{
	if (HasAuthority())
	{
		return 0.0f;
	}
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? FMath::Max(0.0f, static_cast<float>(GameState->GetServerWorldTimeSeconds() - Action.StartServerTime)) : 0.0f;
}

// ---------------------------------------------------------------- latency of the weak points (T-305, R-25)

void AGiantCrab::RecordWeakPoints()
{
	if (!Rules || Rules->WeakPoints.IsEmpty() || Rules->LagCompensationMaxSeconds <= 0.0f)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextWeakPointSample)
	{
		return;
	}
	const float Interval = FMath::Max(0.01f, Rules->WeakPointSampleSeconds);
	NextWeakPointSample = Now + Interval;
	const int32 Points = Rules->WeakPoints.Num();
	if (WeakPointHistory.GetPoints() != Points)
	{
		const float Span = Rules->LagCompensationMaxSeconds + Rules->LagWindowSeconds;
		WeakPointHistory.Reset(Points, FMath::CeilToInt(Span / Interval) + 2);
	}
	const USkeletalMeshComponent* CrabMesh = GetMesh();
	TArray<FVector, TInlineAllocator<8>> Locations;
	for (const FGiantCrabWeakPoint& Weak : Rules->WeakPoints)
	{
		Locations.Add(CrabMesh && CrabMesh->DoesSocketExist(Weak.Socket) ? CrabMesh->GetSocketLocation(Weak.Socket) : GetActorLocation());
	}
	WeakPointHistory.Add(Now, Locations);
}

float AGiantCrab::GetInstigatorRewind(const AController* EventInstigator) const
{
	// the host sees the server's crab: no rewind; nor for the mobs
	const APlayerController* Player = Cast<APlayerController>(EventInstigator);
	if (!Rules || !Player || Player->IsLocalController())
	{
		return 0.0f;
	}
	const APlayerState* State = Player->PlayerState;
	const float RoundTrip = State ? State->GetPingInMilliseconds() * 0.001f : 0.0f;
	return GiantCrabLag::RewindSeconds(RoundTrip, Rules->LagViewDelaySeconds, Rules->LagCompensationMaxSeconds);
}

const FGiantCrabWeakPoint* AGiantCrab::FindLaggedWeakPoint(const FHitResult& Hit, const FVector& Point, float Rewind) const
{
	const USkeletalMeshComponent* CrabMesh = GetMesh();
	if (!Rules || !CrabMesh || Rewind <= 0.0f || WeakPointHistory.Num() == 0 || WeakPointHistory.GetPoints() != Rules->WeakPoints.Num())
	{
		return nullptr;
	}
	// the line of the shot or of the blade up to a little past where it struck the crab here: as far as the crab can
	// have moved since the player saw it, never through its whole body; a blow without a line: its point
	const FVector TraceStart = Hit.TraceStart;
	const FVector TraceEnd = Hit.TraceEnd;
	const bool bLine = !TraceStart.Equals(TraceEnd, 1.0f);
	const FVector Direction = bLine ? (TraceEnd - TraceStart).GetSafeNormal() : FVector::ZeroVector;
	const float Window = Rules->LagWindowSeconds;
	const float Drift = static_cast<float>(GetVelocity().Size()) * (Rewind + Window);
	const double Seen = GetWorld()->GetTimeSeconds() - Rewind;
	TArray<FVector> Path;
	for (int32 Index = 0; Index < Rules->WeakPoints.Num(); ++Index)
	{
		const FGiantCrabWeakPoint& Candidate = Rules->WeakPoints[Index];
		if (!CrabMesh->DoesSocketExist(Candidate.Socket) || !WeakPointHistory.GetPath(Index, Seen - Window, Seen + Window, Path))
		{
			continue;
		}
		const FVector Start = bLine ? TraceStart : Point;
		const FVector End = bLine ? Point + Direction * (Candidate.RadiusCm + Drift) : Point;
		if (GiantCrabLag::RayNearPath(Start, End, Path, Candidate.RadiusCm))
		{
			return &Candidate;
		}
	}
	return nullptr;
}
