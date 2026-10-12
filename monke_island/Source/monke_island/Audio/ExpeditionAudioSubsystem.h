// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Shop/ShopTypes.h"
#include "ExpeditionAudioSubsystem.generated.h"

class ACharacter;
class AGalleonActor;
class ADayNightCycleActor;
class AWindStormActor;
class AShopkeeperActor;
class UExpeditionAudioRules;
class UAudioComponent;
class UIslanderHealthComponent;
class UIslanderInventoryComponent;
class UPrimitiveComponent;
struct FStreamableHandle;

/** Local audio presentation only. Created in Game/PIE, silent on dedicated servers and commandlets. */
UCLASS(Config=Game)
class MONKE_ISLAND_API UExpeditionAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UExpeditionAudioSubsystem();
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Event entry point for authored gameplay/animation events; only this client's audio is played. */
	UFUNCTION(BlueprintCallable, Category="Audio")
	bool PlayEvent(FName Event);

	/** Compact diagnostic; never establishes an audible PIE validation by itself. */
	void Report() const;

private:
	/** Small rules asset loaded before its selected recordings, asynchronously. */
	UPROPERTY(Config)
	FSoftObjectPath RulesPath;

	/** Retained rules for this world. */
	UPROPERTY(Transient)
	TObjectPtr<UExpeditionAudioRules> Rules;

	/** At most one component for each active ambience layer; cleaned up on travel. */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAudioComponent>> Loops;

	TSharedPtr<FStreamableHandle> RulesLoad;
	TSharedPtr<FStreamableHandle> SoundsLoad;
	FTimerHandle PollTimer;
	TWeakObjectPtr<ACharacter> Player;
	TWeakObjectPtr<UIslanderHealthComponent> Health;
	TWeakObjectPtr<UIslanderInventoryComponent> Inventory;
	TWeakObjectPtr<UPrimitiveComponent> Held;
	TWeakObjectPtr<AGalleonActor> Galleon;
	TWeakObjectPtr<AWindStormActor> Wind;
	TWeakObjectPtr<ADayNightCycleActor> DayNight;
	TMap<FName, double> LastPlayed;
	TMap<FName, int32> LastVariation;
	TMap<FName, int32> PlayCounts;
	TMap<FName, float> LoopVolumes;
	double NextWorldRefresh = 0.0;
	double NextCreak = 0.0;
	double NextThunder = 0.0;
	double LastPurchase = -100.0;
	double PreviousTimeLeft = -1.0;
	double HalfTime = 0.0;
	FGuid ExpeditionId;
	int32 InventoryCount = -1;
	int32 PreviousOutcome = 0;
	bool bRunPrimed = false;
	bool bQuotaMet = false;
	bool bHalfPlayed = false;
	bool bStorm = false;
	bool bReady = false;
	bool bShuttingDown = false;

	void RulesReady();
	void SoundsReady();
	void Poll();
	void BindPlayer(ACharacter* NewPlayer);
	void ReadRound();
	void ReadAmbience();
	void SetLoop(FName Event, float Weight);
	void HandleShopOpen(UIslanderInventoryComponent* From, AShopkeeperActor* Shop);
	void HandleShopResult(UIslanderInventoryComponent* From, EShopResult Result, int32 Offer);

	UFUNCTION()
	void HandleMovement(ACharacter* Character, EMovementMode Previous, uint8 PreviousCustom);

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	UFUNCTION()
	void HandleHealth(float NewHealth, float Delta);

	UFUNCTION()
	void HandleDeath(AActor* Killer);
};
