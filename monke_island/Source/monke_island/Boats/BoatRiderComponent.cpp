// monke_island

#include "Boats/BoatRiderComponent.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatRules.h"
#include "CanvasItem.h"
#include "Components/CapsuleComponent.h"
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
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"

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
	if (GetOwner() && GetOwner()->HasAuthority() && SeatedBoat)
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
	const bool bRower = SeatedBoat && SeatedBoat->IsRower(Character);
	if (!bRower)
	{
		Input = FVector2D::ZeroVector;
	}

	TimeSinceRowSend += DeltaTime;
	if (Input != SentRowInput && (Input.IsZero() || TimeSinceRowSend >= RowInputSendInterval))
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

void UBoatRiderComponent::ServerSetRowInput_Implementation(FVector2D Input)
{
	if (SeatedBoat)
	{
		SeatedBoat->SetRowInput(Cast<ACharacter>(GetOwner()), Input);
	}
}

void UBoatRiderComponent::ServerStopRowing_Implementation()
{
	if (SeatedBoat)
	{
		SeatedBoat->SetRowInput(Cast<ACharacter>(GetOwner()), FVector2D::ZeroVector);
	}
}

void UBoatRiderComponent::SetSeat(ABoatActor* Boat, int32 InSeatIndex)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	SeatedBoat = Boat;
	SeatIndex = Boat ? InSeatIndex : INDEX_NONE;
	ApplySeat();
}

void UBoatRiderComponent::OnRep_Seat()
{
	ApplySeat();
}

void UBoatRiderComponent::ApplySeat()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();

	if (SeatedBoat && SeatIndex != INDEX_NONE)
	{
		const FName Socket = SeatedBoat->GetSeatSocket(SeatIndex);
		if (Socket.IsNone())
		{
			return;
		}
		// seated: no walking, no physical contact with the hull (it would push the boat), follows the seat
		Move->StopMovementImmediately();
		Move->DisableMovement();
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Character->AttachToComponent(SeatedBoat->GetHull(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		Character->SetActorRelativeLocation(FVector(0.0f, 0.0f, SeatedBoat->GetSeatedCapsuleOffset()));
		Character->SetActorRelativeRotation(FRotator::ZeroRotator);
		AppliedBoat = SeatedBoat;
	}
	else if (AppliedBoat || Character->GetAttachParentActor())
	{
		// stand up on the floor above the seat, free to walk or jump out
		Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Character->SetActorRotation(FRotator(0.0f, Character->GetActorRotation().Yaw, 0.0f));
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		if (Character->HasAuthority())
		{
			Character->SetActorLocation(Character->GetActorLocation() + FVector(0.0f, 0.0f, 35.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		Move->SetMovementMode(MOVE_Falling);
		AppliedBoat = nullptr;
	}
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
		const bool bOver = Boat->GetLoadKg() > Boat->GetMaxLoadKg();
		FString Status = FString::Printf(TEXT("Barque : %.0f / %.0f kg"), Boat->GetLoadKg(), Boat->GetMaxLoadKg());
		if (Boat->GetBilgeWaterKg() > 1.0f)
		{
			Status += FString::Printf(TEXT("  ·  eau %.0f %%"), 100.0f * Boat->GetBilgeFraction());
		}
		Lines.Emplace(Status, bOver ? FColor(255, 90, 60) : FColor(235, 225, 200));
		if (bOver)
		{
			Lines.Emplace(TEXT("Surcharge : la barque prend l'eau ! Allégez-la"), FColor(255, 90, 60));
		}
		if (SeatedBoat)
		{
			Lines.Emplace(SeatedBoat->IsRower(Character) ? TEXT("[Z Q S D] ramer") : TEXT("Passager"), FColor(255, 235, 150));
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
