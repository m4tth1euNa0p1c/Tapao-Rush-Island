// monke_island

#include "{{IncludePath}}{{ClassName}}.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"

void A{{ClassName}}::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only local players own an input subsystem
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (UInputMappingContext* Context : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(Context, 0);
		}
	}
}
