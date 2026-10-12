// monke_island

#include "Creatures/TikiAshBurst.h"
#include "Creatures/Hostility.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiTactics.h"
#include "Creatures/TikiTacticsMath.h"
#include "Creatures/TikiVeil.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ATikiAshBurst::ATikiAshBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ATikiAshBurst::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ATikiAshBurst, Rules, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiAshBurst, Kind, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiAshBurst, LaunchFrom, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiAshBurst, LandAt, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiAshBurst, FlightSeconds, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ATikiAshBurst, LaunchServerTime, COND_InitialOnly);
	DOREPLIFETIME(ATikiAshBurst, bBurst);
}

ATikiAshBurst* ATikiAshBurst::Launch(UWorld* World, UTikiRules* InRules, ETikiAshBurstKind InKind, APawn* Thrower, const FVector& From, const FVector& To)
{
	if (!World || !InRules || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	const FTikiAshBurstRules& Ash = InRules->Watcher.Ash;
	const FTransform Transform(To);
	ATikiAshBurst* Spawned = World->SpawnActorDeferred<ATikiAshBurst>(ATikiAshBurst::StaticClass(), Transform, Thrower, Thrower,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Spawned)
	{
		UE_LOG(LogMonkeTactics, Warning, TEXT("%s: the ash could not be spawned"), *GetNameSafe(Thrower));
		return nullptr;
	}
	Spawned->Rules = InRules;
	Spawned->Kind = InKind;
	Spawned->LaunchFrom = From;
	Spawned->LandAt = To;
	Spawned->FlightSeconds = InKind == ETikiAshBurstKind::Cloud
		? TikiTactics::FlightSeconds(static_cast<float>(FVector::Dist(From, To)), Ash.FlightSpeed, Ash.MinFlightSeconds) : 0.0f;
	const AGameStateBase* GameState = World->GetGameState();
	Spawned->LaunchServerTime = static_cast<float>(GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds());
	Spawned->FinishSpawning(Transform);
	return Spawned;
}

void ATikiAshBurst::BeginPlay()
{
	Super::BeginPlay();
	if (!Rules)
	{
		if (HasAuthority())
		{
			Destroy();
		}
		return;
	}
	if (HasAuthority())
	{
		if (FlightSeconds > 0.0f)
		{
			GetWorldTimerManager().SetTimer(BurstTimer, this, &ATikiAshBurst::Burst, FlightSeconds, false);
		}
		else
		{
			Burst();
		}
	}
	// a client joining after the burst draws the cloud at once
	if (bBurst)
	{
		ShowBurst();
	}
	else if (FlightSeconds > 0.0f)
	{
		ShowFlight();
	}
}

void ATikiAshBurst::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

double ATikiAshBurst::ServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

void ATikiAshBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Lump || !Rules || bBurst)
	{
		SetActorTickEnabled(false);
		return;
	}
	// the same arc on every machine, from the server time of the throw
	const float Alpha = FlightSeconds > 0.0f ? static_cast<float>((ServerNow() - LaunchServerTime) / FlightSeconds) : 1.0f;
	Lump->SetWorldLocation(TikiTactics::ArcPoint(LaunchFrom, LandAt, Rules->Watcher.Ash.ArcHeight, Alpha));
	if (Alpha >= 1.0f)
	{
		// landed: the burst comes with the server
		Lump->SetVisibility(false);
		SetActorTickEnabled(false);
	}
}

void ATikiAshBurst::Burst()
{
	if (bBurst || !Rules)
	{
		return;
	}
	const FTikiAshBurstRules& Ash = Rules->Watcher.Ash;
	const bool bCloud = Kind == ETikiAshBurstKind::Cloud;
	bBurst = true;
	ForceNetUpdate();
	ShowBurst();
	Pulse();
	GetWorldTimerManager().SetTimer(PulseTimer, this, &ATikiAshBurst::Pulse, FMath::Max(bCloud ? Ash.PulseSeconds : Ash.EmberTickSeconds, 0.05f), true);
	SetLifeSpan(FMath::Max(bCloud ? Ash.CloudSeconds : Ash.EmberSeconds, 0.1f));
	UE_LOG(LogMonkeTactics, Log, TEXT("%s: %s at %s"), *GetName(), bCloud ? TEXT("a cloud of ash bursts") : TEXT("embers burn"),
		*GetActorLocation().ToCompactString());
}

void ATikiAshBurst::Pulse()
{
	UWorld* World = GetWorld();
	if (!Rules || !World || !HasAuthority())
	{
		return;
	}
	const FTikiAshBurstRules& Ash = Rules->Watcher.Ash;
	const FVector Center = GetActorLocation();
	if (Kind == ETikiAshBurstKind::Cloud)
	{
		// the players in the cloud: a veil on their screen, again while they stay inside
		const double Now = World->GetTimeSeconds();
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr;
			if (!Pawn || !Hostility::IsAlive(Pawn) || !TikiTactics::IsInsideBurst(Center, Ash.CloudRadius, Ash.CloudHeight, Pawn->GetActorLocation()))
			{
				continue;
			}
			double& Last = LastBlindTime.FindOrAdd(Pawn, -1.0e9);
			if (Now - Last < Ash.BlindRepeatSeconds)
			{
				continue;
			}
			Last = Now;
			MulticastBlind(Pawn, Ash.BlindSeconds);
		}
		return;
	}
	// the embers burn the foes of the tiki standing in them: the players, the soldiers, the giant crab
	UHostilitySubsystem* Registry = UHostilitySubsystem::Get(this);
	if (!Registry)
	{
		return;
	}
	TArray<ACharacter*, TInlineAllocator<8>> Burnt;
	for (const FHostileCombatant& Other : Registry->GetCombatants())
	{
		ACharacter* Character = Other.Character.Get();
		if (Character && Other.IsLivingFoeOf(EFaction::Tiki) && TikiTactics::IsInsideBurst(Center, Ash.EmberRadius, 150.0f, Character->GetActorLocation()))
		{
			Burnt.Add(Character);
		}
	}
	// the damage after the reading: a death may change the registry
	const float Damage = Ash.EmberDamagePerSecond * Ash.EmberTickSeconds;
	for (ACharacter* Character : Burnt)
	{
		UGameplayStatics::ApplyDamage(Character, Damage, GetInstigatorController(), this, nullptr);
	}
}

void ATikiAshBurst::ShowFlight()
{
	if (Lump || !Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FTikiAshBurstRules& Ash = Rules->Watcher.Ash;
	if (!Ash.LumpMesh)
	{
		return;
	}
	Lump = NewObject<UStaticMeshComponent>(this, TEXT("AshLump"));
	Lump->SetStaticMesh(Ash.LumpMesh);
	if (Ash.LumpMaterial)
	{
		Lump->SetMaterial(0, Ash.LumpMaterial);
	}
	Lump->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Lump->SetCastShadow(false);
	Lump->SetupAttachment(Root);
	Lump->SetAbsolute(true, true, true);
	Lump->RegisterComponent();
	Lump->SetWorldLocationAndRotation(LaunchFrom, FRotator::ZeroRotator);
	Lump->SetWorldScale3D(FVector(Ash.LumpScale));
	SetActorTickEnabled(true);
}

void ATikiAshBurst::ShowBurst()
{
	if (bShown || !Rules)
	{
		return;
	}
	bShown = true;
	SetActorTickEnabled(false);
	if (Lump)
	{
		Lump->SetVisibility(false);
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FTikiAshBurstRules& Ash = Rules->Watcher.Ash;
	const bool bCloud = Kind == ETikiAshBurstKind::Cloud;
	if (UStaticMesh* Mesh = bCloud ? Ash.CloudMesh.Get() : Ash.EmberMesh.Get())
	{
		Patch = NewObject<UStaticMeshComponent>(this, TEXT("AshPatch"));
		Patch->SetStaticMesh(Mesh);
		if (UMaterialInterface* Material = bCloud ? Ash.CloudMaterial.Get() : Ash.EmberMaterial.Get())
		{
			Patch->SetMaterial(0, Material);
		}
		Patch->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Patch->SetCastShadow(false);
		Patch->SetupAttachment(Root);
		// the engine sphere and cylinder are 100 cm wide and high, their origin in the middle
		const float Radius = bCloud ? Ash.CloudRadius : Ash.EmberRadius;
		const float Height = bCloud ? Ash.CloudHeight : Ash.EmberThickness;
		Patch->SetRelativeLocation(FVector(0.0f, 0.0f, bCloud ? Height * 0.4f : Height * 0.5f));
		Patch->SetRelativeScale3D(FVector(Radius / 50.0f, Radius / 50.0f, Height / 100.0f));
		Patch->RegisterComponent();
	}
	const TArray<TObjectPtr<USoundBase>>& Sounds = bCloud ? Ash.BurstSound : Ash.EmberSound;
	if (!Sounds.IsEmpty())
	{
		if (USoundBase* Sound = Sounds[FMath::RandRange(0, Sounds.Num() - 1)])
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), 1.0f, FMath::FRandRange(0.95f, 1.05f), 0.0f, Ash.Attenuation);
		}
	}
	SpawnEffect();
	GetWorldTimerManager().SetTimer(EffectTimer, this, &ATikiAshBurst::SpawnEffect, FMath::Max(bCloud ? Ash.CloudEffectSeconds : Ash.EmberEffectSeconds, 0.1f), true);
}

void ATikiAshBurst::SpawnEffect()
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FTikiAshBurstRules& Ash = Rules->Watcher.Ash;
	const bool bCloud = Kind == ETikiAshBurstKind::Cloud;
	// loaded in the background by the watchers (ATikiWarrior::PreloadTacticEffects): skipped while it loads
	const TSoftObjectPtr<UNiagaraSystem>& Effect = bCloud ? Ash.CloudEffect : Ash.EmberEffect;
	UNiagaraSystem* System = Effect.Get();
	if (!System)
	{
		return;
	}
	// a little around the middle each time: the cloud churns, the embers spit
	const float Spread = (bCloud ? Ash.CloudRadius : Ash.EmberRadius) * 0.5f;
	const FVector Offset(FMath::FRandRange(-Spread, Spread), FMath::FRandRange(-Spread, Spread), bCloud ? Ash.CloudHeight * 0.25f : 5.0f);
	// the effects of the project emit along +X: turned upward, the smoke rises
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, GetActorLocation() + Offset, FRotator(90.0f, 0.0f, 0.0f),
		FVector(bCloud ? Ash.CloudEffectScale : Ash.EmberEffectScale));
}

void ATikiAshBurst::OnRep_Burst()
{
	if (bBurst)
	{
		ShowBurst();
	}
}

void ATikiAshBurst::MulticastBlind_Implementation(APawn* Victim, float Seconds)
{
	// only the machine of the blinded player veils its view
	if (!Rules || !Victim || !Victim->IsLocallyControlled())
	{
		return;
	}
	if (UTikiVeilSubsystem* Veil = UTikiVeilSubsystem::Get(this))
	{
		Veil->Veil(Cast<APlayerController>(Victim->GetController()), Seconds, Rules->Watcher.Ash);
	}
}
