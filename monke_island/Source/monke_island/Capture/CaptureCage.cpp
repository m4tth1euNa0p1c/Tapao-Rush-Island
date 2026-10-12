// monke_island

#include "Capture/CaptureCage.h"
#include "Capture/CaptiveComponent.h"
#include "Capture/CaptureMath.h"
#include "Capture/CapturePrisoner.h"
#include "Capture/CaptureRules.h"
#include "Capture/CaptureSubsystem.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunSubsystem.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/InventoryPickup.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	/** The arrival of a cage is told on the machines that see it within this time of its placing (s): not to a player
	 *  who joins later */
	constexpr double PlacedNoticeWindow = 2.0;

	/** Seconds between two looks for a prisoner led into the cage (AutoSellRadiusCm) and two refreshes of the clock */
	constexpr float AutoSellInterval = 0.25f;
	constexpr float ClockInterval = 0.25f;

	/** A sale stays on the HUD a little longer than the other messages (s) */
	constexpr float SaleNoticeExtraSeconds = 2.0f;

	/** Name of a player the session does not name */
	FText NameOrSomeone(const FString& Name)
	{
		return Name.IsEmpty() ? NSLOCTEXT("Capture", "Someone", "Un équipier") : FText::FromString(Name);
	}
}

ACaptureCage::ACaptureCage()
{
	// only the door's swing ticks, while it moves
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	SetReplicateMovement(false);
	// a few cages, two minutes each: every player hears of their sales and sees their time
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(4.0f);
	SetMinNetUpdateFrequency(1.0f);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	CageMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cage"));
	CageMesh->SetupAttachment(Root);
	CageMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	CageMesh->SetCanEverAffectNavigation(false);
	CageMesh->SetGenerateOverlapEvents(false);

	DoorHinge = CreateDefaultSubobject<USceneComponent>(TEXT("DoorHinge"));
	DoorHinge->SetupAttachment(CageMesh);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Door"));
	DoorMesh->SetupAttachment(DoorHinge);
	DoorMesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	DoorMesh->SetCanEverAffectNavigation(false);
	DoorMesh->SetGenerateOverlapEvents(false);

	// the crosshair's box: the visibility trace only, nothing walks into it
	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(Root);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBox->SetCanEverAffectNavigation(false);
	InteractionBox->SetGenerateOverlapEvents(false);

	TimerText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Clock"));
	TimerText->SetupAttachment(Root);
	TimerText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TimerText->SetCastShadow(false);
	TimerText->SetHiddenInGame(true);
}

void ACaptureCage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACaptureCage, Rules, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACaptureCage, ExpireServerTime, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACaptureCage, PlacerName, COND_InitialOnly);
	DOREPLIFETIME(ACaptureCage, bDoorOpen);
	DOREPLIFETIME(ACaptureCage, Occupant);
	DOREPLIFETIME(ACaptureCage, bGoing);
}

ACaptureCage* ACaptureCage::PlaceCage(UWorld* World, UCaptureRules* InRules, const FTransform& Transform, APawn* Placer,
	const UInventoryItemDefinition* InReturnDefinition)
{
	if (!World || !InRules || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	UClass* Class = InRules->CageClass ? InRules->CageClass.Get() : ACaptureCage::StaticClass();
	ACaptureCage* Cage = World->SpawnActorDeferred<ACaptureCage>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Cage)
	{
		UE_LOG(LogMonkeCapture, Warning, TEXT("Capture: a cage of %s could not be spawned"), *GetNameSafe(Placer));
		return nullptr;
	}
	APlayerState* State = Placer ? Placer->GetPlayerState() : nullptr;
	Cage->Rules = InRules;
	Cage->PlacerPawn = Placer;
	Cage->PlacerState = State;
	Cage->PlacerName = State ? State->GetPlayerName() : FString();
	Cage->ReturnDefinition = InReturnDefinition;
	Cage->ExpireServerTime = Cage->GetServerNow() + InRules->CageLifetimeSeconds;
	Cage->FinishSpawning(Transform);
	UE_LOG(LogMonkeCapture, Log, TEXT("Capture: %s sets a cage at %s for %.0f s"), *GetNameSafe(Placer), *Transform.GetLocation().ToCompactString(),
		InRules->CageLifetimeSeconds);
	return Cage;
}

double ACaptureCage::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

float ACaptureCage::GetSecondsLeft() const
{
	return CaptureMath::SecondsLeft(ExpireServerTime, GetServerNow());
}

void ACaptureCage::BeginPlay()
{
	Super::BeginPlay();
	UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	if (Capture)
	{
		Capture->RegisterCage(this);
	}
	if (!Rules)
	{
		UE_LOG(LogMonkeCapture, Error, TEXT("%s: no DA_CaptureRules, the cage does nothing"), *GetName());
		return;
	}
	ApplyMeshes();
	if (HasAuthority())
	{
		// a cage put in a level by hand gets its time from its start
		if (ExpireServerTime <= 0.0)
		{
			ExpireServerTime = GetServerNow() + Rules->CageLifetimeSeconds;
		}
		GetWorldTimerManager().SetTimer(ExpireTimer, this, &ACaptureCage::Expire, FMath::Max(CaptureMath::SecondsLeft(ExpireServerTime, GetServerNow()), 0.1f), false);
		if (Rules->AutoSellRadiusCm > 0.0f)
		{
			GetWorldTimerManager().SetTimer(AutoSellTimer, this, &ACaptureCage::CheckAutoSell, AutoSellInterval, true);
		}
	}
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (Rules->bShowCageTimer)
	{
		TimerText->SetHiddenInGame(false);
		RefreshTimerText();
		GetWorldTimerManager().SetTimer(TextTimer, this, &ACaptureCage::RefreshTimerText, ClockInterval, true);
	}
	// the arrival, for the crew that sees it set down
	const double PlacedAt = ExpireServerTime - Rules->CageLifetimeSeconds;
	if (Capture && !PlacerName.IsEmpty() && GetServerNow() - PlacedAt < PlacedNoticeWindow)
	{
		Capture->PostNotice(FText::Format(Rules->CagePlacedText, NameOrSomeone(PlacerName)), UCaptureSubsystem::NoticeSeconds, false);
	}
}

void ACaptureCage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	// a cage removed with a prisoner walking in takes it along (it would wait at the door forever)
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed && IsValid(Occupant))
	{
		Occupant->Destroy();
	}
	Occupant = nullptr;
	if (UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this))
	{
		Capture->UnregisterCage(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ACaptureCage::ApplyMeshes()
{
	CageMesh->SetStaticMesh(Rules->CageMesh);
	DoorMesh->SetStaticMesh(Rules->CageDoorMesh);
	// the hinges on their socket: its place and turn only (the sockets of an FBX may carry a scale of 100)
	if (CageMesh->DoesSocketExist(Rules->DoorHingeSocket))
	{
		const FTransform Socket = CageMesh->GetSocketTransform(Rules->DoorHingeSocket, RTS_Component);
		DoorHinge->SetRelativeLocation(Socket.GetLocation());
		DoorClosedRotation = Socket.GetRotation();
	}
	DoorHinge->SetRelativeScale3D(FVector::OneVector);
	InteractionBox->SetBoxExtent(Rules->InteractionHalfExtentCm);
	InteractionBox->SetRelativeLocation(FVector(0.0, 0.0, Rules->InteractionHalfExtentCm.Z));
	TimerText->SetRelativeLocation(FVector(0.0, 0.0, Rules->CageTimerHeightCm));
	TimerText->SetHorizontalAlignment(EHTA_Center);
	TimerText->SetVerticalAlignment(EVRTA_TextCenter);
	TimerText->SetWorldSize(Rules->CageTimerSize);
	TimerText->SetTextRenderColor(FColor(255, 214, 64));
	DoorAlpha = bDoorOpen ? 1.0f : 0.0f;
	ApplyDoor();
}

void ACaptureCage::ApplyDoor()
{
	const float OpenYaw = Rules ? Rules->DoorOpenYawDeg : 0.0f;
	DoorHinge->SetRelativeRotation(DoorClosedRotation * FQuat(FVector::UpVector, FMath::DegreesToRadians(DoorAlpha * OpenYaw)));
}

void ACaptureCage::OnRep_DoorOpen()
{
	// the swing plays on every machine
	SetActorTickEnabled(true);
}

void ACaptureCage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules)
	{
		SetActorTickEnabled(false);
		return;
	}
	DoorAlpha = CaptureMath::DoorStep(DoorAlpha, bDoorOpen, DeltaSeconds, Rules->DoorSwingSeconds);
	ApplyDoor();
	if (DoorAlpha <= 0.0f || DoorAlpha >= 1.0f)
	{
		SetActorTickEnabled(false);
	}
}

void ACaptureCage::RefreshTimerText()
{
	if (!Rules || bGoing)
	{
		return;
	}
	TimerText->SetText(FText::FromString(CaptureMath::ClockText(GetSecondsLeft())));
	// turned toward the local view (the text faces its +X)
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (Controller && Controller->PlayerCameraManager)
	{
		const FVector ToView = Controller->PlayerCameraManager->GetCameraLocation() - TimerText->GetComponentLocation();
		TimerText->SetWorldRotation(FRotator(0.0f, ToView.Rotation().Yaw, 0.0f));
	}
}

// ---------------------------------------------------------------- points

FVector ACaptureCage::GetEntryPoint() const
{
	if (Rules && CageMesh->DoesSocketExist(Rules->EntrySocket))
	{
		return CageMesh->GetSocketLocation(Rules->EntrySocket);
	}
	// in front of the door (+X), past the bars
	const float Ahead = Rules ? static_cast<float>(Rules->CageHalfExtentCm.X) + 45.0f : 105.0f;
	return GetActorLocation() + GetActorForwardVector() * Ahead;
}

FVector ACaptureCage::GetPrisonerPoint() const
{
	if (Rules && CageMesh->DoesSocketExist(Rules->PrisonerSocket))
	{
		return CageMesh->GetSocketLocation(Rules->PrisonerSocket);
	}
	return GetActorLocation();
}

float ACaptureCage::GetPrisonerYaw() const
{
	if (Rules && CageMesh->DoesSocketExist(Rules->PrisonerSocket))
	{
		return CageMesh->GetSocketRotation(Rules->PrisonerSocket).Yaw;
	}
	return GetActorRotation().Yaw;
}

// ---------------------------------------------------------------- sale

FText ACaptureCage::GetInteractionPrompt_Implementation(APawn* User) const
{
	return Rules ? FText::Format(Rules->SellPromptText, FText::AsNumber(CaptureMath::SaleAmount(Rules->SalePrice))) : FText::GetEmpty();
}

bool ACaptureCage::CanInteract_Implementation(APawn* User) const
{
	if (!Rules || bGoing || Occupant || !IsValid(User))
	{
		return false;
	}
	const UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	const ACapturePrisoner* Prisoner = Capture ? Capture->FindPrisonerLedBy(User) : nullptr;
	if (!Prisoner)
	{
		return false;
	}
	// the server allows the latency of a client that saw its prisoner close enough
	const float Tolerance = HasAuthority() && !User->IsLocallyControlled() ? Rules->ServerReachToleranceCm : 0.0f;
	return FVector::Dist2D(Prisoner->GetActorLocation(), GetActorLocation()) <= Rules->SellRadiusCm + Tolerance;
}

void ACaptureCage::Interact_Implementation(APawn* User)
{
	// the grab component checked the reach and the sight of the cage; the prisoner and the cage again here (two jailers
	// at one cage: the first sells, the other waits for the next turn)
	if (!HasAuthority() || !CanInteract_Implementation(User))
	{
		return;
	}
	const UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	if (ACapturePrisoner* Prisoner = Capture ? Capture->FindPrisonerLedBy(User) : nullptr)
	{
		StartSale(Prisoner, User);
	}
}

void ACaptureCage::CheckAutoSell()
{
	if (bGoing || Occupant || !Rules)
	{
		return;
	}
	const UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	ACapturePrisoner* Prisoner = Capture ? Capture->FindLedPrisonerNear(GetActorLocation(), Rules->AutoSellRadiusCm) : nullptr;
	if (Prisoner && Prisoner->GetCaptive())
	{
		StartSale(Prisoner, Prisoner->GetCaptive()->GetJailer());
	}
}

void ACaptureCage::StartSale(ACapturePrisoner* Prisoner, APawn* Jailer)
{
	if (!HasAuthority() || !IsValid(Prisoner) || Occupant || bGoing)
	{
		return;
	}
	Occupant = Prisoner;
	Seller = Jailer;
	bDoorOpen = true;
	OnRep_DoorOpen();
	ForceNetUpdate();
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: %s brings %s in"), *GetName(), *GetNameSafe(Jailer), *GetNameSafe(Prisoner));
	Prisoner->EnterCage(this);
}

void ACaptureCage::OnPrisonerInside(ACapturePrisoner* Prisoner)
{
	const UCaptiveComponent* Captive = Prisoner ? Prisoner->GetCaptive() : nullptr;
	// once per prisoner: a second call (a late timer) pays nothing
	if (!HasAuthority() || !Rules || !Captive || Prisoner != Occupant || Captive->GetState() == ECaptiveState::Caged)
	{
		return;
	}
	bDoorOpen = false;
	OnRep_DoorOpen();
	Prisoner->SetCaged();

	// the crew is paid: the treasury of the run (URunSubsystem, server), what the merchant takes
	const int32 Amount = CaptureMath::SaleAmount(Rules->SalePrice);
	int32 Treasury = 0;
	if (URunSubsystem* Run = URunSubsystem::Get(this))
	{
		Run->AddMoney(GetWorld(), Amount);
		Treasury = Run->GetState().Treasury;
	}
	else
	{
		UE_LOG(LogMonkeCapture, Error, TEXT("%s: no run subsystem, the sale of %s pays nothing"), *GetName(), *GetNameSafe(Prisoner));
	}
	const APawn* Who = Seller.Get();
	const APlayerState* State = Who ? Who->GetPlayerState() : nullptr;
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: %s sold by %s, +%d c, treasury %d c"), *GetName(), *GetNameSafe(Prisoner), *GetNameSafe(Who), Amount, Treasury);
	MulticastSold(State ? State->GetPlayerName() : FString(), Prisoner->GetKind(), Amount, Treasury);
	ForceNetUpdate();
	GetWorldTimerManager().SetTimer(TakeAwayTimer, this, &ACaptureCage::TakeAway, FMath::Max(Rules->TakenAwaySeconds, 0.1f), false);
}

void ACaptureCage::MulticastSold_Implementation(const FString& SellerName, ECaptiveKind Kind, int32 Amount, int32 Treasury)
{
	UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this);
	if (!Capture || !Rules)
	{
		return;
	}
	Capture->PostNotice(FText::Format(Rules->SoldText, NameOrSomeone(SellerName), Rules->GetKindName(Kind), FText::AsNumber(Amount),
		FText::AsNumber(Treasury)), UCaptureSubsystem::NoticeSeconds + SaleNoticeExtraSeconds, false);
}

void ACaptureCage::TakeAway()
{
	if (ACapturePrisoner* Gone = Occupant)
	{
		UE_LOG(LogMonkeCapture, Log, TEXT("%s: %s taken away"), *GetName(), *GetNameSafe(Gone));
		Gone->Destroy();
	}
	Occupant = nullptr;
	Seller.Reset();
	ForceNetUpdate();
}

// ---------------------------------------------------------------- end

void ACaptureCage::Expire()
{
	if (!HasAuthority() || bGoing)
	{
		return;
	}
	// a prisoner walking in finishes first: its sale is paid
	const UCaptiveComponent* Captive = IsValid(Occupant) ? Occupant->GetCaptive() : nullptr;
	if (Captive && Captive->GetState() == ECaptiveState::EnteringCage)
	{
		GetWorldTimerManager().SetTimer(ExpireTimer, this, &ACaptureCage::Expire, 0.5f, false);
		return;
	}
	GetWorldTimerManager().ClearTimer(AutoSellTimer);
	GetWorldTimerManager().ClearTimer(TakeAwayTimer);
	if (IsValid(Occupant))
	{
		Occupant->Destroy();
	}
	Occupant = nullptr;
	bGoing = true;
	OnRep_Going();
	ForceNetUpdate();
	ReturnCage();
	UE_LOG(LogMonkeCapture, Log, TEXT("%s: its time is up"), *GetName());
	// a moment for the clients to hear it before the actor goes
	SetLifeSpan(1.0f);
}

void ACaptureCage::OnRep_Going()
{
	if (!bGoing)
	{
		return;
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetWorldTimerManager().ClearTimer(TextTimer);
	if (UCaptureSubsystem* Capture = UCaptureSubsystem::Get(this); Capture && Rules && GetNetMode() != NM_DedicatedServer)
	{
		Capture->PostNotice(Rules->CageGoneText, UCaptureSubsystem::NoticeSeconds, false);
	}
}

void ACaptureCage::ReturnCage()
{
	if (!Rules || !Rules->bReturnCageToPlacer || !ReturnDefinition)
	{
		return;
	}
	// its player's pawn of now (respawned since, maybe), else the pawn that set it
	APawn* Pawn = PlacerState.IsValid() ? PlacerState->GetPawn() : nullptr;
	if (!Pawn)
	{
		Pawn = PlacerPawn.Get();
	}
	const UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	UIslanderInventoryComponent* Inventory = Pawn && !(Health && Health->IsDead()) ? Pawn->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	if (Inventory && Inventory->GiveItem(ReturnDefinition))
	{
		UE_LOG(LogMonkeCapture, Log, TEXT("%s: the cage goes back to %s"), *GetName(), *GetNameSafe(Pawn));
		return;
	}
	if (Rules->bDropCageWhenNoRoom && GetWorld())
	{
		// no room (or no one): folded on the ground where it stood
		const FTransform Spot(GetActorRotation(), GetActorLocation() + FVector(0.0, 0.0, 30.0));
		AInventoryPickup::SpawnForDefinition(GetWorld(), ReturnDefinition->PickupClass, const_cast<UInventoryItemDefinition*>(ReturnDefinition.Get()), Spot);
		UE_LOG(LogMonkeCapture, Log, TEXT("%s: no room for the cage with %s, it lies folded on the ground"), *GetName(), *GetNameSafe(Pawn));
	}
}
