// monke_island

#include "UI/Tapao/STapaoHUD.h"
// FTimerHandle for TapaoUISubsystem.h and UWorld below: the file compiles on its own (UBT -SingleFile)
#include "Engine/TimerHandle.h"
#include "Engine/World.h"
#include "UI/Tapao/STapaoInventory.h"
#include "UI/Tapao/STapaoSpellSlot.h"
#include "UI/Tapao/TapaoDayEndText.h"
#include "UI/Tapao/TapaoStyle.h"
#include "UI/Tapao/TapaoUISkin.h"
#include "UI/Tapao/TapaoUISubsystem.h"
#include "UI/BossHealthBar.h"
#include "Capture/CaptureSubsystem.h"
#include "Emotes/TrumpetEmoteComponent.h"
#include "Items/Bandage/BandageItem.h"
#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonCrewComponent.h"
#include "Galleon/GalleonDeparture.h"
#include "Galleon/GalleonDepartureComponent.h"
#include "Galleon/GalleonRules.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Expedition/Core/DayTransitionSubsystem.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Execution/ExecutionScene.h"
#include "Run/RunContract.h"
#include "Run/RunRules.h"
#include "Run/RunStateComponent.h"
#include "Run/RunSubsystem.h"
#include "Stealth/StealthSubsystem.h"
#include "Voice/VoiceChatSubsystem.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/FirearmRules.h"
#include "Weapons/FirearmWeapon.h"
#include "Weapons/MusketRules.h"
#include "Weapons/MusketWeapon.h"
#include "Weapons/TorchItem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	/** Kit sprites are @2x of a 4K layout: the HUD draws them at this fraction of their source size (1080p) */
	constexpr float HudScale = 0.3f;

	/** The gauges: a fine line (px at 1080p) after a small icon */
	const FVector2D FineBar(200.0f, 7.0f);
	constexpr float FineIcon = 20.0f;

	/** Fill window of the gauge frames (ui_manifest.json fill_window_px 190, 44, 1060, 112 in 1280 x 200) */
	const FMargin FillWindow(190.0f / 1280.0f, 44.0f / 200.0f, 30.0f / 1280.0f, 44.0f / 200.0f);
	const FVector2D BarSource(1280.0f, 200.0f);
	const FVector2D CounterSource(680.0f, 200.0f);
	constexpr float SlotSize = 64.0f;

	/** Bottom left: the gauges' margin from the left edge, and their bottom above the stance line of AIslanderHUD */
	constexpr float GaugesLeft = 24.0f;
	constexpr float GaugesBottom = 62.0f;

	/** The spell of the player (the trumpet call, T-333): a sober square right of the gauges (their icon, its gap and
	 *  their line), its bottom on theirs; clear of the quick bar, centred at the bottom */
	constexpr float SpellSize = 46.0f;
	constexpr float SpellGap = 14.0f;

	/** The line of the capture (T-336) under the crosshair: its top padding from a centred slot (the line sits half that
	 *  below the middle, clear of the "F" badge of AIslanderHUD), its font and the width of its hold bar (px at 1080p) */
	constexpr float CaptureLineTop = 230.0f;
	constexpr int32 CaptureFontSize = 18;
	const FVector2D CaptureBar(240.0f, 6.0f);

	/** The line of the bandage in hand (T-340) and its discreet gauge, under the line of the capture */
	constexpr float BandageLineTop = 300.0f;
	constexpr int32 BandageFontSize = 16;
	const FVector2D BandageBar(160.0f, 4.0f);

	/** The day, the quota and the money: small, in the corners (owner 2026-10-08: "rends beaucoup plus petit l'UI : les
	 *  quotas, l'argent et le jour, tout en plus petit"); px at 1080p, and the corners' margin */
	constexpr int32 DayFontSize = 15;
	constexpr int32 QuotaFontSize = 11;
	constexpr int32 CargoFontSize = 14;
	constexpr int32 CornerOutline = 1;
	constexpr float CornerMargin = 16.0f;

	const UIslanderInventoryComponent* GetInventory(const APawn* Pawn)
	{
		return Pawn ? Pawn->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	}

	/** How the mobs perceive the local player (UStealthSubsystem, T-262): Hidden crouched or at night, Noisy running */
	EStealthExposure StealthExposureOf(const APawn* Pawn)
	{
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		const UStealthSubsystem* Stealth = Character ? UStealthSubsystem::Get(Character) : nullptr;
		if (!Stealth)
		{
			return EStealthExposure::Visible;
		}
		const UIslanderMovementComponent* Move = Cast<UIslanderMovementComponent>(Character->GetCharacterMovement());
		return Stealth->GetExposure(Character, Move && Move->IsSprinting() && Move->IsMovingOnGround());
	}

	template <typename T>
	const T* FindItem(const APawn* Pawn, bool bEquippedOnly)
	{
		const UIslanderInventoryComponent* Inventory = GetInventory(Pawn);
		if (!Inventory)
		{
			return nullptr;
		}
		if (bEquippedOnly)
		{
			return Cast<T>(Inventory->GetEquippedItem());
		}
		for (int32 Slot = 0; Slot < Inventory->GetSlotNum(); ++Slot)
		{
			if (const T* Item = Cast<T>(Inventory->GetItem(Slot)))
			{
				return Item;
			}
		}
		return nullptr;
	}
}

namespace
{
	/** The gestures of a reload of the pirate firearms (a reload in one gesture or in steps) */
	bool IsReloadGesture(EFirearmGesture Gesture)
	{
		switch (Gesture)
		{
		case EFirearmGesture::Reload:
		case EFirearmGesture::ReloadStart:
		case EFirearmGesture::ReloadInsert:
		case EFirearmGesture::ReloadEnd:
		case EFirearmGesture::ReloadEmpty:
		case EFirearmGesture::GroundReload:
		case EFirearmGesture::HalfCock:
		case EFirearmGesture::PrimePan:
		case EFirearmGesture::CloseFrizzen:
		case EFirearmGesture::Powder:
		case EFirearmGesture::BallAndPatch:
		case EFirearmGesture::DrawRamrod:
		case EFirearmGesture::TurnRamrod:
		case EFirearmGesture::RamrodStroke:
		case EFirearmGesture::ReturnRamrod:
			return true;
		default:
			return false;
		}
	}

	/** "loaded / in the pouch balls", and the reload time left while it reloads */
	FText AmmoLine(int32 Loaded, int32 Reserve, bool bReloading, float SecondsLeft)
	{
		const FText Balls = FText::Format(NSLOCTEXT("TapaoUI", "AmmoBalls", "{0} / {1} balles"), Loaded, Reserve);
		if (!bReloading)
		{
			return Balls;
		}
		FNumberFormattingOptions OneDecimal;
		OneDecimal.MinimumFractionalDigits = 1;
		OneDecimal.MaximumFractionalDigits = 1;
		return FText::Format(NSLOCTEXT("TapaoUI", "AmmoReload", "{0}   ·   rechargement {1} s"), Balls,
			FText::AsNumber(FMath::Max(SecondsLeft, 0.0f), &OneDecimal));
	}
}

FText STapaoHUD::GetAmmoText() const
{
	// owner request 2026-10-07: the balls and the reload time over the quick bar, for the firearm in hand
	const APawn* Pawn = GetPawn();
	if (const AMusketWeapon* Musket = FindItem<AMusketWeapon>(Pawn, true))
	{
		const float Seconds = Musket->Rules ? (1.0f - Musket->GetReloadProgress()) * Musket->Rules->ReloadSeconds : 0.0f;
		return AmmoLine(Musket->IsLoaded() ? 1 : 0, Musket->GetPouchBalls(), Musket->IsReloading(), Seconds);
	}
	if (const AFirearmWeapon* Firearm = FindItem<AFirearmWeapon>(Pawn, true))
	{
		const bool bReloading = IsReloadGesture(Firearm->GetServerGesture());
		return AmmoLine(Firearm->GetLoadedRounds(), Firearm->GetReserveRounds(), bReloading, bReloading ? Firearm->GetServerGestureSecondsLeft() : 0.0f);
	}
	return FText::GetEmpty();
}

FText STapaoHUD::GetDayText() const
{
	// owner request 2026-10-07: a counter of the days up to the tenth
	const UGameInstance* Instance = GameInstance.Get();
	const URunStateComponent* Run = Instance ? URunStateComponent::Get(Instance->GetWorld()) : nullptr;
	if (!Run)
	{
		return FText::GetEmpty();
	}
	if (CampaignDays < 0)
	{
		CampaignDays = 0;
		const URunSubsystem* Subsystem = Instance->GetSubsystem<URunSubsystem>();
		const URunRules* Rules = Subsystem && Subsystem->GetRules() ? Subsystem->GetRules() : GetDefault<URunRules>();
		const FRunContractRules Contract = Rules->MakeContractRules();
		for (int32 Zone = 0; Zone < Contract.Zones.Num(); ++Zone)
		{
			CampaignDays += FRunContractCalculator::DaysOfZone(Contract, Zone);
		}
	}
	const int32 Day = Run->GetDaysPlayed() + 1;
	return CampaignDays > 0 ? FText::Format(NSLOCTEXT("TapaoUI", "CampaignDay", "Jour {0} / {1}"), Day, CampaignDays)
		: FText::Format(NSLOCTEXT("TapaoUI", "CampaignDayOnly", "Jour {0}"), Day);
}

bool STapaoHUD::IsSceneUnderWay() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const UDayTransitionSubsystem* Transition = UDayTransitionSubsystem::Get(Instance ? Instance->GetWorld() : nullptr);
	return Transition && Transition->IsScenePlaying();
}

FText STapaoHUD::GetCenterText() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const UWorld* World = Instance ? Instance->GetWorld() : nullptr;
	const AExpeditionGameState* Expedition = World ? World->GetGameState<AExpeditionGameState>() : nullptr;
	// the execution of the crew: its titles (quota missed, the campaign is over)
	if (Expedition && Expedition->IsExecution())
	{
		const AExecutionScene* Scene = AExecutionScene::Get(World);
		float Alpha = 0.0f;
		return Scene ? Scene->GetTitle(Alpha) : FText::GetEmpty();
	}
	// the departure decided at the wheel (T-286): the countdown of its notice (whoever is ashore at 0 is left behind)
	const UGalleonDepartureComponent* Departure = Expedition ? Expedition->GetDeparture() : nullptr;
	if (Departure && Departure->GetStage() == EGalleonDepartureStage::Notice && !Expedition->IsExpeditionOver())
	{
		const double Left = Departure->GetState().StageEndTime - Expedition->GetServerWorldTimeSeconds();
		return FText::Format(NSLOCTEXT("TapaoUI", "DepartureIn", "Départ dans {0}"), FMath::Max(0, FMath::CeilToInt32(Left)));
	}
	// the final countdown of the day (owner request 2026-10-07), while the galleon sails off; named after the next island
	// since the notice at the wheel says "Départ dans" before it (2026-10-11: two "Départ dans" in a row)
	if (Expedition && Expedition->IsDaySurvived() && !Expedition->IsTeamWiped())
	{
		const double Left = Expedition->GetNextTravelTime() - Expedition->GetServerWorldTimeSeconds();
		if (Left > 0.0)
		{
			return FText::Format(NSLOCTEXT("TapaoUI", "NextIslandIn", "Île suivante dans {0}"), FMath::CeilToInt32(Left));
		}
		return FText::GetEmpty();
	}
	// "Jour N" as the boat comes to the new island
	const UDayTransitionSubsystem* Transition = UDayTransitionSubsystem::Get(World);
	if (Transition && Transition->GetTitleAlpha() > 0.0f)
	{
		const URunStateComponent* Run = URunStateComponent::Get(World);
		return Run ? FText::Format(NSLOCTEXT("TapaoUI", "ArrivalDay", "Jour {0}"), Run->GetDaysPlayed() + 1) : FText::GetEmpty();
	}
	return FText::GetEmpty();
}

float STapaoHUD::GetCenterAlpha() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const UWorld* World = Instance ? Instance->GetWorld() : nullptr;
	const AExpeditionGameState* Expedition = World ? World->GetGameState<AExpeditionGameState>() : nullptr;
	if (Expedition && Expedition->IsExecution())
	{
		const AExecutionScene* Scene = AExecutionScene::Get(World);
		float Alpha = 0.0f;
		if (Scene)
		{
			Scene->GetTitle(Alpha);
		}
		return Alpha;
	}
	const UGalleonDepartureComponent* Departure = Expedition ? Expedition->GetDeparture() : nullptr;
	if (Expedition && (Expedition->IsDaySurvived() || (Departure && Departure->GetStage() == EGalleonDepartureStage::Notice)))
	{
		return 1.0f;
	}
	const UDayTransitionSubsystem* Transition = UDayTransitionSubsystem::Get(World);
	return Transition ? Transition->GetTitleAlpha() : 0.0f;
}

FText STapaoHUD::GetDepartureText() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const UWorld* World = Instance ? Instance->GetWorld() : nullptr;
	const AExpeditionGameState* Expedition = World ? World->GetGameState<AExpeditionGameState>() : nullptr;
	if (!Expedition || Expedition->IsExpeditionOver())
	{
		return FText::GetEmpty();
	}
	const double Now = Expedition->GetServerWorldTimeSeconds();
	// T-292: the clock waits for the players still loading the island
	if (Expedition->IsAwaitingCrew())
	{
		return FText::Format(NSLOCTEXT("TapaoUI", "CrewWait", "En attente de l'équipage : {0} / {1} ({2} s)"), Expedition->GetCrewLoaded(),
			Expedition->GetCrewExpected(), FMath::Max(0, FMath::CeilToInt32(Expedition->GetCrewWaitEndTime() - Now)));
	}
	const UGalleonDepartureComponent* Departure = Expedition->GetDeparture();
	if (!Departure)
	{
		return FText::GetEmpty();
	}
	const FGalleonDepartureState& State = Departure->GetState();
	const APlayerController* Controller = Instance->GetFirstLocalPlayerController();
	const APlayerState* Me = Controller ? Controller->PlayerState.Get() : nullptr;
	const UGalleonCrewComponent* Crew = Controller ? Controller->FindComponentByClass<UGalleonCrewComponent>() : nullptr;
	const bool bAtHelm = Crew && Crew->IsAtUsableHelm();
	const FText HelmKey = Crew ? Crew->GetHelmKeyName() : FText::GetEmpty();
	const FText Cancel = bAtHelm ? FText::Format(NSLOCTEXT("TapaoUI", "DepartureCancelKey", "   ·   {0} : annuler"), HelmKey) : FText::GetEmpty();

	if (State.Stage == EGalleonDepartureStage::Vote)
	{
		// the vote of the crew: the count, the time left, how to vote (cahier §6.5: the majority of the living)
		FText Mine = FText::GetEmpty();
		if (Crew && Crew->IsVoteOffered())
		{
			Mine = FText::Format(NSLOCTEXT("TapaoUI", "DepartureVoteKey", "   ·   {0} : voter le départ"), Crew->GetVoteKeyName());
		}
		else if (Departure->HasVoted(Me))
		{
			Mine = NSLOCTEXT("TapaoUI", "DepartureVoted", "   ·   vous avez voté");
		}
		return FText::Format(NSLOCTEXT("TapaoUI", "DepartureVote", "Vote de départ : {0} / {1} ({2} s){3}{4}"), State.Voters.Num(), State.Needed,
			FMath::Max(0, FMath::CeilToInt32(State.StageEndTime - Now)), Mine, Cancel);
	}
	if (State.Stage == EGalleonDepartureStage::Notice)
	{
		// the countdown is the centre text; this line says who stays behind
		const FText Who = State.bAllAboard ? NSLOCTEXT("TapaoUI", "DepartureAllAboard", "Tout l'équipage est à bord")
			: NSLOCTEXT("TapaoUI", "DepartureLeftBehind", "Les joueurs restés à terre seront abandonnés");
		return FText::Format(NSLOCTEXT("TapaoUI", "DepartureNotice", "{0}{1}"), Who, Cancel);
	}

	// the wheel held by the local player: the anchor rises
	const FGalleonHelmHold& Hold = Departure->GetHold();
	if (Me && Hold.Holder.Get() == Me)
	{
		FNumberFormattingOptions OneDecimal;
		OneDecimal.MinimumFractionalDigits = 1;
		OneDecimal.MaximumFractionalDigits = 1;
		return FText::Format(NSLOCTEXT("TapaoUI", "DepartureHolding", "Lever l'ancre…  {0} s"),
			FText::AsNumber(FMath::Max(0.0, Hold.EndTime - Now), &OneDecimal));
	}

	// why the last vote or notice failed, a moment for everyone
	const UGalleonRules* Rules = Departure->GetRules();
	const float MessageSeconds = Rules ? Rules->DepartureMessageSeconds : 0.0f;
	if (State.LastEndTime >= 0.0 && Now - State.LastEndTime < MessageSeconds)
	{
		switch (State.LastEnd)
		{
		case EGalleonDepartureEnd::Cancelled:		return NSLOCTEXT("TapaoUI", "DepartureCancelled", "Départ annulé à la barre");
		case EGalleonDepartureEnd::Lapsed:		return NSLOCTEXT("TapaoUI", "DepartureLapsed", "Vote de départ expiré : pas de majorité");
		case EGalleonDepartureEnd::NobodyAboard:	return NSLOCTEXT("TapaoUI", "DepartureNobodyAboard", "Personne à bord : départ annulé");
		default:									break;
		}
	}

	// at the wheel: how to raise the anchor, and when a new vote may open
	if (bAtHelm)
	{
		if (State.NextVoteTime > Now)
		{
			return FText::Format(NSLOCTEXT("TapaoUI", "DepartureHelmCooldown", "Maintenir {0} : lever l'ancre   ·   nouveau vote dans {1} s"), HelmKey,
				FMath::CeilToInt32(State.NextVoteTime - Now));
		}
		return FText::Format(NSLOCTEXT("TapaoUI", "DepartureHelm", "Maintenir {0} : lever l'ancre"), HelmKey);
	}
	return FText::GetEmpty();
}

FText STapaoHUD::GetQuotaText() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const URunStateComponent* Run = Instance ? URunStateComponent::Get(Instance->GetWorld()) : nullptr;
	if (!Run || Run->GetQuota() <= 0)
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("TapaoUI", "StageQuota", "Quota {0} / {1} c"), FText::AsNumber(Run->GetHoldValue()), FText::AsNumber(Run->GetQuota()));
}

FText STapaoHUD::GetContractText() const
{
	// T-283: one count of the days, the campaign's: the day the contract ends on, and the greed day (ADR-007 §16); the
	// flood timer top right keeps the clock alone
	const UGameInstance* Instance = GameInstance.Get();
	const URunStateComponent* Run = Instance ? URunStateComponent::Get(Instance->GetWorld()) : nullptr;
	if (!Run || Run->GetQuota() <= 0)
	{
		return FText::GetEmpty();
	}
	return TapaoDayEndText::ContractLine(Run->GetDaysPlayed(), Run->GetDayIndex(), Run->GetDaysPerPeriod(), Run->IsGreedDay());
}

APawn* STapaoHUD::GetPawn() const
{
	const UGameInstance* Instance = GameInstance.Get();
	const APlayerController* Controller = Instance ? Instance->GetFirstLocalPlayerController() : nullptr;
	return Controller ? Controller->GetPawn() : nullptr;
}

const UTrumpetEmoteComponent* STapaoHUD::GetTrumpetCall() const
{
	const UTrumpetEmoteComponent* Call = TrumpetCall.Get();
	if (!Call || !Call->HasCall() || Call->GetOwner() != GetPawn())
	{
		return nullptr;
	}
	const UIslanderHealthComponent* Health = PawnHealth.Get();
	return Health && Health->IsDead() ? nullptr : Call;
}

namespace
{
	/** Proximity voice chat of the HUD's world (T-263), or nullptr */
	const UVoiceChatSubsystem* VoiceChatOf(const TWeakObjectPtr<UGameInstance>& GameInstance)
	{
		const UGameInstance* Instance = GameInstance.Get();
		return Instance ? UVoiceChatSubsystem::Get(Instance->GetWorld()) : nullptr;
	}

	/** Line of the local microphone (security.md §6: shown whenever the microphone is open); empty when closed */
	FText VoiceIndicatorText(EVoiceIndicator Indicator)
	{
		switch (Indicator)
		{
		case EVoiceIndicator::Open: return NSLOCTEXT("TapaoUI", "VoiceOpen", "Micro ouvert");
		case EVoiceIndicator::Talking: return NSLOCTEXT("TapaoUI", "VoiceTalking", "Vous parlez");
		case EVoiceIndicator::Test: return NSLOCTEXT("TapaoUI", "VoiceTest", "Test du micro : vous seul vous entendez");
		case EVoiceIndicator::Dead: return NSLOCTEXT("TapaoUI", "VoiceDead", "Mort : votre voix ne porte plus");
		default: return FText::GetEmpty();
		}
	}
}

void STapaoHUD::Construct(const FArguments& InArgs)
{
	Style = InArgs._Style;
	GameInstance = InArgs._GameInstance;
	InventoryModel = MakeShared<FTapaoInventoryModel>();
	InventoryBrushes = MakeShared<FTapaoInventoryBrushes>(*Style);
	SetVisibility(EVisibility::HitTestInvisible);
	RegisterActiveTimer(0.5f, FWidgetActiveTimerDelegate::CreateSP(this, &STapaoHUD::Refresh));

	auto Health = [this]() -> const UIslanderHealthComponent*
	{
		const APawn* Pawn = GetPawn();
		return Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	};
	auto Movement = [this]() -> const UIslanderMovementComponent*
	{
		const ACharacter* Character = Cast<ACharacter>(GetPawn());
		return Character ? Cast<UIslanderMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	};
	// the capture (T-336): the prompt of the cage or the handcuffs in hand, the hold, the messages of the crew; the domain
	// decides it (UCaptureSubsystem), the HUD only reads it
	auto CaptureLine = [this](FCaptureHudLine& Out)
	{
		const UGameInstance* Instance = GameInstance.Get();
		const UCaptureSubsystem* Capture = UCaptureSubsystem::Get(Instance ? Instance->GetWorld() : nullptr);
		return Capture && Capture->GetHudLine(GetPawn(), Out);
	};
	// the bandage in hand (T-340): its prompt, the gauge of the application, why not; the item decides, the HUD reads
	auto BandageLine = [this](FCaptureHudLine& Out)
	{
		return ABandageItem::GetHudLine(GetPawn(), Out.Text, Out.Progress, Out.bWarning);
	};

	ChildSlot
	[
		SNew(SOverlay)
		// the HUD of the player, none during the arrival and departure scenes (owner 2026-10-07)
		+ SOverlay::Slot()
		[
			SNew(SOverlay)
			.Visibility_Lambda([this]() { return IsSceneUnderWay() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			// bottom left: torch fuel (in hand only), health, stamina, above the stance line of AIslanderHUD
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(GaugesLeft, 0.0f, 0.0f, GaugesBottom)
			[
				SNew(SVerticalBox)
				// voice chat (T-263): the players heard near me, nearest first ("Nom parle")
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]()
					{
						const UVoiceChatSubsystem* Voice = VoiceChatOf(GameInstance);
						return Voice && !Voice->GetNearbyTalkersText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
					})
					.Text_Lambda([this]()
					{
						const UVoiceChatSubsystem* Voice = VoiceChatOf(GameInstance);
						return Voice ? Voice->GetNearbyTalkersText() : FText::GetEmpty();
					})
					.Font(Style->Font(ETapaoFont::UI, 12, 1))
					.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.8f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]
				// voice chat (T-263): my microphone, whenever it is open ("Vous parlez" once heard, by the others and the mobs)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]()
					{
						const UVoiceChatSubsystem* Voice = VoiceChatOf(GameInstance);
						return Voice && Voice->GetLocalIndicator() != EVoiceIndicator::None ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
					})
					.Text_Lambda([this]()
					{
						const UVoiceChatSubsystem* Voice = VoiceChatOf(GameInstance);
						return VoiceIndicatorText(Voice ? Voice->GetLocalIndicator() : EVoiceIndicator::None);
					})
					.Font(Style->Font(ETapaoFont::UI, 13, 1))
					.ColorAndOpacity_Lambda([this]()
					{
						const UVoiceChatSubsystem* Voice = VoiceChatOf(GameInstance);
						const UTapaoUISkin* Skin = Style->GetSkin();
						const bool bTalking = Voice && Voice->GetLocalIndicator() == EVoiceIndicator::Talking;
						return FSlateColor(Skin ? (bTalking ? Skin->Gold : Skin->Cream) : FLinearColor::White);
					})
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]
				// stealth (T-262): "Caché" crouched or at night (the mobs see the player only at contact), "Bruyant" while
				// running (they hear each step)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return StealthExposureOf(GetPawn()) == EStealthExposure::Visible ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					.Text_Lambda([this]()
					{
						return StealthExposureOf(GetPawn()) == EStealthExposure::Noisy ? NSLOCTEXT("TapaoUI", "StealthNoisy", "Bruyant")
							: NSLOCTEXT("TapaoUI", "StealthHidden", "Caché");
					})
					.Font(Style->Font(ETapaoFont::UI, 13, 1))
					.ColorAndOpacity_Lambda([this]()
					{
						const UTapaoUISkin* Skin = Style->GetSkin();
						const bool bNoisy = StealthExposureOf(GetPawn()) == EStealthExposure::Noisy;
						return FSlateColor(Skin ? (bNoisy ? Skin->Gold : Skin->Cream) : FLinearColor::White);
					})
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeBar(TEXT("hud_flame"), TEXT("fill_fuel"), FuelStyle,
						[this]()
						{
							const ATorchItem* Torch = FindItem<ATorchItem>(GetPawn(), true);
							return Torch && Torch->Rules && Torch->Rules->BurnTime > 0.0f ? Torch->GetFuel() / Torch->Rules->BurnTime : 0.0f;
						},
						[this]()
						{
							const ATorchItem* Torch = FindItem<ATorchItem>(GetPawn(), true);
							return Torch ? FText::AsNumber(FMath::CeilToInt32(Torch->GetFuel())) : FText::GetEmpty();
						},
						TAttribute<EVisibility>::CreateLambda([this]() { return FindItem<ATorchItem>(GetPawn(), true) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; }))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeBar(TEXT("hud_heart"), TEXT("fill_health"), HealthStyle,
						[Health]() { const UIslanderHealthComponent* H = Health(); return H ? H->GetHealthFraction() : 0.0f; },
						[Health]() { const UIslanderHealthComponent* H = Health(); return H ? FText::AsNumber(FMath::CeilToInt32(H->GetHealth())) : FText::GetEmpty(); },
						EVisibility::HitTestInvisible)
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeBar(TEXT("hud_bolt"), TEXT("fill_stamina"), StaminaStyle,
						[Movement]() { const UIslanderMovementComponent* M = Movement(); return M ? M->GetStaminaFraction() : 0.0f; },
						[Movement]() { const UIslanderMovementComponent* M = Movement(); return M ? FText::AsNumber(FMath::CeilToInt32(M->GetStamina())) : FText::GetEmpty(); },
						EVisibility::HitTestInvisible)
				]
			]
			// bottom left, right of the gauges: the spell of the player as in League of Legends (owner 2026-10-11, T-333): the
			// trumpet call on A, its recharge and its time left; none without it, nor once dead
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom)
				.Padding(GaugesLeft + FineIcon + 6.0f + static_cast<float>(FineBar.X) + SpellGap, 0.0f, 0.0f, GaugesBottom)
			[
				SNew(STapaoSpellSlot)
				.Visibility_Lambda([this]() { return GetTrumpetCall() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.Style(Style)
				.Size(SpellSize)
				.Icon_Lambda([this]() -> const FSlateBrush*
				{
					const UTrumpetEmoteComponent* Call = GetTrumpetCall();
					UTexture2D* Picture = Call ? Call->GetCallIcon() : nullptr;
					return Picture ? Style->TextureBrush(Picture) : nullptr;
				})
				.KeyText_Lambda([this]() { const UTrumpetEmoteComponent* Call = GetTrumpetCall(); return Call ? Call->GetCallKeyText() : FText::GetEmpty(); })
				.CooldownFraction_Lambda([this]() { const UTrumpetEmoteComponent* Call = GetTrumpetCall(); return Call ? Call->GetCallCooldownFraction() : 0.0f; })
				.CooldownSeconds_Lambda([this]() { const UTrumpetEmoteComponent* Call = GetTrumpetCall(); return Call ? Call->GetCallCooldownLeft() : 0.0f; })
				.ActiveFraction_Lambda([this]() { const UTrumpetEmoteComponent* Call = GetTrumpetCall(); return Call ? Call->GetCallFractionLeft() : -1.0f; })
				.Usable_Lambda([this]() { const UTrumpetEmoteComponent* Call = GetTrumpetCall(); return Call && Call->CanCallNow(); })
			]
			// bottom right: the money alone (value of the cargo aboard the galleon), no plate, no icon; the flood clock is top right
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, CornerMargin, CornerMargin)
			[
				SNew(STextBlock)
				.Visibility_Lambda([this]() { return bHasGalleon ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				.Text_Lambda([this]() { return FText::Format(NSLOCTEXT("TapaoUI", "Cargo", "{0} c"), CargoValue); })
				.Font(Style->Font(ETapaoFont::UI, CargoFontSize, CornerOutline))
				.ColorAndOpacity(Style->GetSkin() ? Style->GetSkin()->Cream : FLinearColor::White)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]
			// top left: the day of the campaign and the quota of its stage (the flood clock is top right)
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(CornerMargin, CornerMargin, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([this]() { return GetDayText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return GetDayText(); })
					.Font(Style->Font(ETapaoFont::UI, DayFontSize, CornerOutline))
					.ColorAndOpacity(Style->GetSkin() ? Style->GetSkin()->Cream : FLinearColor::White)
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return GetQuotaText(); })
					.Font(Style->Font(ETapaoFont::UI, QuotaFontSize, CornerOutline))
					.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]
				// the deadline of the contract and the greed day, as small as the quota (T-283)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Visibility_Lambda([this]() { return GetContractText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
					.Text_Lambda([this]() { return GetContractText(); })
					.Font(Style->Font(ETapaoFont::UI, QuotaFontSize, CornerOutline))
					.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]
			]
			// bottom centre, over the quick bar: the balls and the reload time of the firearm in hand
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, 18.0f + SlotSize + 12.0f)
			[
				SNew(STextBlock)
				.Visibility_Lambda([this]() { return GetAmmoText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
				.Text_Lambda([this]() { return GetAmmoText(); })
				.Font(Style->Font(ETapaoFont::UI, 20, 2))
				.ColorAndOpacity(Style->GetSkin() ? Style->GetSkin()->Cream : FLinearColor::White)
				.ShadowOffset(FVector2D(1.0f, 1.0f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
			]
			// bottom centre: quick bar
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.0f, 0.0f, 0.0f, 18.0f)
			[
				SAssignNew(BarBox, SHorizontalBox)
				.Visibility_Lambda([this]()
				{
					if (!InventoryModel->GetInventory())
					{
						return EVisibility::Collapsed;
					}
					// the inventory key brings this bar to life in the inventory screen, drawn at the same place
					const UGameInstance* Instance = GameInstance.Get();
					const UTapaoUISubsystem* Ui = Instance ? Instance->GetSubsystem<UTapaoUISubsystem>() : nullptr;
					return Ui && Ui->IsInventoryOpen() ? EVisibility::Hidden : EVisibility::HitTestInvisible;
				})
			]
			// bottom centre, over the ammo line: the bar of the boss that fights the player (T-261, UI/BossHealthBar)
			+ SOverlay::Slot()
			[
				SNew(SBossHealthBar).Style(Style).GameInstance(GameInstance)
			]
			// under the crosshair: the capture (T-336): "F : poser la cage", "Maintenir F : menotter le soldat" and its bar,
			// why not, the sales and captures of the crew
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.0f, CaptureLineTop, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([CaptureLine]() { FCaptureHudLine Line; return CaptureLine(Line) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([CaptureLine]() { FCaptureHudLine Line; return CaptureLine(Line) ? Line.Text : FText::GetEmpty(); })
					.Font(Style->Font(ETapaoFont::UI, CaptureFontSize, 1))
					.ColorAndOpacity_Lambda([this, CaptureLine]()
					{
						FCaptureHudLine Line;
						CaptureLine(Line);
						const UTapaoUISkin* Skin = Style->GetSkin();
						return FSlateColor(Line.bWarning ? FLinearColor(1.0f, 0.58f, 0.45f) : (Skin ? Skin->Cream : FLinearColor::White));
					})
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 5.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(CaptureBar.X)
					.HeightOverride(CaptureBar.Y)
					.Visibility_Lambda([CaptureLine]() { FCaptureHudLine Line; return CaptureLine(Line) && Line.Progress >= 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SProgressBar)
						.Percent_Lambda([CaptureLine]() { FCaptureHudLine Line; CaptureLine(Line); return TOptional<float>(FMath::Clamp(Line.Progress, 0.0f, 1.0f)); })
						.FillColorAndOpacity(Style->GetSkin() ? Style->GetSkin()->Gold : FLinearColor(1.0f, 0.82f, 0.25f))
					]
				]
			]
			// under the line of the capture: the bandage in hand (T-340), "F : appliquer un bandage (3)", its gauge
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0.0f, BandageLineTop, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				.Visibility_Lambda([BandageLine]() { FCaptureHudLine Line; return BandageLine(Line) ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text_Lambda([BandageLine]() { FCaptureHudLine Line; return BandageLine(Line) ? Line.Text : FText::GetEmpty(); })
					.Font(Style->Font(ETapaoFont::UI, BandageFontSize, 1))
					.ColorAndOpacity_Lambda([this, BandageLine]()
					{
						FCaptureHudLine Line;
						BandageLine(Line);
						const UTapaoUISkin* Skin = Style->GetSkin();
						return FSlateColor(Line.bWarning ? FLinearColor(1.0f, 0.58f, 0.45f) : (Skin ? Skin->Cream : FLinearColor::White));
					})
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(BandageBar.X)
					.HeightOverride(BandageBar.Y)
					.Visibility_Lambda([BandageLine]() { FCaptureHudLine Line; return BandageLine(Line) && Line.Progress >= 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
					[
						SNew(SProgressBar)
						.Percent_Lambda([BandageLine]() { FCaptureHudLine Line; BandageLine(Line); return TOptional<float>(FMath::Clamp(Line.Progress, 0.0f, 1.0f)); })
						.FillColorAndOpacity(Style->GetSkin() ? Style->GetSkin()->Cream : FLinearColor(0.95f, 0.92f, 0.82f))
					]
				]
			]
		]
		// top centre, under the flood clock: the final countdown of the day, then "Jour N" at the arrival (T-214); the
		// middle stays free for the galleon sailing off and coming in
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.0f, 130.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return GetCenterText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			.Text_Lambda([this]() { return GetCenterText(); })
			.Font(Style->Font(ETapaoFont::UI, 64, 2))
			.ColorAndOpacity_Lambda([this]()
			{
				const FLinearColor Cream = Style->GetSkin() ? Style->GetSkin()->Cream : FLinearColor::White;
				return FSlateColor(FLinearColor(Cream.R, Cream.G, Cream.B, GetCenterAlpha()));
			})
			.ShadowOffset(FVector2D(2.0f, 2.0f))
			.ShadowColorAndOpacity_Lambda([this]() { return FLinearColor(0.0f, 0.0f, 0.0f, 0.85f * GetCenterAlpha()); })
		]
		// under the centre (T-286, T-292): the wait for the crew, the vote of the departure, the notice, the wheel
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(0.0f, 218.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Visibility_Lambda([this]() { return GetDepartureText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			.Text_Lambda([this]() { return GetDepartureText(); })
			.Font(Style->Font(ETapaoFont::UI, 20, 1))
			.ColorAndOpacity(Style->GetSkin() ? Style->GetSkin()->Cream : FLinearColor::White)
			.ShadowOffset(FVector2D(1.0f, 1.0f))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
		]
	];
	Refresh(0.0, 0.0f);
}

void STapaoHUD::RebuildBar()
{
	BarBox->ClearChildren();
	BuiltBarNum = InventoryModel->BarNum;
	for (int32 Slot = 0; Slot < BuiltBarNum; ++Slot)
	{
		BarBox->AddSlot().AutoWidth().Padding(3.0f, 0.0f)
			[
				SNew(STapaoInventoryCell)
				.Style(Style)
				.Brushes(InventoryBrushes)
				.Model(InventoryModel)
				.Index(Slot)
				.Size(SlotSize)
				.bInteractive(false)
				.KeyNumber(Slot + 1)
			];
	}
}

TSharedRef<SWidget> STapaoHUD::MakeBar(FName Frame, FName Fill, FProgressBarStyle& BarStyle, TFunction<float()> Fraction, TFunction<FText()> Label, TAttribute<EVisibility> Visibility)
{
	// a fine line and its small icon (owner request 2026-10-04: much finer, no frame, no number); Frame names the icon
	UTapaoUISkin* Skin = Style->GetSkin();
	BarStyle = FProgressBarStyle()
		.SetBackgroundImage(*Style->TextureBrush(Skin ? Skin->Fills.FindRef(TEXT("fill_background")) : nullptr))
		.SetFillImage(*Style->TextureBrush(Skin ? Skin->Fills.FindRef(Fill) : nullptr))
		.SetMarqueeImage(FSlateNoResource());
	return SNew(SHorizontalBox)
		.Visibility(Visibility)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 2.0f, 6.0f, 2.0f)
		[
			SNew(SBox).WidthOverride(FineIcon).HeightOverride(FineIcon)
			[
				SNew(SImage).Image(Style->Brush(Frame))
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(FineBar.X).HeightOverride(FineBar.Y)
			[
				SNew(SProgressBar).Style(&BarStyle).Percent_Lambda([Fraction]() { return TOptional<float>(FMath::Clamp(Fraction(), 0.0f, 1.0f)); })
			]
		];
}

TSharedRef<SWidget> STapaoHUD::MakeCounter(FName Icon, TFunction<FText()> Value, TAttribute<EVisibility> Visibility)
{
	const UTapaoUISkin* Skin = Style->GetSkin();
	const FVector2D Size = CounterSource * HudScale;
	return SNew(SBox).WidthOverride(Size.X).HeightOverride(Size.Y).Visibility(Visibility)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage).Image(Style->Brush(TEXT("counter_plate")))
			]
			+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(46.0f).HeightOverride(46.0f)
				[
					SNew(SImage).Image(Style->Brush(Icon))
				]
			]
			+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 26.0f, 0.0f)
			[
				SNew(STextBlock).Text_Lambda([Value]() { return Value(); }).Font(Style->Font(ETapaoFont::UI, 24, 2)).ColorAndOpacity(Skin ? Skin->Cream : FLinearColor::White)
			]
		];
}

EActiveTimerReturnType STapaoHUD::Refresh(double CurrentTime, float DeltaTime)
{
	const APawn* Pawn = GetPawn();
	// the bar follows the inventory of the pawn in hand (respawn, possession); its cells follow its change event
	InventoryModel->Bind(const_cast<UIslanderInventoryComponent*>(GetInventory(Pawn)));
	if (InventoryModel->BarNum != BuiltBarNum)
	{
		RebuildBar();
	}
	const AGalleonActor* Galleon = Pawn ? AGalleonActor::Get(Pawn) : nullptr;
	bHasGalleon = Galleon != nullptr;
	CargoValue = Galleon ? Galleon->GetCargoValue() : 0;
	// the spell slot follows the trumpet of the pawn in hand (respawn, possession)
	TrumpetCall = Pawn ? Pawn->FindComponentByClass<UTrumpetEmoteComponent>() : nullptr;
	PawnHealth = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	return EActiveTimerReturnType::Continue;
}
