// monke_island

#include "Creatures/TikiTactics.h"
#include "Creatures/Hostility.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiTacticsMath.h"
#include "Creatures/TikiWarrior.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY(LogMonkeTactics);

UTikiTactics::UTikiTactics()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UTikiTactics::Bind(ATikiWarrior* InWarrior)
{
	Warrior = InWarrior;
}

void UTikiTactics::OnNoFoe()
{
	SetState(ETikiTacticState::None, false, false);
}

const UTikiRules* UTikiTactics::GetRules() const
{
	return Warrior ? Warrior->Rules.Get() : nullptr;
}

double UTikiTactics::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

float UTikiTactics::MoveToward(const FVector& Target, float Speed)
{
	return Warrior ? Warrior->MoveToward(Target, Speed) : 0.0f;
}

void UTikiTactics::StopMoving()
{
	if (Warrior && Warrior->GetCharacterMovement())
	{
		Warrior->GetCharacterMovement()->StopMovementImmediately();
	}
}

void UTikiTactics::FaceToward(const FVector& Target)
{
	if (Warrior)
	{
		Warrior->FaceToward(Target);
	}
}

void UTikiTactics::TurnToward(const FVector& Target, float DeltaSeconds, float RateDeg)
{
	if (!Warrior)
	{
		return;
	}
	FVector Delta = Target - Warrior->GetActorLocation();
	Delta.Z = 0.0;
	if (Delta.IsNearlyZero())
	{
		return;
	}
	const UTikiRules* Rules = GetRules();
	const float Rate = RateDeg > 0.0f ? RateDeg : (Rules ? Rules->Pack.StrafeTurnRate : 360.0f);
	const FRotator Desired(0.0f, Delta.Rotation().Yaw, 0.0f);
	Warrior->SetActorRotation(FMath::RInterpConstantTo(Warrior->GetActorRotation(), Desired, DeltaSeconds, Rate));
}

void UTikiTactics::SetState(ETikiTacticState State, bool bStrafe, bool bGuardUp)
{
	if (Warrior)
	{
		FTikiTacticRep Rep;
		Rep.State = State;
		Rep.bStrafe = bStrafe;
		Rep.bGuardUp = bGuardUp;
		Warrior->SetTacticRep(Rep);
	}
}

void UTikiTactics::PlayAction(ETikiAction Action, uint8 Variant)
{
	if (Warrior)
	{
		Warrior->PlayAction(Action, Variant);
	}
}

void UTikiTactics::PlayCue(ETikiTacticCue Cue, const FVector& Location)
{
	if (Warrior)
	{
		Warrior->MulticastTacticCue(static_cast<uint8>(Cue), Location);
	}
}

bool UTikiTactics::UpdateStuck(bool bWantsToMove)
{
	return Warrior && Warrior->UpdateStuck(bWantsToMove);
}

bool UTikiTactics::SeesFoe() const
{
	return Warrior && Warrior->bSeesTarget;
}

FVector UTikiTactics::KnownFoeLocation(const APawn* Foe) const
{
	if (!Warrior)
	{
		return FVector::ZeroVector;
	}
	return Foe && Warrior->bSeesTarget ? Foe->GetActorLocation() : Warrior->TargetSeenAt;
}

void UTikiTactics::FoeView(const APawn* Foe, FVector& OutLocation, FVector& OutDirection) const
{
	OutLocation = Foe ? Foe->GetActorLocation() : FVector::ZeroVector;
	OutDirection = Foe ? Foe->GetActorForwardVector() : FVector::ForwardVector;
	// a player looks where its camera looks: the control rotation, sent to the server with its moves
	if (Foe && Foe->GetController())
	{
		OutDirection = Foe->GetControlRotation().Vector();
	}
}

FTikiPackView UTikiTactics::ReadPack(const APawn* Foe) const
{
	FTikiPackView View;
	const UTikiRules* Rules = GetRules();
	UHostilitySubsystem* Registry = Warrior ? UHostilitySubsystem::Get(Warrior) : nullptr;
	if (!Rules || !Registry || !Foe)
	{
		return View;
	}
	const FVector Here = Warrior->GetActorLocation();
	const double RadiusSq = FMath::Square(static_cast<double>(Rules->Pack.PackRadius));
	const ETikiArchetype Mine = Warrior->GetArchetype();
	TArray<uint32, TInlineAllocator<16>> SameRoleIds;
	for (const FHostileCombatant& Other : Registry->GetCombatants())
	{
		const ATikiWarrior* Ally = Cast<ATikiWarrior>(Other.Character.Get());
		if (!Ally || Ally == Warrior || Ally->IsDead() || Ally->GetTarget() != Foe || FVector::DistSquared2D(Ally->GetActorLocation(), Here) > RadiusSq)
		{
			continue;
		}
		++View.Hunters;
		View.Attacking += Ally->IsCommitted() ? 1 : 0;
		if (Ally->GetArchetype() == Mine)
		{
			++View.SameRole;
			SameRoleIds.Add(Ally->GetUniqueID());
		}
	}
	View.SameRoleRank = TikiTactics::PackRank(Warrior->GetUniqueID(), SameRoleIds);
	return View;
}

int32 UTikiTactics::CountAlliesNear(float Radius) const
{
	UHostilitySubsystem* Registry = Warrior ? UHostilitySubsystem::Get(Warrior) : nullptr;
	if (!Registry)
	{
		return 0;
	}
	const FVector Here = Warrior->GetActorLocation();
	const double RadiusSq = FMath::Square(static_cast<double>(Radius));
	int32 Count = 0;
	for (const FHostileCombatant& Other : Registry->GetCombatants())
	{
		const ATikiWarrior* Ally = Cast<ATikiWarrior>(Other.Character.Get());
		if (!Ally || Ally == Warrior || Ally->IsDead() || FVector::DistSquared2D(Ally->GetActorLocation(), Here) > RadiusSq)
		{
			continue;
		}
		const ETikiTacticState State = Ally->GetTactic().State;
		Count += State == ETikiTacticState::Flee || State == ETikiTacticState::Cower ? 0 : 1;
	}
	return Count;
}

bool UTikiTactics::TryTakeAttackToken(const APawn* Foe) const
{
	const UTikiRules* Rules = GetRules();
	return Rules && TikiTactics::HasAttackToken(ReadPack(Foe).Attacking, Rules->Pack.MaxAttackersPerFoe);
}

bool UTikiTactics::IsFoeDowned(const APawn* Foe) const
{
	const UTikiRules* Rules = GetRules();
	const AIslanderCharacter* Player = Cast<AIslanderCharacter>(Foe);
	return Rules && Rules->Pack.bSpareDownedFoes && Player && Player->IsRagdoll();
}

float UTikiTactics::GetHealthFraction() const
{
	const UIslanderHealthComponent* Life = Warrior ? Warrior->GetHealth() : nullptr;
	return Life ? Life->GetHealthFraction() : 1.0f;
}

ATikiDirector* UTikiTactics::GetDirector() const
{
	return Warrior ? Warrior->Director.Get() : nullptr;
}

bool UTikiTactics::FindBush(const FVector& Location, float Radius, FVector& OutBush) const
{
	ATikiDirector* TikiDirector = GetDirector();
	return TikiDirector && Radius > 0.0f && TikiDirector->FindNearestBush(Location, Radius, OutBush);
}

FVector UTikiTactics::GroundUnder(const FVector& Point, float Above, float Depth) const
{
	UWorld* World = GetWorld();
	if (!World || !Warrior)
	{
		return Point;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiTacticsGround), false, Warrior);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	// the vegetation (instanced meshes: bushes, palms) is skipped: only the ground counts
	for (int32 Try = 0; Try < 4; ++Try)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, Point + FVector(0.0f, 0.0f, Above), Point - FVector(0.0f, 0.0f, Depth), Objects, Params))
		{
			return Point;
		}
		if (Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
		{
			Params.AddIgnoredComponent(Hit.GetComponent());
			continue;
		}
		return Hit.ImpactPoint;
	}
	return Point;
}

void UTikiTactics::BeginStrike(APawn* Victim, double Now)
{
	if (Warrior)
	{
		Warrior->BeginStrike(Victim, Now);
	}
}

bool UTikiTactics::CanStartStrike(double Now) const
{
	return Warrior && Warrior->CanStartStrike(Now);
}

int32 UTikiTactics::StrikeWith(float Reach, float Radius, float Damage, float Knockback)
{
	return Warrior ? Warrior->StrikeWith(Reach, Radius, Damage, Knockback) : 0;
}
