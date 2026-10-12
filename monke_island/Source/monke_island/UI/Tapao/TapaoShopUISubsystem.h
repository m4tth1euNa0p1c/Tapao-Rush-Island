// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Shop/ShopTypes.h"
#include "TapaoShopUISubsystem.generated.h"

class AShopkeeperActor;
class FTapaoStyle;
class STapaoShopScreen;
class UIslanderInventoryComponent;
class URunStateComponent;
class UTapaoUISkin;

/**
 *  Opens the shop screen of the galleon merchant for the local player (owner request 2026-10-06): the server asks
 *  through UIslanderInventoryComponent::ClientOpenShop, the answers of the purchases come back the same way.
 *  The screen takes the mouse; Échap or F closes it (before the pause), a new map closes it too.
 *  While it is open, the replicated run state (URunStateComponent::OnRunStateChanged) refreshes its treasury: the
 *  screen never polls it (owner request 2026-10-11, T-320).
 */
UCLASS()
class MONKE_ISLAND_API UTapaoShopUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The shop screen is shown */
	bool IsShopOpen() const { return Screen.IsValid(); }

	/** Closes the shop screen and gives the game its input back */
	void CloseShop();

private:

	/** Kit of the TAPAO screens (same as UTapaoUISubsystem) */
	FSoftObjectPath SkinPath = FSoftObjectPath(TEXT("/Game/MonkeIsland/UI/Tapao/DA_TapaoUISkin.DA_TapaoUISkin"));

	UPROPERTY(Transient)
	TObjectPtr<UTapaoUISkin> Skin;

	TSharedPtr<FTapaoStyle> Style;
	TSharedPtr<STapaoShopScreen> Screen;
	TSharedPtr<class IInputProcessor> InputProcessor;

	void HandleShopRequested(UIslanderInventoryComponent* Inventory, AShopkeeperActor* Shop);
	void HandleShopResult(UIslanderInventoryComponent* Inventory, EShopResult Result, int32 Offer);
	void HandlePreLoadMap(const FString& MapName);

	/** The run state changed (treasury after a purchase, a day): the open screen reads its treasury again */
	UFUNCTION()
	void HandleRunStateChanged();

	FDelegateHandle RequestHandle;
	FDelegateHandle ResultHandle;
	FDelegateHandle PreLoadHandle;

	/** Run state the open screen listens to */
	TWeakObjectPtr<URunStateComponent> WatchedRunState;
};
