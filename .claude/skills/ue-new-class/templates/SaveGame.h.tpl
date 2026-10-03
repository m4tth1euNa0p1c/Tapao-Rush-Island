// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "{{ClassName}}.generated.h"

/**
 *  {{Description}}
 *  Saved with UGameplayStatics::SaveGameToSlot(..., SlotName, 0).
 */
UCLASS()
class MONKE_ISLAND_API U{{ClassName}} : public USaveGame
{
	GENERATED_BODY()

public:

	/** Default save slot name */
	static constexpr const TCHAR* SlotName = TEXT("{{ClassName}}");

	/** Bump when the layout changes, to migrate old saves */
	UPROPERTY(VisibleAnywhere, Category="Save")
	int32 SaveVersion = 1;
};
