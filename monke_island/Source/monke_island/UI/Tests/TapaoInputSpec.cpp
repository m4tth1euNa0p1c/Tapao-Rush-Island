// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Slate/SceneViewport.h"
#include "TimerManager.h"
#include "UI/Tapao/TapaoUISubsystem.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SViewport.h"

BEGIN_DEFINE_SPEC(FTapaoInputSpec, "monke_island.UI.TapaoInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UGameInstance* GameInstance = nullptr;
	UWorld* World = nullptr;
	UGameViewportClient* Viewport = nullptr;
	ULocalPlayer* Player = nullptr;
	APlayerController* Controller = nullptr;
	UTapaoUISubsystem* UI = nullptr;
	TSharedPtr<SViewport> ViewportWidget;
	TSharedPtr<FSceneViewport> SceneViewport;
END_DEFINE_SPEC(FTapaoInputSpec)

void FTapaoInputSpec::Define()
{
	BeforeEach([this]()
	{
		// No game-instance Init: do not load saves, apply user settings or register global UI delegates.
		GameInstance = NewObject<UGameInstance>(GEngine);
		GameInstance->AddToRoot();
		const UWorld::InitializationValues Init = UWorld::InitializationValues()
			.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
		World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
		World->SetGameInstance(GameInstance);
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		Context.SetCurrentWorld(World);
		Context.OwningGameInstance = GameInstance;
		Viewport = NewObject<UGameViewportClient>(GEngine);
		Context.GameViewport = Viewport;
		ViewportWidget = SNew(SViewport);
		SceneViewport = FSceneViewport::Create(Viewport, ViewportWidget);
		Player = NewObject<ULocalPlayer>(GEngine);
		GameInstance->AddLocalPlayer(Player, FPlatformUserId::CreateFromInternalId(0));
		Controller = World->SpawnActor<APlayerController>();
		Controller->SetPlayer(Player);
		UI = NewObject<UTapaoUISubsystem>(GameInstance);
		UI->AddToRoot();
		UI->bInGame = true;
		UI->Curtain = SNew(SBox);
		// Travel inherits the viewport's UI-only mode from the main menu.
		Controller->SetInputMode(FInputModeUIOnly());
		Controller->SetShowMouseCursor(true);
	});

	AfterEach([this]()
	{
		GameInstance->GetTimerManager().ClearAllTimersForObject(UI);
		UI->RemoveFromRoot();
		UI = nullptr;
		Controller->Player = nullptr;
		Player->PlayerController = nullptr;
		GameInstance->RemoveLocalPlayer(Player);
		World->EndPlay(EEndPlayReason::Quit);
		World->DestroyWorld(false);
		SceneViewport.Reset();
		ViewportWidget.Reset();
		GEngine->DestroyWorldContext(World);
		GameInstance->RemoveFromRoot();
	});

	It("restores mouse capture and game input when the island loading curtain closes", [this]()
	{
		TestTrue(TEXT("menu initially ignores game input"), Viewport->IgnoreInput());
		UI->HideCurtain();
		TestFalse(TEXT("curtain removed"), UI->Curtain.IsValid());
		TestFalse(TEXT("game input enabled without opening pause"), Viewport->IgnoreInput());
		TestEqual(TEXT("permanent mouse capture"), Viewport->GetMouseCaptureMode(), EMouseCaptureMode::CapturePermanently);
		TestFalse(TEXT("game cursor hidden"), Controller->ShouldShowMouseCursor());
		TestTrue(TEXT("relative mouse movement requested"), Player->GetSlateOperations().ShouldUseHighPrecisionMouse());
		TestTrue(TEXT("viewport receives mouse capture"), Player->GetSlateOperations().GetMouseCaptor() == ViewportWidget);
	});

	It("keeps pause interactive if it was opened during the loading curtain", [this]()
	{
		UI->Pause = SNew(SBox);
		UI->HideCurtain();
		TestFalse(TEXT("curtain removed"), UI->Curtain.IsValid());
		TestTrue(TEXT("pause keeps UI input"), Viewport->IgnoreInput());
		TestEqual(TEXT("pause releases mouse"), Viewport->GetMouseCaptureMode(), EMouseCaptureMode::NoCapture);
		TestTrue(TEXT("pause cursor visible"), Controller->ShouldShowMouseCursor());
	});

	It("does not capture the mouse while returning to the main menu", [this]()
	{
		UI->bInGame = false;
		UI->MainMenu = SNew(SBox);
		UI->HideCurtain();
		TestTrue(TEXT("main menu keeps UI input"), Viewport->IgnoreInput());
		TestTrue(TEXT("main menu cursor visible"), Controller->ShouldShowMouseCursor());
	});

	It("waits for the local controller before revealing the island and restoring input", [this]()
	{
		Player->PlayerController = nullptr;
		UI->HideCurtain();
		TestTrue(TEXT("loading stays until local controller exists"), UI->Curtain.IsValid());
		Player->PlayerController = Controller;
		GameInstance->GetTimerManager().Tick(1.0f / 60.0f);
		TestFalse(TEXT("loading ends once controller exists"), UI->Curtain.IsValid());
		TestFalse(TEXT("late controller receives game input"), Viewport->IgnoreInput());
		TestEqual(TEXT("late controller captures mouse"), Viewport->GetMouseCaptureMode(), EMouseCaptureMode::CapturePermanently);
	});
}

#endif
