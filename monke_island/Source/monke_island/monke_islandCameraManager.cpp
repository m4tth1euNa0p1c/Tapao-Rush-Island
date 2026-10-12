// Copyright Epic Games, Inc. All Rights Reserved.


#include "monke_islandCameraManager.h"
#include "Death/DeathSpectatorComponent.h"
#include "GameFramework/PlayerController.h"

Amonke_islandCameraManager::Amonke_islandCameraManager()
{
	// set the min/max pitch
	ViewPitchMin = -70.0f;
	ViewPitchMax = 80.0f;
}

void Amonke_islandCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	// a dead player watching a living teammate (T-135): behind him rather than inside his head
	if (UDeathSpectatorComponent* Spectator = PCOwner ? PCOwner->FindComponentByClass<UDeathSpectatorComponent>() : nullptr)
	{
		Spectator->ApplyFollowView(OutVT.Target, OutVT.POV, DeltaTime);
	}
}
