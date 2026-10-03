// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "{{ClassName}}.generated.h"

class UInputAction;
struct FInputActionValue;

/**
 *  {{Description}}
 */
UCLASS(abstract)
class MONKE_ISLAND_API A{{ClassName}} : public ACharacter
{
	GENERATED_BODY()

public:

	A{{ClassName}}();

protected:

	/** Move input action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	/** Look input action */
	UPROPERTY(EditAnywhere, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	virtual void BeginPlay() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Handles move input */
	void MoveInput(const FInputActionValue& Value);

	/** Handles look input */
	void LookInput(const FInputActionValue& Value);
};
