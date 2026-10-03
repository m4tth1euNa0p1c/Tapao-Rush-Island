// monke_island

#include "{{IncludePath}}{{ClassName}}.h"
#include "Components/StateTreeAIComponent.h"
#include "Perception/AIPerceptionComponent.h"

A{{ClassName}}::A{{ClassName}}()
{
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	StateTreeAI->SetStartLogicAutomatically(false);

	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &A{{ClassName}}::OnPerceptionUpdated);
}

void A{{ClassName}}::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// start the behavior once a pawn is controlled
	StateTreeAI->StartLogic();
}

void A{{ClassName}}::OnUnPossess()
{
	StateTreeAI->StopLogic(FString(TEXT("Unpossessed")));

	Super::OnUnPossess();
}

void A{{ClassName}}::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
}
