// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "monke_islandCameraManager.generated.h"

/**
 *  Basic First Person camera manager.
 *  Limits min/max look pitch.
 *  A dead player who follows a teammate sees him from behind (T-135, UDeathSpectatorComponent).
 */
UCLASS()
class Amonke_islandCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:

	/** Constructor */
	Amonke_islandCameraManager();

protected:

	/** The engine's view, then the follow camera when the view target is a teammate's pawn (T-135) */
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
};
