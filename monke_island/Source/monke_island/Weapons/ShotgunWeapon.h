// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmWeapon.h"
#include "ShotgunWeapon.generated.h"

class UShotgunRules;
class UStaticMeshComponent;

/**
 *  Bean's pirate pump shotgun (BP_Shotgun, DA_ShotgunRules; second PC: bean-pirate-shotgun). A cone of pellets per
 *  shot (their hits on one target add up into one blow), the pump after each shot (FirePump: the spent shell flies
 *  out at EjectSpentShell), a reload shell by shell (ReloadStart, ReloadInsert until full or out of shells, ReloadEnd)
 *  that a press on fire cuts to shoot at once, and ReloadEmpty (three shells, then the pump) when the gun is empty.
 *  The server owns the shells and counts each one at the InsertSlug event of the clip.
 */
UCLASS()
class MONKE_ISLAND_API AShotgunWeapon : public AFirearmWeapon
{
	GENERATED_BODY()

public:

	AShotgunWeapon();

	/** A tiki killed from at most this distance bursts (cm, from DA_ShotgunRules; 0 = none) */
	float GetCloseBurstDistance() const;

	virtual int32 GetLoadedRounds() const override { return Rounds; }
	virtual int32 GetReserveRounds() const override { return ReserveShells; }
	virtual int32 GetMaxRounds() const override;
	virtual void SetRounds(int32 Loaded, int32 Reserve) override { Rounds = FMath::Max(0, Loaded); ReserveShells = FMath::Max(0, Reserve); }

	/** Every step of the reload lowers the aim too (network review R-38) */
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
	virtual void OnCosmeticEvent(EFirearmGesture Gesture, FName Event) override;
	virtual void TickCosmetics(float DeltaSeconds) override;
	virtual bool HasCosmeticsRunning() const override;
	virtual void DescribeShot(FFirearmDamageEvent& Shot, float Distance) override;

private:

	/** Server: the first step of a reload (ReloadEmpty or ReloadStart), nothing when full or out of shells */
	void BeginReload();

	/** Shells ReloadEmpty shows going in (its InsertEvent count); a big number without that clip (shell by shell then) */
	int32 GetEmptyReloadShells() const;

	/** Server: the shot */
	void Shoot();

	/** Rules of the shotgun (Rules cast) */
	const UShotgunRules* GetShotgunRules() const;

	/** Throws a spent shell out of the visible gun (every machine) */
	void EjectShell();

	/** Shells in the gun */
	UPROPERTY(Replicated)
	int32 Rounds = 0;

	/** Shells left in the pouch (carrier only) */
	UPROPERTY(Replicated)
	int32 ReserveShells = 0;

	/** Server: step of the reload running (FirearmMath::EReloadPhase) */
	uint8 ReloadPhase = 0;

	/** Server: a shell went in during the ReloadInsert playing */
	bool bInsertedThisStep = false;

	/** Server: R was pressed during the pump of a shot: the reload follows it (T-335: the press is no longer lost) */
	bool bReloadQueued = false;

	/** Spent shells in flight (local pool, world space) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> ShellComponents;

	/** One spent shell */
	struct FShell
	{
		FVector Velocity = FVector::ZeroVector;
		FVector Spin = FVector::ZeroVector;
		float Age = 0.0f;
		bool bActive = false;
		bool bResting = false;
	};

	/** Same order as ShellComponents */
	TArray<FShell> Shells;
};
