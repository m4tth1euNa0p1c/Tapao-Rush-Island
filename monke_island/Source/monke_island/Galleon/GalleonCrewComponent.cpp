// monke_island

#include "Galleon/GalleonCrewComponent.h"
#include "Galleon/GalleonActor.h"
#include "Galleon/GalleonDepartureComponent.h"
#include "Galleon/GalleonRules.h"
#include "Interaction/IslanderGrabComponent.h"
#include "monke_island.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

namespace
{
	/** The owner checks the wheel and the vote this often (s): presentation and key mappings, the server decides */
	constexpr float LocalUpdateSeconds = 0.1f;

	/** The galleon is looked for at most this often while a map has none (s): no actor search every frame (ARC-05) */
	constexpr double GalleonSearchSeconds = 1.0;

	/** The test hold of monke.Galleon.RaiseAnchor lasts this much longer than HelmHoldSeconds (s): the server completes it */
	constexpr float DebugHoldMarginSeconds = 0.5f;

	/** Mapping changes apply to the keys already down: a held key (sprint, aim) is never cut by a vote or the wheel */
	FModifyContextOptions KeepHeldKeys()
	{
		FModifyContextOptions Options;
		Options.bIgnoreAllPressedKeysUntilRelease = false;
		return Options;
	}
}

UGalleonCrewComponent::UGalleonCrewComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// the requests are server RPCs of this component: it replicates (to its owner, the controller's connection)
	SetIsReplicatedByDefault(true);
}

UGalleonCrewComponent* UGalleonCrewComponent::Ensure(APlayerController* Controller)
{
	if (!Controller || !Controller->HasAuthority())
	{
		return nullptr;
	}
	if (UGalleonCrewComponent* Existing = Controller->FindComponentByClass<UGalleonCrewComponent>())
	{
		return Existing;
	}
	UGalleonCrewComponent* Crew = NewObject<UGalleonCrewComponent>(Controller, MakeUniqueObjectName(Controller, StaticClass(), TEXT("GalleonCrew")));
	Crew->SetIsReplicated(true);
	Controller->AddInstanceComponent(Crew);
	Crew->RegisterComponent();
	UE_LOG(Logmonke_island, Verbose, TEXT("Departure: crew keys given to %s"), *Controller->GetName());
	return Crew;
}

UGalleonCrewComponent* UGalleonCrewComponent::GetLocal(const UObject* WorldContextObject)
{
	// the local player's: on a listen server the first controller of the world may be a client's
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	const APlayerController* Controller = World && GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	return Controller ? Controller->FindComponentByClass<UGalleonCrewComponent>() : nullptr;
}

APlayerController* UGalleonCrewComponent::GetController() const
{
	return Cast<APlayerController>(GetOwner());
}

void UGalleonCrewComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// the owner only: on a client every controller it holds is its own; on a listen server, the host's
	const APlayerController* Controller = GetController();
	if (Controller && (Controller->IsLocalController() || !Controller->HasAuthority()))
	{
		GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &UGalleonCrewComponent::LocalUpdate, LocalUpdateSeconds, true);
	}
}

void UGalleonCrewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
		World->GetTimerManager().ClearTimer(DebugReleaseTimer);
	}
	APlayerController* Controller = GetController();
	const ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
	{
		if (HelmContext && bHelmContextActive)
		{
			Input->RemoveMappingContext(HelmContext, KeepHeldKeys());
		}
		if (VoteContext)
		{
			Input->RemoveMappingContext(VoteContext, KeepHeldKeys());
		}
	}
	if (CrewInput)
	{
		if (Controller)
		{
			Controller->PopInputComponent(CrewInput);
		}
		CrewInput->DestroyComponent();
		CrewInput = nullptr;
	}
	bHelmContextActive = false;
	bInputBuilt = false;
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------- owner

AGalleonActor* UGalleonCrewComponent::FindGalleon()
{
	if (!Galleon.IsValid())
	{
		const UWorld* World = GetWorld();
		const double Now = World ? World->GetRealTimeSeconds() : 0.0;
		if (World && Now >= NextGalleonSearch)
		{
			NextGalleonSearch = Now + GalleonSearchSeconds;
			Galleon = AGalleonActor::Get(this);
		}
	}
	return Galleon.Get();
}

void UGalleonCrewComponent::LocalUpdate()
{
	APlayerController* Controller = GetController();
	if (!Controller || !Controller->IsLocalController())
	{
		return;
	}
	const AGalleonActor* Ship = FindGalleon();
	const UGalleonRules* Rules = Ship ? Ship->Rules.Get() : nullptr;
	if (!Rules || !EnsureInput(Controller, *Rules))
	{
		return;
	}
	const UGalleonDepartureComponent* Departure = UGalleonDepartureComponent::Get(this);
	const APawn* Pawn = Controller->GetPawn();
	const bool bAlive = UGalleonDepartureComponent::IsLivingPawn(Pawn);
	const UIslanderGrabComponent* Grab = Pawn ? Pawn->FindComponentByClass<UIslanderGrabComponent>() : nullptr;

	// the wheel's key: alive at the wheel, hands free (the interaction key lets go of a held object first)
	const bool bAtHelm = Departure && Departure->IsHelmUsable() && bAlive && !(Grab && Grab->IsHolding()) && Ship->IsAtHelm(Pawn);
	SetHelmContextActive(Controller, bAtHelm, Rules->CrewInputPriority);
	if (!bAtHelm && bHelmKeyHeld)
	{
		// walked away, died or picked something up with the key down: the hold ends
		bHelmKeyHeld = false;
		RequestHelmKey(false);
	}

	// the vote key: every living player who has not voted, aboard or ashore
	bVoteOffered = Departure && bAlive && Departure->GetStage() == EGalleonDepartureStage::Vote && !Departure->HasVoted(Controller->PlayerState);
}

bool UGalleonCrewComponent::EnsureInput(APlayerController* Controller, const UGalleonRules& Rules)
{
	if (bInputBuilt)
	{
		return true;
	}
	const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input)
	{
		// the local player comes a little after the controller on a client: next update
		return false;
	}

	// the wheel's key consumes its keys while its mapping is on: the interaction (F) does not see them at the wheel
	HelmAction = NewObject<UInputAction>(this, TEXT("IA_GalleonHelm"), RF_Transient);
	HelmAction->ValueType = EInputActionValueType::Boolean;
	HelmAction->bConsumeInput = true;
	HelmContext = NewObject<UInputMappingContext>(this, TEXT("IMC_GalleonHelm"), RF_Transient);
	HelmKeys.Reset();
	for (const FKey& Key : Rules.HelmKeys)
	{
		if (Key.IsValid())
		{
			HelmContext->MapKey(HelmAction, Key);
			HelmKeys.Add(Key);
		}
	}

	// the vote key does nothing outside a vote and takes no other binding's key
	VoteAction = NewObject<UInputAction>(this, TEXT("IA_GalleonVote"), RF_Transient);
	VoteAction->ValueType = EInputActionValueType::Boolean;
	VoteAction->bConsumeInput = false;
	VoteContext = NewObject<UInputMappingContext>(this, TEXT("IMC_GalleonVote"), RF_Transient);
	for (const FKey& Key : Rules.VoteKeys)
	{
		if (Key.IsValid())
		{
			VoteContext->MapKey(VoteAction, Key);
		}
	}

	CrewInput = NewObject<UEnhancedInputComponent>(Controller, MakeUniqueObjectName(Controller, UEnhancedInputComponent::StaticClass(), TEXT("GalleonCrewInput")), RF_Transient);
	CrewInput->bBlockInput = false;
	CrewInput->RegisterComponent();
	CrewInput->BindAction(HelmAction, ETriggerEvent::Started, this, &UGalleonCrewComponent::HandleHelmPressed);
	CrewInput->BindAction(HelmAction, ETriggerEvent::Completed, this, &UGalleonCrewComponent::HandleHelmReleased);
	CrewInput->BindAction(HelmAction, ETriggerEvent::Canceled, this, &UGalleonCrewComponent::HandleHelmReleased);
	CrewInput->BindAction(VoteAction, ETriggerEvent::Started, this, &UGalleonCrewComponent::HandleVotePressed);
	// on the controller's own stack: the keys work with any pawn
	Controller->PushInputComponent(CrewInput);
	Input->AddMappingContext(VoteContext, Rules.CrewInputPriority, KeepHeldKeys());

	HelmKeyName = Rules.HelmKeys.Num() > 0 ? Rules.HelmKeys[0].GetDisplayName() : FText::GetEmpty();
	VoteKeyName = Rules.VoteKeys.Num() > 0 ? Rules.VoteKeys[0].GetDisplayName() : FText::GetEmpty();
	bInputBuilt = true;
	UE_LOG(Logmonke_island, Log, TEXT("Departure: keys of the wheel (%s) and of the vote (%s) ready for %s"), *HelmKeyName.ToString(),
		*VoteKeyName.ToString(), *Controller->GetName());
	return true;
}

void UGalleonCrewComponent::SetHelmContextActive(APlayerController* Controller, bool bActive, int32 Priority)
{
	if (bActive == bHelmContextActive || !HelmContext)
	{
		return;
	}
	const ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input)
	{
		return;
	}
	if (bActive)
	{
		// a key already down when the player reaches the wheel (F still held after letting go of an object there) raises
		// nothing: only a press made at the wheel counts
		bIgnoreHeldHelmKey = false;
		for (const FKey& Key : HelmKeys)
		{
			bIgnoreHeldHelmKey |= Controller->IsInputKeyDown(Key);
		}
		Input->AddMappingContext(HelmContext, Priority, KeepHeldKeys());
	}
	else
	{
		Input->RemoveMappingContext(HelmContext, KeepHeldKeys());
	}
	bHelmContextActive = bActive;
}

void UGalleonCrewComponent::HandleHelmPressed(const FInputActionValue& Value)
{
	if (!bHelmContextActive || bHelmKeyHeld || bIgnoreHeldHelmKey)
	{
		return;
	}
	bHelmKeyHeld = true;
	RequestHelmKey(true);
}

void UGalleonCrewComponent::HandleHelmReleased(const FInputActionValue& Value)
{
	bIgnoreHeldHelmKey = false;
	if (!bHelmKeyHeld)
	{
		return;
	}
	bHelmKeyHeld = false;
	RequestHelmKey(false);
}

void UGalleonCrewComponent::HandleVotePressed(const FInputActionValue& Value)
{
	// outside a vote the key does nothing: no request for nothing
	if (bVoteOffered)
	{
		RequestVote();
	}
}

void UGalleonCrewComponent::RequestHelmKey(bool bPressed)
{
	// on a listen server the host's call runs here at once
	ServerHelmKey(bPressed);
}

void UGalleonCrewComponent::RequestVote()
{
	ServerVote();
}

void UGalleonCrewComponent::DebugHoldHelm()
{
	const AGalleonActor* Ship = FindGalleon();
	const float Seconds = (Ship && Ship->Rules ? Ship->Rules->HelmHoldSeconds : GetDefault<UGalleonRules>()->HelmHoldSeconds) + DebugHoldMarginSeconds;
	RequestHelmKey(true);
	GetWorld()->GetTimerManager().SetTimer(DebugReleaseTimer, this, &UGalleonCrewComponent::DebugReleaseHelm, Seconds, false);
}

void UGalleonCrewComponent::DebugReleaseHelm()
{
	RequestHelmKey(false);
}

// ---------------------------------------------------------------- server

void UGalleonCrewComponent::ServerHelmKey_Implementation(bool bPressed)
{
	APlayerController* Controller = GetController();
	UGalleonDepartureComponent* Departure = UGalleonDepartureComponent::Get(this);
	if (!Controller || !Departure)
	{
		return;
	}
	// presses are rate limited (SEC-03); a release always goes through, or a held hold would raise the anchor
	if (bPressed)
	{
		const UGalleonRules* Rules = Departure->GetRules();
		const double Now = GetWorld()->GetRealTimeSeconds();
		const float MinInterval = Rules ? Rules->DepartureRequestMinInterval : GetDefault<UGalleonRules>()->DepartureRequestMinInterval;
		if (Now - LastPressTime < MinInterval)
		{
			UE_LOG(Logmonke_island, Verbose, TEXT("Departure: %s presses the wheel's key too fast: dropped"), *Controller->GetName());
			return;
		}
		LastPressTime = Now;
	}
	Departure->HandleHelmKey(Controller, bPressed);
}

void UGalleonCrewComponent::ServerVote_Implementation()
{
	APlayerController* Controller = GetController();
	UGalleonDepartureComponent* Departure = UGalleonDepartureComponent::Get(this);
	if (!Controller || !Departure)
	{
		return;
	}
	const UGalleonRules* Rules = Departure->GetRules();
	const double Now = GetWorld()->GetRealTimeSeconds();
	const float MinInterval = Rules ? Rules->DepartureRequestMinInterval : GetDefault<UGalleonRules>()->DepartureRequestMinInterval;
	if (Now - LastVoteTime < MinInterval)
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Departure: %s votes too fast: dropped"), *Controller->GetName());
		return;
	}
	LastVoteTime = Now;
	Departure->HandleVote(Controller);
}
