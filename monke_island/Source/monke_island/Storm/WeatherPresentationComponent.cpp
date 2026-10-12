// monke_island

#include "Storm/WeatherPresentationComponent.h"
#include "Storm/WeatherDirector.h"
#include "Storm/WeatherMath.h"
#include "Storm/WeatherRules.h"
#include "Storm/WindStormActor.h"
#include "DayNight/DayNightCycleActor.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace WeatherPresentation
{
	/** Parameter of MPC_Weather read by the rain material: the fade of the rain */
	static const FName RainOpacity(TEXT("RainOpacity"));

	/** Tick interval while there is no weather to show: the final storm is still watched */
	static constexpr float IdleTickInterval = 0.5f;

	/** Below this, a fade is over */
	static constexpr float Faded = 0.001f;
}

UWeatherPresentationComponent::UWeatherPresentationComponent()
{
	// after the camera moved this frame: the rain is placed around the view the frame is drawn from
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

AWeatherDirector* UWeatherPresentationComponent::GetDirector() const
{
	return Cast<AWeatherDirector>(GetOwner());
}

void UWeatherPresentationComponent::Prepare(UWeatherRules* InRules)
{
	Rules = InRules;
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const FSoftObjectPath& Path : { Rules->RainSystem.ToSoftObjectPath(), Rules->MaterialParameters.ToSoftObjectPath(),
		Rules->RainSound.ToSoftObjectPath(), Rules->StormWindSound.ToSoftObjectPath(), Rules->SeizeSound.ToSoftObjectPath(),
		Rules->ThunderSound.ToSoftObjectPath() })
	{
		if (!Path.IsNull())
		{
			Paths.Add(Path);
		}
	}
	// kept loaded by the handle; a spawn before the end of the load is skipped (no hitch at the seizure)
	if (!Paths.IsEmpty())
	{
		Load = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}
	if (Rules->RainSystem.IsNull())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Weather: DA_WeatherRules has no RainSystem, the turned weather will not show rain"));
	}
	// the final storm brings rain too: watched slowly until there is weather to show
	if (Rules->bRainInFinalStorm)
	{
		SetComponentTickInterval(WeatherPresentation::IdleTickInterval);
		SetComponentTickEnabled(true);
	}
}

void UWeatherPresentationComponent::OnTurned(bool bJustNow)
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	SetComponentTickInterval(0.0f);
	SetComponentTickEnabled(true);
	if (bJustNow)
	{
		PlaySting(Rules->SeizeSound.Get());
		if (!Rules->ThunderSound.IsNull())
		{
			GetWorld()->GetTimerManager().SetTimer(ThunderTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				PlaySting(Rules ? Rules->ThunderSound.Get() : nullptr);
			}), FMath::Max(0.01f, Rules->ThunderDelaySeconds), false);
		}
	}
}

void UWeatherPresentationComponent::OnCleared()
{
	// the fades run down in the tick, which then idles
	GetWorld()->GetTimerManager().ClearTimer(ThunderTimer);
	SetComponentTickInterval(0.0f);
	SetComponentTickEnabled(true);
}

void UWeatherPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThunderTimer);
	}
	if (Load.IsValid())
	{
		Load->CancelHandle();
		Load.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UWeatherPresentationComponent::RefreshWorld()
{
	WorldLookupTime = GetWorld()->GetTimeSeconds();
	Wind = AWindStormActor::Get(this);
	Clock = ADayNightCycleActor::Get(this);
	CalmZones.Reset();
	// the wind keeps the calm zones (WindCalmZone) of its world; only looked at when they change the rain
	if (const AWindStormActor* WindActor = Wind.Get(); WindActor && Rules && Rules->RainInCalmZones < 1.0f)
	{
		CalmZones = WindActor->GetCalmZones();
	}
}

void UWeatherPresentationComponent::CheckCover(const FVector& View)
{
	UWorld* World = GetWorld();
	bUnderwater = false;
	if (const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this); Water && !Water->GetWaterSurfaces().IsEmpty())
	{
		bUnderwater = View.Z < Water->GetWaterSurfaceAt(View);
	}

	bInCalmZone = false;
	for (const FSphere& Zone : CalmZones)
	{
		if (FVector::DistSquared2D(View, Zone.Center) <= FMath::Square(Zone.W))
		{
			bInCalmZone = true;
			break;
		}
	}

	// a roof straight above the camera: the drops fall through meshes, the rain stops under it
	bUnderRoof = false;
	if (Rules->RoofCheckHeight > 0.0f && !Rules->RoofMeshKeywords.IsEmpty())
	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(WeatherRoof), false);
		World->LineTraceMultiByObjectType(Hits, View, View + FVector(0.0, 0.0, Rules->RoofCheckHeight),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params);
		for (const FHitResult& Hit : Hits)
		{
			const UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Hit.GetComponent());
			const UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
			if (Mesh && WeatherMath::HasKeyword(Mesh->GetName(), Rules->RoofMeshKeywords))
			{
				bUnderRoof = true;
				break;
			}
		}
	}
}

void UWeatherPresentationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AWeatherDirector* Director = GetDirector();
	UWorld* World = GetWorld();
	if (!Rules || !Director || !World)
	{
		SetComponentTickEnabled(false);
		return;
	}
	if (World->GetTimeSeconds() - WorldLookupTime >= 1.0)
	{
		RefreshWorld();
	}

	const float Weight = Director->GetTurnWeight();
	// the final storm (ultra violent wind of the submersion), faded here: what it changes is only seen and heard
	const AWindStormActor* WindActor = Wind.Get();
	FinalStorm = WeatherMath::Approach(FinalStorm, WindActor && WindActor->IsDeadlineWind() ? 1.0f : 0.0f, DeltaTime,
		FMath::Max(Rules->TransitionSeconds, 0.1f));
	UpdateSky(Director->GetSkyFactor());

	// nothing to show nor to fade: idle, without a look at the camera (the final storm is still watched when it brings rain)
	const bool bIdle = Weight <= 0.0f && FinalStorm <= WeatherPresentation::Faded && RainIntensity <= WeatherPresentation::Faded
		&& LoopFactor <= WeatherPresentation::Faded && AppliedFallbackSky <= WeatherPresentation::Faded;
	if (bIdle)
	{
		if (RainEffect && RainEffect->IsActive())
		{
			RainEffect->Deactivate();
		}
		SetComponentTickInterval(WeatherPresentation::IdleTickInterval);
		SetComponentTickEnabled(Rules->bRainInFinalStorm);
		return;
	}
	SetComponentTickInterval(0.0f);

	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(World, 0))
	{
		const FVector View = Camera->GetCameraLocation();
		CoverTimer -= DeltaTime;
		if (CoverTimer <= 0.0f)
		{
			CoverTimer = Rules->CoverCheckInterval;
			CheckCover(View);
		}
		const float RainTarget = WeatherMath::RainTarget(*Rules, Weight, FinalStorm, bUnderRoof, bUnderwater, bInCalmZone);
		RainIntensity = WeatherMath::Approach(RainIntensity, RainTarget, DeltaTime, Rules->RainFadeSeconds);
		UpdateRain(View);
	}
	LoopFactor = WeatherMath::Approach(LoopFactor, WeatherMath::LoopVolume(*Rules, Weight, FinalStorm, bUnderRoof, bUnderwater),
		DeltaTime, Rules->RainFadeSeconds);
	UpdateLoops();
}

void UWeatherPresentationComponent::UpdateRain(const FVector& View)
{
	UWorld* World = GetWorld();
	if (!Parameters)
	{
		Parameters = Rules->MaterialParameters.Get();
	}
	if (UMaterialParameterCollectionInstance* Instance = Parameters ? World->GetParameterCollectionInstance(Parameters) : nullptr)
	{
		Instance->SetScalarParameterValue(WeatherPresentation::RainOpacity, RainIntensity);
	}

	const bool bRaining = RainIntensity > WeatherPresentation::Faded;
	if (!RainEffect)
	{
		UNiagaraSystem* System = bRaining ? Rules->RainSystem.Get() : nullptr;
		if (!System)
		{
			return;
		}
		// world units around the camera, never attached: the director's transform means nothing to it
		RainEffect = NewObject<UNiagaraComponent>(GetOwner(), TEXT("RainEffect"));
		RainEffect->SetAsset(System);
		RainEffect->SetAutoActivate(false);
		RainEffect->SetUsingAbsoluteLocation(true);
		RainEffect->SetUsingAbsoluteRotation(true);
		RainEffect->SetUsingAbsoluteScale(true);
		RainEffect->SetCastShadow(false);
		RainEffect->SetupAttachment(GetOwner()->GetRootComponent());
		RainEffect->RegisterComponent();
		UE_LOG(Logmonke_island, Log, TEXT("Weather: rain %s around the local camera"), *System->GetName());
	}

	// the drops lean downwind; their volume sits upwind so the slanted drops still cross the view
	const FVector WindVector = Wind.IsValid() ? Wind->GetWind() : FVector::ZeroVector;
	const FVector Fall = WeatherMath::RainFallDirection(WindVector, WeatherMath::RainTiltDeg(*Rules, static_cast<float>(WindVector.Size())));
	const FVector Upwind = -FVector(Fall.X, Fall.Y, 0.0) * (Rules->RainHeight / FMath::Max(0.2, -Fall.Z));
	RainEffect->SetWorldLocationAndRotation(View + FVector(0.0, 0.0, Rules->RainHeight) + Upwind,
		FQuat::FindBetweenNormals(FVector::DownVector, Fall));

	if (bRaining && !RainEffect->IsActive())
	{
		RainEffect->Activate(true);
	}
	else if (!bRaining && RainEffect->IsActive())
	{
		// the last drops finish falling, nothing new spawns
		RainEffect->Deactivate();
	}
}

UAudioComponent* UWeatherPresentationComponent::MakeLoop(USoundBase* Sound, FName Name)
{
	if (!Sound)
	{
		return nullptr;
	}
	UAudioComponent* Loop = NewObject<UAudioComponent>(GetOwner(), Name);
	Loop->SetSound(Sound);
	Loop->bAutoActivate = false;
	// heard the same everywhere: the weather is around the listener, not at the director
	Loop->bAllowSpatialization = false;
	Loop->bIsUISound = false;
	Loop->SetupAttachment(GetOwner()->GetRootComponent());
	Loop->RegisterComponent();
	return Loop;
}

void UWeatherPresentationComponent::UpdateLoops()
{
	struct FLoopSpec
	{
		TObjectPtr<UAudioComponent>* Component;
		USoundBase* Sound;
		float Volume;
		FName Name;
	};
	const FLoopSpec Specs[] = {
		{ &RainLoop, Rules->RainSound.Get(), Rules->RainVolume, TEXT("RainLoop") },
		{ &WindLoop, Rules->StormWindSound.Get(), Rules->StormWindVolume, TEXT("StormWindLoop") },
	};
	for (const FLoopSpec& Spec : Specs)
	{
		const float Volume = LoopFactor * Spec.Volume;
		UAudioComponent* Loop = Spec.Component->Get();
		if (Volume <= WeatherPresentation::Faded)
		{
			if (Loop && Loop->IsPlaying())
			{
				Loop->Stop();
			}
			continue;
		}
		if (!Loop)
		{
			Loop = MakeLoop(Spec.Sound, Spec.Name);
			*Spec.Component = Loop;
		}
		if (Loop)
		{
			Loop->SetVolumeMultiplier(Volume);
			// muffled under a roof
			Loop->SetLowPassFilterEnabled(bUnderRoof);
			Loop->SetLowPassFilterFrequency(Rules->IndoorLowPassHz);
			if (!Loop->IsPlaying())
			{
				Loop->Play();
			}
		}
	}
}

void UWeatherPresentationComponent::UpdateSky(float SkyFactor)
{
	if (ADayNightCycleActor* DayNight = Clock.Get())
	{
		// the wind merges the weather into the storm factor of the cycle; a map without wind, here
		if (!Wind.IsValid())
		{
			DayNight->SetStormFactor(SkyFactor);
		}
		return;
	}
	// a map without day / night cycle (test maps): its lights as placed, darkened
	if (FMath::IsNearlyEqual(SkyFactor, AppliedFallbackSky, 0.002f) || (AppliedFallbackSky < 0.0f && SkyFactor <= 0.0f))
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!Fallback.bCaptured)
	{
		Fallback.bCaptured = true;
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			// ALight::GetLightComponent: ADirectionalLight::GetComponent exists only in editor builds
			UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(It->GetLightComponent());
			// the atmosphere sun first, else the first directional light
			if (Light && (!Fallback.Sun.IsValid() || Light->IsUsedAsAtmosphereSunLight()))
			{
				Fallback.Sun = Light;
				Fallback.SunIntensity = Light->Intensity;
				if (Light->IsUsedAsAtmosphereSunLight())
				{
					break;
				}
			}
		}
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			if (USkyLightComponent* Sky = It->GetLightComponent())
			{
				Fallback.SkyLight = Sky;
				Fallback.SkyLightIntensity = Sky->Intensity;
				break;
			}
		}
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			if (UExponentialHeightFogComponent* Fog = It->GetComponent())
			{
				Fallback.Fog = Fog;
				Fallback.FogDensity = Fog->FogDensity;
				break;
			}
		}
		UE_LOG(Logmonke_island, Log, TEXT("Weather: no day / night cycle in this map, the storm darkens its lights (sun %s, sky light %s, fog %s)"),
			*GetNameSafe(Fallback.Sun.Get()), *GetNameSafe(Fallback.SkyLight.Get()), *GetNameSafe(Fallback.Fog.Get()));
	}
	if (ULightComponent* Sun = Fallback.Sun.Get())
	{
		Sun->SetIntensity(Fallback.SunIntensity * FMath::Lerp(1.0f, Rules->FallbackSunFraction, SkyFactor));
	}
	if (USkyLightComponent* Sky = Fallback.SkyLight.Get())
	{
		Sky->SetIntensity(Fallback.SkyLightIntensity * FMath::Lerp(1.0f, Rules->FallbackSkyLightFraction, SkyFactor));
	}
	if (UExponentialHeightFogComponent* Fog = Fallback.Fog.Get())
	{
		Fog->SetFogDensity(Fallback.FogDensity * FMath::Lerp(1.0f, Rules->FallbackFogMultiplier, SkyFactor));
	}
	if (!FallbackExposure)
	{
		FallbackExposure = NewObject<UPostProcessComponent>(GetOwner(), TEXT("StormExposure"));
		FallbackExposure->bUnbound = true;
		FallbackExposure->Priority = 1.0f;
		FallbackExposure->SetupAttachment(GetOwner()->GetRootComponent());
		FallbackExposure->RegisterComponent();
	}
	FallbackExposure->Settings.bOverride_AutoExposureBias = true;
	FallbackExposure->Settings.AutoExposureBias = Rules->FallbackExposureBias * SkyFactor;
	AppliedFallbackSky = SkyFactor;
}

void UWeatherPresentationComponent::PlaySting(USoundBase* Sound) const
{
	if (Sound && Rules)
	{
		UGameplayStatics::PlaySound2D(this, Sound, Rules->StingVolume, 1.0f, 0.0f, nullptr, GetOwner(), false);
	}
}
