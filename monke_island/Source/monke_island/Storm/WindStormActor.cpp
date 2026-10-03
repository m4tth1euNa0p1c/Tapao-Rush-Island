// monke_island

#include "Storm/WindStormActor.h"
#include "Storm/WindRules.h"
#include "DayNight/DayNightCycleActor.h"
#include "monke_island.h"
#include "Components/PrimitiveComponent.h"
#include "Components/WindDirectionalSourceComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

namespace
{
	/** Name of the wind drift root motion on characters */
	const FName StormDriftName(TEXT("StormWindDrift"));

	/** Actors with this tag are not pushed by the wind (dense objects in free fall, e.g. a falling coconut) */
	const FName WindShelteredTag(TEXT("WindSheltered"));
}

AWindStormActor::AWindStormActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	EngineWind = CreateDefaultSubobject<UWindDirectionalSourceComponent>(TEXT("EngineWind"));
	EngineWind->SetupAttachment(RootComponent);
	EngineWind->SetMobility(EComponentMobility::Movable);
	EngineWind->Strength = 0.0f;
}

void AWindStormActor::BeginPlay()
{
	Super::BeginPlay();

	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_WindRules: using the default rules"), *GetName());
		Rules = NewObject<UWindRules>(this);
	}
	MaterialParameters = Rules->MaterialParameters.LoadSynchronous();
}

FVector AWindStormActor::WindAt(const UWindRules& InRules, double Elapsed)
{
	const double T = Elapsed - InRules.StartSeconds;
	if (T <= 0.0)
	{
		return FVector::ZeroVector;
	}
	const float Ramp = FMath::SmoothStep(0.0f, 1.0f, static_cast<float>(T / InRules.RampSeconds));
	const float Phase = static_cast<float>(T * InRules.GustFrequency);
	// two octaves of noise: long surges and short gusts, never below a third of the mean
	const float Gust = 0.65f * FMath::PerlinNoise1D(Phase) + 0.35f * FMath::PerlinNoise1D(Phase * 3.1f + 17.0f);
	const float Strength = Ramp * FMath::Max(0.33f, 1.0f + InRules.GustAmplitude * Gust * 1.6f);
	const float Yaw = InRules.BlowTowardYawDeg + InRules.DirectionSwingDeg * FMath::PerlinNoise1D(Phase * 0.4f + 41.0f) * 1.6f;
	return FRotator(0.0, Yaw, 0.0).Vector() * Strength;
}

FVector AWindStormActor::GetWindAccelerationOn(const AActor* Actor) const
{
	const UPrimitiveComponent* Body = Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
	if (!Rules || !Body || Wind.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	const FVector Extent = Body->Bounds.BoxExtent;
	const float AreaM2 = static_cast<float>(4.0 * FMath::Max(Extent.X, Extent.Y) * Extent.Z / 10000.0);
	const float MassKg = FMath::Max(Body->GetMass(), 0.1f);
	const float Strength = static_cast<float>(Wind.Size());
	const float Acceleration = FMath::Min(Rules->WindPressurePa * Strength * Strength * AreaM2 / MassKg, Rules->MaxObjectAcceleration);
	return Wind.GetSafeNormal() * Acceleration * 100.0f;
}

void AWindStormActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Rules)
	{
		return;
	}
	ADayNightCycleActor* Clock = ADayNightCycleActor::Get(this);
	const double Elapsed = Clock ? Clock->GetElapsedSeconds() : GetWorld()->GetTimeSeconds();
	Wind = WindAt(*Rules, Elapsed);
	const float Strength = static_cast<float>(Wind.Size());

	EngineWind->SetWorldRotation(Wind.IsNearlyZero() ? FRotator::ZeroRotator : Wind.Rotation());
	EngineWind->SetStrength(Strength);
	EngineWind->SetSpeed(Rules->VisualWindSpeed * Strength);

	// foliage sway (palm trunks bend, fronds flutter) follows the gusts
	if (UMaterialParameterCollectionInstance* Parameters = MaterialParameters ? GetWorld()->GetParameterCollectionInstance(MaterialParameters) : nullptr)
	{
		const FVector Direction = Wind.GetSafeNormal2D();
		Parameters->SetScalarParameterValue(TEXT("WindStrength"), Strength);
		Parameters->SetVectorParameterValue(TEXT("WindDirection"), FLinearColor(Direction.X, Direction.Y, 0.0f, 0.0f));
	}

	if (Clock)
	{
		const float Ramp = FMath::SmoothStep(0.0f, 1.0f, static_cast<float>((Elapsed - Rules->StartSeconds) / Rules->RampSeconds));
		Clock->SetStormFactor(Rules->StormSkyFactor * Ramp);
	}

	if (Strength > 0.0f)
	{
		ApplyForces(DeltaSeconds);
	}
}

void AWindStormActor::ApplyForces(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const bool bServer = HasAuthority();

	// characters: an additive constant-force root motion, refreshed often; it survives the ground braking and is
	// predicted like any root motion (applied by the server and by the owning client)
	DriftTimer -= DeltaSeconds;
	if (DriftTimer <= 0.0f)
	{
		DriftTimer = Rules->DriftRefreshInterval;
		const FVector Drift = FVector(Wind.X, Wind.Y, 0.0) * Rules->CharacterDriftSpeed;
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* Character = *It;
			UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			if (!Movement || !(bServer || Character->IsLocallyControlled()))
			{
				continue;
			}
			Movement->RemoveRootMotionSource(StormDriftName);
			TSharedPtr<FRootMotionSource_ConstantForce> Source = MakeShared<FRootMotionSource_ConstantForce>();
			Source->InstanceName = StormDriftName;
			Source->AccumulateMode = ERootMotionAccumulateMode::Additive;
			Source->Priority = 5;
			Source->Force = Drift;
			Source->Duration = Rules->DriftRefreshInterval * 1.5f;
			Source->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;
			Movement->ApplyRootMotionSource(Source);
		}
	}

	if (!bServer)
	{
		return;
	}
	// free physics objects and boats: pressure (grows with the square of the wind) times the area facing the wind, replicated physics pushed by the server
	// the drag works on the speed relative to the air: an object carried as fast as the wind is no longer pushed
	const float ReferenceSpeed = FMath::Max(Rules->VisualWindSpeed, 1.0f);
	const FVector AirVelocity = Wind * ReferenceSpeed;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(It->GetRootComponent());
		if (!Body || !Body->IsSimulatingPhysics() || It->IsA<ACharacter>() || It->ActorHasTag(WindShelteredTag))
		{
			continue;
		}
		const FVector Relative = AirVelocity - FVector(Body->GetPhysicsLinearVelocity().X, Body->GetPhysicsLinearVelocity().Y, 0.0) / 100.0;
		const float RelativeSpeed = static_cast<float>(Relative.Size());
		if (RelativeSpeed < UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}
		const FVector Extent = Body->Bounds.BoxExtent;
		const float AreaM2 = static_cast<float>(4.0 * FMath::Max(Extent.X, Extent.Y) * Extent.Z / 10000.0);
		const float MassKg = FMath::Max(Body->GetMass(), 0.1f);
		const float Pressure = Rules->WindPressurePa * FMath::Square(RelativeSpeed / ReferenceSpeed);
		const float Acceleration = FMath::Min(Pressure * AreaM2 / MassKg, Rules->MaxObjectAcceleration);
		Body->AddForce(Relative / RelativeSpeed * Acceleration * 100.0f, NAME_None, true);
	}
}
