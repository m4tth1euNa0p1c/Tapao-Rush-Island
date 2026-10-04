// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Pacification/PacificationRules.h"
#include "Expedition/Pacification/PacificationTypes.h"
#include "Expedition/Pacification/PacificationZone.h"
#include "Expedition/Pacification/RoyalFlag.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AIController.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_TESTS

// Cahier §6.2 / recette n°15 (double validation = one success) and n°18 (sermon).
// Run with: ./scripts/ue.ps1 test -Filter monke_island.Expedition.Sermon
BEGIN_DEFINE_SPEC(FSermonSpec, "monke_island.Expedition.Sermon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UPacificationRules* Rules = nullptr;
	UWorld* World = nullptr;
	APacificationZone* Zone = nullptr;
	AExpeditionGameState* Expedition = nullptr;
	int32 PacifiedCount = 0;
	ESermonInterruptReason LastInterrupt = ESermonInterruptReason::None;

	/** Spawns a movable pawn (sphere root) at this location */
	APawn* SpawnPawn(const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ADefaultPawn>(ADefaultPawn::StaticClass(), FTransform(Location), Params);
	}

	/** Gives an actor a registered health component (player or creature health) */
	UIslanderHealthComponent* AddHealth(AActor* Actor)
	{
		UIslanderHealthComponent* Health = NewObject<UIslanderHealthComponent>(Actor);
		Health->RegisterComponent();
		return Health;
	}

	/** Sets the world clock read by the zone (no tick runs in the test world) */
	void SetTime(double Seconds)
	{
		World->TimeSeconds = Seconds;
	}

	/** Both the sermon and the authoritative clock observe the same server instant */
	void UpdateSermon(double Seconds)
	{
		SetTime(Seconds);
		Zone->ServerUpdateSermon(Seconds);
	}

	/** Objective without threats, flag planted by a pawn standing at the socket */
	void PlantFlagWithoutThreats(APawn* Planter)
	{
		Zone->BeginObjective({});
		Zone->TryPlantFlag(Planter);
	}
END_DEFINE_SPEC(FSermonSpec)

void FSermonSpec::Define()
{
	BeforeEach([this]()
	{
		Rules = NewObject<UPacificationRules>();
		Rules->AddToRoot();
	});

	AfterEach([this]()
	{
		Rules->RemoveFromRoot();
		Rules = nullptr;
	});

	Describe("Logic", [this]()
	{
		It("uses the Standard values of the cahier: 20 s sermon, 10 cm tolerance", [this]()
		{
			TestEqual(TEXT("duration"), Rules->SermonDuration, 20.0f);
			TestEqual(TEXT("tolerance"), Rules->PositionToleranceCm, 10.0f);
		});

		It("lets jitter under the tolerance through and breaks on a real step", [this]()
		{
			const FVector Start(100.0, 200.0, 50.0);
			TestTrue(TEXT("9 cm jitter"), FPacificationLogic::IsWithinTolerance(Start, Start + FVector(6.0, 6.0, 0.0), 10.0f, 25.0f));
			TestFalse(TEXT("11 cm step"), FPacificationLogic::IsWithinTolerance(Start, Start + FVector(11.0, 0.0, 0.0), 10.0f, 25.0f));
			TestTrue(TEXT("slope bobbing"), FPacificationLogic::IsWithinTolerance(Start, Start + FVector(0.0, 0.0, 20.0), 10.0f, 25.0f));
			TestFalse(TEXT("jump"), FPacificationLogic::IsWithinTolerance(Start, Start + FVector(0.0, 0.0, 40.0), 10.0f, 25.0f));
		});

		It("names the first reason that stops a running sermon", [this]()
		{
			FSermonState State;
			State.Begin(nullptr, FVector::ZeroVector, 0.0, true);

			FSermonObservation Still;
			TestEqual(TEXT("still"), FPacificationLogic::EvaluateSermon(State, Still, 10.0f, 25.0f), ESermonInterruptReason::None);

			FSermonObservation Obs = Still;
			Obs.bHolding = false;
			TestEqual(TEXT("released"), FPacificationLogic::EvaluateSermon(State, Obs, 10.0f, 25.0f), ESermonInterruptReason::Released);

			Obs = Still;
			Obs.bDamaged = true;
			TestEqual(TEXT("damaged"), FPacificationLogic::EvaluateSermon(State, Obs, 10.0f, 25.0f), ESermonInterruptReason::Damaged);

			Obs = Still;
			Obs.Location = FVector(15.0, 0.0, 0.0);
			TestEqual(TEXT("moved"), FPacificationLogic::EvaluateSermon(State, Obs, 10.0f, 25.0f), ESermonInterruptReason::Moved);

			Obs = Still;
			Obs.bOfficiantAlive = false;
			TestEqual(TEXT("died"), FPacificationLogic::EvaluateSermon(State, Obs, 10.0f, 25.0f), ESermonInterruptReason::OfficiantDied);

			Obs = Still;
			Obs.bOfficiantConnected = false;
			TestEqual(TEXT("disconnected"), FPacificationLogic::EvaluateSermon(State, Obs, 10.0f, 25.0f), ESermonInterruptReason::OfficiantLost);

			Obs = Still;
			Obs.bOfficiantValid = false;
			TestEqual(TEXT("destroyed"), FPacificationLogic::EvaluateSermon(State, Obs, 10.0f, 25.0f), ESermonInterruptReason::OfficiantLost);

			Obs = Still;
			Obs.bThreatInPerimeter = true;
			TestEqual(TEXT("threat"), FPacificationLogic::EvaluateSermon(State, Obs, 10.0f, 25.0f), ESermonInterruptReason::ThreatInPerimeter);

			Obs = Still;
			Obs.bZoneSubmerged = true;
			TestEqual(TEXT("submerged"), FPacificationLogic::EvaluateSermon(State, Obs, 10.0f, 25.0f), ESermonInterruptReason::ZoneSubmerged);

			FSermonState Idle;
			TestEqual(TEXT("nothing to stop"), FPacificationLogic::EvaluateSermon(Idle, Obs, 10.0f, 25.0f), ESermonInterruptReason::None);
		});

		It("counts consecutive seconds only and never carries time over a restart", [this]()
		{
			FSermonState State;
			State.Begin(nullptr, FVector::ZeroVector, 100.0, false);
			TestEqual(TEXT("half"), State.Progress01(110.0, 20.0f), 0.5f, 0.001f);
			TestFalse(TEXT("19.9 s"), State.IsComplete(119.9, 20.0f));
			TestTrue(TEXT("20 s"), State.IsComplete(120.0, 20.0f));

			State.Reset();
			TestEqual(TEXT("reset"), State.Progress01(120.0, 20.0f), 0.0f);
			State.Begin(nullptr, FVector::ZeroVector, 125.0, false);
			TestEqual(TEXT("restart from zero"), State.Progress01(130.0, 20.0f), 0.25f, 0.001f);
		});

		It("plants the flag only when every designated threat is dead, sermon only on a planted flag", [this]()
		{
			TestFalse(TEXT("threat alive"), FPacificationLogic::CanPlantFlag(EZoneState::Contested, 1));
			TestTrue(TEXT("all dead"), FPacificationLogic::CanPlantFlag(EZoneState::Contested, 0));
			TestFalse(TEXT("undiscovered"), FPacificationLogic::CanPlantFlag(EZoneState::Undiscovered, 0));
			TestFalse(TEXT("already pacified"), FPacificationLogic::CanPlantFlag(EZoneState::Pacified, 0));
			TestTrue(TEXT("securing"), FPacificationLogic::AcceptsSermon(EZoneState::Securing, true));
			TestFalse(TEXT("no flag"), FPacificationLogic::AcceptsSermon(EZoneState::Securing, false));
			TestFalse(TEXT("pacified: not farmable"), FPacificationLogic::AcceptsSermon(EZoneState::Pacified, true));
		});
	});

	Describe("Zone", [this]()
	{
		BeforeEach([this]()
		{
			const FName WorldName = MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("SermonSpecWorld"), EUniqueObjectNameOptions::GloballyUnique);
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, WorldName, GetTransientPackage());
			World->AddToRoot();
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			SetTime(0.0);
			Expedition = World->SpawnActor<AExpeditionGameState>();
			Expedition->bStartOnBeginPlay = false;
			Expedition->Rules = NewObject<UExpeditionRules>(Expedition);
			World->SetGameState(Expedition);
			TestTrue(TEXT("expedition started"), Expedition->StartExpedition());

			Zone = World->SpawnActor<APacificationZone>(APacificationZone::StaticClass(), FTransform::Identity);
			Zone->SetRules(Rules);
			Zone->SetZoneId(TEXT("Zone_Test"));

			PacifiedCount = 0;
			LastInterrupt = ESermonInterruptReason::None;
			Zone->OnZonePacifiedNative.AddLambda([this](const FGuid&, FName, double) { ++PacifiedCount; });
			Zone->OnSermonInterruptedNative.AddLambda([this](FName, ESermonInterruptReason Reason) { LastInterrupt = Reason; });
		});

		AfterEach([this]()
		{
			Zone = nullptr;
			Expedition = nullptr;
			if (World)
			{
				GEngine->ShutdownWorldNetDriver(World);
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
				World->RemoveFromRoot();
				World = nullptr;
			}
		});

		It("fixes the designated threats once and refuses the flag while one is alive", [this]()
		{
			APawn* Planter = SpawnPawn(FVector(50.0, 0.0, 0.0));
			APawn* Threat = SpawnPawn(FVector(5000.0, 0.0, 0.0));
			TestEqual(TEXT("objective"), Zone->BeginObjective({ Threat }), EPacificationRequestResult::Accepted);
			TestEqual(TEXT("fixed once"), Zone->BeginObjective({}), EPacificationRequestResult::WrongState);
			TestEqual(TEXT("state"), Zone->GetZoneState(), EZoneState::Contested);

			TestEqual(TEXT("threat alive"), Zone->TryPlantFlag(Planter), EPacificationRequestResult::ThreatsRemaining);
			Threat->Destroy();
			TestEqual(TEXT("threat dead"), Zone->TryPlantFlag(Planter), EPacificationRequestResult::Accepted);
			TestNotNull(TEXT("flag"), Zone->GetFlag());
			TestEqual(TEXT("flag zone"), Zone->GetFlag() ? Zone->GetFlag()->GetZoneId() : NAME_None, FName(TEXT("Zone_Test")));
		});

		It("grants no bonus for the flag alone, and refuses the sermon before it", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			Zone->BeginObjective({});
			TestEqual(TEXT("no flag"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::FlagNotPlanted);
			Zone->TryPlantFlag(Officiant);
			TestEqual(TEXT("securing"), Zone->GetZoneState(), EZoneState::Securing);
			TestEqual(TEXT("no bonus"), PacifiedCount, 0);
			TestFalse(TEXT("not awarded"), Zone->WasPacificationAwarded());
		});

		It("refuses a pawn too far from the flag", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			APawn* FarAway = SpawnPawn(FVector(Rules->FlagReachRadiusCm + 100.0f, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			TestEqual(TEXT("out of reach"), Zone->TryStartSermon(FarAway), EPacificationRequestResult::OutOfReach);
		});

		It("validates 20 s without moving once, then the zone is not farmable", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			SetTime(10.0);
			TestEqual(TEXT("start"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::Accepted);

			UpdateSermon(29.9);
			TestEqual(TEXT("not yet"), Zone->GetZoneState(), EZoneState::Securing);
			TestEqual(TEXT("no bonus before the end"), PacifiedCount, 0);

			UpdateSermon(30.0);
			TestEqual(TEXT("pacified"), Zone->GetZoneState(), EZoneState::Pacified);
			TestEqual(TEXT("one bonus"), PacifiedCount, 1);

			UpdateSermon(31.0);
			SetTime(40.0);
			TestEqual(TEXT("not farmable"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::WrongState);
			UpdateSermon(80.0);
			TestEqual(TEXT("still one bonus"), PacifiedCount, 1);
			TestTrue(TEXT("awarded"), Zone->WasPacificationAwarded());
		});

		It("keeps a single officiant: simultaneous validation gives one success", [this]()
		{
			APawn* First = SpawnPawn(FVector(50.0, 0.0, 0.0));
			APawn* Second = SpawnPawn(FVector(-50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(First);
			TestEqual(TEXT("first"), Zone->TryStartSermon(First), EPacificationRequestResult::Accepted);
			TestEqual(TEXT("second refused"), Zone->TryStartSermon(Second), EPacificationRequestResult::OfficiantBusy);
			TestEqual(TEXT("repeat keeps the progress"), Zone->TryStartSermon(First), EPacificationRequestResult::AlreadyOfficiant);

			UpdateSermon(20.0);
			TestEqual(TEXT("second after the end"), Zone->TryStartSermon(Second), EPacificationRequestResult::WrongState);
			UpdateSermon(20.0);
			TestEqual(TEXT("one success"), PacifiedCount, 1);
		});

		It("lets looking around and jitter through", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);

			Officiant->SetActorRotation(FRotator(0.0, 170.0, 0.0));
			Officiant->SetActorLocation(FVector(55.0, 4.0, 0.0));
			UpdateSermon(10.0);
			TestEqual(TEXT("not interrupted"), LastInterrupt, ESermonInterruptReason::None);
			TestEqual(TEXT("still officiant"), Zone->GetOfficiant(), Officiant);
			UpdateSermon(20.0);
			TestEqual(TEXT("pacified"), PacifiedCount, 1);
		});

		It("resets to zero when the officiant moves more than the tolerance", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			UpdateSermon(15.0);

			Officiant->SetActorLocation(FVector(50.0 + Rules->PositionToleranceCm + 1.0, 0.0, 0.0));
			UpdateSermon(16.0);
			TestEqual(TEXT("moved"), LastInterrupt, ESermonInterruptReason::Moved);
			TestNull(TEXT("no officiant"), Zone->GetOfficiant());
			TestEqual(TEXT("progress reset"), Zone->GetSermonProgress01(), 0.0f);
			UpdateSermon(25.0);
			TestEqual(TEXT("no bonus"), PacifiedCount, 0);
			TestEqual(TEXT("flag stays"), Zone->GetZoneState(), EZoneState::Securing);
		});

		It("resets to zero on damage", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			Officiant->OnTakeAnyDamage.Broadcast(Officiant, 5.0f, nullptr, nullptr, nullptr);
			TestEqual(TEXT("damaged"), LastInterrupt, ESermonInterruptReason::Damaged);
			TestNull(TEXT("no officiant"), Zone->GetOfficiant());
		});

		It("resets to zero on release, and a new officiant restarts from zero", [this]()
		{
			APawn* First = SpawnPawn(FVector(50.0, 0.0, 0.0));
			APawn* Second = SpawnPawn(FVector(-50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(First);
			Zone->TryStartSermon(First);
			UpdateSermon(15.0);

			Zone->StopSermon(Second);
			TestEqual(TEXT("only the officiant can release"), Zone->GetOfficiant(), First);
			Zone->StopSermon(First);
			TestEqual(TEXT("released"), LastInterrupt, ESermonInterruptReason::Released);

			SetTime(15.0);
			TestEqual(TEXT("second starts"), Zone->TryStartSermon(Second), EPacificationRequestResult::Accepted);
			UpdateSermon(20.0);
			TestEqual(TEXT("no cumulated time"), PacifiedCount, 0);
			TestEqual(TEXT("quarter"), Zone->GetSermonProgress01(), 0.25f, 0.001f);
			UpdateSermon(35.0);
			TestEqual(TEXT("full 20 s of the second"), PacifiedCount, 1);
		});

		It("resets to zero when the officiant is destroyed", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			Officiant->Destroy();
			TestEqual(TEXT("lost"), LastInterrupt, ESermonInterruptReason::OfficiantLost);
			UpdateSermon(25.0);
			TestEqual(TEXT("no bonus"), PacifiedCount, 0);
		});

		It("stops the sermon when the zone goes under water", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			Zone->SetSubmerged();
			TestEqual(TEXT("submerged"), LastInterrupt, ESermonInterruptReason::ZoneSubmerged);
			TestEqual(TEXT("state"), Zone->GetZoneState(), EZoneState::Submerged);
			TestEqual(TEXT("no sermon"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::WrongState);
		});

		It("breaks when a living designated threat stands inside the perimeter", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			APawn* Threat = SpawnPawn(FVector(500.0, 0.0, 0.0));
			UIslanderHealthComponent* ThreatHealth = AddHealth(Threat);
			Zone->BeginObjective({ Threat });
			TestEqual(TEXT("one threat"), Zone->GetRemainingThreats(), 1);

			ThreatHealth->ApplyHealthDamage(ThreatHealth->MaxHealth, nullptr);
			TestEqual(TEXT("recounted on death"), Zone->GetRemainingThreats(), 0);
			TestEqual(TEXT("dead threat: plant"), Zone->TryPlantFlag(Officiant), EPacificationRequestResult::Accepted);
			TestEqual(TEXT("start"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::Accepted);

			ThreatHealth->ResetHealth();
			UpdateSermon(5.0);
			TestEqual(TEXT("threat in perimeter"), LastInterrupt, ESermonInterruptReason::ThreatInPerimeter);
			TestEqual(TEXT("refused while inside"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::ThreatsRemaining);

			Threat->SetActorLocation(FVector(Rules->PerimeterRadiusCm + 500.0f, 0.0, 0.0));
			TestEqual(TEXT("outside: accepted"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::Accepted);
		});

		It("recounts the designated threats when one is destroyed", [this]()
		{
			APawn* First = SpawnPawn(FVector(5000.0, 0.0, 0.0));
			APawn* Second = SpawnPawn(FVector(6000.0, 0.0, 0.0));
			Zone->BeginObjective({ First, Second });
			TestEqual(TEXT("two"), Zone->GetRemainingThreats(), 2);
			First->Destroy();
			TestEqual(TEXT("one"), Zone->GetRemainingThreats(), 1);
		});

		It("resets on direct health damage and on death through the health component", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			UIslanderHealthComponent* Health = AddHealth(Officiant);
			PlantFlagWithoutThreats(Officiant);

			Zone->TryStartSermon(Officiant);
			Health->ApplyHealthDamage(5.0f, nullptr);
			TestEqual(TEXT("coconut damage"), LastInterrupt, ESermonInterruptReason::Damaged);

			TestEqual(TEXT("restart"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::Accepted);
			Health->ApplyHealthDamage(Health->MaxHealth, nullptr);
			TestEqual(TEXT("death"), LastInterrupt, ESermonInterruptReason::OfficiantDied);
			TestNull(TEXT("no officiant"), Zone->GetOfficiant());
			TestEqual(TEXT("dead but not destroyed counts as dead"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::InvalidPawn);
			UpdateSermon(30.0);
			TestEqual(TEXT("no bonus"), PacifiedCount, 0);
		});

		It("resets when the officiant loses its controller (disconnection)", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			AAIController* Controller = World->SpawnActor<AAIController>(AAIController::StaticClass(), FTransform::Identity);
			if (!TestNotNull(TEXT("controller"), Controller))
			{
				return;
			}
			Controller->Possess(Officiant);
			TestEqual(TEXT("possessed"), Officiant->GetController(), static_cast<AController*>(Controller));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);

			Controller->UnPossess();
			UpdateSermon(5.0);
			TestEqual(TEXT("lost"), LastInterrupt, ESermonInterruptReason::OfficiantLost);
		});

		It("refuses a moving pawn and a pawn already officiating at another flag", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);

			Officiant->GetMovementComponent()->Velocity = FVector(Rules->MaxStartSpeedCmS + 50.0f, 0.0, 0.0);
			TestEqual(TEXT("moving"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::NotStandingStill);
			Officiant->GetMovementComponent()->Velocity = FVector::ZeroVector;
			TestEqual(TEXT("still"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::Accepted);

			APacificationZone* Other = World->SpawnActor<APacificationZone>(APacificationZone::StaticClass(), FTransform(FVector(100.0, 0.0, 0.0)));
			Other->SetRules(Rules);
			Other->SetZoneId(TEXT("Zone_Other"));
			Other->BeginObjective({});
			TestEqual(TEXT("other flag"), Other->TryPlantFlag(Officiant), EPacificationRequestResult::Accepted);
			TestEqual(TEXT("busy elsewhere"), Other->TryStartSermon(Officiant), EPacificationRequestResult::OfficiantBusy);
		});

		It("commits the 120 s team bonus after 20 s and only once on the real GameState", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			TestEqual(TEXT("Standard deadline without a sermon"), Expedition->GetFloodStartTime(), 540.0);
			SetTime(55.0);
			TestEqual(TEXT("start"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::Accepted);
			TestEqual(TEXT("automatic expedition binding"), Zone->GetExpeditionId(), Expedition->GetExpeditionId());
			UpdateSermon(74.9);
			TestEqual(TEXT("no early bonus"), Expedition->GetBonusedZoneCount(), 0);
			Zone->OnZonePacifiedNative.AddLambda([this](const FGuid&, FName, double)
			{
				TestTrue(TEXT("bonus committed before success event"), Expedition->HasZoneBonus(Zone->GetZoneId()));
				TestEqual(TEXT("deadline visible to listeners"), Expedition->GetFloodStartTime(), 660.0);
			});
			UpdateSermon(75.0);
			TestEqual(TEXT("pacified"), Zone->GetZoneState(), EZoneState::Pacified);
			TestEqual(TEXT("remaining time"), Expedition->GetTimeBeforeSubmersion(), 585.0);
			UpdateSermon(76.0);
			TestEqual(TEXT("repeat refused"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::WrongState);
			TestEqual(TEXT("one event"), PacifiedCount, 1);
			TestEqual(TEXT("one bonus"), Expedition->GetBonusedZoneCount(), 1);
			TestEqual(TEXT("deadline unchanged by repeats"), Expedition->GetFloodStartTime(), 660.0);
		});

		It("refuses a sermon without an expedition clock", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			World->SetGameState(nullptr);
			TestEqual(TEXT("no clock"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::NoExpedition);
			TestEqual(TEXT("no success"), PacifiedCount, 0);
			World->SetGameState(Expedition);
		});

		It("refuses a zone from another expedition", [this]()
		{
			Zone->SetExpeditionId(FGuid::NewGuid());
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			TestEqual(TEXT("stale expedition"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::WrongExpedition);
			TestEqual(TEXT("no bonus"), Expedition->GetBonusedZoneCount(), 0);
		});

		It("refuses an unidentified zone", [this]()
		{
			Zone->SetZoneId(NAME_None);
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			TestEqual(TEXT("invalid id"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::InvalidZone);
		});

		It("refuses to start when the full sermon would end at the flood deadline", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			SetTime(520.0);
			TestEqual(TEXT("equality loses"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::TooLate);
			TestNull(TEXT("no officiant"), Zone->GetOfficiant());
		});

		It("does not pacify at the deadline even if the phase timer has not fired", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			SetTime(519.0);
			TestEqual(TEXT("start before deadline"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::Accepted);
			UpdateSermon(540.0);
			TestEqual(TEXT("submerged"), Zone->GetZoneState(), EZoneState::Submerged);
			TestEqual(TEXT("no success"), PacifiedCount, 0);
			TestFalse(TEXT("not awarded"), Zone->WasPacificationAwarded());
			TestEqual(TEXT("deadline unchanged"), Expedition->GetFloodStartTime(), 540.0);
		});

		It("stops the officiant when the GameState enters submersion", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			SetTime(540.0);
			TestEqual(TEXT("clock settles its phase"), Expedition->AddZoneBonus(TEXT("LateZone")), EZoneBonusResult::TooLate);
			TestEqual(TEXT("phase propagated"), Zone->GetZoneState(), EZoneState::Submerged);
			TestEqual(TEXT("interrupted"), LastInterrupt, ESermonInterruptReason::ZoneSubmerged);
			TestNull(TEXT("officiant released"), Zone->GetOfficiant());
		});

		It("does not announce pacification when the clock refuses the zone limit", [this]()
		{
			Expedition->Rules->ZonesCount = 1;
			TestEqual(TEXT("another zone used the limit"), Expedition->AddZoneBonus(TEXT("Other")), EZoneBonusResult::Granted);
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			AddExpectedError(TEXT("bonus of zone 'Zone_Test' refused"), EAutomationExpectedErrorFlags::Contains, 1);
			AddExpectedError(TEXT("bonus refused"), EAutomationExpectedErrorFlags::Contains, 1);
			UpdateSermon(20.0);
			TestEqual(TEXT("not pacified"), Zone->GetZoneState(), EZoneState::Securing);
			TestFalse(TEXT("not awarded"), Zone->WasPacificationAwarded());
			TestEqual(TEXT("no success"), PacifiedCount, 0);
			TestEqual(TEXT("failure exposed"), LastInterrupt, ESermonInterruptReason::BonusRefused);
			TestEqual(TEXT("deadline unchanged"), Expedition->GetFloodStartTime(), 660.0);
		});

		It("sends the expedition id with the single pacification", [this]()
		{
			const FGuid ExpeditionId = Expedition->GetExpeditionId();
			FGuid Received;
			Zone->SetExpeditionId(ExpeditionId);
			Zone->OnZonePacifiedNative.AddLambda([&Received](const FGuid& Id, FName, double) { Received = Id; });

			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			UpdateSermon(20.0);
			TestEqual(TEXT("expedition"), Received, ExpeditionId);
			TestEqual(TEXT("once"), PacifiedCount, 1);
		});
	});
}

#endif
