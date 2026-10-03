// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "{{ClassName}}.generated.h"

/**
 *  {{Description}}
 *  Shown in Project Settings > Game > {{DisplayName}}, saved in Config/DefaultGame.ini
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="{{DisplayName}}"))
class MONKE_ISLAND_API U{{ClassName}} : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	/** Example tunable value */
	UPROPERTY(Config, EditAnywhere, Category="General")
	float ExampleValue = 1.0f;

	/** Returns the settings defaults */
	static const U{{ClassName}}* Get() { return GetDefault<U{{ClassName}}>(); }

	virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
