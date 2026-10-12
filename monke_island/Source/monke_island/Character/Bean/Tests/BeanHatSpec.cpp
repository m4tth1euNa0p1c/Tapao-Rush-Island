// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Character/Bean/BeanDroppedHat.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"

BEGIN_DEFINE_SPEC(FBeanHatSpec, "monke_island.Character.BeanHat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* World = nullptr;
	AIslanderCharacter* Wearer = nullptr;
	UBeanPuppetComponent* Puppet = nullptr;
	ABeanDroppedHat* Hat = nullptr;

	void Advance(float Seconds)
	{
		for (int32 Frame = 0; Frame < FMath::CeilToInt(Seconds * 60.f); ++Frame)
		{
			// TimerManager only advances once per engine frame, even in a manually ticked test world.
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 1.f / 60.f);
		}
	}

	bool DropAtFeet()
	{
		if (!Puppet || !Puppet->GetHatComponent())
		{
			AddError(TEXT("Hat fixture is missing its skin"));
			return false;
		}
		Puppet->DropHat(FVector::ZeroVector);
		for (TActorIterator<ABeanDroppedHat> It(World); It; ++It)
		{
			if (It->GetOwner() == Wearer)
			{
				Hat = *It;
				break;
			}
		}
		if (!TestNotNull(TEXT("dropped actor"), Hat))
		{
			return false;
		}
		// Freeze the launch only: the real actor, interaction, component tick and world clock still run.
		UStaticMeshComponent* Mesh = CastChecked<UStaticMeshComponent>(Hat->GetRootComponent());
		Mesh->SetEnableGravity(false);
		Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Hat->SetActorLocation(FVector(45.0, 0.0, 10.0), false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}
END_DEFINE_SPEC(FBeanHatSpec)

void FBeanHatSpec::Define()
{
	BeforeEach([this]()
	{
		const UWorld::InitializationValues Init = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(true)
			.CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.0, 0.0, -5.0), FRotator::ZeroRotator);
		Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Floor->SetActorScale3D(FVector(100.0, 100.0, 0.1));
		// The native character is abstract; exercise the actual Blueprint and its serialized skin settings.
		UClass* PlayerClass = LoadClass<AIslanderCharacter>(nullptr,
			TEXT("/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean.BP_IslanderBean_C"));
		if (!TestNotNull(TEXT("player Blueprint"), PlayerClass)) { return; }
		Wearer = World->SpawnActor<AIslanderCharacter>(PlayerClass, FVector(0.0, 0.0, 90.0), FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("player"), Wearer)) { return; }
		Wearer->SetActorLocation(FVector(0.0, 0.0, Wearer->GetSimpleCollisionHalfHeight()));
		Puppet = Wearer->FindComponentByClass<UBeanPuppetComponent>();
		if (!TestNotNull(TEXT("bean component"), Puppet)) { return; }
		Puppet->HatPickupDelay = 0.f;
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
		Wearer->GetCharacterMovement()->DisableMovement();
		World->Tick(LEVELTICK_All, 1.f / 60.f);
		TestNotNull(TEXT("worn hat"), Puppet->GetHatComponent());
	});

	AfterEach([this]()
	{
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
		World = nullptr;
		Wearer = nullptr;
		Puppet = nullptr;
		Hat = nullptr;
	});

	It("drops exactly one physical hat on damage and hides the worn hat", [this]()
	{
		if (!Wearer || !Puppet || !Puppet->GetHatComponent()) { return; }
		Wearer->GetHealthComponent()->ApplyHealthDamage(1.f, nullptr);
		TestTrue(TEXT("damage drops hat"), Puppet->IsHatDropped());
		TestTrue(TEXT("worn hat hidden"), Puppet->GetHatComponent()->bHiddenInGame);
		Puppet->DropHat(FVector::ZeroVector);
		int32 Count = 0;
		for (TActorIterator<ABeanDroppedHat> It(World); It; ++It)
		{
			++Count;
			TestTrue(TEXT("hat is physical"), CastChecked<UStaticMeshComponent>(It->GetRootComponent())->IsSimulatingPhysics());
		}
		TestEqual(TEXT("no duplicate on repeated hit"), Count, 1);
	});

	It("does not return after five seconds or contact and distance", [this]()
	{
		if (!DropAtFeet()) { return; }
		Advance(6.f);
		TestTrue(TEXT("waiting next to the hat never equips it"), Puppet->IsHatDropped());
		Wearer->SetActorLocation(FVector(4000.0, 0.0, 90.0));
		Advance(1.f);
		TestTrue(TEXT("walking farther than the old lost threshold never equips it"), Puppet->IsHatDropped());
		TestTrue(TEXT("same hat remains in world"), IsValid(Hat));
		Wearer->SetActorLocation(FVector(0.0, 0.0, 90.0));
		Advance(1.f);
		TestTrue(TEXT("returning to it never equips it without F"), Puppet->IsHatDropped());
	});

	It("lands and remains on the floor as a physical object", [this]()
	{
		if (!DropAtFeet()) { return; }
		CastChecked<UStaticMeshComponent>(Hat->GetRootComponent())->SetEnableGravity(true);
		Hat->SetActorLocation(FVector(60.0, 0.0, 100.0), false, nullptr, ETeleportType::TeleportPhysics);
		Advance(6.f);
		TestTrue(TEXT("still dropped after landing"), Puppet->IsHatDropped());
		TestTrue(TEXT("hat rests above the floor"), IsValid(Hat) && Hat->GetActorLocation().Z > -20.0);
		TestTrue(TEXT("hat fell to the floor"), IsValid(Hat) && Hat->GetActorLocation().Z < 60.0);
	});

	It("equips once through the F interaction path when aimed at nearby", [this]()
	{
		if (!DropAtFeet()) { return; }
		const FVector Eyes = Wearer->GetFirstPersonCamera()->GetComponentLocation();
		Wearer->GetFirstPersonCamera()->SetAbsolute(true, true, false);
		Wearer->GetFirstPersonCamera()->SetWorldLocation(Eyes);
		const FRotator Aim = (Hat->GetComponentsBoundingBox().GetCenter() - Eyes).Rotation();
		if (AController* Controller = Wearer->GetController())
		{
			Controller->SetControlRotation(Aim);
		}
		else
		{
			Wearer->SetActorRotation(Aim);
		}
		Wearer->GetFirstPersonCamera()->SetWorldRotation(Aim);
		TestTrue(TEXT("nearby hat can be used"), Hat->CanInteract_Implementation(Wearer));
		AActor* Target = nullptr;
		UPrimitiveComponent* Physics = nullptr;
		FVector Point;
		TestTrue(TEXT("crosshair finds hat"), Wearer->GetGrabComponent()->FindInteractionTarget(Target, Physics, Point));
		TestEqual(TEXT("crosshair target"), Target, static_cast<AActor*>(Hat));
		Wearer->GetGrabComponent()->RequestInteract();
		TestFalse(TEXT("F equipped hat"), Puppet->IsHatDropped());
		TestFalse(TEXT("worn hat visible"), Puppet->GetHatComponent()->bHiddenInGame);
		TestFalse(TEXT("ground hat consumed"), IsValid(Hat));
		Wearer->GetGrabComponent()->RequestInteract();
		TestFalse(TEXT("second F cannot duplicate hat"), Puppet->IsHatDropped());
	});

	It("refuses pickup before the delay and while too far away", [this]()
	{
		if (!DropAtFeet()) { return; }
		Puppet->HatPickupDelay = 1.f;
		Hat->Interact_Implementation(Wearer);
		TestTrue(TEXT("delay enforced"), Puppet->IsHatDropped());
		Advance(1.1f);
		Wearer->SetActorLocation(FVector(1000.0, 0.0, 90.0));
		Hat->Interact_Implementation(Wearer);
		TestTrue(TEXT("actual hat position checked"), Puppet->IsHatDropped());
	});

	It("refuses a different player and a different hat", [this]()
	{
		if (!DropAtFeet()) { return; }
		AIslanderCharacter* Other = World->SpawnActor<AIslanderCharacter>(Wearer->GetClass(), FVector(0.0, 100.0, 90.0), FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("other player"), Other)) { return; }
		Hat->Interact_Implementation(Other);
		TestTrue(TEXT("other player cannot take hat"), Puppet->IsHatDropped());
		ABeanDroppedHat* Impostor = World->SpawnActor<ABeanDroppedHat>();
		TestFalse(TEXT("only the original hat may be equipped"), Puppet->TryPickUpHat(Impostor, Wearer));
	});

	It("refuses pickup through a wall", [this]()
	{
		if (!DropAtFeet()) { return; }
		const FVector Eyes = Wearer->GetFirstPersonCamera()->GetComponentLocation();
		AStaticMeshActor* Wall = World->SpawnActor<AStaticMeshActor>((Eyes + Hat->GetActorLocation()) * 0.5, FRotator::ZeroRotator);
		Wall->SetMobility(EComponentMobility::Movable);
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Hat->Interact_Implementation(Wearer);
		TestTrue(TEXT("wall blocks use"), Puppet->IsHatDropped());
	});

	It("refuses pickup during ragdoll and stays dropped after recovery", [this]()
	{
		if (!DropAtFeet()) { return; }
		Wearer->StartRagdoll(FVector::ZeroVector);
		TestTrue(TEXT("ragdoll started"), Wearer->IsRagdoll());
		Hat->Interact_Implementation(Wearer);
		TestTrue(TEXT("ragdoll cannot equip"), Puppet->IsHatDropped());
		Advance(9.f);
		TestFalse(TEXT("ragdoll recovered"), Wearer->IsRagdoll());
		TestTrue(TEXT("recovery does not equip"), Puppet->IsHatDropped());
	});

	It("refuses pickup after death", [this]()
	{
		if (!DropAtFeet()) { return; }
		Wearer->GetHealthComponent()->ApplyHealthDamage(1000.f, nullptr);
		Hat->Interact_Implementation(Wearer);
		TestTrue(TEXT("dead player cannot equip"), Puppet->IsHatDropped());
	});

	It("does not recreate a destroyed dropped hat", [this]()
	{
		if (!DropAtFeet()) { return; }
		Hat->Destroy();
		Advance(6.f);
		TestTrue(TEXT("destroyed hat never respawns on head"), Puppet->IsHatDropped());
		TestTrue(TEXT("worn hat remains hidden"), Puppet->GetHatComponent()->bHiddenInGame);
	});
}

#endif
