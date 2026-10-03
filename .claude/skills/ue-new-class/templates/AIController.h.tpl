// monke_island

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "{{ClassName}}.generated.h"

class UStateTreeAIComponent;
class UAIPerceptionComponent;
struct FAIStimulus;

/**
 *  {{Description}}
 *  Behavior runs in a StateTree; senses are configured on the perception component in the Blueprint.
 */
UCLASS(abstract)
class MONKE_ISLAND_API A{{ClassName}} : public AAIController
{
	GENERATED_BODY()

	/** Runs the behavior StateTree */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeAI;

	/** Detects other actors through sight, hearing and other senses */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UAIPerceptionComponent> AIPerception;

public:

	A{{ClassName}}();

protected:

	virtual void OnPossess(APawn* InPawn) override;

	virtual void OnUnPossess() override;

	/** Called when the perception of an actor is updated */
	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};
