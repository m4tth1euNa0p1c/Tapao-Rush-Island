// monke_island

#include "Footsteps/FootstepSubsystem.h"
#include "Footsteps/FootstepMath.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	TAutoConsoleVariable<int32> CVarFootsteps(
		TEXT("monke.Footsteps"),
		1,
		TEXT("Footsteps of the characters (1 on, 0 off), read when a world starts playing"));
}

const TCHAR* UFootstepSubsystem::DefaultRulesPath = TEXT("/Game/MonkeIsland/Player/Footsteps/DA_FootstepRules.DA_FootstepRules");

bool UFootstepSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UFootstepSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_DedicatedServer || CVarFootsteps.GetValueOnGameThread() == 0)
	{
		return;
	}
	Rules = LoadObject<UFootstepRules>(nullptr, DefaultRulesPath, nullptr, LOAD_NoWarn);
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Footsteps: %s not found, no footsteps"), DefaultRulesPath);
	}
}

TStatId UFootstepSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFootstepSubsystem, STATGROUP_Tickables);
}

EFootstepSurface UFootstepSubsystem::FindSurface(const ACharacter* Character) const
{
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f);

	// wading first: the water hides the ground
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(Character);
	if (Water && Water->GetWaterDepthAt(Feet) >= Rules->WadingDepthCm)
	{
		return EFootstepSurface::Water;
	}

	// the floor the movement already found, else a short trace (simulated characters without a floor)
	FHitResult Floor;
	const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (Move && Move->CurrentFloor.IsWalkableFloor())
	{
		Floor = Move->CurrentFloor.HitResult;
	}
	else
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(Footstep), false, Character);
		GetWorld()->LineTraceSingleByChannel(Floor, Feet + FVector(0.0f, 0.0f, 20.0f), Feet - FVector(0.0f, 0.0f, 40.0f), ECC_Visibility, Params);
	}
	const UPrimitiveComponent* Component = Floor.GetComponent();
	if (!Component)
	{
		return Rules->DefaultSurface;
	}

	TArray<FString, TInlineAllocator<4>> Names;
	if (const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component); MeshComponent && MeshComponent->GetStaticMesh())
	{
		Names.Add(MeshComponent->GetStaticMesh()->GetName());
	}
	Names.Add(Component->GetName());
	if (const AActor* Owner = Component->GetOwner())
	{
		Names.Add(Owner->GetName());
		Names.Add(Owner->GetClass()->GetName());
	}

	for (const FString& Name : Names)
	{
		if (!Rules->TerrainKeyword.IsEmpty() && Name.Contains(Rules->TerrainKeyword, ESearchCase::IgnoreCase))
		{
			const float WaterZ = Water ? Water->GetWaterSurfaceAt(Feet) : 0.0f;
			return FootstepMath::TerrainSurface(static_cast<float>(Floor.ImpactPoint.Z) - WaterZ, Rules->WetSandBandCm);
		}
	}
	EFootstepSurface Surface;
	return FootstepMath::SurfaceFromNames(Rules->Keywords, Names, Surface) ? Surface : Rules->DefaultSurface;
}

void UFootstepSubsystem::PlayStep(const ACharacter* Character, EFootstepSurface Surface, float Volume) const
{
	const FFootstepSurfaceSounds* Set = Rules->Sounds.Find(Surface);
	if (!Set || Set->Sounds.IsEmpty())
	{
		return;
	}
	USoundBase* Sound = Set->Sounds[FMath::RandRange(0, Set->Sounds.Num() - 1)];
	if (!Sound)
	{
		return;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const FVector Feet = Character->GetActorLocation() - FVector(0.0f, 0.0f, Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.0f);
	const float Scale = Character->IsPlayerControlled() ? 1.0f : Rules->CreatureVolume;
	const float Pitch = 1.0f + FMath::FRandRange(-Rules->PitchJitter, Rules->PitchJitter);
	UGameplayStatics::PlaySoundAtLocation(Character, Sound, Feet, Volume * Set->Volume * Scale, Pitch, 0.0f, Rules->Attenuation);
}

void UFootstepSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	const APlayerController* Local = World ? World->GetFirstPlayerController() : nullptr;
	if (!Local || !Local->PlayerCameraManager)
	{
		return;
	}
	const FVector Listener = Local->PlayerCameraManager->GetCameraLocation();
	const float MaxDistanceSq = FMath::Square(Rules->MaxDistanceCm);

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		const ACharacter* Character = *It;
		// a creature that plays its own steps (the giant crab's legs, T-261) gets no generic ones
		if (Character->Tags.Contains(TEXT("NoGenericFootsteps")) || FVector::DistSquared(Character->GetActorLocation(), Listener) > MaxDistanceSq)
		{
			continue;
		}
		const UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		if (!Move)
		{
			continue;
		}
		FStepState& State = States.FindOrAdd(Character);

		// seated in the boat, ragdoll, swimming: no step
		const bool bOnGround = Move->IsMovingOnGround() && !Character->GetAttachParentActor();
		if (!bOnGround)
		{
			if (Move->IsFalling())
			{
				State.FallSpeed = FMath::Max(0.0f, static_cast<float>(-Move->Velocity.Z));
			}
			State.bWasOnGround = false;
			continue;
		}

		if (!State.bWasOnGround)
		{
			State.bWasOnGround = true;
			const float Landing = FootstepMath::LandingVolume(State.FallSpeed, Rules->LandingMinSpeed, Rules->LandingVolume);
			State.FallSpeed = 0.0f;
			if (Landing > 0.0f)
			{
				PlayStep(Character, FindSurface(Character), Landing);
				State.Phase = 0.3f;
				continue;
			}
		}

		const float Speed = static_cast<float>(Move->Velocity.Size2D());
		const float Rate = FootstepMath::StepRate(Speed, Rules->MinSpeed, Rules->SlowStepRate, Rules->WalkCadence, Rules->SprintCadence);
		if (Rate <= 0.0f)
		{
			// the first step comes quickly once it moves again
			State.Phase = 0.7f;
			continue;
		}
		State.Phase += Rate * DeltaTime;
		if (State.Phase >= 1.0f)
		{
			State.Phase = FMath::Fmod(State.Phase, 1.0f);
			PlayStep(Character, FindSurface(Character), FootstepMath::StepVolume(Speed, Rules->MinSpeed, Rules->SprintCadence.X, Rules->VolumeRange));
		}
	}

	PurgeTimer -= DeltaTime;
	if (PurgeTimer <= 0.0f)
	{
		PurgeTimer = 5.0f;
		for (auto It = States.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
}
