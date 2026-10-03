// monke_island

#include "Misc/AutomationTest.h"
#include "Expedition/Pacification/PacificationRules.h"
#include "Expedition/Pacification/PacificationTypes.h"
#include "Expedition/Pacification/PacificationZone.h"
#include "Expedition/Pacification/RoyalFlag.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_TESTS

// Cahier §6.2 / recette n°15 (double validation = one success) and n°18 (sermon).
// Run with: ./scripts/ue.ps1 test -Filter monke_island.Expedition.Sermon
BEGIN_DEFINE_SPEC(FSermonSpec, "monke_island.Expedition.Sermon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UPacificationRules* Rules = nullptr;
	UWorld* World = nullptr;
	APacificationZone* Zone = nullptr;
	int32 PacifiedCount = 0;
	ESermonInterruptReason LastInterrupt = ESermonInterruptReason::None;

	/** Spawns a movable pawn (sphere root) at this location */
	APawn* SpawnPawn(const FVector& Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<ADefaultPawn>(ADefaultPawn::StaticClass(), FTransform(Location), Params);
	}

	/** Sets the world clock read by the zone (no tick runs in the test world) */
	void SetTime(double Seconds)
	{
		World->TimeSeconds = Seconds;
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

			Zone = World->SpawnActor<APacificationZone>(APacificationZone::StaticClass(), FTransform::Identity);
			Zone->SetRules(Rules);
			Zone->SetZoneId(TEXT("Zone_Test"));

			PacifiedCount = 0;
			LastInterrupt = ESermonInterruptReason::None;
			Zone->OnZonePacifiedNative.AddLambda([this](FName, double) { ++PacifiedCount; });
			Zone->OnSermonInterruptedNative.AddLambda([this](FName, ESermonInterruptReason Reason) { LastInterrupt = Reason; });
		});

		AfterEach([this]()
		{
			Zone = nullptr;
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

			Zone->ServerUpdateSermon(29.9);
			TestEqual(TEXT("not yet"), Zone->GetZoneState(), EZoneState::Securing);
			TestEqual(TEXT("no bonus before the end"), PacifiedCount, 0);

			Zone->ServerUpdateSermon(30.0);
			TestEqual(TEXT("pacified"), Zone->GetZoneState(), EZoneState::Pacified);
			TestEqual(TEXT("one bonus"), PacifiedCount, 1);

			Zone->ServerUpdateSermon(31.0);
			SetTime(40.0);
			TestEqual(TEXT("not farmable"), Zone->TryStartSermon(Officiant), EPacificationRequestResult::WrongState);
			Zone->ServerUpdateSermon(80.0);
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

			Zone->ServerUpdateSermon(20.0);
			TestEqual(TEXT("second after the end"), Zone->TryStartSermon(Second), EPacificationRequestResult::WrongState);
			Zone->ServerUpdateSermon(20.0);
			TestEqual(TEXT("one success"), PacifiedCount, 1);
		});

		It("lets looking around and jitter through", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);

			Officiant->SetActorRotation(FRotator(0.0, 170.0, 0.0));
			Officiant->SetActorLocation(FVector(55.0, 4.0, 0.0));
			Zone->ServerUpdateSermon(10.0);
			TestEqual(TEXT("not interrupted"), LastInterrupt, ESermonInterruptReason::None);
			TestEqual(TEXT("still officiant"), Zone->GetOfficiant(), Officiant);
			Zone->ServerUpdateSermon(20.0);
			TestEqual(TEXT("pacified"), PacifiedCount, 1);
		});

		It("resets to zero when the officiant moves more than the tolerance", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			Zone->ServerUpdateSermon(15.0);

			Officiant->SetActorLocation(FVector(50.0 + Rules->PositionToleranceCm + 1.0, 0.0, 0.0));
			Zone->ServerUpdateSermon(16.0);
			TestEqual(TEXT("moved"), LastInterrupt, ESermonInterruptReason::Moved);
			TestNull(TEXT("no officiant"), Zone->GetOfficiant());
			TestEqual(TEXT("progress reset"), Zone->GetSermonProgress01(), 0.0f);
			Zone->ServerUpdateSermon(25.0);
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
			Zone->ServerUpdateSermon(15.0);

			Zone->StopSermon(Second);
			TestEqual(TEXT("only the officiant can release"), Zone->GetOfficiant(), First);
			Zone->StopSermon(First);
			TestEqual(TEXT("released"), LastInterrupt, ESermonInterruptReason::Released);

			SetTime(15.0);
			TestEqual(TEXT("second starts"), Zone->TryStartSermon(Second), EPacificationRequestResult::Accepted);
			Zone->ServerUpdateSermon(20.0);
			TestEqual(TEXT("no cumulated time"), PacifiedCount, 0);
			TestEqual(TEXT("quarter"), Zone->GetSermonProgress01(), 0.25f, 0.001f);
			Zone->ServerUpdateSermon(35.0);
			TestEqual(TEXT("full 20 s of the second"), PacifiedCount, 1);
		});

		It("resets to zero when the officiant is destroyed", [this]()
		{
			APawn* Officiant = SpawnPawn(FVector(50.0, 0.0, 0.0));
			PlantFlagWithoutThreats(Officiant);
			Zone->TryStartSermon(Officiant);
			Officiant->Destroy();
			TestEqual(TEXT("lost"), LastInterrupt, ESermonInterruptReason::OfficiantLost);
			Zone->ServerUpdateSermon(25.0);
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
	});
}

#endif
