// monke_island

#include "Boats/BoatRiderComponent.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatPhysics.h"
#include "Boats/BoatRules.h"
#include "Boats/BoatSeat.h"
#include "CanvasItem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"

DEFINE_LOG_CATEGORY(LogMonkeBoat);

namespace
{
	/** Rowing keys: ZQSD (AZERTY), WASD (QWERTY) and the arrows */
	struct FRowKeyDef
	{
		const TCHAR* Name;
		FVector2D Direction;
		TArray<FKey> Keys;
	};
}

UBoatRiderComponent::UBoatRiderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f;
	SetIsReplicatedByDefault(true);
}

void UBoatRiderComponent::BeginPlay()
{
	Super::BeginPlay();
	bInputBound = TryBindInput();
}

void UBoatRiderComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDHandle.IsValid())
	{
		AHUD::OnHUDPostRender.Remove(HUDHandle);
		HUDHandle.Reset();
	}
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Input = PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
			{
				Input->RemoveMappingContext(RowingContext);
			}
		}
	}
	if (GetOwner() && GetOwner()->HasAuthority() && IsSeated())
	{
		SeatedBoat->StandCharacter(Cast<ACharacter>(GetOwner()));
	}
	Super::EndPlay(EndPlayReason);
}

void UBoatRiderComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBoatRiderComponent, SeatedBoat);
	DOREPLIFETIME(UBoatRiderComponent, SeatIndex);
}

bool UBoatRiderComponent::TryBindInput()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return false;
	}
	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	UEnhancedInputComponent* Bindings = Cast<UEnhancedInputComponent>(Pawn->InputComponent);
	UEnhancedInputLocalPlayerSubsystem* Input = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Bindings || !Input)
	{
		return false;
	}

	// rowing: only active while seated, above the character's context so the movement keys row instead
	RowingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_BoatRowing"));
	const FRowKeyDef Defs[] = {
		{TEXT("IA_BoatRowForward"), FVector2D(0.0f, 1.0f), {EKeys::Z, EKeys::W, EKeys::Up}},
		{TEXT("IA_BoatRowBack"), FVector2D(0.0f, -1.0f), {EKeys::S, EKeys::Down}},
		{TEXT("IA_BoatTurnLeft"), FVector2D(-1.0f, 0.0f), {EKeys::Q, EKeys::A, EKeys::Left}},
		{TEXT("IA_BoatTurnRight"), FVector2D(1.0f, 0.0f), {EKeys::D, EKeys::Right}}};
	RowActions.Reset();
	RowDirections.Reset();
	HeldDirections.Init(FVector2D::ZeroVector, UE_ARRAY_COUNT(Defs));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Defs); ++Index)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Defs[Index].Name);
		Action->ValueType = EInputActionValueType::Boolean;
		RowActions.Add(Action);
		for (const FKey& Key : Defs[Index].Keys)
		{
			RowingContext->MapKey(Action, Key);
		}
		RowDirections.Add(Defs[Index].Direction);
		Bindings->BindAction(Action, ETriggerEvent::Started, this, &UBoatRiderComponent::OnRowKey, Index, true);
		Bindings->BindAction(Action, ETriggerEvent::Completed, this, &UBoatRiderComponent::OnRowKey, Index, false);
	}

	HUDHandle = AHUD::OnHUDPostRender.AddUObject(this, &UBoatRiderComponent::DrawHUD);

	// once bound, tick every frame to stream the rowing input
	SetComponentTickInterval(0.0f);
	return true;
}

void UBoatRiderComponent::UpdateRowingContext()
{
	const bool bWant = IsSeated();
	if (bWant == bRowingContextActive)
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Input = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input)
	{
		return;
	}
	if (bWant)
	{
		Input->AddMappingContext(RowingContext, RowingContextPriority);
	}
	else
	{
		Input->RemoveMappingContext(RowingContext);
		for (FVector2D& Held : HeldDirections)
		{
			Held = FVector2D::ZeroVector;
		}
	}
	bRowingContextActive = bWant;
}

void UBoatRiderComponent::OnRowKey(const FInputActionValue& Value, int32 Index, bool bPressed)
{
	if (HeldDirections.IsValidIndex(Index))
	{
		HeldDirections[Index] = bPressed ? RowDirections[Index] : FVector2D::ZeroVector;
	}
}

void UBoatRiderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// seat guard, every machine (T-182): the character matches its seat whatever the order of the replicated events
	ReconcileSeat(true);
	// thrown off a sinking boat: once off its seat here (review R-12)
	ApplyPendingLaunch();

	if (!bInputBound)
	{
		bInputBound = TryBindInput();
		if (!bInputBound)
		{
			return;
		}
	}

	UpdateRowingContext();

	FVector2D Input = FVector2D::ZeroVector;
	for (const FVector2D& Held : HeldDirections)
	{
		Input += Held;
	}
	Input = FVector2D(FMath::Clamp(Input.X, -1.0f, 1.0f), FMath::Clamp(Input.Y, -1.0f, 1.0f));

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const bool bRower = IsSeated() && SeatedBoat->IsRower(Character);
	if (!bRower)
	{
		Input = FVector2D::ZeroVector;
	}

	TimeSinceRowSend += DeltaTime;
	// a change goes out at once (a stop) or after RowInputSendInterval; a held input goes out again every
	// RowInputResendInterval, since one lost unreliable packet would leave the server with the old one (review R-09)
	const bool bChanged = Input != SentRowInput && (Input.IsZero() || TimeSinceRowSend >= RowInputSendInterval);
	const bool bResend = !Input.IsZero() && TimeSinceRowSend >= RowInputResendInterval;
	if (bChanged || bResend)
	{
		if (Input.IsZero())
		{
			ServerStopRowing();
		}
		else
		{
			ServerSetRowInput(Input);
		}
		SentRowInput = Input;
		TimeSinceRowSend = 0.0f;
	}
}

void UBoatRiderComponent::ClientLaunch_Implementation(FVector_NetQuantize10 Velocity, bool bXYOverride, bool bZOverride)
{
	PendingLaunch = Velocity;
	bPendingLaunchXY = bXYOverride;
	bPendingLaunchZ = bZOverride;
	bHasPendingLaunch = true;
	PendingLaunchTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ApplyPendingLaunch();
}

void UBoatRiderComponent::ApplyPendingLaunch()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!bHasPendingLaunch || !Character || !GetWorld())
	{
		return;
	}
	// the seat may reach this machine after the call: the launch waits for the owner to be off it, not for long
	if (bSeatApplied || IsSeated())
	{
		bHasPendingLaunch = GetWorld()->GetTimeSeconds() - PendingLaunchTime < 1.0;
		return;
	}
	bHasPendingLaunch = false;
	Character->LaunchCharacter(PendingLaunch, bPendingLaunchXY, bPendingLaunchZ);
}

void UBoatRiderComponent::ServerSetRowInput_Implementation(FVector2D Input)
{
	if (IsSeated())
	{
		SeatedBoat->SetRowInput(Cast<ACharacter>(GetOwner()), Input);
	}
}

void UBoatRiderComponent::ServerStopRowing_Implementation()
{
	if (IsSeated())
	{
		SeatedBoat->SetRowInput(Cast<ACharacter>(GetOwner()), FVector2D::ZeroVector);
	}
}

bool UBoatRiderComponent::IsSeated() const
{
	return IsValid(SeatedBoat);
}

void UBoatRiderComponent::SetSeat(ABoatActor* Boat, int32 InSeatIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	SeatedBoat = Boat;
	SeatIndex = Boat ? InSeatIndex : INDEX_NONE;
	ReconcileSeat(false);
}

void UBoatRiderComponent::LeaveBoat()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->HasAuthority())
	{
		return;
	}
	// the boat frees the seat (and hands the oars over), which stands the owner up
	if (IsSeated())
	{
		SeatedBoat->StandCharacter(Character);
	}
	// a seat its boat no longer knows (boat gone): cleared here
	if (SeatedBoat || SeatIndex != INDEX_NONE)
	{
		SetSeat(nullptr, INDEX_NONE);
	}
	// and off a hull it may hang on without a seat
	ReconcileSeat(false);
}

void UBoatRiderComponent::OnRep_Seat()
{
	ReconcileSeat(false);
}

void UBoatRiderComponent::ReconcileSeat(bool bFromGuard)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !Character->GetCharacterMovement() || !Character->GetCapsuleComponent() || !Character->GetRootComponent())
	{
		return;
	}

	// what this machine sees (T-182, BoatSeat::Resolve)
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Character);
	BoatSeat::FRiderState State;
	State.bSeated = IsSeated() && SeatIndex != INDEX_NONE;
	State.bBoatReady = State.bSeated && SeatedBoat->HasActorBegunPlay() && SeatedBoat->Rules && !SeatedBoat->GetSeatSocket(SeatIndex).IsNone();
	State.bApplied = bSeatApplied;
	State.bOnSeatHull = State.bSeated && Character->GetRootComponent()->GetAttachParent() == SeatedBoat->GetHull();
	State.bOnBoatHull = Cast<ABoatActor>(Character->GetAttachParentActor()) != nullptr;
	State.bMovementOff = Character->GetCharacterMovement()->MovementMode == MOVE_None;
	State.bRagdoll = Islander && Islander->IsRagdoll();
	State.bAuthority = Character->HasAuthority();

	switch (BoatSeat::Resolve(State))
	{
	case BoatSeat::EAction::None:
	case BoatSeat::EAction::Wait:
		return;

	case BoatSeat::EAction::Sit:
		if (bFromGuard && State.bAuthority)
		{
			UE_LOG(LogMonkeBoat, Warning, TEXT("Seat guard: %s seated in %s but %s: sat again"), *Character->GetName(), *SeatedBoat->GetName(),
				!State.bApplied ? TEXT("never sat here") : (!State.bOnSeatHull ? TEXT("off its hull") : TEXT("its movement on")));
		}
		else if (bFromGuard)
		{
			// a client: the seat came before its boat knew its seats, or a correction switched the movement back on
			UE_LOG(LogMonkeBoat, Verbose, TEXT("Seat guard: %s sits in %s (late seat on this client)"), *Character->GetName(), *SeatedBoat->GetName());
		}
		SitOnSeat(Character);
		return;

	case BoatSeat::EAction::LeaveSeat:
		// knocked out on its seat (tumble, blow, claw): the body falls off it, the boat frees the seat and the oars (server)
		UE_LOG(LogMonkeBoat, Log, TEXT("%s knocked out on its seat: falls off %s"), *Character->GetName(), *SeatedBoat->GetName());
		SeatedBoat->StandCharacter(Character);
		return;

	case BoatSeat::EAction::Detach:
		ReleaseFromBoat(Character, true);
		return;

	case BoatSeat::EAction::Release:
		if (bFromGuard && !bReleaseAfterRagdoll)
		{
			const AActor* Parent = Character->GetAttachParentActor();
			UE_LOG(LogMonkeBoat, Warning, TEXT("Seat guard: %s seated nowhere but %s %s, movement %s: freed"), *Character->GetName(),
				State.bOnBoatHull ? TEXT("hanging on") : TEXT("still held by the seat of"),
				*GetNameSafe(State.bOnBoatHull ? Parent : AppliedBoat.Get()), State.bMovementOff ? TEXT("off") : TEXT("on"));
		}
		ReleaseFromBoat(Character, false);
		return;
	}
}

void UBoatRiderComponent::SitOnSeat(ACharacter* Character)
{
	ABoatActor* Boat = SeatedBoat;
	const UBoatRules* Rules = Boat->Rules;
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();

	// seated: no walking, no physical contact with the hull (it would push the boat), follows the seat
	Move->StopMovementImmediately();
	Move->DisableMovement();
	// the hands go to the oars: the item in hand is put away (the inventory refuses one while seated)
	if (Character->HasAuthority())
	{
		if (UIslanderInventoryComponent* Inventory = Character->FindComponentByClass<UIslanderInventoryComponent>();
			Inventory && Inventory->GetActiveSlot() != INDEX_NONE)
		{
			Inventory->SelectSlot(Inventory->GetActiveSlot());
		}
	}
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// the FBX sockets come in rotated (yaw 180, roll 180: Z down): sit upright in the hull frame, facing the bow,
	// whatever the socket rotation, otherwise the rider hangs upside down inside the keel
	if (Rules->bOnlyRowerSits)
	{
		// the rower sits on the rowing post, centred between the oarlocks: feet on the post, capsule above
		const FTransform Hull = Boat->GetHull()->GetComponentTransform();
		const FQuat Facing = Hull.GetRotation() * FQuat(FVector::UpVector, Rules->bRowerFacesStern ? UE_PI : 0.0);
		const FVector Feet = Hull.TransformPosition(Rules->RowerSeatLocation);
		const FVector Centre = Feet + Facing.GetUpVector() * (Capsule->GetScaledCapsuleHalfHeight() + Rules->RowerHeightOffsetCm);
		Character->SetActorLocationAndRotation(Centre, Facing.Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
		Character->AttachToComponent(Boat->GetHull(), FAttachmentTransformRules::KeepWorldTransform);
	}
	else
	{
		const FName Socket = Boat->GetSeatSocket(SeatIndex);
		const FQuat SocketInHull = Boat->GetHull()->GetSocketTransform(Socket, RTS_Component).GetRotation();
		const FTransform Seated = BoatPhysics::SeatedRiderRelative(SocketInHull, Boat->GetSeatedCapsuleOffset());
		Character->AttachToComponent(Boat->GetHull(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		Character->SetActorRelativeLocation(Seated.GetLocation());
		Character->SetActorRelativeRotation(Seated.GetRotation());
	}
	AppliedBoat = Boat;
	bSeatApplied = true;
	bReleaseAfterRagdoll = false;
	// rowing reads better from behind: third person while seated, the player's own view back once standing
	if (AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Character))
	{
		Islander->SetForceThirdPerson(true);
	}
}

void UBoatRiderComponent::ReleaseFromBoat(ACharacter* Character, bool bKeepRagdoll)
{
	const ABoatActor* Hull = Cast<ABoatActor>(Character->GetAttachParentActor());
	const FString From = GetNameSafe(Hull ? Hull : AppliedBoat.Get());
	if (Hull)
	{
		// off the hull where it is: the boat no longer carries it
		Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		if (!bKeepRagdoll)
		{
			// stands up upright above its seat, free to walk or jump out (a ragdoll's body is left where it is)
			Character->SetActorRotation(FRotator(0.0f, Character->GetActorRotation().Yaw, 0.0f));
			if (Character->HasAuthority())
			{
				const float Lift = Hull->Rules ? Hull->Rules->StandUpLiftCm : 0.0f;
				Character->SetActorLocation(BoatSeat::StandUpLocation(Character->GetActorLocation(), Lift), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}
	}
	// the player's own view back (a ragdoll keeps third person until it stands up)
	if (AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Character))
	{
		Islander->SetForceThirdPerson(false);
	}
	if (bKeepRagdoll)
	{
		// the ragdoll owns the movement and the capsule collision: they come back once it stands up (Release, next ticks)
		if (Hull)
		{
			UE_LOG(LogMonkeBoat, Log, TEXT("%s knocked out: off %s, free once it stands up"), *Character->GetName(), *From);
		}
		bReleaseAfterRagdoll = bSeatApplied;
		return;
	}

	// the capsule touches the hull again, and the movement comes back where the seat had switched it off
	Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (Move->MovementMode == MOVE_None)
	{
		Move->SetMovementMode(MOVE_Falling);
	}
	UE_LOG(LogMonkeBoat, Log, TEXT("%s stands up from %s (%s)"), *Character->GetName(), *From,
		Character->HasAuthority() ? TEXT("server") : TEXT("client"));
	AppliedBoat = nullptr;
	bSeatApplied = false;
	bReleaseAfterRagdoll = false;
}

void UBoatRiderComponent::DrawHUD(AHUD* HUD, UCanvas* Canvas)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!HUD || !Canvas || !Character || HUD->GetOwningPawn() != Character)
	{
		return;
	}

	TArray<TPair<FString, FColor>> Lines;

	// drowning warning (cahier §4, ADR-003 B-04)
	if (const UIslanderMovementComponent* Move = Cast<UIslanderMovementComponent>(Character->GetCharacterMovement()))
	{
		if (Move->IsDrowning())
		{
			Lines.Emplace(TEXT("Tu te noies ! Regagne la barque ou la rive"), FColor(255, 70, 50));
		}
	}

	// the boat we are in or next to
	const ABoatActor* Boat = SeatedBoat;
	if (!Boat)
	{
		double BestDistSq = FMath::Square(450.0);
		for (TActorIterator<ABoatActor> It(GetWorld()); It; ++It)
		{
			const double DistSq = FVector::DistSquared(It->GetActorLocation(), Character->GetActorLocation());
			if (DistSq < BestDistSq && !It->IsSunk())
			{
				BestDistSq = DistSq;
				Boat = *It;
			}
		}
	}
	if (Boat)
	{
		// no load counter on screen (owner request 2026-10-04): only the warning when the barque is overloaded
		const bool bOver = Boat->GetLoadKg() > Boat->GetMaxLoadKg();
		if (bOver)
		{
			Lines.Emplace(TEXT("Surcharge : la barque prend l'eau ! Allégez-la"), FColor(255, 90, 60));
		}
	}

	const float Scale = Canvas->ClipY / 1080.0f;
	float Y = Canvas->ClipY * 0.18f;
	for (const TPair<FString, FColor>& Line : Lines)
	{
		FCanvasTextItem Text(FVector2D(Canvas->ClipX * 0.5f, Y), FText::FromString(Line.Key), GEngine->GetMediumFont(), FLinearColor(Line.Value));
		Text.Scale = FVector2D(Scale, Scale);
		Text.bCentreX = true;
		Text.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Text);
		Y += 26.0f * Scale;
	}
}
