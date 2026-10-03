// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "{{ClassName}}.generated.h"

class UInputMappingContext;

/**
 *  {{Description}}
 */
UCLASS(abstract)
class MONKE_ISLAND_API A{{ClassName}} : public APlayerController
{
	GENERATED_BODY()

protected:

	/** Input mapping contexts added for the local player */
	UPROPERTY(EditAnywhere, Category="Input")
	TArray<TObjectPtr<UInputMappingContext>> DefaultMappingContexts;

	virtual void SetupInputComponent() override;
};
