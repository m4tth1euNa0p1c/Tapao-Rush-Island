// monke_island

#include "Expedition/Core/DayTransitionSubsystem.h"
#include "Expedition/Core/DayTransitionRules.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Execution/ExecutionRules.h"
#include "Execution/ExecutionScene.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatRiderComponent.h"
#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonRules.h"
#include "Galleon/GalleonSpawner.h"
#include "Run/RunStateComponent.h"
#include "UI/Tapao/TapaoUISubsystem.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"

namespace
{
	/** DA_DayTransitionRules when DefaultGame.ini names none */
	const TCHAR* DefaultDayTransitionRulesPath = TEXT("/Game/MonkeIsland/Expedition/Data/DA_DayTransitionRules.DA_DayTransitionRules");

	/** Eased in and out (smoothstep) */
	double Ease(double Alpha)
	{
		const double A = FMath::Clamp(Alpha, 0.0, 1.0);
		return A * A * (3.0 - 2.0 * A);
	}

	/** This machine in the logs */
	const TCHAR* NetRoleName(const UWorld& World)
	{
		switch (World.GetNetMode())
		{
		case NM_Client: return TEXT("client");
		case NM_ListenServer: return TEXT("host");
		case NM_DedicatedServer: return TEXT("server");
		default: return TEXT("solo");
		}
	}

	/**
	 *  T-088: what this machine sees of the day, written at the departure and once the arrival is over, on the host and on
	 *  each client: the expedition (a new one on each island), the run (kept) and the local player's equipment (kept by
	 *  the server, FRunLoadout), so the logs of both machines prove the travel from island to island
	 */
	void LogLocalState(UWorld* World, const TCHAR* Moment)
	{
		if (!World)
		{
			return;
		}
		const AExpeditionGameState* Expedition = World->GetGameState<AExpeditionGameState>();
		const URunStateComponent* Run = URunStateComponent::Get(World);
		const APlayerController* Controller = World->GetFirstPlayerController();
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		const UIslanderInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
		FString Items;
		if (Inventory)
		{
			// the cells as this machine received them (rounds of a firearm: loaded + pouch)
			for (const FRunLoadoutItem& Item : Inventory->ExportLoadout().Items)
			{
				Items += FString::Printf(TEXT(" [%d] %s x%d"), Item.Slot, *Item.Definition.GetAssetName(), Item.Count);
				if (Item.LoadedRounds >= 0)
				{
					Items += FString::Printf(TEXT(" (%d+%d)"), Item.LoadedRounds, Item.ReserveRounds);
				}
			}
		}
		if (Items.IsEmpty())
		{
			Items = Inventory ? TEXT(" empty") : TEXT(" none (no pawn)");
		}
		const FString ExpeditionId = Expedition ? Expedition->GetExpeditionId().ToString() : FString(TEXT("none"));
		const FString FragmentId = Expedition ? Expedition->GetFragmentId().ToString() : FString(TEXT("none"));
		UE_LOG(Logmonke_island, Log, TEXT("DayTransition: %s on %s (%s): expedition %s, fragment %s, days played %d, contract day %d/%d, value %d/%d, treasury %d; inventory%s"),
			Moment, *World->GetMapName(), NetRoleName(*World), *ExpeditionId, *FragmentId, Run ? Run->GetDaysPlayed() : -1,
			Run ? Run->GetDayIndex() : 0, Run ? Run->GetDaysPerPeriod() : 0, Run ? Run->GetHoldValue() : 0, Run ? Run->GetQuota() : 0,
			Run ? Run->GetTreasury() : 0, *Items);
	}
}

UDayTransitionSubsystem::UDayTransitionSubsystem()
	: RulesPath(DefaultDayTransitionRulesPath)
{
}

bool UDayTransitionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld() && !IsRunningDedicatedServer();
}

UDayTransitionSubsystem* UDayTransitionSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine && WorldContext ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UDayTransitionSubsystem>() : nullptr;
}

void UDayTransitionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bLocal = InWorld.GetNetMode() != NM_DedicatedServer;
	Rules = Cast<UDayTransitionRules>(RulesPath.TryLoad());
	WorldStart = FPlatformTime::Seconds();
	bAwaitGalleon = false;
	for (TActorIterator<AGalleonSpawner> It(&InWorld); It && !bAwaitGalleon; ++It)
	{
		bAwaitGalleon = true;
	}
}

void UDayTransitionSubsystem::HoldBlack(bool bHold)
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CameraManager = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	if (!CameraManager || bHold == bHoldingBlack)
	{
		return;
	}
	bHoldingBlack = bHold;
	if (bHold)
	{
		CameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, false);
	}
	else
	{
		CameraManager->StartCameraFade(1.0f, 0.0f, FMath::Max(Rules->ArrivalFadeInSeconds, 0.1f), FLinearColor::Black, false, false);
	}
}

bool UDayTransitionSubsystem::IsIslandSettled()
{
	if (bSettled)
	{
		return true;
	}
	const double Now = FPlatformTime::Seconds();
	const double Since = Now - WorldStart;
	const float FrameMs = SmoothedFrameSeconds * 1000.0f;
	if (Since >= Rules->ArrivalSettleMaxSeconds || (Since >= Rules->ArrivalSettleMinSeconds && FrameMs <= Rules->ArrivalSettleFrameMs))
	{
		bSettled = true;
		SettledAt = Now;
		UE_LOG(Logmonke_island, Log, TEXT("DayTransition: the island settled after %.1f s in the black (frame %.0f ms)"), Since, FrameMs);
	}
	return bSettled;
}

void UDayTransitionSubsystem::Deinitialize()
{
	HideScreenMessages(false);
	Super::Deinitialize();
}

void UDayTransitionSubsystem::HideScreenMessages(bool bHide)
{
	if (!GEngine || bHide == bScreenMessagesHidden)
	{
		return;
	}
	if (bHide)
	{
		bScreenMessagesWereShown = GEngine->bEnableOnScreenDebugMessagesDisplay;
		GEngine->bEnableOnScreenDebugMessagesDisplay = false;
	}
	else
	{
		GEngine->bEnableOnScreenDebugMessagesDisplay = bScreenMessagesWereShown;
	}
	bScreenMessagesHidden = bHide;
}

TStatId UDayTransitionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDayTransitionSubsystem, STATGROUP_Tickables);
}

float UDayTransitionSubsystem::GetTitleAlpha() const
{
	if (!Rules || ArrivalStart < 0.0)
	{
		return 0.0f;
	}
	const double Age = FPlatformTime::Seconds() - ArrivalStart;
	if (Age < 0.0 || Age > Rules->TitleSeconds)
	{
		return 0.0f;
	}
	return static_cast<float>(FMath::Min(FMath::Min(Age / 0.5, 1.0), (Rules->TitleSeconds - Age) / 0.8));
}

void UDayTransitionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!Rules || !bLocal || !World)
	{
		return;
	}
	SmoothedFrameSeconds = FMath::Lerp(SmoothedFrameSeconds, FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.5f), 0.1f);
	const AExpeditionGameState* Expedition = World->GetGameState<AExpeditionGameState>();
	if (!Expedition)
	{
		return;
	}
	if (!bDeparting && !bExecuting && Expedition->IsDaySurvived() && !Expedition->IsTeamWiped())
	{
		if (Expedition->IsExecution())
		{
			StartExecution();
		}
		else
		{
			AGalleonActor* Ship = nullptr;
			for (TActorIterator<AGalleonActor> It(World); It && !Ship; ++It)
			{
				Ship = *It;
			}
			StartDeparture(Ship);
		}
	}
	HideScreenMessages(IsScenePlaying());
	if (bExecuting)
	{
		UpdateExecution();
		return;
	}
	if (bDeparting)
	{
		UpdateDeparture(DeltaTime);
		return;
	}
	if (!bArrivalDone)
	{
		const UGameInstance* GameInstance = World->GetGameInstance();
		const UTapaoUISubsystem* Screens = GameInstance ? GameInstance->GetSubsystem<UTapaoUISubsystem>() : nullptr;
		if (bArriving)
		{
			UpdateArrival();
		}
		else if (Screens && Screens->IsCurtainShown())
		{
			// the loading screen still covers the game: the scene waits for it, the wait counts from its end
			WorldStart = FPlatformTime::Seconds();
			bSettled = false;
			if (bAwaitGalleon)
			{
				HoldBlack(true);
			}
		}
		else if (!IsIslandSettled())
		{
			// the island's first heavy frames pass in the black, the camera moves once they are over
			HoldBlack(true);
		}
		else if (FPlatformTime::Seconds() - SettledAt > Rules->ArrivalWaitSeconds)
		{
			// nobody came to the deck or the boat in time: no scene, the screen comes back
			HoldBlack(false);
			bArrivalDone = true;
			LogLocalState(World, TEXT("arrival without scene"));
		}
		else
		{
			if (bAwaitGalleon)
			{
				HoldBlack(true);
			}
			StartArrival();
		}
	}
}

void UDayTransitionSubsystem::StartDeparture(AGalleonActor* Ship)
{
	UWorld* World = GetWorld();
	bDeparting = true;
	APlayerController* Controller = World->GetFirstPlayerController();
	HoldBlack(false);
	if (bArriving && Controller)
	{
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
		bArriving = false;
	}
	bArrivalDone = true;
	if (Ship)
	{
		Galleon = Ship;
		// the ship sails off bow first (its length runs along its Y); the camera is placed in that frame: X ahead
		const float BowYaw = static_cast<float>(Ship->GetActorRotation().Yaw) + (Ship->Rules ? Ship->Rules->BowYawDeg : 90.0f);
		SailDirection = FRotator(0.0f, BowYaw, 0.0f).Vector().GetSafeNormal2D();
		if (USceneComponent* Root = Ship->GetRootComponent())
		{
			Root->SetMobility(EComponentMobility::Movable);
		}
		const FTransform Frame(FRotator(0.0, BowYaw, 0.0), Ship->GetActorLocation());
		const FVector From = Frame.TransformPosition(Rules->DepartureCamera);
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Camera = World->SpawnActor<ACameraActor>(From, (Ship->GetActorLocation() - From).Rotation(), Params);
		if (Controller && Camera)
		{
			Controller->SetViewTargetWithBlend(Camera, Rules->DepartureBlendSeconds, VTBlend_Cubic);
		}
	}
	// the day is over and the crew aboard: nobody plays any more, the galleon sails off alone in the picture
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (It->IsPlayerControlled())
		{
			It->SetActorHiddenInGame(true);
			TArray<AActor*> Attached;
			It->GetAttachedActors(Attached, true, true);
			for (AActor* Gear : Attached)
			{
				Gear->SetActorHiddenInGame(true);
			}
		}
	}
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);
		Controller->SetIgnoreLookInput(true);
	}
	UE_LOG(Logmonke_island, Log, TEXT("DayTransition: departure%s"), Ship ? TEXT(", the galleon sails off") : TEXT(" (no galleon)"));
	LogLocalState(World, TEXT("departure"));
}

void UDayTransitionSubsystem::StartExecution()
{
	UWorld* World = GetWorld();
	bExecuting = true;
	APlayerController* Controller = World->GetFirstPlayerController();
	HoldBlack(false);
	bArriving = false;
	bArrivalDone = true;
	if (Controller)
	{
		Controller->SetIgnoreMoveInput(true);
		Controller->SetIgnoreLookInput(true);
	}
	UE_LOG(Logmonke_island, Log, TEXT("DayTransition: the quota is missed, execution of the crew"));
}

void UDayTransitionSubsystem::UpdateExecution()
{
	UWorld* World = GetWorld();
	AExecutionScene* Scene = AExecutionScene::Get(World);
	APlayerController* Controller = World->GetFirstPlayerController();
	if (!Scene || !Controller || !Controller->PlayerCameraManager)
	{
		// the scene comes with the next updates of the server
		return;
	}
	const UExecutionRules* Scenario = Scene->GetRules();
	const float Time = Scene->GetSceneTime();
	if (Time < 0.0f)
	{
		return;
	}
	APlayerCameraManager* CameraManager = Controller->PlayerCameraManager;
	// the title over the deck, then black while the crew is taken to the plank
	if (!bExecutionFadedOut && Time >= Scenario->PlaceAtSeconds - Scenario->FadeSeconds)
	{
		CameraManager->StartCameraFade(0.0f, 1.0f, Scenario->FadeSeconds, FLinearColor::Black, false, true);
		bExecutionFadedOut = true;
	}
	if (!bExecutionOnScene && Time >= Scenario->PlaceAtSeconds + 0.3f)
	{
		Controller->SetViewTargetWithBlend(Scene, 0.0f);
		CameraManager->SetGameCameraCutThisFrame();
		CameraManager->StartCameraFade(1.0f, 0.0f, Scenario->FadeSeconds, FLinearColor::Black, false, false);
		bExecutionOnScene = true;
	}
	if (!bExecutionEndFade && Time >= Scenario->EndAtSeconds - Scenario->FadeSeconds)
	{
		CameraManager->StartCameraFade(0.0f, 1.0f, Scenario->FadeSeconds, FLinearColor::Black, false, true);
		bExecutionEndFade = true;
	}
}

bool UDayTransitionSubsystem::IsExecutionOver() const
{
	const AExecutionScene* Scene = bExecuting ? AExecutionScene::Get(GetWorld()) : nullptr;
	return Scene && Scene->IsOver();
}

void UDayTransitionSubsystem::UpdateDeparture(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (AGalleonActor* Ship = Galleon.Get())
	{
		SailSpeedNow = FMath::Min(Rules->SailSpeed, SailSpeedNow + Rules->SailAcceleration * DeltaTime);
		Ship->AddActorWorldOffset(SailDirection * SailSpeedNow * DeltaTime);
		if (Camera)
		{
			// the eyes follow it as it goes
			const FRotator Look = (Ship->GetActorLocation() - Camera->GetActorLocation()).Rotation();
			Camera->SetActorRotation(FMath::RInterpTo(Camera->GetActorRotation(), Look, DeltaTime, 2.0f));
		}
	}
	const AExpeditionGameState* Expedition = World->GetGameState<AExpeditionGameState>();
	APlayerController* Controller = World->GetFirstPlayerController();
	if (bFaded || !Expedition || !Controller || !Controller->PlayerCameraManager)
	{
		return;
	}
	const double Left = Expedition->GetNextTravelTime() - Expedition->GetServerWorldTimeSeconds();
	if (Left <= Rules->FadeSeconds)
	{
		Controller->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FMath::Max(static_cast<float>(Left), 0.1f), FLinearColor::Black, false, true);
		bFaded = true;
	}
}

void UDayTransitionSubsystem::StartArrival()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World->GetFirstPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	// on the deck of the galleon at anchor: the camera comes from the open sea (X of the galleon), the island beyond it
	const AActor* Anchor = nullptr;
	FVector Offset = FVector::ZeroVector;
	for (TActorIterator<AGalleonActor> It(World); It && !Anchor; ++It)
	{
		if (It->IsActorAboard(Pawn))
		{
			Anchor = *It;
			Offset = Rules->ArrivalCamera;
		}
	}
	// no galleon in this map: from behind the boat the player sits in (while a galleon is awaited, the boat start is
	// only a step before the deck)
	if (!Anchor && !bAwaitGalleon)
	{
		const UBoatRiderComponent* Rider = Pawn->FindComponentByClass<UBoatRiderComponent>();
		Anchor = Rider ? Rider->GetSeatedBoat() : nullptr;
		Offset = Rules->ArrivalBoatCamera;
	}
	if (!Anchor)
	{
		return;
	}
	const FTransform Frame(FRotator(0.0, Anchor->GetActorRotation().Yaw, 0.0), Anchor->GetActorLocation());
	const FVector From = Frame.TransformPosition(Offset);
	ArrivalTo = Frame.TransformPosition(Anchor->IsA<AGalleonActor>() ? Rules->ArrivalCameraEnd : Rules->ArrivalBoatCameraEnd);
	const UCameraComponent* PawnCamera = Pawn->FindComponentByClass<UCameraComponent>();
	const FVector Eyes = PawnCamera ? PawnCamera->GetComponentLocation() : Pawn->GetActorLocation();
	ArrivalFrom = FTransform((Eyes - From).Rotation(), From);
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Camera = World->SpawnActor<ACameraActor>(From, ArrivalFrom.Rotator(), Params);
	if (!Camera)
	{
		HoldBlack(false);
		bArrivalDone = true;
		LogLocalState(World, TEXT("arrival without scene"));
		return;
	}
	Controller->SetViewTargetWithBlend(Camera, 0.0f);
	if (Controller->PlayerCameraManager)
	{
		// a cut: no motion blur nor temporal history across the jump of the view
		Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
	}
	Controller->SetIgnoreMoveInput(true);
	Controller->SetIgnoreLookInput(true);
	if (bHoldingBlack)
	{
		HoldBlack(false);
	}
	else if (Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FMath::Max(Rules->ArrivalFadeInSeconds, 0.1f), FLinearColor::Black, false, false);
	}
	bArriving = true;
	bCutting = false;
	ArrivalStart = FPlatformTime::Seconds();
	UE_LOG(Logmonke_island, Log, TEXT("DayTransition: arrival at %s"), *Anchor->GetName());
}

void UDayTransitionSubsystem::UpdateArrival()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World->GetFirstPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	APlayerCameraManager* CameraManager = Controller ? Controller->PlayerCameraManager.Get() : nullptr;
	const double Now = FPlatformTime::Seconds();
	const double Glide = FMath::Max(Rules->ArrivalSeconds, 0.5f);
	const double Age = Now - ArrivalStart;
	const UCameraComponent* PawnCamera = Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
	if (Camera && PawnCamera && !bCutting)
	{
		// in from the open sea toward the ship, the player's place in the middle of the picture; the glide stops outside
		// the hull and the rigging
		const FVector Location = FMath::Lerp(ArrivalFrom.GetLocation(), ArrivalTo, Ease(Age / Glide));
		Camera->SetActorLocationAndRotation(Location, (PawnCamera->GetComponentLocation() - Location).Rotation());
	}
	if (Pawn && Controller && CameraManager)
	{
		if (Age < Glide)
		{
			return;
		}
		// the cut: to black, then the player's eyes
		const float Cut = FMath::Max(Rules->ArrivalCutSeconds, 0.05f);
		if (!bCutting)
		{
			bCutting = true;
			CutStart = Now;
			CameraManager->StartCameraFade(0.0f, 1.0f, Cut, FLinearColor::Black, false, true);
			return;
		}
		if (Now - CutStart < Cut)
		{
			return;
		}
	}
	if (Controller)
	{
		if (Pawn)
		{
			Controller->SetViewTarget(Pawn);
			if (CameraManager)
			{
				CameraManager->SetGameCameraCutThisFrame();
			}
		}
		Controller->SetIgnoreMoveInput(false);
		Controller->SetIgnoreLookInput(false);
		if (bCutting && CameraManager)
		{
			CameraManager->StartCameraFade(1.0f, 0.0f, FMath::Max(Rules->ArrivalCutSeconds, 0.05f) * 1.5f, FLinearColor::Black, false, false);
		}
	}
	if (Camera)
	{
		Camera->SetLifeSpan(1.0f);
		Camera = nullptr;
	}
	bCutting = false;
	bArriving = false;
	bArrivalDone = true;
	LogLocalState(World, TEXT("arrival"));
}
