// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Items/PhysicsPropActor.h"
#include "Items/ValuableComponent.h"
#include "UObject/UnrealType.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Items.Valuable
// The value of a loot object as the server decides it and as a client keeps it (network review T-268 R-07, R-39)
BEGIN_DEFINE_SPEC(FValuableSpec, "monke_island.Items.Valuable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* World = nullptr;

	/** A loot object spawned deferred: Prepare runs before its BeginPlay, as the replication does on a client */
	APhysicsPropActor* SpawnProp(TFunctionRef<void(APhysicsPropActor*)> Prepare)
	{
		const FTransform Where(FVector(0.0, 0.0, 100.0));
		APhysicsPropActor* Prop = World->SpawnActorDeferred<APhysicsPropActor>(APhysicsPropActor::StaticClass(), Where);
		if (Prop)
		{
			Prepare(Prop);
			Prop->FinishSpawning(Where);
		}
		return Prop;
	}

	/** Writes the replicated value as the network does on a client, before BeginPlay (the property is private) */
	static void SetReplicatedValue(UValuableComponent* Valuable, int32 InValue)
	{
		if (const FIntProperty* Property = FindFProperty<FIntProperty>(UValuableComponent::StaticClass(), TEXT("Value")))
		{
			Property->SetPropertyValue_InContainer(Valuable, InValue);
		}
	}
END_DEFINE_SPEC(FValuableSpec)

void FValuableSpec::Define()
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

	It("keeps on a client the value the server sent, never its own unscaled base (R-07)", [this]()
	{
		// a zone at x1.5, an object of 100 knocked down to 120: what the client receives with the object
		const APhysicsPropActor* Prop = SpawnProp([](APhysicsPropActor* Spawned)
		{
			Spawned->SetRole(ROLE_SimulatedProxy);
			Spawned->GetValuable()->BaseValue = 100;
			SetReplicatedValue(Spawned->GetValuable(), 120);
		});
		if (!TestNotNull(TEXT("object"), Prop))
		{
			return;
		}
		TestFalse(TEXT("a client"), Prop->HasAuthority());
		TestEqual(TEXT("the server's value kept"), Prop->GetValuable()->GetValue(), 120);
	});

	It("takes its value from its base on the server", [this]()
	{
		const APhysicsPropActor* Prop = SpawnProp([](APhysicsPropActor* Spawned)
		{
			Spawned->GetValuable()->BaseValue = 140;
		});
		if (!TestNotNull(TEXT("object"), Prop))
		{
			return;
		}
		// no run in the test world: a multiplier of 1
		TestEqual(TEXT("value of the intact object"), Prop->GetValuable()->GetValue(), 140);
	});

	It("keeps a value it already had elsewhere, losses kept and no multiplier again (T-309, R-39)", [this]()
	{
		const APhysicsPropActor* Prop = SpawnProp([](APhysicsPropActor* Spawned)
		{
			Spawned->GetValuable()->RestoreValue(150, 110);
		});
		if (!TestNotNull(TEXT("object"), Prop))
		{
			return;
		}
		TestEqual(TEXT("base with the multiplier of its zone"), Prop->GetValuable()->BaseValue, 150);
		TestEqual(TEXT("value left"), Prop->GetValuable()->GetValue(), 110);

		const APhysicsPropActor* Other = SpawnProp([](APhysicsPropActor*) {});
		if (!TestNotNull(TEXT("second object"), Other))
		{
			return;
		}
		UValuableComponent* Later = Other->GetValuable();
		Later->RestoreValue(80, 500);
		TestEqual(TEXT("never above its base"), Later->GetValue(), 80);
		Later->RestoreValue(-10, -5);
		TestEqual(TEXT("never below zero"), Later->GetValue(), 0);
	});
}

#endif
