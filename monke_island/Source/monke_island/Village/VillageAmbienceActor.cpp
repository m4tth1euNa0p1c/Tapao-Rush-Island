// monke_island

#include "Village/VillageAmbienceActor.h"
#include "Village/VillageAmbienceMath.h"
#include "Village/VillageAmbienceRules.h"
#include "DayNight/DayNightCycleActor.h"
#include "monke_island.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AVillageAmbienceActor::AVillageAmbienceActor()
{
	// cosmetic animation: ticks every frame only while a local camera is near (see Tick)
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	bReplicates = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	for (TObjectPtr<UInstancedStaticMeshComponent>* Slot : { &Butterflies, &Fireflies })
	{
		const FName Name = Slot == &Butterflies ? TEXT("Butterflies") : TEXT("Fireflies");
		UInstancedStaticMeshComponent* Component = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Component->SetupAttachment(RootComponent);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetCanEverAffectNavigation(false);
		Component->SetVisibility(false);
		*Slot = Component;
	}
}

void AVillageAmbienceActor::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		// nobody looks at it there: no camera, no lights to dim, no flyers
		SetActorTickEnabled(false);
		return;
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_VillageAmbience, the village stays still"), *GetName());
		SetActorTickEnabled(false);
		return;
	}
	Random.Initialize(GetTypeHash(VillageId) ^ GetTypeHash(GetActorLocation().X));
	FindLights();
	if (Rules->ButterflyMesh)
	{
		Butterflies->SetStaticMesh(Rules->ButterflyMesh);
		InitFlyers(ButterflyState, Butterflies, Rules->ButterflyCount, Rules->ButterflyHeight, Rules->ButterflyRadiusFraction);
	}
	if (Rules->FireflyMesh)
	{
		Fireflies->SetStaticMesh(Rules->FireflyMesh);
		InitFlyers(FireflyState, Fireflies, Rules->FireflyCount, Rules->FireflyHeight, Rules->ButterflyRadiusFraction);
	}
	UpdateLights();
	UE_LOG(Logmonke_island, Log, TEXT("%s: village %s, %d lights, %d butterflies, %d fireflies"),
		*GetName(), *VillageId.ToString(), Lights.Num(), ButterflyState.Num(), FireflyState.Num());
}

float AVillageAmbienceActor::GetDaylight() const
{
	const ADayNightCycleActor* Cycle = ADayNightCycleActor::Get(this);
	return Cycle ? Cycle->GetDaylight() : 1.0f;
}

bool AVillageAmbienceActor::IsLocalViewNear() const
{
	const double RadiusSq = FMath::Square(static_cast<double>(Rules->ActiveRadius));
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (Controller && Controller->IsLocalController() && Controller->PlayerCameraManager
			&& FVector::DistSquared2D(Controller->PlayerCameraManager->GetCameraLocation(), GetActorLocation()) <= RadiusSq)
		{
			return true;
		}
	}
	return false;
}

void AVillageAmbienceActor::FindLights()
{
	static const FName LightTag(TEXT("VillageLight"));
	static const FName LanternTag(TEXT("LightKind.Lantern"));
	const FName VillageTag(*FString::Printf(TEXT("Village.%s"), *VillageId.ToString()));
	Lights.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(LightTag) || !It->ActorHasTag(VillageTag))
		{
			continue;
		}
		const bool bInterior = !It->ActorHasTag(LanternTag);
		TInlineComponentArray<ULightComponent*> Components(*It);
		for (ULightComponent* Light : Components)
		{
			FVillageLight& Entry = Lights.AddDefaulted_GetRef();
			Entry.Light = Light;
			Entry.BaseIntensity = Light->Intensity;
			Entry.bInterior = bInterior;
		}
	}
}

void AVillageAmbienceActor::UpdateLights()
{
	const float Daylight = GetDaylight();
	for (const FVillageLight& Entry : Lights)
	{
		ULightComponent* Light = Entry.Light.Get();
		if (!Light)
		{
			continue;
		}
		const float Factor = VillageAmbienceMath::LightFactor(Daylight, Entry.bInterior ? Rules->InteriorDayFactor : Rules->LanternDayFactor);
		const float Target = Entry.BaseIntensity * Factor;
		if (FMath::Abs(Light->Intensity - Target) > 0.01f * FMath::Max(Entry.BaseIntensity, 1.0f))
		{
			Light->SetIntensity(Target);
		}
		const bool bVisible = Factor > 0.01f;
		if (Light->IsVisible() != bVisible)
		{
			Light->SetVisibility(bVisible);
		}
	}
}

FVector AVillageAmbienceActor::NewGoal(const FVector2D& Height, float RadiusFraction)
{
	return VillageAmbienceMath::GoalInDisc(GetActorLocation(), VillageRadius * RadiusFraction, Height.X, Height.Y,
		Random.FRand(), Random.FRand(), Random.FRand());
}

void AVillageAmbienceActor::InitFlyers(TArray<FFlyer>& Flyers, UInstancedStaticMeshComponent* Component, int32 Count, const FVector2D& Height, float RadiusFraction)
{
	Flyers.SetNum(Count);
	TArray<FTransform> Transforms;
	Transforms.Reserve(Count);
	for (FFlyer& Flyer : Flyers)
	{
		Flyer.Location = NewGoal(Height, RadiusFraction);
		Flyer.Goal = NewGoal(Height, RadiusFraction);
		Flyer.Phase = Random.FRandRange(0.0f, 2.0f * PI);
		Transforms.Add(FTransform(Flyer.Location));
	}
	Component->ClearInstances();
	Component->AddInstances(Transforms, false, true);
}

void AVillageAmbienceActor::UpdateButterflies(float DeltaSeconds)
{
	TArray<FTransform> Transforms;
	Transforms.Reserve(ButterflyState.Num());
	for (FFlyer& Flyer : ButterflyState)
	{
		if (FVector::DistSquared(Flyer.Location, Flyer.Goal) < FMath::Square(40.0))
		{
			Flyer.Goal = NewGoal(Rules->ButterflyHeight, Rules->ButterflyRadiusFraction);
		}
		// fluttering: a zigzag around the straight line to the goal
		const FVector Wobble(FMath::Sin(Clock * 2.7f + Flyer.Phase) * 70.0f, FMath::Cos(Clock * 2.1f + Flyer.Phase) * 70.0f,
			FMath::Sin(Clock * 4.3f + Flyer.Phase) * 45.0f);
		const FVector Previous = Flyer.Location;
		Flyer.Location = VillageAmbienceMath::StepToward(Flyer.Location, Flyer.Goal, Rules->ButterflySpeed, Wobble, DeltaSeconds);
		const FVector Heading = (Flyer.Location - Previous).GetSafeNormal2D();
		const float Yaw = Heading.IsNearlyZero() ? 0.0f : static_cast<float>(Heading.Rotation().Yaw);
		const float Span = VillageAmbienceMath::FlapSpan(Clock, Rules->ButterflyFlapHz, Flyer.Phase);
		Transforms.Add(FTransform(FRotator(0.0f, Yaw, 0.0f), Flyer.Location, FVector(1.0f, Span, 1.0f)));
	}
	Butterflies->BatchUpdateInstancesTransforms(0, Transforms, true, true, false);
}

void AVillageAmbienceActor::UpdateFireflies(float DeltaSeconds)
{
	TArray<FTransform> Transforms;
	Transforms.Reserve(FireflyState.Num());
	for (FFlyer& Flyer : FireflyState)
	{
		if (FVector::DistSquared(Flyer.Location, Flyer.Goal) < FMath::Square(30.0))
		{
			Flyer.Goal = NewGoal(Rules->FireflyHeight, Rules->ButterflyRadiusFraction);
		}
		const FVector Wobble(FMath::Sin(Clock * 0.9f + Flyer.Phase) * 20.0f, FMath::Cos(Clock * 0.7f + Flyer.Phase) * 20.0f,
			FMath::Sin(Clock * 1.3f + Flyer.Phase) * 15.0f);
		Flyer.Location = VillageAmbienceMath::StepToward(Flyer.Location, Flyer.Goal, Rules->FireflySpeed, Wobble, DeltaSeconds);
		const float Glow = VillageAmbienceMath::Blink(Clock, Rules->FireflyBlinkHz, Flyer.Phase);
		Transforms.Add(FTransform(FQuat::Identity, Flyer.Location, FVector(Glow)));
	}
	Fireflies->BatchUpdateInstancesTransforms(0, Transforms, true, true, false);
}

void AVillageAmbienceActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules)
	{
		return;
	}
	Clock += DeltaSeconds;

	NearTimer -= DeltaSeconds;
	if (NearTimer <= 0.0f)
	{
		NearTimer = 0.5f;
		bNear = IsLocalViewNear();
		// far from every local camera: a light update twice a second is all it needs
		SetActorTickInterval(bNear ? 0.0f : 0.5f);
	}
	LightTimer -= DeltaSeconds;
	if (LightTimer <= 0.0f)
	{
		LightTimer = Rules->LightUpdateInterval;
		UpdateLights();
	}

	const bool bNight = GetDaylight() < Rules->NightDaylight;
	const bool bWantButterflies = bNear && !bNight && ButterflyState.Num() > 0;
	const bool bWantFireflies = bNear && bNight && FireflyState.Num() > 0;
	if (bWantButterflies != bShowButterflies)
	{
		bShowButterflies = bWantButterflies;
		Butterflies->SetVisibility(bShowButterflies);
	}
	if (bWantFireflies != bShowFireflies)
	{
		bShowFireflies = bWantFireflies;
		Fireflies->SetVisibility(bShowFireflies);
	}
	if (bShowButterflies)
	{
		UpdateButterflies(DeltaSeconds);
	}
	if (bShowFireflies)
	{
		UpdateFireflies(DeltaSeconds);
	}
}
