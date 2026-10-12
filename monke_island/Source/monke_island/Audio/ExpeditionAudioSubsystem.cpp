// monke_island

#include "Audio/ExpeditionAudioSubsystem.h"
#include "Audio/ExpeditionAudioRules.h"
#include "Audio/ExpeditionAudioMath.h"
#include "Boats/BoatRiderComponent.h"
#include "DayNight/DayNightCycleActor.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Galleon/GalleonActor.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunStateComponent.h"
#include "Storm/WindStormActor.h"
#include "Water/IslandWaterSubsystem.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "monke_island.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs AudioPlay(
		TEXT("monke.Audio.Play"), TEXT("Audition a configured audio event locally: monke.Audio.Play QuotaMet"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World && Args.Num() == 1)
			{
				if (UExpeditionAudioSubsystem* Audio = World->GetSubsystem<UExpeditionAudioSubsystem>())
				{
					UE_LOG(Logmonke_island, Log, TEXT("ExpeditionAudio audition %s: %s"), *Args[0],
						Audio->PlayEvent(FName(*Args[0])) ? TEXT("submitted") : TEXT("unavailable/cooldown"));
				}
			}
		}));
	FAutoConsoleCommandWithWorld AudioReport(
		TEXT("monke.Audio.Report"), TEXT("Report audio readiness and submitted event counts; not an audibility test"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (World)
			{
				if (const UExpeditionAudioSubsystem* Audio = World->GetSubsystem<UExpeditionAudioSubsystem>())
				{
					Audio->Report();
				}
			}
		}));
}

UExpeditionAudioSubsystem::UExpeditionAudioSubsystem()
	: RulesPath(TEXT("/Game/MonkeIsland/Audio/Expedition/DA_ExpeditionAudioRules.DA_ExpeditionAudioRules"))
{
}

bool UExpeditionAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return !IsRunningCommandlet() && !IsRunningDedicatedServer() && World
		&& (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UExpeditionAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	RulesLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(RulesPath,
		FStreamableDelegate::CreateUObject(this, &UExpeditionAudioSubsystem::RulesReady));
}

void UExpeditionAudioSubsystem::RulesReady()
{
	if (bShuttingDown)
	{
		return;
	}
	Rules = Cast<UExpeditionAudioRules>(RulesPath.ResolveObject());
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("ExpeditionAudio: no rules at %s; additional audio disabled"), *RulesPath.ToString());
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const auto& Pair : Rules->Cues)
	{
		for (const auto& Sound : Pair.Value.Sounds)
		{
			if (!Sound.IsNull())
			{
				Paths.AddUnique(Sound.ToSoftObjectPath());
			}
		}
	}
	if (Paths.IsEmpty())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("ExpeditionAudio: empty cue catalogue; audio disabled"));
		return;
	}
	SoundsLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateUObject(this, &UExpeditionAudioSubsystem::SoundsReady));
}

void UExpeditionAudioSubsystem::SoundsReady()
{
	if (bShuttingDown || !GetWorld() || !Rules)
	{
		return;
	}
	bReady = true;
	UIslanderInventoryComponent::OnShopRequested.AddUObject(this, &UExpeditionAudioSubsystem::HandleShopOpen);
	UIslanderInventoryComponent::OnShopResult.AddUObject(this, &UExpeditionAudioSubsystem::HandleShopResult);
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &UExpeditionAudioSubsystem::Poll,
		FMath::Max(Rules->CheckInterval, 0.05f), true);
	Report();
}

void UExpeditionAudioSubsystem::Deinitialize()
{
	bShuttingDown = true;
	bReady = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PollTimer);
	}
	BindPlayer(nullptr);
	UIslanderInventoryComponent::OnShopRequested.RemoveAll(this);
	UIslanderInventoryComponent::OnShopResult.RemoveAll(this);
	for (auto& Pair : Loops)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Stop();
			Pair.Value->DestroyComponent();
		}
	}
	Loops.Reset();
	if (RulesLoad) { RulesLoad->CancelHandle(); }
	if (SoundsLoad) { SoundsLoad->CancelHandle(); }
	RulesLoad.Reset();
	SoundsLoad.Reset();
	Rules = nullptr;
	Super::Deinitialize();
}

bool UExpeditionAudioSubsystem::PlayEvent(FName Event)
{
	if (!bReady || !Rules || !GetWorld()) { return false; }
	const FExpeditionAudioCue* Cue = Rules->Cues.Find(Event);
	if (!Cue || Cue->Sounds.IsEmpty()) { return false; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastPlayed.Find(Event))
	{
		if (Now - *Last < Cue->Cooldown) { return false; }
	}
	int32 Index = FMath::RandHelper(Cue->Sounds.Num());
	if (const int32* Previous = LastVariation.Find(Event))
	{
		if (Cue->Sounds.Num() > 1 && Index == *Previous) { Index = (Index + 1) % Cue->Sounds.Num(); }
	}
	USoundBase* Sound = Cue->Sounds[Index].Get();
	// Loops must go through SetLoop so that travel and shutdown always stop them.
	if (!Sound || Sound->IsLooping()) { return false; }
	UGameplayStatics::PlaySound2D(this, Sound, Cue->Volume);
	LastPlayed.Add(Event, Now);
	LastVariation.Add(Event, Index);
	++PlayCounts.FindOrAdd(Event);
	UE_LOG(Logmonke_island, Verbose, TEXT("ExpeditionAudio: %s -> %s"), *Event.ToString(), *Sound->GetName());
	return true;
}

void UExpeditionAudioSubsystem::SetLoop(FName Event, float Weight)
{
	const FExpeditionAudioCue* Cue = Rules->Cues.Find(Event);
	if (!Cue || Cue->Sounds.IsEmpty()) { return; }
	float& Volume = LoopVolumes.FindOrAdd(Event);
	const float Target = FMath::Clamp(Weight, 0.0f, 1.0f) * FMath::Max(Cue->Volume, 0.0f);
	Volume = FMath::FInterpConstantTo(Volume, Target, Rules->CheckInterval,
		FMath::Max(Cue->Volume, 0.01f) / FMath::Max(Rules->FadeSeconds, 0.1f));
	TObjectPtr<UAudioComponent>& Component = Loops.FindOrAdd(Event);
	if (Volume <= KINDA_SMALL_NUMBER)
	{
		if (IsValid(Component)) { Component->Stop(); Component->DestroyComponent(); Component = nullptr; }
		return;
	}
	if (!IsValid(Component))
	{
		USoundBase* Sound = Cue->Sounds[0].Get();
		if (!Sound || !Sound->IsLooping()) { return; }
		Component = UGameplayStatics::CreateSound2D(this, Sound, Volume, 1.0f, 0.0f, nullptr, false, false);
		if (Component) { Component->Play(); }
	}
	if (Component) { Component->SetVolumeMultiplier(Volume); }
}

void UExpeditionAudioSubsystem::BindPlayer(ACharacter* NewPlayer)
{
	if (ACharacter* Old = Player.Get())
	{
		Old->MovementModeChangedDelegate.RemoveAll(this);
		Old->LandedDelegate.RemoveAll(this);
	}
	if (Health.IsValid())
	{
		Health->OnHealthChanged.RemoveAll(this);
		Health->OnDeath.RemoveAll(this);
	}
	Player = NewPlayer;
	Health = NewPlayer ? NewPlayer->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	Inventory = NewPlayer ? NewPlayer->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	InventoryCount = -1;
	Held.Reset();
	if (NewPlayer)
	{
		NewPlayer->MovementModeChangedDelegate.AddDynamic(this, &UExpeditionAudioSubsystem::HandleMovement);
		NewPlayer->LandedDelegate.AddDynamic(this, &UExpeditionAudioSubsystem::HandleLanded);
	}
	if (Health.IsValid())
	{
		Health->OnHealthChanged.AddDynamic(this, &UExpeditionAudioSubsystem::HandleHealth);
		Health->OnDeath.AddDynamic(this, &UExpeditionAudioSubsystem::HandleDeath);
	}
}

void UExpeditionAudioSubsystem::HandleMovement(ACharacter* Character, EMovementMode Previous, uint8 PreviousCustom)
{
	const UCharacterMovementComponent* Move = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Move || (Health.IsValid() && Health->IsDead())) { return; }
	if (Move->IsSwimming() && Previous != MOVE_Swimming) { PlayEvent(TEXT("SwimEnter")); }
	else if (!Move->IsSwimming() && Previous == MOVE_Swimming) { PlayEvent(TEXT("SwimExit")); }
	else if (Move->IsFalling() && Previous == MOVE_Walking && Move->Velocity.Z > 0.0f) { PlayEvent(TEXT("Jump")); }
}

void UExpeditionAudioSubsystem::HandleLanded(const FHitResult& Hit)
{
	if (!Health.IsValid() || !Health->IsDead()) { PlayEvent(TEXT("Land")); }
}

void UExpeditionAudioSubsystem::HandleHealth(float NewHealth, float Delta)
{
	if (Delta < 0.0f && NewHealth > 0.0f) { PlayEvent(TEXT("PlayerHurt")); }
}

void UExpeditionAudioSubsystem::HandleDeath(AActor* Killer)
{
	PlayEvent(TEXT("PlayerDeath"));
}

void UExpeditionAudioSubsystem::HandleShopOpen(UIslanderInventoryComponent* From, AShopkeeperActor* Shop)
{
	if (From && From == Inventory.Get()) { PlayEvent(TEXT("Merchant")); }
}

void UExpeditionAudioSubsystem::HandleShopResult(UIslanderInventoryComponent* From, EShopResult Result, int32 Offer)
{
	if (From && From == Inventory.Get())
	{
		LastPurchase = GetWorld()->GetTimeSeconds();
		PlayEvent(Result == EShopResult::Bought ? TEXT("Purchase") : TEXT("PurchaseDenied"));
	}
}

void UExpeditionAudioSubsystem::ReadRound()
{
	AExpeditionGameState* State = AExpeditionGameState::Get(this);
	if (State && State->GetExpeditionId().IsValid())
	{
		const double Left = State->GetTimeBeforeSubmersion();
		if (ExpeditionId != State->GetExpeditionId())
		{
			ExpeditionId = State->GetExpeditionId();
			PreviousTimeLeft = Left;
			HalfTime = FMath::Max(0.0, (State->GetFloodStartTime() - State->GetExpeditionStartTime()) * 0.5);
			bHalfPlayed = Left <= HalfTime;
			bRunPrimed = false;
			const double Age = State->GetServerWorldTimeSeconds() - State->GetExpeditionStartTime();
			if (Age >= 0.0 && Age <= Rules->StartGraceSeconds) { PlayEvent(TEXT("DayStart")); }
		}
		else if (State->GetPhase() == EExpeditionPhase::Exploring)
		{
			if (!bHalfPlayed && ExpeditionAudioMath::Crossed(PreviousTimeLeft, Left, HalfTime))
			{
				PlayEvent(TEXT("HalfTime"));
				bHalfPlayed = true;
			}
			if (ExpeditionAudioMath::CountdownTick(PreviousTimeLeft, Left, Rules->CountdownFrom))
			{
				PlayEvent(TEXT("Countdown"));
			}
			PreviousTimeLeft = Left;
		}
	}
	// The campaign verdict can also be published outside an expedition (end-of-day choice): read it on any game state.
	const URunStateComponent* Run = URunStateComponent::Get(this);
	if (!Run) { return; }
	const int32 Outcome = static_cast<int32>(Run->GetOutcome());
	if (bRunPrimed)
	{
		if (Outcome != PreviousOutcome && Run->GetOutcome() != ERunOutcome::InProgress)
		{
			PlayEvent(Run->GetOutcome() == ERunOutcome::Victory ? TEXT("Victory") : TEXT("Defeat"));
		}
		else if (!bQuotaMet && Run->IsQuotaMet()) { PlayEvent(TEXT("QuotaMet")); }
	}
	bQuotaMet = Run->IsQuotaMet();
	PreviousOutcome = Outcome;
	bRunPrimed = true;
}

void UExpeditionAudioSubsystem::ReadAmbience()
{
	ACharacter* Pawn = Player.Get();
	const bool bAlive = Pawn && (!Health.IsValid() || !Health->IsDead());
	UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	const bool bHasWater = Water && !Water->GetWaterSurfaces().IsEmpty();
	const UBoatRiderComponent* Rider = Pawn ? Pawn->FindComponentByClass<UBoatRiderComponent>() : nullptr;
	const bool bBoat = Rider && Rider->IsSeated();
	const bool bAboard = Pawn && Galleon.IsValid() && Galleon->IsActorAboard(Pawn);
	FVector View = Pawn ? Pawn->GetPawnViewLocation() : FVector::ZeroVector;
	FRotator Rotation;
	if (Pawn && Pawn->GetController()) { Pawn->GetController()->GetPlayerViewPoint(View, Rotation); }
	const bool bUnderwater = bAlive && bHasWater && View.Z + Rules->UnderwaterDepth < Water->GetWaterSurfaceAt(View);
	const bool bFullStorm = bAlive && Wind.IsValid() && Wind->IsDeadlineWind();
	const bool bStrong = Wind.IsValid() && Wind->GetWind().Size() > Rules->StrongWindThreshold;
	const bool bNight = DayNight.IsValid() && DayNight->GetPhase() == EDayPhase::Night;
	const bool bOutdoors = bAlive && bHasWater && !bUnderwater;
	SetLoop(TEXT("HullWater"), bAlive && bAboard && !bUnderwater ? 1.0f : 0.0f);
	SetLoop(TEXT("Underwater"), bUnderwater ? 1.0f : 0.0f);
	SetLoop(TEXT("WindLight"), bOutdoors && !bStrong && !bFullStorm ? 1.0f : 0.0f);
	SetLoop(TEXT("WindStrong"), bOutdoors && bStrong && !bFullStorm ? 1.0f : 0.0f);
	SetLoop(TEXT("WindStorm"), bOutdoors && bFullStorm ? 1.0f : 0.0f);
	SetLoop(TEXT("Rain"), bOutdoors && bFullStorm ? 1.0f : 0.0f);
	SetLoop(TEXT("Crickets"), bOutdoors && bNight && !bAboard && !bBoat && !bFullStorm ? 1.0f : 0.0f);
	const double Now = GetWorld()->GetTimeSeconds();
	if (bAlive && bAboard && Now >= NextCreak)
	{
		PlayEvent(TEXT("WoodCreak"));
		NextCreak = Now + FMath::Max(1.0f, FMath::FRandRange(Rules->CreakInterval.X, Rules->CreakInterval.Y));
	}
	if (bFullStorm && !bStorm)
	{
		PlayEvent(TEXT("StormStart"));
		NextThunder = Now + FMath::Max(1.0f, Rules->ThunderInterval.X);
	}
	if (bFullStorm && Now >= NextThunder)
	{
		PlayEvent(TEXT("Thunder"));
		NextThunder = Now + FMath::Max(1.0f, FMath::FRandRange(Rules->ThunderInterval.X, Rules->ThunderInterval.Y));
	}
	bStorm = bFullStorm;
}

void UExpeditionAudioSubsystem::Poll()
{
	if (!bReady || !GetWorld() || !Rules) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	ACharacter* Pawn = Controller && Controller->IsLocalController() ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
	if (Pawn != Player.Get()) { BindPlayer(Pawn); }
	if (Now >= NextWorldRefresh)
	{
		Galleon = AGalleonActor::Get(this);
		Wind = AWindStormActor::Get(this);
		DayNight = ADayNightCycleActor::Get(this);
		NextWorldRefresh = Now + FMath::Max(0.5f, Rules->WorldRefreshInterval);
	}
	if (Inventory.IsValid())
	{
		int32 Count = 0;
		for (const FInventorySlot& Slot : Inventory->GetSlots()) { Count += FMath::Max(Slot.Count, 0); }
		if (ExpeditionAudioMath::GainedItems(InventoryCount, Count) && Now - LastPurchase > Rules->PurchasePickupSuppression)
		{
			PlayEvent(TEXT("Pickup"));
		}
		InventoryCount = Count;
	}
	if (Pawn)
	{
		const UIslanderGrabComponent* Grab = Pawn->FindComponentByClass<UIslanderGrabComponent>();
		UPrimitiveComponent* Current = Grab ? Grab->GetHeldComponent() : nullptr;
		if (Current != Held.Get() && Current) { PlayEvent(TEXT("Lift")); }
		Held = Current;
	}
	ReadRound();
	ReadAmbience();
}

void UExpeditionAudioSubsystem::Report() const
{
	UE_LOG(Logmonke_island, Log, TEXT("ExpeditionAudio: ready=%d cues=%d loops=%d; submissions do not prove audibility"),
		bReady, Rules ? Rules->Cues.Num() : 0, Loops.Num());
	for (const auto& Pair : PlayCounts)
	{
		UE_LOG(Logmonke_island, Log, TEXT("ExpeditionAudio: submitted %s=%d"), *Pair.Key.ToString(), Pair.Value);
	}
}
