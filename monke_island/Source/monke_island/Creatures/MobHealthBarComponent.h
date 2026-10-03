// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/WidgetComponent.h"
#include "MobHealthBarComponent.generated.h"

class UIslanderHealthComponent;
class UProgressBar;

/** Bar of a mob health bar: a progress bar built in C++ (no widget asset) */
UCLASS()
class MONKE_ISLAND_API UMobHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Fills the bar (0..1) and colours it between Low and Full */
	void SetFraction(float Fraction, const FLinearColor& Low, const FLinearColor& Full);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;

private:

	/** The bar */
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> Bar;
};

/**
 *  Health bar floating above a mob (tiki, shark), drawn on screen for the local players only. It shows up once
 *  the mob is hurt, follows its UIslanderHealthComponent (replicated health) and disappears at its death or
 *  beyond MaxDistance. Ticks only while it is shown.
 */
UCLASS(ClassGroup=(Creatures), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UMobHealthBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:

	UMobHealthBarComponent();

	/** Height of the bar above the top of the mob's bounds (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar", meta=(Units="cm"))
	float Margin = 25.0f;

	/** Size of the bar on screen (px) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar")
	FIntPoint BarSize = FIntPoint(70, 9);

	/** Hidden beyond this distance from the local player's view (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar", meta=(ClampMin="0", Units="cm"))
	float MaxDistance = 4000.0f;

	/** Hidden while the mob is within this distance of a bush (cm, 0 = never): a mob hiding there does not give itself away */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar", meta=(ClampMin="0", Units="cm"))
	float BushHideRadius = 200.0f;

	/** Time between two bush checks (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar", meta=(ClampMin="0.05", Units="s"))
	float BushCheckInterval = 0.3f;

	/** Shown only once the mob has been hurt */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar")
	bool bOnlyWhenHurt = true;

	/** Colour of a full bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar")
	FLinearColor FullColor = FLinearColor(0.2f, 0.85f, 0.25f);

	/** Colour of an almost empty bar */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Health Bar")
	FLinearColor LowColor = FLinearColor(0.9f, 0.12f, 0.08f);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;

private:

	UFUNCTION()
	void HandleHealthChanged(float NewHealth, float Delta);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	/** Shows or hides the bar from the health and the distance to the local view */
	void Refresh();

	/** Health of the mob */
	UPROPERTY(Transient)
	TObjectPtr<UIslanderHealthComponent> Health;

	/** The mob has been hurt at least once */
	bool bHurt = false;

	/** The mob was near a bush at the last check */
	bool bInBush = false;

	/** Time until the next bush check (s) */
	float BushCheckCountdown = 0.0f;
};
