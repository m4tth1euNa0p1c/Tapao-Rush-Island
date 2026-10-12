// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Capture/CaptureItem.h"
#include "HandcuffsItem.generated.h"

class ACapturePrisoner;
class ACharacter;

/**
 *  Steel handcuffs of the inventory (T-336, DA_Item_Handcuffs), open in the right hand. On a subdued French soldier or
 *  Veilleur in front (DA_CaptureRules: at or under SubduedHealthFraction of its health, or down without a leg; within
 *  CuffReachCm and in sight; never a villager, the giant crab nor a player), the interaction key held CuffHoldSeconds
 *  closes them: the cuffing gesture plays (A_Bean_Cuff, stretched over the hold), the server checks again at the end,
 *  and the mob becomes a prisoner on the chain of the player (ACapturePrisoner::CaptureFrom). Released early, nothing.
 *  One pair holds one prisoner at a time: in use until it is sold, flees or goes, then free again (reusable).
 *  Two players on the same target: the first hold wins (UCaptureSubsystem claims, §12 n°2).
 */
UCLASS()
class MONKE_ISLAND_API AHandcuffsItem : public ACaptureItem
{
	GENERATED_BODY()

public:

	AHandcuffsItem();

	/** Prisoner wearing these handcuffs (replicated), nullptr while free */
	ACapturePrisoner* GetPrisoner() const { return Prisoner; }

	/** On the wrists of a prisoner */
	UFUNCTION(BlueprintPure, Category="Capture")
	bool IsInUse() const { return Prisoner != nullptr; }

	/** Why Target cannot be cuffed now by this carrier (None when it can); its share of health; the owner's preview and the
	 *  server's check alike (bServer adds the latency tolerance and the claims of the others) */
	ECaptureRefusal JudgeTarget(const ACharacter* Target, bool bServer, float& OutHealthFraction) const;

	/** The best target in front of the carrier (the one nearest the aim within the reach), nullptr when none */
	ACharacter* FindTarget() const;

	/** Server: the prisoner is sold, fled or gone: the handcuffs are free again */
	void ReleasePrisoner(const ACapturePrisoner* InPrisoner);

	virtual bool GetPrompt(FCaptureHudLine& Out) const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual UStaticMesh* GetHeldMesh() const override;
	virtual FTransform GetHeldGrip() const override;
	virtual bool ShouldShowHeldMesh() const override;
	virtual bool WantsUseKey() const override;
	virtual void OnUsePressed() override;
	virtual void OnUseReleased() override;
	virtual void RefreshLocal() override;
	virtual void OnEquippedChanged() override;

	/** Starts the hold on Target */
	UFUNCTION(Server, Reliable)
	void ServerBeginCuff(ACharacter* Target);

	/** The key was released before the end */
	UFUNCTION(Server, Reliable)
	void ServerCancelCuff();

private:

	/** Server: the hold is over: checks again and makes the prisoner */
	void CompleteCuff();

	/** Server: the hold stops (released, put away, the carrier fell) */
	void CancelCuff();

	/** Server: the claim on the target ends */
	void ReleaseClaim();

	/** Prisoner wearing these handcuffs (replicated: in use) */
	UPROPERTY(Replicated)
	TObjectPtr<ACapturePrisoner> Prisoner;

	/** Server: the target of the hold under way */
	TWeakObjectPtr<ACharacter> CuffTarget;

	FTimerHandle CuffTimer;

	/** Owner: the last check (target in front, why not, its health and kind) */
	TWeakObjectPtr<ACharacter> LocalTarget;
	ECaptureRefusal LocalRefusal = ECaptureRefusal::NoTarget;
	float LocalHealth = 1.0f;
	ECaptiveKind LocalKind = ECaptiveKind::Other;

	/** Owner: the key went down on a target, the hold is asked; when (world time) */
	bool bLocalHolding = false;
	double LocalHoldStart = 0.0;

	/** Owner: the crosshair is on something to use with the key (the interaction keeps it) */
	bool bInteractableInView = false;
};
