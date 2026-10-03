// monke_island

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "{{ClassName}}.generated.h"

class AAIController;

/**
 *  Instance data for F{{ClassName}}
 */
USTRUCT()
struct F{{ClassName}}InstanceData
{
	GENERATED_BODY()

	/** AI Controller running the StateTree */
	UPROPERTY(EditAnywhere, Category=Context)
	TObjectPtr<AAIController> Controller;

	/** Example input, bound in the StateTree editor */
	UPROPERTY(EditAnywhere, Category=Input)
	TObjectPtr<AActor> TargetActor;
};

/**
 *  {{Description}}
 */
USTRUCT(meta=(DisplayName="{{DisplayName}}", Category="monke_island"))
struct F{{ClassName}} : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	F{{ClassName}}()
	{
		// enable only if the task really needs per-frame updates
		bShouldCallTick = false;
	}

	using FInstanceDataType = F{{ClassName}}InstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	/** Runs when the owning state is entered */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	/** Runs when the owning state is exited */
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
