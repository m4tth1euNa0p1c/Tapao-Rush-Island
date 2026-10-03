// monke_island

#include "DayNight/DayNightCycleActor.h"
#include "DayNight/DayNightMath.h"
#include "DayNight/DayNightRules.h"
#include "monke_island.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

ADayNightCycleActor::ADayNightCycleActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(RootComponent);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->bAtmosphereSunLight = true;
	Moon->AtmosphereSunLightIndex = 1;
	Moon->ForwardShadingPriority = -1;
	Moon->bUseTemperature = true;
	Moon->Intensity = 0.0f;
	Moon->SetVisibility(false);

	NightExposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("NightExposure"));
	NightExposure->SetupAttachment(RootComponent);
	NightExposure->bUnbound = true;
	NightExposure->Priority = 10.0f;
}

ADayNightCycleActor* ADayNightCycleActor::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ADayNightCycleActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADayNightCycleActor::BeginPlay()
{
	Super::BeginPlay();

	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_DayNightRules: using the default rules"), *GetName());
		Rules = NewObject<UDayNightRules>(this);
	}
	if (Rules->bLoop && !DayNightMath::IsLooping(*Rules))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: the clock keys do not span exactly 24 h, the cycle stops at the last key"), *GetName());
	}

	if (HasAuthority())
	{
		Clock.AnchorServerTime = GetServerTime();
		Clock.AnchorElapsed = StartElapsedSeconds;
		Clock.Speed = 1.0f;
	}

	BindLevelLights();
	SetActorTickInterval(Rules->UpdateInterval);
	UpdateLighting(true);
}

void ADayNightCycleActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLighting(false);

	// the night has passed: a new day on every machine (pure function of the replicated clock)
	const int32 Day = GetDayNumber();
	if (Day != LastDay)
	{
		LastDay = Day;
		UE_LOG(Logmonke_island, Log, TEXT("Day %d begins"), Day);
		OnDayChanged.Broadcast(Day);
	}
}

int32 ADayNightCycleActor::GetDayNumber() const
{
	return Rules ? DayNightMath::DayNumberAt(*Rules, GetElapsedSeconds()) : 1;
}

double ADayNightCycleActor::GetDayStartElapsed(int32 Day) const
{
	return Rules ? DayNightMath::DayStartElapsed(*Rules, Day) : -1.0;
}

void ADayNightCycleActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADayNightCycleActor, Clock);
}

double ADayNightCycleActor::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

double ADayNightCycleActor::GetElapsedSeconds() const
{
	return FMath::Max(0.0, Clock.AnchorElapsed + (GetServerTime() - Clock.AnchorServerTime) * Clock.Speed);
}

float ADayNightCycleActor::GetSolarHour() const
{
	return Rules ? static_cast<float>(FMath::Fmod(DayNightMath::SolarHourAt(*Rules, GetElapsedSeconds()), 24.0)) : 12.0f;
}

void ADayNightCycleActor::SetSolarHour(float Hour)
{
	if (!HasAuthority() || !Rules)
	{
		return;
	}
	const double Now = GetServerTime();
	const double Elapsed = GetElapsedSeconds();
	Clock.AnchorElapsed = DayNightMath::ElapsedForHour(*Rules, FMath::Fmod(FMath::Max(Hour, 0.0f), 24.0f), Elapsed);
	Clock.AnchorServerTime = Now;
	UpdateLighting(true);
}

void ADayNightCycleActor::SetElapsedSeconds(double Seconds)
{
	if (!HasAuthority())
	{
		return;
	}
	Clock.AnchorElapsed = FMath::Max(0.0, Seconds);
	Clock.AnchorServerTime = GetServerTime();
	UpdateLighting(true);
}

void ADayNightCycleActor::SetClockSpeed(float NewSpeed)
{
	if (!HasAuthority())
	{
		return;
	}
	const double Now = GetServerTime();
	Clock.AnchorElapsed = GetElapsedSeconds();
	Clock.AnchorServerTime = Now;
	Clock.Speed = FMath::Max(NewSpeed, 0.0f);
}

void ADayNightCycleActor::BindLevelLights()
{
	UWorld* World = GetWorld();
	if (!Sun)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			const UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			if (Light && Light->bAtmosphereSunLight && Light->AtmosphereSunLightIndex == 0)
			{
				Sun = *It;
				break;
			}
		}
	}
	if (!SkyLight)
	{
		TActorIterator<ASkyLight> It(World);
		SkyLight = It ? *It : nullptr;
	}
	if (!HeightFog)
	{
		TActorIterator<AExponentialHeightFog> It(World);
		HeightFog = It ? *It : nullptr;
	}

	if (UDirectionalLightComponent* SunLight = Sun ? Cast<UDirectionalLightComponent>(Sun->GetLightComponent()) : nullptr)
	{
		SunLight->SetMobility(EComponentMobility::Movable);
		SunLight->SetUseTemperature(true);
		SunNoonIntensity = SunLight->Intensity;
		Moon->SetCastShadows(Rules->bMoonCastsShadows);
	}
	else
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s found no atmosphere sun light in the level: only the moon follows the cycle"), *GetName());
	}
	if (const UExponentialHeightFogComponent* Fog = HeightFog ? HeightFog->GetComponent() : nullptr)
	{
		FogDayInscattering = Fog->FogInscatteringLuminance;
		FogDayDirectionalInscattering = Fog->DirectionalInscatteringLuminance;
		FogDayDensity = Fog->FogDensity;
	}

	FPostProcessSettings& Settings = NightExposure->Settings;
	Settings.bOverride_AutoExposureBias = true;
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.AutoExposureMinBrightness = Rules->MinExposureEV100;
}

void ADayNightCycleActor::UpdateLighting(bool bForce)
{
	if (!Rules)
	{
		return;
	}

	const double Hour = DayNightMath::SolarHourAt(*Rules, GetElapsedSeconds());
	const FVector SunENU = DayNightMath::SunDirectionENU(Hour, Rules->LatitudeDeg, Rules->DayOfYear);
	const FVector SunDir = DayNightMath::ENUToWorld(SunENU, Rules->NorthYawDeg);
	const double SunElevation = DayNightMath::ElevationDeg(SunDir);

	// Sun: on the real solar path, fading under the horizon, warm when low
	const float SunFactor = FMath::SmoothStep(Rules->SunOffElevationDeg, Rules->SunFullElevationDeg, static_cast<float>(SunElevation));
	if (UDirectionalLightComponent* SunLight = Sun ? Cast<UDirectionalLightComponent>(Sun->GetLightComponent()) : nullptr)
	{
		Sun->SetActorRotation((-SunDir).Rotation());
		SunLight->SetIntensity(SunNoonIntensity * SunFactor * FMath::Lerp(1.0f, Rules->StormSunFraction, StormFactor));
		const float WarmAlpha = FMath::Clamp(static_cast<float>(SunElevation) / Rules->SunNoonTemperatureElevationDeg, 0.0f, 1.0f);
		SunLight->SetTemperature(FMath::Lerp(Rules->SunHorizonTemperature, Rules->SunNoonTemperature, WarmAlpha));
		SunLight->SetVisibility(SunFactor > 0.0f);
	}

	// Moon: full moon opposite the sun, kept high enough to light the island
	FVector MoonENU = -SunENU;
	const double MinUp = FMath::Sin(FMath::DegreesToRadians(Rules->MoonMinElevationDeg));
	if (MoonENU.Z < MinUp)
	{
		const FVector Flat = FVector(MoonENU.X, MoonENU.Y, 0.0).GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0, 1.0, 0.0));
		MoonENU = Flat * FMath::Sqrt(1.0 - MinUp * MinUp) + FVector(0.0, 0.0, MinUp);
	}
	const float MoonFactor = 1.0f - FMath::SmoothStep(-Rules->TwilightElevationDeg * 2.0f, 0.0f, static_cast<float>(SunElevation));
	Moon->SetWorldRotation((-DayNightMath::ENUToWorld(MoonENU, Rules->NorthYawDeg)).Rotation());
	Moon->SetIntensity(SunNoonIntensity * Rules->MoonIntensityFraction * MoonFactor);
	Moon->SetTemperature(Rules->MoonTemperature);
	Moon->SetVisibility(MoonFactor > 0.0f);

	// Daylight drives the exposure and the fog
	Daylight = FMath::SmoothStep(-Rules->TwilightElevationDeg * 2.0f, Rules->TwilightElevationDeg, static_cast<float>(SunElevation));
	NightExposure->Settings.AutoExposureBias = FMath::Lerp(Rules->NightExposureBias, 0.0f, Daylight) + Rules->StormExposureBias * StormFactor;
	if (UExponentialHeightFogComponent* Fog = HeightFog ? HeightFog->GetComponent() : nullptr)
	{
		const float FogFactor = FMath::Lerp(Rules->NightFogFraction, 1.0f, Daylight);
		Fog->SetFogInscatteringColor(FogDayInscattering * FogFactor);
		Fog->SetDirectionalInscatteringColor(FogDayDirectionalInscattering * SunFactor * (1.0f - StormFactor));
		Fog->SetFogDensity(FogDayDensity * FMath::Lerp(1.0f, Rules->StormFogMultiplier, StormFactor));
	}

	// A captured (non real-time) sky light must be recaptured as the sky changes
	if (USkyLightComponent* Sky = SkyLight ? SkyLight->GetLightComponent() : nullptr)
	{
		if (!Sky->bRealTimeCapture && (bForce || FMath::Abs(Hour - LastSkyCaptureHour) >= Rules->SkyRecaptureHourStep))
		{
			Sky->RecaptureSky();
			LastSkyCaptureHour = Hour;
		}
	}

	// Phase from the sun elevation
	EDayPhase NewPhase = EDayPhase::Day;
	if (SunElevation < -Rules->TwilightElevationDeg)
	{
		NewPhase = EDayPhase::Night;
	}
	else if (SunElevation < Rules->TwilightElevationDeg)
	{
		NewPhase = FMath::Fmod(Hour, 24.0) >= 12.0 ? EDayPhase::Dusk : EDayPhase::Dawn;
	}
	if (NewPhase != Phase)
	{
		Phase = NewPhase;
		OnPhaseChanged.Broadcast(Phase);
	}
}

#if !UE_BUILD_SHIPPING

static FAutoConsoleCommandWithWorldAndArgs GSetHourCommand(
	TEXT("monke.SetHour"),
	TEXT("Jumps the day / night cycle to a solar hour (server). Usage: monke.SetHour 18.5"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ADayNightCycleActor* Cycle = ADayNightCycleActor::Get(World);
		if (Cycle && Args.Num() > 0)
		{
			Cycle->SetSolarHour(FCString::Atof(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GSetTimeCommand(
	TEXT("monke.SetTime"),
	TEXT("Jumps the expedition clock to an elapsed play time in seconds (server). Usage: monke.SetTime 280"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ADayNightCycleActor* Cycle = ADayNightCycleActor::Get(World);
		if (Cycle && Args.Num() > 0)
		{
			Cycle->SetElapsedSeconds(FCString::Atod(*Args[0]));
		}
	}));

static FAutoConsoleCommandWithWorldAndArgs GTimeSpeedCommand(
	TEXT("monke.TimeSpeed"),
	TEXT("Changes the speed of the day / night clock (server). Usage: monke.TimeSpeed 10"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		ADayNightCycleActor* Cycle = ADayNightCycleActor::Get(World);
		if (Cycle && Args.Num() > 0)
		{
			Cycle->SetClockSpeed(FCString::Atof(*Args[0]));
		}
	}));

#endif
