// monke_island

#include "Music/MusicSubsystem.h"
#include "Music/MusicRules.h"
#include "Boats/BoatRiderComponent.h"
#include "Ceremony/FlagCeremonySite.h"
#include "DayNight/DayNightCycleActor.h"
#include "Boats/BoatActor.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Galleon/GalleonActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "monke_island.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
	/** DA_MusicRules when DefaultGame.ini names none */
	const TCHAR* DefaultMusicRulesPath = TEXT("/Game/MonkeIsland/Audio/Music/DA_MusicRules.DA_MusicRules");
}

UMusicSubsystem::UMusicSubsystem()
	: MusicRulesPath(DefaultMusicRulesPath)
{
}

bool UMusicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

void UMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Rules = Cast<UMusicRules>(MusicRulesPath.TryLoad());
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Music: no DA_MusicRules at '%s', no music"), *MusicRulesPath.ToString());
		return;
	}
	Components.SetNum(static_cast<int32>(EMusicTrack::Count));
	bReady = true;
	UE_LOG(Logmonke_island, Log, TEXT("Music: %s ready (%s, %s, %s, galleon %s, boat %s)"), *Rules->GetName(), *GetNameSafe(Rules->Hawaiian),
		*GetNameSafe(Rules->Night), *GetNameSafe(Rules->Crown), *GetNameSafe(Rules->Galleon), *GetNameSafe(Rules->Boat));
}

void UMusicSubsystem::Deinitialize()
{
	for (UAudioComponent* Component : Components)
	{
		if (Component)
		{
			Component->Stop();
		}
	}
	Components.Reset();
	bReady = false;
	Super::Deinitialize();
}

TStatId UMusicSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMusicSubsystem, STATGROUP_Tickables);
}

bool UMusicSubsystem::IsTickable() const
{
	return bReady && !IsTemplate();
}

UWorld* UMusicSubsystem::GetTickableGameObjectWorld() const
{
	const UGameInstance* Instance = GetGameInstance();
	return Instance ? Instance->GetWorld() : nullptr;
}

void UMusicSubsystem::ReadWorld(UWorld* World, double Now)
{
	const APlayerController* Controller = World->GetFirstPlayerController();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;

	// at a place of a boat (seated or standing), or aboard the galleon: their own tracks (T-259), and the crew sails
	bool bInBoat = false;
	bool bAboardGalleon = false;
	if (Pawn)
	{
		const UBoatRiderComponent* Rider = Pawn->FindComponentByClass<UBoatRiderComponent>();
		bInBoat = Rider && Rider->IsSeated();
		for (TActorIterator<AGalleonActor> It(World); It && !bInBoat && !bAboardGalleon; ++It)
		{
			bAboardGalleon = It->IsActorAboard(Pawn);
		}
	}
	const bool bSailing = bInBoat || bAboardGalleon;
	const EMusicPlace Place = MusicMath::PickPlace(bInBoat, bAboardGalleon);

	// an expedition plays no island song until the player sets foot on the island (owner 2026-10-07): swimming out to the
	// island stays silent, the galleon and the boat have their own tracks (T-259); the menus and the maps without
	// expedition always play
	if (LandedWorld.Get() != World)
	{
		LandedWorld = World;
		bLanded = false;
	}
	bool bOnIsland = true;
	if (World->GetGameState<AExpeditionGameState>())
	{
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		const UCharacterMovementComponent* Walk = Character ? Character->GetCharacterMovement() : nullptr;
		const UPrimitiveComponent* Base = Character ? Cast<UPrimitiveComponent>(Character->GetMovementBaseObject()) : nullptr;
		const AActor* Floor = Base ? Base->GetOwner() : nullptr;
		const bool bOnBoard = Floor && (Floor->IsA<AGalleonActor>() || Floor->IsA<ABoatActor>());
		if (!Character || bSailing)
		{
			bLanded = false;
		}
		else if (Walk && Walk->IsMovingOnGround() && !bOnBoard)
		{
			bLanded = true;
		}
		bOnIsland = bLanded;
	}

	// night on the island: the Hawaiian song is lowered with the daylight at dusk, the night song comes with the night
	bool bNight = false;
	DaylightFactor = 1.0f;
	if (const ADayNightCycleActor* Cycle = ADayNightCycleActor::Get(World))
	{
		const EDayPhase Phase = Cycle->GetPhase();
		bNight = Phase == EDayPhase::Night;
		if (!bSailing && (Phase == EDayPhase::Dusk || Phase == EDayPhase::Night))
		{
			DaylightFactor = MusicMath::DaylightVolume(Cycle->GetDaylight(), Rules->DaylightSilent, Rules->DaylightFull);
		}
	}

	// a flag rising nearby: the British song, a minute at most
	const AFlagCeremonySite* Site = Pawn ? AFlagCeremonySite::FindNearest(World, Pawn->GetActorLocation(), Rules->CeremonyRadius) : nullptr;
	const bool bRaising = Site && Site->GetPhase() == EFlagCeremonyPhase::Raising;
	if (bRaising && !bCeremonyWasNear)
	{
		CrownStart = Now;
	}
	bCeremonyWasNear = bRaising;
	if (!bRaising)
	{
		CrownStart = -1.0;
	}
	const bool bCrown = bRaising && CrownStart >= 0.0 && Now - CrownStart < Rules->CrownSeconds;

	const EMusicTrack Before = Wanted;
	Wanted = MusicMath::PickTrack(bCrown, bNight, bOnIsland, Place);
	if (Wanted != Before)
	{
		static const TCHAR* const Names[] = { TEXT("Hawaiian"), TEXT("Night"), TEXT("Crown"), TEXT("Galleon"), TEXT("Boat"), TEXT("none") };
		static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EMusicTrack::Count) + 1, "a name per track, and none");
		UE_LOG(Logmonke_island, Log, TEXT("Music: %s (night %d, boat %d, galleon %d, on the island %d, flag %d)"),
			Names[FMath::Min(static_cast<int32>(Wanted), static_cast<int32>(EMusicTrack::Count))], bNight, bInBoat, bAboardGalleon, bOnIsland, bCrown);
	}
}

UAudioComponent* UMusicSubsystem::GetComponent(EMusicTrack Track, UWorld* World)
{
	const int32 Index = static_cast<int32>(Track);
	if (Components[Index])
	{
		return Components[Index];
	}
	USoundBase* Sound = nullptr;
	switch (Track)
	{
	case EMusicTrack::Hawaiian:	Sound = Rules->Hawaiian.Get(); break;
	case EMusicTrack::Night:	Sound = Rules->Night.Get(); break;
	case EMusicTrack::Crown:	Sound = Rules->Crown.Get(); break;
	case EMusicTrack::Galleon:	Sound = Rules->Galleon.Get(); break;
	case EMusicTrack::Boat:		Sound = Rules->Boat.Get(); break;
	default:					break;
	}
	if (!Sound)
	{
		return nullptr;
	}
	// a 2D sound that goes on from map to map (menus, galleon, islands) and through the pause menu
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(World, Sound, 0.001f, 1.0f, 0.0f, nullptr, true, false);
	if (Component)
	{
		Component->bIsUISound = true;
		Component->Play();
		UE_LOG(Logmonke_island, Log, TEXT("Music: %s starts"), *Sound->GetName());
	}
	Components[Index] = Component;
	return Component;
}

void UMusicSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetTickableGameObjectWorld();
	if (!World || !Rules)
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now >= NextCheck)
	{
		NextCheck = Now + Rules->CheckInterval;
		ReadWorld(World, Now);
	}
	const float FullVolumes[] = { Rules->HawaiianVolume, Rules->NightVolume, Rules->CrownVolume, Rules->GalleonVolume, Rules->BoatVolume };
	static_assert(UE_ARRAY_COUNT(FullVolumes) == static_cast<int32>(EMusicTrack::Count), "a volume per track");
	for (int32 Index = 0; Index < static_cast<int32>(EMusicTrack::Count); ++Index)
	{
		const EMusicTrack Track = static_cast<EMusicTrack>(Index);
		float Target = Track == Wanted ? 1.0f : 0.0f;
		if (Track == EMusicTrack::Hawaiian)
		{
			Target *= DaylightFactor;
		}
		const float Before = Volumes[Index];
		Volumes[Index] = MusicMath::StepFade(Before, Target, DeltaTime, Track == EMusicTrack::Night ? Rules->NightFadeSeconds : Rules->FadeSeconds);
		UAudioComponent* Component = Components[Index];
		if (Volumes[Index] > 0.0f || Target > 0.0f)
		{
			const bool bNew = !Component;
			Component = Component ? Component : GetComponent(Track, World);
			if (!Component)
			{
				continue;
			}
			if (Track == EMusicTrack::Crown && Before <= 0.0f && !bNew)
			{
				// the British song starts over at each flag
				Component->Play(0.0f);
			}
			else if (Component->GetPlayState() == EAudioComponentPlayState::Paused)
			{
				Component->SetPaused(false);
			}
			Component->SetVolumeMultiplier(FMath::Max(Volumes[Index] * FullVolumes[Index], 0.001f));
		}
		else if (Component && Component->GetPlayState() == EAudioComponentPlayState::Playing)
		{
			// silent for good: paused, it goes on from there next time
			Component->SetPaused(true);
		}
	}
}
