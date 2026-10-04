// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TrumpetEmoteComponent.generated.h"

class UAudioComponent;
class UBeanPuppetComponent;
class UStaticMeshComponent;
class UTrumpetEmoteRules;

/** Part of the trumpet emote playing */
UENUM(BlueprintType)
enum class ETrumpetPhase : uint8
{
	None,
	Equip,
	Fanfare,
	Unequip
};

/**
 *  Trumpet emote of the bean (key V): the server starts it (alive, on foot, out of the water, cooldown), puts the
 *  item in hand away and takes it back at the end; the start replicates as a counter and every machine plays the
 *  clips (UBeanAnimDriverComponent), shows the trumpet in the right hand and sounds the fanfare.
 */
UCLASS(ClassGroup=(Islander), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UTrumpetEmoteComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UTrumpetEmoteComponent();

	/** Trumpet, timing, sound and key (DA_TrumpetEmoteRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	TObjectPtr<UTrumpetEmoteRules> Rules;

	/** Asks the server for a fanfare (key V) */
	UFUNCTION(BlueprintCallable, Category="Trumpet")
	void PlayFanfare();

	/** Part playing on this machine, with its elapsed time and duration (s) */
	ETrumpetPhase GetPhase(float& OutElapsed, float& OutDuration) const;

	/** The emote is playing on this machine */
	UFUNCTION(BlueprintPure, Category="Trumpet")
	bool IsPlaying() const { return LocalStart >= 0.0 && GetElapsed() < GetTotal(); }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	UFUNCTION(Server, Reliable)
	void ServerPlayFanfare();

	UFUNCTION()
	void OnRep_PlayCount();

	/** Every machine: starts the local playback */
	void StartLocal();

	/** Server: the emote ends, the item put away comes back */
	void FinishOnServer();

	/** Can the owner sound it now (server) */
	bool CanPlay() const;

	void TryBindInput();

	float GetElapsed() const;
	float GetTotal() const;

	/** Trumpet on the right hand of the puppet, shown while playing */
	void UpdateTrumpet();

	/** Fanfares started (server), replicated: each change plays it */
	UPROPERTY(ReplicatedUsing=OnRep_PlayCount)
	uint8 PlayCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Trumpet;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Sound;

	TWeakObjectPtr<UBeanPuppetComponent> Puppet;

	/** World time of the start on this machine, < 0 when idle */
	double LocalStart = -1.0;

	/** Fanfare sounded for the current playback */
	bool bSounded = false;

	/** Server: time of the last start, slot put away (-1 none) */
	double LastServerStart = -1.0e9;
	int32 PutAwaySlot = INDEX_NONE;
	FTimerHandle EndTimer;

	bool bInputBound = false;
	int32 BindAttempts = 0;
	FTimerHandle BindTimer;
};
