// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/IslanderInteractable.h"
#include "Capture/CaptureTypes.h"
#include "CapturePrisoner.generated.h"

class ACaptureCage;
class AHandcuffsItem;
class UCaptiveComponent;
class UCaptureRules;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 *  A cuffed French soldier or Veilleur (T-336). The capture replaces the mob by this pawn at its place, with its look
 *  (mesh, materials, size, lost bones): the mob leaves the fight for good, so neither its fort, its arena, its AI nor
 *  the other sides (Creatures/Hostility) touch the prisoner, and no line of the mob classes changes. It walks with the
 *  captive clips of its skeleton (A_BeanFR_Captive_*, T-337) or with its own and the bound hands
 *  (UCapturePrisonerAnimInstance), wears the closed handcuffs, and follows its jailer on a chain (UCaptiveComponent).
 *  Let go, it waits handcuffed: the interaction key on it takes its chain (any player leading none), and it flees after
 *  DA_CaptureRules.AbandonedEscapeSeconds. Put in a cage, it is taken away a few seconds later.
 *  Server authoritative: its look and rules replicate once, its state and jailer with UCaptiveComponent. No blow hurts
 *  it (bPrisonerInvulnerable). Ticks every frame (its pose, its steps, the chain), like the mobs it replaces.
 */
UCLASS()
class MONKE_ISLAND_API ACapturePrisoner : public ACharacter, public IIslanderInteractable
{
	GENERATED_BODY()

public:

	ACapturePrisoner();

	/** Capture rules (DA_CaptureRules), set by the capture and replicated once */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Capture")
	TObjectPtr<UCaptureRules> Rules;

	/** Server: replaces Original (a subdued soldier or Veilleur) by a prisoner on the chain of Jailer, wearing Handcuffs;
	 *  Original is destroyed. nullptr (Original untouched) when it cannot */
	static ACapturePrisoner* CaptureFrom(ACharacter* Original, APawn* Jailer, AHandcuffsItem* Handcuffs, UCaptureRules* InRules);

	/** State, jailer and chain */
	UCaptiveComponent* GetCaptive() const { return Captive; }

	/** Kind of prisoner (texts) */
	ECaptiveKind GetKind() const { return Look.Kind; }

	/** On the ground without a leg: it crawls */
	bool IsCrawling() const { return Look.bCrawling; }

	/** Where the chain starts on its handcuffs (world) */
	FVector GetCuffsAnchor() const;

	/** Server: walks into a cage that took it (the sale) */
	void EnterCage(ACaptureCage* InCage);

	/** Server: behind the closed door of its cage; the handcuffs are free again */
	void SetCaged();

	/** Server (UCaptiveComponent): its jailer let it go; the jailer is told, it flees later */
	void OnAbandoned(APawn* FormerJailer, ECaptiveRelease Reason);

	// IIslanderInteractable: the chain of a prisoner let go
	virtual FText GetInteractionPrompt_Implementation(APawn* User) const override;
	virtual bool CanInteract_Implementation(APawn* User) const override;
	virtual void Interact_Implementation(APawn* User) override;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** State, jailer, steps and chain */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCaptiveComponent> Captive;

	/** Closed handcuffs on its wrists */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CuffsMesh;

	/** Links of the chain to its jailer (cosmetic, every machine with a picture) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInstancedStaticMeshComponent> ChainLinks;

	/** Its look, copied from the mob it replaces (replicated once) */
	UPROPERTY(ReplicatedUsing=OnRep_Look)
	FCapturePrisonerLook Look;

	/** Every machine: the look arrived */
	UFUNCTION()
	void OnRep_Look();

	/** Its former jailer's machine says it was let go */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastReleased(APawn* FormerJailer, ECaptiveRelease Reason);

	/** Every machine near it says it fled */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastEscaped();

	/** Every machine: mesh, materials, size, lost bones, handcuffs */
	void ApplyLook();

	/** Every machine: the closed handcuffs on the first cuff bone it still has */
	void AttachCuffs();

	/** Every machine: the collision of each state (found by the crosshair only when let go) */
	void HandleStateChanged(ECaptiveState NewState);

	/** Server: the walk into the cage ended */
	void HandleCageReached();

	/** Server: let go too long, it flees */
	void Escape();

	/** Server: its handcuffs are free again (sold, fled, gone) */
	void FreeHandcuffs();

	/** Every machine: the clips of this frame and the bound hands */
	void UpdateAnimation(float DeltaSeconds);

	/** Server: the cage it walks into or stands in */
	TWeakObjectPtr<ACaptureCage> Cage;

	FTimerHandle EscapeTimer;

	/** The look was put on the body on this machine */
	bool bLookApplied = false;

	/** Clocks of the clips and the weight of the walk (this machine) */
	float IdleTime = 0.0f;
	float WalkTime = 0.0f;
	float CrawlTime = 0.0f;
	float WalkAlpha = 0.0f;
};
