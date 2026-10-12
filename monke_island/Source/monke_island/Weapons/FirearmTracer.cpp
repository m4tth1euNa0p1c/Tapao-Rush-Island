// monke_island

#include "Weapons/FirearmTracer.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

AFirearmTracer::AFirearmTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;
	SetCanBeDamaged(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	auto MakePart = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Root);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCastShadow(false);
		Part->bReceivesDecals = false;
		Part->SetCanEverAffectNavigation(false);
		Part->SetUsingAbsoluteLocation(true);
		Part->SetUsingAbsoluteRotation(true);
		Part->SetUsingAbsoluteScale(true);
		return Part;
	};
	Streak = MakePart(TEXT("Streak"));
	Round = MakePart(TEXT("Round"));
}

AFirearmTracer* AFirearmTracer::Launch(UWorld* World, const FFirearmTracerLook& Look, const FVector& From, const FVector& To)
{
	if (!World || World->GetNetMode() == NM_DedicatedServer || !Look.bEnabled || (!Look.StreakMesh && !Look.RoundMesh))
	{
		return nullptr;
	}
	const FVector End = FirearmTracerMath::FlightEnd(From, To, Look.MaxDistanceCm);
	const float Distance = FVector::Dist(From, End);
	if (Distance < 50.0f)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AFirearmTracer* Tracer = World->SpawnActor<AFirearmTracer>(AFirearmTracer::StaticClass(), FTransform(From), Params);
	if (!Tracer)
	{
		return nullptr;
	}
	Tracer->Start = From;
	Tracer->Direction = (End - From).GetSafeNormal();
	Tracer->Distance = Distance;
	Tracer->Speed = FMath::Max(Look.SpeedCmps, 1000.0f);
	Tracer->StreakLengthCm = Look.LengthCm;
	Tracer->StreakWidthCm = Look.WidthCm;
	// the colour and the brightness go through custom primitive data: no material instance per round
	const FVector4f Glow(Look.Color.R, Look.Color.G, Look.Color.B, Look.Intensity);
	if (Look.StreakMesh)
	{
		Tracer->Streak->SetStaticMesh(Look.StreakMesh);
		if (Look.StreakMaterial)
		{
			Tracer->Streak->SetMaterial(0, Look.StreakMaterial);
		}
		Tracer->Streak->SetCustomPrimitiveDataVector4(0, FVector4(Glow));
	}
	else
	{
		Tracer->Streak->SetVisibility(false);
	}
	if (Look.RoundMesh)
	{
		Tracer->Round->SetStaticMesh(Look.RoundMesh);
		if (Look.StreakMaterial)
		{
			for (int32 Slot = 0; Slot < Tracer->Round->GetNumMaterials(); ++Slot)
			{
				Tracer->Round->SetMaterial(Slot, Look.StreakMaterial);
			}
		}
		// the head glows a little brighter than the streak; sized in cm whatever the size of the mesh
		Tracer->Round->SetCustomPrimitiveDataVector4(0, FVector4(Glow.X, Glow.Y, Glow.Z, Glow.W * 1.3f));
		const float MeshSize = FMath::Max(Look.RoundMesh->GetBounds().SphereRadius * 2.0f, 0.01f);
		Tracer->Round->SetWorldScale3D(FVector(Look.RoundSizeCm / MeshSize));
	}
	else
	{
		Tracer->Round->SetVisibility(false);
	}
	// a safety net: gone even if it stops ticking
	Tracer->SetLifeSpan(Distance / Tracer->Speed + 0.5f);
	Tracer->Place();
	return Tracer;
}

void AFirearmTracer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Travelled += Speed * DeltaSeconds;
	if (Travelled >= Distance)
	{
		Destroy();
		return;
	}
	Place();
}

void AFirearmTracer::Place()
{
	const FVector Head = Start + Direction * FMath::Min(Travelled, Distance);
	const FRotator Along = FRotationMatrix::MakeFromZ(Direction).Rotator();
	if (Streak->IsVisible())
	{
		// the cylinder stands 100 cm along +Z around its centre: stretched behind the head, grown out of the muzzle
		const float Length = FMath::Max(FirearmTracerMath::StreakLength(Travelled, StreakLengthCm), 1.0f);
		Streak->SetWorldLocationAndRotation(Head - Direction * (Length * 0.5f), Along);
		Streak->SetWorldScale3D(FVector(StreakWidthCm / 100.0f, StreakWidthCm / 100.0f, Length / 100.0f));
	}
	if (Round->IsVisible())
	{
		Round->SetWorldLocationAndRotation(Head, Direction.Rotation());
	}
}
