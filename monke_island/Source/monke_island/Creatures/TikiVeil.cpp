// monke_island

#include "Creatures/TikiVeil.h"
#include "Creatures/TikiTactics.h"
#include "Creatures/TikiTacticsTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UTikiVeilSubsystem* UTikiVeilSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UTikiVeilSubsystem>() : nullptr;
}

bool UTikiVeilSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

void UTikiVeilSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}
	Veils.Reset();
	Super::Deinitialize();
}

bool UTikiVeilSubsystem::IsVeiled(const APlayerController* Player) const
{
	return Player && Veils.ContainsByPredicate([Player](const FVeil& Entry) { return Entry.Player.Get() == Player; });
}

void UTikiVeilSubsystem::Veil(APlayerController* Player, float Seconds, const FTikiAshBurstRules& Rules)
{
	UWorld* World = GetWorld();
	APlayerCameraManager* Camera = Player ? Player->PlayerCameraManager.Get() : nullptr;
	if (!World || !Camera || !Player->IsLocalController() || !(Seconds > 0.0f))
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	FVeil* Current = Veils.FindByPredicate([Player](const FVeil& Entry) { return Entry.Player.Get() == Player; });
	if (!Current)
	{
		// a fade of another system (the black of a day transition) is left alone
		if (Camera->bEnableFading && Camera->FadeAmount > 0.01f && !Camera->FadeColor.Equals(Rules.VeilColor))
		{
			return;
		}
		Camera->StartCameraFade(Camera->bEnableFading ? Camera->FadeAmount : 0.0f, Rules.VeilOpacity, Rules.VeilFadeIn, Rules.VeilColor, false, true);
		Current = &Veils.AddDefaulted_GetRef();
		Current->Player = Player;
		// the ringing of the ears of the blinded player, for it alone
		if (!Rules.VeilSound.IsEmpty())
		{
			if (USoundBase* Sound = Rules.VeilSound[FMath::RandRange(0, Rules.VeilSound.Num() - 1)])
			{
				UGameplayStatics::SpawnSound2D(Player, Sound);
			}
		}
		UE_LOG(LogMonkeTactics, Log, TEXT("%s is blinded by the ash for %.1f s"), *Player->GetName(), Seconds);
	}
	Current->EndTime = FMath::Max(Current->EndTime, Now + Seconds);
	Current->FadeOut = Rules.VeilFadeOut;
	Current->Color = Rules.VeilColor;
	if (!World->GetTimerManager().IsTimerActive(Timer))
	{
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &UTikiVeilSubsystem::Update), 0.1f, true);
	}
}

void UTikiVeilSubsystem::Update()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	for (int32 Index = Veils.Num() - 1; Index >= 0; --Index)
	{
		const FVeil& Entry = Veils[Index];
		APlayerController* Player = Entry.Player.Get();
		APlayerCameraManager* Camera = Player ? Player->PlayerCameraManager.Get() : nullptr;
		if (Camera && Now < Entry.EndTime)
		{
			continue;
		}
		// over: our veil fades out; a fade another system took over is left alone
		if (Camera && Camera->FadeColor.Equals(Entry.Color))
		{
			Camera->StartCameraFade(Camera->FadeAmount, 0.0f, Entry.FadeOut, Entry.Color, false, false);
		}
		Veils.RemoveAtSwap(Index);
	}
	if (Veils.IsEmpty())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}
}
