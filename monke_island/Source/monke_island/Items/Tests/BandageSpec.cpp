// monke_island

#include "Misc/AutomationTest.h"
#include "Items/Bandage/BandageItem.h"
#include "Items/Bandage/BandageMath.h"
#include "Items/Bandage/BandageRules.h"

#if WITH_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/IslanderInventoryComponent.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Items.Bandage
BEGIN_DEFINE_SPEC(FBandageSpec, "monke_island.Items.Bandage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	UBandageRules* Rules = nullptr;
	UInventoryItemDefinition* Definition = nullptr;
	UWorld* World = nullptr;
	AIslanderCharacter* Bean = nullptr;
	UIslanderInventoryComponent* Inventory = nullptr;
	UIslanderHealthComponent* Health = nullptr;
	ABandageItem* Bandage = nullptr;

	/** A hurt bean standing on the ground, able to bandage itself */
	BandageMath::FCarrierFacts Able() const
	{
		BandageMath::FCarrierFacts Facts;
		Facts.bEquipped = true;
		Facts.bCarried = true;
		Facts.Health = 40.0f;
		Facts.MaxHealth = 100.0f;
		return Facts;
	}

	/** Ticks the test world (timers advance once per engine frame) */
	void Advance(float Seconds)
	{
		for (int32 Frame = 0; Frame < FMath::CeilToInt(Seconds * 30.0f); ++Frame)
		{
			++GFrameCounter;
			World->Tick(LEVELTICK_All, 1.0f / 30.0f);
		}
	}

	/** The bandages' cell, INDEX_NONE without */
	int32 BandageSlot() const
	{
		return Inventory && Bandage ? Inventory->FindSlotOfItem(Bandage) : INDEX_NONE;
	}

	/** The bean on its floor with Count bandages in hand and Hurt health taken; false when the fixture failed */
	bool Prepare(int32 Count, float Hurt)
	{
		const UWorld::InitializationValues Init = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false)
			.CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.0, 0.0, -5.0), FRotator::ZeroRotator);
		Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		Floor->SetActorScale3D(FVector(100.0, 100.0, 0.1));
		// the native character is abstract: the real Blueprint of the bean, its inventory and health
		UClass* PlayerClass = LoadClass<AIslanderCharacter>(nullptr, TEXT("/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean.BP_IslanderBean_C"));
		if (!TestNotNull(TEXT("player Blueprint"), PlayerClass))
		{
			return false;
		}
		Bean = World->SpawnActor<AIslanderCharacter>(PlayerClass, FVector(0.0, 0.0, 120.0), FRotator::ZeroRotator);
		if (!TestNotNull(TEXT("bean"), Bean))
		{
			return false;
		}
		Bean->SetActorLocation(FVector(0.0, 0.0, Bean->GetSimpleCollisionHalfHeight() + 1.0));
		Inventory = Bean->FindComponentByClass<UIslanderInventoryComponent>();
		Health = Bean->GetHealthComponent();
		if (!TestNotNull(TEXT("inventory"), Inventory) || !TestNotNull(TEXT("health"), Health))
		{
			return false;
		}
		// empty-handed: only the bandages of the test
		Inventory->StartingItems.Empty();
		World->InitializeActorsForPlay(FURL());
		World->GetWorldSettings()->NotifyBeginPlay();
		Bean->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Advance(0.1f);

		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!TestTrue(TEXT("bandage given"), Inventory->GiveItem(Definition)))
			{
				return false;
			}
		}
		for (int32 Slot = 0; Slot < Inventory->GetSlotNum() && !Bandage; ++Slot)
		{
			Bandage = Cast<ABandageItem>(Inventory->GetItem(Slot));
		}
		if (!TestNotNull(TEXT("bandage item"), Bandage))
		{
			return false;
		}
		Bandage->Rules = Rules;
		if (!Bandage->IsEquipped())
		{
			Inventory->SelectSlot(BandageSlot());
		}
		if (Hurt > 0.0f)
		{
			Health->ApplyHealthDamage(Hurt, nullptr);
		}
		Bean->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Advance(0.1f);
		return TestTrue(TEXT("bandage in hand"), Bandage->IsEquipped());
	}
END_DEFINE_SPEC(FBandageSpec)

void FBandageSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UBandageRules>();
		Rules->AddToRoot();
		Definition = NewObject<UInventoryItemDefinition>();
		Definition->AddToRoot();
		Definition->MaxStack = 5;
		Definition->ItemClass = ABandageItem::StaticClass();
	});

	AfterEach([this]()
	{
		if (World)
		{
			World->EndPlay(EEndPlayReason::Quit);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
		World = nullptr;
		Bean = nullptr;
		Inventory = nullptr;
		Health = nullptr;
		Bandage = nullptr;
		Definition->RemoveFromRoot();
		Rules->RemoveFromRoot();
	});

	Describe("Start", [this]()
	{
		It("starts for a hurt carrier standing with the bandage in hand", [this]()
		{
			TestEqual(TEXT("able"), BandageMath::JudgeStart(Able(), *Rules), EBandageRefusal::None);
		});

		It("is refused at full health unless the rules allow it", [this]()
		{
			BandageMath::FCarrierFacts Facts = Able();
			Facts.Health = 100.0f;
			TestEqual(TEXT("full"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::FullHealth);
			Rules->bUsableAtFullHealth = true;
			TestEqual(TEXT("full, allowed"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::None);
		});

		It("is refused for an item out of hand or carried by no inventory (not its owner's)", [this]()
		{
			BandageMath::FCarrierFacts Facts = Able();
			Facts.bEquipped = false;
			TestEqual(TEXT("not in hand"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::NotInHand);
			Facts = Able();
			Facts.bCarried = false;
			TestEqual(TEXT("in no inventory"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::NotCarried);
		});

		It("is refused dead, down, in the air, swimming, in deep water, running or already under way", [this]()
		{
			BandageMath::FCarrierFacts Facts = Able();
			Facts.bAlive = false;
			TestEqual(TEXT("dead"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::CarrierUnable);
			Facts = Able();
			Facts.bRagdoll = true;
			TestEqual(TEXT("ragdoll"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::CarrierUnable);
			Facts = Able();
			Facts.bOnGround = false;
			TestEqual(TEXT("in the air"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::CarrierUnable);
			Facts = Able();
			Facts.bSwimming = true;
			TestEqual(TEXT("swimming"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::CarrierUnable);
			Facts = Able();
			Facts.WaterDepthCm = Rules->MaxWaterDepthCm + 1.0f;
			TestEqual(TEXT("deep water"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::CarrierUnable);
			Facts = Able();
			Facts.WaterDepthCm = Rules->MaxWaterDepthCm - 1.0f;
			TestEqual(TEXT("wading"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::None);
			Facts = Able();
			Facts.bSprinting = true;
			TestEqual(TEXT("running"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::Sprinting);
			Facts = Able();
			Facts.bBusy = true;
			TestEqual(TEXT("under way"), BandageMath::JudgeStart(Facts, *Rules), EBandageRefusal::Busy);
		});
	});

	Describe("Interrupt", [this]()
	{
		It("goes on while the carrier stands or walks", [this]()
		{
			BandageMath::FCarrierFacts Facts = Able();
			Facts.Speed2D = 250.0f;
			TestEqual(TEXT("walking"), BandageMath::CheckInterrupt(Facts, *Rules), EBandageInterrupt::None);
		});

		It("is cut by a blow, not by a scratch under MinHitDamage", [this]()
		{
			TestEqual(TEXT("blow"), BandageMath::CheckInterrupt(Able(), *Rules, 10.0f), EBandageInterrupt::Hit);
			TestEqual(TEXT("scratch"), BandageMath::CheckInterrupt(Able(), *Rules, Rules->MinHitDamage * 0.5f), EBandageInterrupt::None);
			Rules->bInterruptOnHit = false;
			TestEqual(TEXT("blows ignored"), BandageMath::CheckInterrupt(Able(), *Rules, 10.0f), EBandageInterrupt::None);
		});

		It("is cut by a sprint, and by a fast walk when MaxMoveSpeedCmS is set", [this]()
		{
			BandageMath::FCarrierFacts Facts = Able();
			Facts.bSprinting = true;
			TestEqual(TEXT("sprint"), BandageMath::CheckInterrupt(Facts, *Rules), EBandageInterrupt::Sprint);
			Facts = Able();
			Facts.Speed2D = 300.0f;
			Rules->MaxMoveSpeedCmS = 200.0f;
			TestEqual(TEXT("too fast"), BandageMath::CheckInterrupt(Facts, *Rules), EBandageInterrupt::Sprint);
		});

		It("is cut by a jump, deep water, putting it away and going down", [this]()
		{
			BandageMath::FCarrierFacts Facts = Able();
			Facts.bOnGround = false;
			TestEqual(TEXT("jump"), BandageMath::CheckInterrupt(Facts, *Rules), EBandageInterrupt::Jump);
			Facts = Able();
			Facts.bSwimming = true;
			TestEqual(TEXT("water"), BandageMath::CheckInterrupt(Facts, *Rules), EBandageInterrupt::Water);
			Facts = Able();
			Facts.bEquipped = false;
			TestEqual(TEXT("put away"), BandageMath::CheckInterrupt(Facts, *Rules), EBandageInterrupt::PutAway);
			Facts = Able();
			Facts.bRagdoll = true;
			TestEqual(TEXT("down"), BandageMath::CheckInterrupt(Facts, *Rules), EBandageInterrupt::Down);
		});
	});

	Describe("Timing", [this]()
	{
		It("lands the heal at BandageDone of the clip stretched over the application", [this]()
		{
			Rules->ApplySeconds = 6.0f;
			Rules->ClipSeconds = 3.0f;
			TestEqual(TEXT("heal at"), Rules->ToApplyTime(2.73f), 5.46f, 0.001f);
			TestEqual(TEXT("clip time"), Rules->ToClipTime(3.0f), 1.5f, 0.001f);
			TestEqual(TEXT("play rate"), BandageMath::PlayRate(3.0f, 6.0f), 0.5f, 0.001f);
			TestEqual(TEXT("progress"), BandageMath::Progress(13.0, 10.0, 6.0f), 0.5f, 0.001f);
			TestEqual(TEXT("progress clamped"), BandageMath::Progress(30.0, 10.0, 6.0f), 1.0f, 0.001f);
		});

		It("heals to the maximum, never above it", [this]()
		{
			TestEqual(TEXT("to max"), Rules->ComputeHeal(12.0f, 100.0f), 88.0f, 0.001f);
			TestEqual(TEXT("full"), Rules->ComputeHeal(100.0f, 100.0f), 0.0f, 0.001f);
			Rules->bHealToMax = false;
			Rules->HealAmount = 30.0f;
			TestEqual(TEXT("amount"), Rules->ComputeHeal(50.0f, 100.0f), 30.0f, 0.001f);
			TestEqual(TEXT("amount capped"), Rules->ComputeHeal(90.0f, 100.0f), 10.0f, 0.001f);
		});

		It("samples the first person keys between their times", [this]()
		{
			FBandageArmKey A;
			A.TimeSeconds = 1.0f;
			A.Right = FBeanArmPose(FVector(0.0, 0.0, 0.0), FRotator::ZeroRotator, 1.0f);
			FBandageArmKey B;
			B.TimeSeconds = 2.0f;
			B.Right = FBeanArmPose(FVector(10.0, 0.0, 0.0), FRotator::ZeroRotator, 1.5f);
			FBeanArmPose Right;
			FBeanArmPose Left;
			TestFalse(TEXT("no keys"), BandageMath::SampleArmKeys({}, 1.0f, Right, Left));
			TestTrue(TEXT("keys"), BandageMath::SampleArmKeys({ A, B }, 1.5f, Right, Left));
			TestEqual(TEXT("halfway"), Right.Location.X, 5.0, 0.001);
			TestEqual(TEXT("stretch halfway"), Right.Stretch, 1.25f, 0.001f);
			BandageMath::SampleArmKeys({ A, B }, 9.0f, Right, Left);
			TestEqual(TEXT("held after the last"), Right.Location.X, 10.0, 0.001);
		});
	});

	Describe("Item", [this]()
	{
		It("heals to the maximum and uses one bandage of the stack", [this]()
		{
			if (!Prepare(2, 60.0f))
			{
				return;
			}
			const int32 Slot = BandageSlot();
			TestEqual(TEXT("two bandages"), Inventory->GetCount(Slot), 2);
			TestEqual(TEXT("hurt"), Health->GetHealth(), Health->MaxHealth - 60.0f, 0.01f);
			Bandage->RequestApply();
			TestTrue(TEXT("under way"), Bandage->IsApplying());
			Advance(Rules->ToApplyTime(Rules->HealEventSeconds) * 0.5f);
			TestTrue(TEXT("still hurt halfway"), Health->GetHealth() < Health->MaxHealth);
			Advance(Rules->ApplySeconds);
			TestEqual(TEXT("health at its maximum"), Health->GetHealth(), Health->MaxHealth, 0.01f);
			TestEqual(TEXT("one bandage left"), Inventory->GetCount(Slot), 1);
			TestFalse(TEXT("ended"), Bandage->IsApplying());
		});

		It("is cut by a blow: no heal, nothing used", [this]()
		{
			if (!Prepare(2, 60.0f))
			{
				return;
			}
			const int32 Slot = BandageSlot();
			Bandage->RequestApply();
			Advance(1.0f);
			Health->ApplyHealthDamage(5.0f, nullptr);
			TestFalse(TEXT("cut"), Bandage->IsApplying());
			TestEqual(TEXT("cause"), Bandage->GetApplyState().Interrupt, EBandageInterrupt::Hit);
			Advance(Rules->ApplySeconds);
			TestEqual(TEXT("not healed"), Health->GetHealth(), Health->MaxHealth - 65.0f, 0.01f);
			TestEqual(TEXT("both bandages kept"), Inventory->GetCount(Slot), 2);
		});

		It("is cut when put away", [this]()
		{
			if (!Prepare(1, 60.0f))
			{
				return;
			}
			Bandage->RequestApply();
			Advance(0.5f);
			Inventory->SelectSlot(BandageSlot());
			TestFalse(TEXT("put away"), Bandage->IsEquipped());
			TestFalse(TEXT("cut"), Bandage->IsApplying());
			TestEqual(TEXT("cause"), Bandage->GetApplyState().Interrupt, EBandageInterrupt::PutAway);
		});

		It("is refused at full health", [this]()
		{
			if (!Prepare(1, 0.0f))
			{
				return;
			}
			Bandage->RequestApply();
			TestFalse(TEXT("refused"), Bandage->IsApplying());
			TestEqual(TEXT("why"), Bandage->JudgeStart(), EBandageRefusal::FullHealth);
		});

		It("is refused for a bandage in no inventory of its carrier", [this]()
		{
			if (!Prepare(1, 60.0f))
			{
				return;
			}
			// a stray item owned by the bean but in none of its cells (another player's request reaches nothing else)
			FActorSpawnParameters Params;
			Params.Owner = Bean;
			ABandageItem* Stray = World->SpawnActor<ABandageItem>(ABandageItem::StaticClass(), FTransform::Identity, Params);
			if (!TestNotNull(TEXT("stray"), Stray))
			{
				return;
			}
			Stray->Rules = Rules;
			Stray->RequestApply();
			TestFalse(TEXT("refused"), Stray->IsApplying());
			TestEqual(TEXT("why"), Stray->JudgeStart(), EBandageRefusal::NotCarried);
		});

		It("refuses a flood of requests (rate limit)", [this]()
		{
			if (!Prepare(1, 0.0f))
			{
				return;
			}
			Rules->bUsableAtFullHealth = true;
			Rules->RequestBurst = 1.0f;
			Rules->RequestsPerSecond = 0.1f;
			Bandage->RequestApply();
			TestTrue(TEXT("first passes"), Bandage->IsApplying());
			Inventory->SelectSlot(BandageSlot());
			Inventory->SelectSlot(BandageSlot());
			TestTrue(TEXT("back in hand"), Bandage->IsEquipped());
			Bandage->RequestApply();
			TestFalse(TEXT("second refused at once"), Bandage->IsApplying());
		});
	});
}

#endif
