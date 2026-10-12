// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmWeapon.h"
#include "BoltRifleWeapon.generated.h"

class UBoltRifleRules;
class UStaticMeshComponent;

/**
 *  Bean's bolt-action marksman rifle with a scope (BP_BoltRifle, DA_BoltRifleRules; owner 2026-10-11, T-335; model and
 *  clips T-334). Five rounds in the rifle; a shot (Fire, or AimFire through the scope) leaves the case in the chamber,
 *  then the bolt is worked (BoltCycle: BoltOpen, CaseEject, BoltClose) and no shot leaves before BoltClose: a press
 *  after it cuts the end of the clip and fires at once. An empty rifle takes a stripper clip from the pouch (Reload, the
 *  rounds count at MagIn, the bolt closes on the first at BoltClose). Right click held raises the scope (the base's aim:
 *  AimIn, AimIdle, AimOut, narrow AimFieldOfView, the eyepiece on the view axis, the weapon hidden behind it).
 *  The server owns the rounds, the chamber and every gesture that changes them (BoltRifleMath); the carrier shows its
 *  shot at once.
 */
UCLASS()
class MONKE_ISLAND_API ABoltRifleWeapon : public AFirearmWeapon
{
	GENERATED_BODY()

public:

	ABoltRifleWeapon();

	/** A live round is chambered under a closed bolt: the next press fires */
	UFUNCTION(BlueprintPure, Category="Bolt Rifle")
	bool IsChambered() const { return bChambered; }

	virtual int32 GetLoadedRounds() const override { return Rounds; }
	virtual int32 GetReserveRounds() const override { return ReserveRounds; }
	virtual int32 GetMaxRounds() const override;
	virtual void SetRounds(int32 Loaded, int32 Reserve) override;

	/** The reload lowers the scope, the bolt too (bBoltCycleLowersScope) */
	virtual bool LowersAim(EFirearmGesture Gesture) const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void ServerHandleFire() override;
	virtual void ServerHandleReload() override;
	virtual void OnActionEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void OnActionFinished(EFirearmGesture Gesture) override;
	virtual void OnActionStopped(EFirearmGesture Gesture) override;
	virtual bool PredictFire() const override;
	virtual float GetShotSpreadDegrees(bool bMoving) const override;
	virtual float GetGestureRate(const FFirearmClip& Clip) const override;
	virtual void OnCosmeticEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void TickCosmetics(float DeltaSeconds) override;
	virtual bool HasCosmeticsRunning() const override;

private:

	/** Rules of the rifle (Rules cast) */
	const UBoltRifleRules* GetBoltRules() const;

	/** Server: the shot of the chambered round, then what follows it once its clip played */
	void Shoot();

	/** Server: what follows a shot (the bolt worked, or the reload of an empty rifle) */
	void FollowShot();

	/** Server: the bolt is worked (BoltCycle; without its clip, at once) */
	void CycleBolt();

	/** Server: the stripper clip, when there is room and rounds in the pouch (false otherwise or busy) */
	bool BeginReload();

	/** Server: the bolt closes on the next round */
	void CloseBolt();

	/** Server: the clip is pressed in, its rounds go from the pouch into the rifle */
	void LoadClip();

	/** Shot gesture now: AimFire through the scope when the pack has it, else Fire */
	EFirearmGesture ShotGesture() const;

	/** Throws a piece out of the rifle seen here (every machine): the spent case, the empty stripper clip; from Socket
	 *  (FallbackSocket, the weapon's origin without them), at Velocity relative to the carrier's view */
	void ThrowPiece(UStaticMesh* Mesh, FName Socket, FName FallbackSocket, const FVector& Velocity);

	/** Rounds in the rifle, the chambered one included */
	UPROPERTY(Replicated)
	int32 Rounds = 0;

	/** A live round is chambered under a closed bolt */
	UPROPERTY(Replicated)
	bool bChambered = false;

	/** Rounds left in the pouch (carrier only) */
	UPROPERTY(Replicated)
	int32 ReserveRounds = 0;

	/** Server: the clip went in during the Reload playing */
	bool bClipLoadedThisReload = false;

	/** Server: the bolt closed during the BoltCycle or Reload playing */
	bool bBoltClosedThisGesture = false;

	/** Spent cases and empty clips in flight (local pool, world space) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CaseComponents;

	/** One spent case or empty clip */
	struct FCase
	{
		FVector Velocity = FVector::ZeroVector;
		FVector Spin = FVector::ZeroVector;
		float Age = 0.0f;
		bool bActive = false;
		bool bResting = false;
	};

	/** Same order as CaseComponents */
	TArray<FCase> Cases;
};
