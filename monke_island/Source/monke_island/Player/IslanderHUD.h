// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "IslanderHUD.generated.h"

/**
 *  Provisional canvas HUD: health and stamina bars with the current stance (exhausted, crouched, guard).
 *  Stands in for WBP_HUD (cahier §19) until the UI lot.
 */
UCLASS()
class MONKE_ISLAND_API AIslanderHUD : public AHUD
{
	GENERATED_BODY()

public:

	/** Size of each bar at 1080p (px) */
	UPROPERTY(EditDefaultsOnly, Category="HUD")
	FVector2D BarSize = FVector2D(260.0f, 12.0f);

	/** Distance of the bars from the bottom left corner at 1080p (px) */
	UPROPERTY(EditDefaultsOnly, Category="HUD")
	FVector2D Margin = FVector2D(40.0f, 48.0f);

	/** Health bar colour */
	UPROPERTY(EditDefaultsOnly, Category="HUD")
	FLinearColor HealthColor = FLinearColor(0.80f, 0.12f, 0.10f);

	/** Stamina bar colour */
	UPROPERTY(EditDefaultsOnly, Category="HUD")
	FLinearColor StaminaColor = FLinearColor(0.95f, 0.78f, 0.20f);

	/** Stamina bar colour while exhausted */
	UPROPERTY(EditDefaultsOnly, Category="HUD")
	FLinearColor ExhaustedColor = FLinearColor(0.85f, 0.35f, 0.10f);

	virtual void DrawHUD() override;

private:

	/** Draws one bar with its label; returns the y of the next line */
	float DrawBar(float X, float Y, float Scale, float Fraction, const FLinearColor& Color, const FString& Label);
};
