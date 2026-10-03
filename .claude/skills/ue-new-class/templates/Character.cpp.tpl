// monke_island

#include "{{IncludePath}}{{ClassName}}.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "monke_island.h"

A{{ClassName}}::A{{ClassName}}()
{
	PrimaryActorTick.bCanEverTick = false;
}

void A{{ClassName}}::BeginPlay()
{
	Super::BeginPlay();
}

void A{{ClassName}}::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &A{{ClassName}}::MoveInput);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &A{{ClassName}}::LookInput);
	}
	else
	{
		UE_LOG(Logmonke_island, Error, TEXT("'%s' requires an Enhanced Input Component"), *GetNameSafe(this));
	}
}

void A{{ClassName}}::MoveInput(const FInputActionValue& Value)
{
	const FVector2D Movement = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Movement.Y);
	AddMovementInput(GetActorRightVector(), Movement.X);
}

void A{{ClassName}}::LookInput(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	AddControllerYawInput(Look.X);
	AddControllerPitchInput(Look.Y);
}
