// monke_island

#include "Misc/AutomationTest.h"
#include "Player/IslanderHealthComponent.h"

#if WITH_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Player.Health
BEGIN_DEFINE_SPEC(FIslanderHealthSpec, "monke_island.Player.Health",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* World = nullptr;

	/**
	 *  A creature whose health component begins play with Health and bDead as the network wrote them before (the
	 *  properties are private); a client when bClient (its role is a simulated proxy), else the server
	 */
	UIslanderHealthComponent* MakeHealth(bool bClient, float InHealth, bool bInDead)
	{
		AActor* Creature = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (!Creature)
		{
			return nullptr;
		}
		if (bClient)
		{
			Creature->SetRole(ROLE_SimulatedProxy);
		}
		UIslanderHealthComponent* Health = NewObject<UIslanderHealthComponent>(Creature, TEXT("Health"));
		if (const FFloatProperty* HealthProperty = FindFProperty<FFloatProperty>(UIslanderHealthComponent::StaticClass(), TEXT("Health")))
		{
			HealthProperty->SetPropertyValue_InContainer(Health, InHealth);
		}
		if (const FBoolProperty* DeadProperty = FindFProperty<FBoolProperty>(UIslanderHealthComponent::StaticClass(), TEXT("bDead")))
		{
			DeadProperty->SetPropertyValue_InContainer(Health, bInDead);
		}
		// the world has begun play: registering begins play at once, after the "replicated" values
		Health->RegisterComponent();
		return Health;
	}
END_DEFINE_SPEC(FIslanderHealthSpec)

void FIslanderHealthSpec::Define()
{
	Describe("Difficulty of the day (owner 2026-10-09)", [this]()
	{
		It("makes the mobs, the soldiers and the crab hurt the players more, and nobody else", [this]()
		{
			TestEqual(TEXT("a mob on a player: x1.6"), UIslanderHealthComponent::ScaleMobDamage(25.0f, 1.6f, true, true), 40.0f, 1.0e-4f);
			TestEqual(TEXT("a player on a player: as is"), UIslanderHealthComponent::ScaleMobDamage(25.0f, 1.6f, true, false), 25.0f, 1.0e-4f);
			TestEqual(TEXT("a player on a mob: as is"), UIslanderHealthComponent::ScaleMobDamage(25.0f, 1.6f, false, false), 25.0f, 1.0e-4f);
			TestEqual(TEXT("a mob on a mob: as is"), UIslanderHealthComponent::ScaleMobDamage(25.0f, 1.6f, false, true), 25.0f, 1.0e-4f);
			TestEqual(TEXT("a gentle day"), UIslanderHealthComponent::ScaleMobDamage(25.0f, 0.9f, true, true), 22.5f, 1.0e-4f);
			TestEqual(TEXT("no negative scale"), UIslanderHealthComponent::ScaleMobDamage(25.0f, -1.0f, true, true), 0.0f, 1.0e-4f);
		});

		It("tells a blow of the island from a blow of a creature", [this]()
		{
			// no causer (a fall, the moat): not a mob; no player's pawn: not a player
			TestFalse(TEXT("nothing behind the blow"), UIslanderHealthComponent::IsMobCauser(nullptr, nullptr));
			TestFalse(TEXT("nobody is a player"), UIslanderHealthComponent::IsPlayerPawn(nullptr));
		});
	});

	Describe("Heal", [this]()
	{
		It("never heals over the maximum nor a negative amount", [this]()
		{
			TestEqual(TEXT("capped"), UIslanderHealthComponent::ComputeHeal(90.0f, 100.0f, 25.0f), 10.0f, 1.0e-4f);
			TestEqual(TEXT("full"), UIslanderHealthComponent::ComputeHeal(100.0f, 100.0f, 25.0f), 0.0f, 1.0e-4f);
			TestEqual(TEXT("negative"), UIslanderHealthComponent::ComputeHeal(50.0f, 100.0f, -5.0f), 0.0f, 1.0e-4f);
		});
	});

	Describe("Replicated health at BeginPlay (network review T-268 R-26)", [this]()
	{
		BeforeEach([this]()
		{
			const UWorld::InitializationValues Init = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false)
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

		It("keeps on a client the wounds the server sent (a creature back into relevance is not whole again)", [this]()
		{
			const UIslanderHealthComponent* Health = MakeHealth(true, 40.0f, false);
			if (!TestNotNull(TEXT("health"), Health))
			{
				return;
			}
			TestEqual(TEXT("wounds kept"), Health->GetHealth(), 40.0f, 1.0e-4f);
			TestFalse(TEXT("alive"), Health->IsDead());
			TestFalse(TEXT("not a replay"), Health->WasDeadOnArrival());
		});

		It("keeps a creature received dead dead on a client, its death told again once", [this]()
		{
			const UIslanderHealthComponent* Health = MakeHealth(true, 0.0f, true);
			if (!TestNotNull(TEXT("health"), Health))
			{
				return;
			}
			TestEqual(TEXT("not refilled"), Health->GetHealth(), 0.0f, 1.0e-4f);
			TestTrue(TEXT("still dead"), Health->IsDead());
			TestTrue(TEXT("its death is a replay"), Health->WasDeadOnArrival());
		});

		It("fills the health on the server", [this]()
		{
			const UIslanderHealthComponent* Health = MakeHealth(false, 40.0f, false);
			if (!TestNotNull(TEXT("health"), Health))
			{
				return;
			}
			TestEqual(TEXT("full"), Health->GetHealth(), Health->MaxHealth, 1.0e-4f);
			TestFalse(TEXT("never a replay on the server"), Health->WasDeadOnArrival());
		});
	});
}

#endif
