// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Boats/BoatActor.h"
#include "Boats/BoatRules.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Storm/WindRules.h"
#include "Storm/WindStormActor.h"

BEGIN_DEFINE_SPEC(FBoatWindSpec, "monke_island.Storm.BoatWind",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* World = nullptr;
	ABoatActor* Boat = nullptr;
	AWindStormActor* Storm = nullptr;
	AStaticMeshActor* Crate = nullptr;
	UStaticMeshComponent* Hull = nullptr;

	void StepWind()
	{
		for (int32 Frame = 0; Frame < 30; ++Frame)
		{
			Storm->Tick(1.0f / 60.0f);
			World->Tick(LEVELTICK_All, 1.0f / 60.0f);
		}
	}
END_DEFINE_SPEC(FBoatWindSpec)

void FBoatWindSpec::Define()
{
	BeforeEach([this]()
	{
		const UWorld::InitializationValues Init = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(true)
			.CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		Boat = World->SpawnActor<ABoatActor>(FVector(400.0, 0.0, 0.0), FRotator::ZeroRotator);
		Boat->Rules = NewObject<UBoatRules>(Boat);
		// Existing Blueprint/placed instances can serialize their own tag array, hiding native defaults.
		Boat->Tags.Reset();
		Hull = CastChecked<UStaticMeshComponent>(Boat->GetRootComponent());
		Hull->SetStaticMesh(Cube);
		Hull->SetEnableGravity(false);
		Crate = World->SpawnActor<AStaticMeshActor>(FVector(-400.0, 0.0, 0.0), FRotator::ZeroRotator);
		Crate->SetMobility(EComponentMobility::Movable);
		Crate->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Crate->GetStaticMeshComponent()->SetSimulatePhysics(true);
		Crate->GetStaticMeshComponent()->SetEnableGravity(false);
		Storm = World->SpawnActor<AWindStormActor>();
		Storm->Rules = NewObject<UWindRules>(Storm);
		Storm->Rules->ClockMode = EHazardClockMode::DayNightOnly;
		Storm->Rules->CalmStrength = 3.0f;
		Storm->Rules->FirstGustSeconds = 100000.0f;
		Storm->Rules->GustAmplitude = 0.0f;
		Storm->Rules->DirectionSwingDeg = 0.0f;
		Storm->Rules->BlowTowardYawDeg = 0.0f;
		Storm->Rules->ShelterDistance = 0.0f;
		World->SpawnActor<APlayerController>()->Possess(World->SpawnActor<APawn>());
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
		// Isolate the external wind force: no rowing, buoyancy, waves or gravity in this fixture.
		Boat->SetActorTickEnabled(false);
		Storm->SetActorTickEnabled(false);
	});

	AfterEach([this]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		World = nullptr;
		Boat = nullptr;
		Storm = nullptr;
		Crate = nullptr;
		Hull = nullptr;
	});

	It("leaves the boat still under strong wind while pushing a nearby loose object", [this]()
	{
		StepWind();
		TestTrue(TEXT("strong wind active"), Storm->IsBlowing());
		TestTrue(TEXT("boat remains a physical body"), Hull->IsSimulatingPhysics());
		TestTrue(TEXT("loose object was pushed"), Crate->GetStaticMeshComponent()->GetPhysicsLinearVelocity().X > 1.0);
		TestTrue(TEXT("no wind velocity on boat"), Hull->GetPhysicsLinearVelocity().IsNearlyZero(0.01));
		TestTrue(TEXT("no boat drift"), Boat->GetActorLocation().Equals(FVector(400.0, 0.0, 0.0), 0.01));
		TestTrue(TEXT("no reported wind acceleration on boat"), Storm->GetWindAccelerationOn(Boat).IsNearlyZero());
	});

	It("reports zero acceleration for wind-sheltered objects and keeps exposed objects affected", [this]()
	{
		Storm->Tick(1.0f / 60.0f);
		TestTrue(TEXT("exposed object acceleration"), Storm->GetWindAccelerationOn(Crate).X > 0.0);
		Crate->Tags.AddUnique(TEXT("WindSheltered"));
		TestTrue(TEXT("sheltered object acceleration"), Storm->GetWindAccelerationOn(Crate).IsNearlyZero());
	});
}

#endif
