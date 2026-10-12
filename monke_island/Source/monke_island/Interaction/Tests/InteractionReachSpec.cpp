// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IslanderGrabComponent.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Interaction.Reach
// The server measures a reach to the target itself, never to a point sent by a client (T-289, security review T-269 C1)
BEGIN_DEFINE_SPEC(FInteractionReachSpec, "monke_island.Interaction.Reach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* World = nullptr;
	UStaticMesh* Cube = nullptr;

	/** A 1 m cube with the engine's collision (blocks the visibility channel) */
	AStaticMeshActor* MakeBox(const FVector& Location, const FVector& Scale = FVector::OneVector)
	{
		AStaticMeshActor* Box = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
		Box->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Box->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Box->SetActorScale3D(Scale);
		return Box;
	}
END_DEFINE_SPEC(FInteractionReachSpec)

void FInteractionReachSpec::Define()
{
	BeforeEach([this]()
	{
		const UWorld::InitializationValues Init = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false)
			.CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
	});

	AfterEach([this]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		World = nullptr;
		Cube = nullptr;
	});

	It("refuses a target 30 m away, whatever point a client says it touched", [this]()
	{
		if (!TestNotNull(TEXT("engine cube"), Cube))
		{
			return;
		}
		const FVector Eyes(0.0, 0.0, 100.0);
		AStaticMeshActor* Far = MakeBox(FVector(3000.0, 0.0, 100.0));
		TestFalse(TEXT("30 m: out of reach"), UIslanderGrabComponent::IsActorInReach(World, Far, nullptr, nullptr, Eyes, 250.0f));
	});

	It("accepts a target within reach and in sight", [this]()
	{
		if (!TestNotNull(TEXT("engine cube"), Cube))
		{
			return;
		}
		const FVector Eyes(0.0, 0.0, 100.0);
		AStaticMeshActor* Near = MakeBox(FVector(180.0, 0.0, 100.0));
		TestTrue(TEXT("1.3 m to its face: in reach"), UIslanderGrabComponent::IsActorInReach(World, Near, nullptr, nullptr, Eyes, 250.0f));
	});

	It("refuses a target in reach behind a wall, unless the wall is the ignored actor", [this]()
	{
		if (!TestNotNull(TEXT("engine cube"), Cube))
		{
			return;
		}
		const FVector Eyes(0.0, 0.0, 100.0);
		AStaticMeshActor* Behind = MakeBox(FVector(200.0, 0.0, 100.0));
		AStaticMeshActor* Wall = MakeBox(FVector(90.0, 0.0, 100.0), FVector(0.1, 3.0, 3.0));
		TestFalse(TEXT("wall in between: out of sight"), UIslanderGrabComponent::IsActorInReach(World, Behind, nullptr, nullptr, Eyes, 250.0f));
		TestTrue(TEXT("the held object aside"), UIslanderGrabComponent::IsActorInReach(World, Behind, nullptr, Wall, Eyes, 250.0f));
	});
}

#endif
