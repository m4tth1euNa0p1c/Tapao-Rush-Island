// monke_island

#include "{{IncludePath}}{{ClassName}}.h"
#include "StateTreeExecutionContext.h"
#include "AIController.h"

EStateTreeRunStatus F{{ClassName}}::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!IsValid(InstanceData.Controller))
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void F{{ClassName}}::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
}
