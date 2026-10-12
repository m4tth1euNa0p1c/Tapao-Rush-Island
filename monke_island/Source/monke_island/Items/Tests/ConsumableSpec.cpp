// monke_island

#include "Misc/AutomationTest.h"
#include "Items/ConsumablePickupActor.h"
#include "Player/IslanderHealthComponent.h"

#if WITH_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IslanderInteractable.h"
#include "Items/ConsumableDefinition.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Items.Consumable
BEGIN_DEFINE_SPEC(FConsumableSpec, "monke_island.Items.Consumable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** World of the network test, without a net driver: relevancy is asked of the actor itself */
	UWorld* World = nullptr;
END_DEFINE_SPEC(FConsumableSpec)

void FConsumableSpec::Define()
{
	Describe("Heal", [this]()
	{
		It("restores the amount when there is room", [this]()
		{
			TestEqual(TEXT("healed"), UIslanderHealthComponent::ComputeHeal(40.0f, 100.0f, 25.0f), 25.0f);
		});

		It("never goes above MaxHealth", [this]()
		{
			TestEqual(TEXT("healed"), UIslanderHealthComponent::ComputeHeal(90.0f, 100.0f, 25.0f), 10.0f);
		});

		It("restores nothing at full health or with a non positive amount", [this]()
		{
			TestEqual(TEXT("full"), UIslanderHealthComponent::ComputeHeal(100.0f, 100.0f, 25.0f), 0.0f);
			TestEqual(TEXT("zero"), UIslanderHealthComponent::ComputeHeal(50.0f, 100.0f, 0.0f), 0.0f);
			TestEqual(TEXT("negative"), UIslanderHealthComponent::ComputeHeal(50.0f, 100.0f, -10.0f), 0.0f);
		});
	});

	Describe("Pickup", [this]()
	{
		It("is consumable by a hurt living player", [this]()
		{
			TestTrue(TEXT("hurt"), AConsumablePickupActor::CanConsume(false, false, 60.0f, 100.0f, false));
		});

		It("is kept at full health unless usable at full health", [this]()
		{
			TestFalse(TEXT("full, kept"), AConsumablePickupActor::CanConsume(false, false, 100.0f, 100.0f, false));
			TestTrue(TEXT("full, allowed"), AConsumablePickupActor::CanConsume(false, false, 100.0f, 100.0f, true));
		});

		It("is consumed only once and never by a dead player", [this]()
		{
			TestFalse(TEXT("already consumed"), AConsumablePickupActor::CanConsume(true, false, 10.0f, 100.0f, true));
			TestFalse(TEXT("dead"), AConsumablePickupActor::CanConsume(false, true, 0.0f, 100.0f, true));
		});
	});

	Describe("Network", [this]()
	{
		BeforeEach([this]()
		{
			const UWorld::InitializationValues Init = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false)
				.CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
		});

		AfterEach([this]()
		{
			World->EndPlay(EEndPlayReason::Quit);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			World = nullptr;
		});

		It("stays relevant once drunk, so every client hides it before it is destroyed (T-316)", [this]()
		{
			UConsumableDefinition* Coconut = NewObject<UConsumableDefinition>();
			Coconut->AddToRoot();
			Coconut->bUsableAtFullHealth = true;
			ADefaultPawn* Drinker = World->SpawnActor<ADefaultPawn>(FVector(-100.0, 0.0, 100.0), FRotator::ZeroRotator);
			UIslanderHealthComponent* Health = Drinker ? NewObject<UIslanderHealthComponent>(Drinker, TEXT("Health")) : nullptr;
			AConsumablePickupActor* Pickup = World->SpawnActor<AConsumablePickupActor>(FVector(0.0, 0.0, 60.0), FRotator::ZeroRotator);
			if (!TestNotNull(TEXT("drinker"), Health) || !TestNotNull(TEXT("coconut"), Pickup))
			{
				Coconut->RemoveFromRoot();
				return;
			}
			Health->RegisterComponent();
			Pickup->Definition = Coconut;
			const FVector Viewer = Drinker->GetActorLocation();
			TestTrue(TEXT("placed in the level, initially dormant"), Pickup->NetDormancy == DORM_Initial);

			IIslanderInteractable::Execute_Interact(Pickup, Drinker);
			if (!TestTrue(TEXT("drunk"), Pickup->IsConsumed()))
			{
				Coconut->RemoveFromRoot();
				return;
			}
			TestTrue(TEXT("woken: no longer initially dormant"), Pickup->NetDormancy != DORM_Initial);
			TestFalse(TEXT("its mesh is hidden"), Pickup->GetMesh()->IsVisible());
			TestFalse(TEXT("nothing blocks on it"), Pickup->GetActorEnableCollision());
			TestTrue(TEXT("still relevant: bConsumed and the sound reach the clients"), Pickup->IsNetRelevantFor(Drinker, Drinker, Viewer));
			// the engine rule it keeps clear of: an actor hidden without collision is relevant to no client
			Pickup->SetActorHiddenInGame(true);
			TestFalse(TEXT("hidden as an actor, it would reach no client"), Pickup->IsNetRelevantFor(Drinker, Drinker, Viewer));
			Coconut->RemoveFromRoot();
		});
	});
}

#endif
