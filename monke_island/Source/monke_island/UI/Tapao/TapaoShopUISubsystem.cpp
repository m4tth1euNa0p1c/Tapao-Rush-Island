// monke_island

#include "UI/Tapao/TapaoShopUISubsystem.h"
#include "UI/Tapao/STapaoShop.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Run/RunStateComponent.h"
#include "Shop/ShopkeeperActor.h"
#include "UObject/UObjectGlobals.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"

/** Échap or F closes the shop before anything else sees the key (the pause would open on Échap) */
class FTapaoShopInputProcessor : public IInputProcessor
{
public:

	explicit FTapaoShopInputProcessor(UTapaoShopUISubsystem* InOwner) : Owner(InOwner) {}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		UTapaoShopUISubsystem* Subsystem = Owner.Get();
		if (!Subsystem || !Subsystem->IsShopOpen() || InKeyEvent.IsRepeat())
		{
			return false;
		}
		if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::F)
		{
			Subsystem->CloseShop();
			return true;
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("TapaoShop"); }

private:

	TWeakObjectPtr<UTapaoShopUISubsystem> Owner;
};

bool UTapaoShopUISubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && !IsRunningCommandlet();
}

void UTapaoShopUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Skin = Cast<UTapaoUISkin>(SkinPath.TryLoad());
	if (!Skin)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("TapaoShop: %s not found, no shop screen"), *SkinPath.ToString());
		return;
	}
	Style = MakeShared<FTapaoStyle>(Skin);
	RequestHandle = UIslanderInventoryComponent::OnShopRequested.AddUObject(this, &UTapaoShopUISubsystem::HandleShopRequested);
	ResultHandle = UIslanderInventoryComponent::OnShopResult.AddUObject(this, &UTapaoShopUISubsystem::HandleShopResult);
	PreLoadHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UTapaoShopUISubsystem::HandlePreLoadMap);
	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FTapaoShopInputProcessor>(this);
		// first in line, before the pause of UTapaoUISubsystem
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
	}
}

void UTapaoShopUISubsystem::Deinitialize()
{
	UIslanderInventoryComponent::OnShopRequested.Remove(RequestHandle);
	UIslanderInventoryComponent::OnShopResult.Remove(ResultHandle);
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadHandle);
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	CloseShop();
	Super::Deinitialize();
}

void UTapaoShopUISubsystem::HandleShopRequested(UIslanderInventoryComponent* Inventory, AShopkeeperActor* Shop)
{
	UGameInstance* GameInstance = GetGameInstance();
	APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	// only the screen of this machine's player (PIE runs several game instances in one process)
	if (!Style.IsValid() || !Inventory || !Shop || !Controller || !Viewport || Controller->GetPawn() != Inventory->GetOwner())
	{
		return;
	}
	CloseShop();
	Screen = SNew(STapaoShopScreen)
		.Style(Style)
		.Shop(Shop)
		.Inventory(Inventory)
		.OnClose(FSimpleDelegate::CreateUObject(this, &UTapaoShopUISubsystem::CloseShop));
	Controller->FlushPressedKeys();
	Viewport->AddViewportWidgetContent(Screen.ToSharedRef(), 860);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Screen);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(Mode);
	Controller->SetShowMouseCursor(true);
	FSlateApplication::Get().SetKeyboardFocus(Screen);
	// the treasury follows the replicated run state (a client's after the host's answer), no polling
	if (URunStateComponent* RunState = URunStateComponent::Get(Inventory))
	{
		RunState->OnRunStateChanged.AddUniqueDynamic(this, &UTapaoShopUISubsystem::HandleRunStateChanged);
		WatchedRunState = RunState;
	}
}

void UTapaoShopUISubsystem::HandleRunStateChanged()
{
	if (Screen.IsValid())
	{
		Screen->RefreshTreasury();
	}
}

void UTapaoShopUISubsystem::HandleShopResult(UIslanderInventoryComponent* Inventory, EShopResult Result, int32 Offer)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	if (Screen.IsValid() && Controller && Inventory && Controller->GetPawn() == Inventory->GetOwner())
	{
		Screen->ShowResult(Result, Offer);
	}
}

void UTapaoShopUISubsystem::CloseShop()
{
	if (URunStateComponent* RunState = WatchedRunState.Get())
	{
		RunState->OnRunStateChanged.RemoveDynamic(this, &UTapaoShopUISubsystem::HandleRunStateChanged);
	}
	WatchedRunState.Reset();
	if (!Screen.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (UGameViewportClient* Viewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr)
	{
		Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	}
	Screen.Reset();
	if (APlayerController* Controller = GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr)
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(false);
	}
}

void UTapaoShopUISubsystem::HandlePreLoadMap(const FString& MapName)
{
	CloseShop();
}
