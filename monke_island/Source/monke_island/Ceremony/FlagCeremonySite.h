// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FlagCeremonySite.generated.h"

class APawn;
class ULocalPlayer;
class UFlagCeremonyRules;
class UStaticMeshComponent;

/** Where the flag ceremony of a site is */
UENUM(BlueprintType)
enum class EFlagCeremonyPhase : uint8
{
	/** Nothing going on: a player standing in the area may start it */
	Idle,
	/** The flag goes up while the officiant sounds the trumpet */
	Raising,
	/** The flag is up for good: the village is pacified */
	Raised
};

/** How the last ceremony of a site ended */
UENUM(BlueprintType)
enum class EFlagCeremonyEnd : uint8
{
	None,
	/** The flag reached the top */
	Raised,
	/** The officiant moved (walked, was pushed, jumped, fell) */
	Moved,
	/** The officiant took a blow */
	Hurt,
	/** The officiant died */
	Died,
	/** The officiant left the game */
	Gone
};

/** Replicated state of a ceremony */
USTRUCT()
struct FFlagCeremonyState
{
	GENERATED_BODY()

	/** Where the ceremony is */
	UPROPERTY()
	EFlagCeremonyPhase Phase = EFlagCeremonyPhase::Idle;

	/** Server world time the raising started (AGameStateBase::GetServerWorldTimeSeconds) */
	UPROPERTY()
	double StartTime = 0.0;

	/** Player sounding the trumpet while it rises */
	UPROPERTY()
	TObjectPtr<APawn> Officiant;

	/** The pole stands (from the first start on: it stays after an interruption, cahier §6.2) */
	UPROPERTY()
	bool bPlanted = false;

	/** Ceremonies ended so far: each change shows how the last one ended */
	UPROPERTY()
	uint8 EndCount = 0;

	/** How the last one ended */
	UPROPERTY()
	EFlagCeremonyEnd LastEnd = EFlagCeremonyEnd::None;
};

/**
 *  Flag site in the middle of a village square (owner requests 2026-10-07, cahier §6.2), DA_FlagCeremonyRules: a
 *  small pile of stones; a player standing near it presses F (IMC_FlagCeremony, mapped only while the ceremony can
 *  start) to raise the British flag. The pole rises out of the stones and the flag climbs it for RitualSeconds while
 *  the officiant sounds the trumpet (UTrumpetEmoteComponent); the villagers leave their routine and stand in a ring
 *  around the officiant, those of the houses with the Tidal Relic of their house, which they lay around the officiant
 *  once the flag is up (ATikiDirector). The officiant must stay put: moving, a blow, death or leaving interrupts and
 *  the flag comes down to start again from zero; the pole stays. Once up, the zone is pacified: its bonus goes to the
 *  expedition once (cahier §6.1) and the sealed houses of the village (the shrine) open for good.
 *  The server decides, the state replicates (the progress from the server clock); every machine raises the flag the
 *  same way and draws the bar (AIslanderHUD). AVillageSpawner puts one in every pacifiable village of the fragment.
 */
UCLASS()
class MONKE_ISLAND_API AFlagCeremonySite : public AActor
{
	GENERATED_BODY()

public:

	AFlagCeremonySite();

	/** Stones, pole, flag, area, timing and texts (DA_FlagCeremonyRules); set by the spawner, replicated once */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Rules, Category="Ceremony")
	TObjectPtr<UFlagCeremonyRules> Rules;

	/** Zone pacified by the ceremony: the id of its village (F01_Z1); found from the villages when left empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Ceremony")
	FName ZoneId;

	/** Index of its village in AVillageSpawner (server; INDEX_NONE: the village nearest to the site, once built) */
	int32 VillageIndex = INDEX_NONE;

	/** Site whose area holds Location, or nullptr */
	static AFlagCeremonySite* FindAt(const UObject* WorldContextObject, const FVector& Location);

	/** Site nearest to Location within MaxDistance, or nullptr */
	static AFlagCeremonySite* FindNearest(const UObject* WorldContextObject, const FVector& Location, float MaxDistance);

	/** True when Location stands in the area of the ceremony */
	UFUNCTION(BlueprintPure, Category="Ceremony")
	bool IsInArea(const FVector& Location) const;

	/** True when this pawn could start the ceremony now (idle, in the area, alive, on its feet, on land) */
	bool CanStart(const APawn* Pawn) const;

	/** Server: starts the ceremony for Pawn after checking it all again; true when it started */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Ceremony")
	bool TryStart(APawn* Pawn);

	/** Where the ceremony is */
	UFUNCTION(BlueprintPure, Category="Ceremony")
	EFlagCeremonyPhase GetPhase() const { return State.Phase; }

	/** Player raising the flag now, or nullptr */
	UFUNCTION(BlueprintPure, Category="Ceremony")
	APawn* GetOfficiant() const { return State.Phase == EFlagCeremonyPhase::Raising ? State.Officiant.Get() : nullptr; }

	/** Share of the raising done (0 to 1), on any machine */
	UFUNCTION(BlueprintPure, Category="Ceremony")
	float GetProgress() const;

	/** Seconds before the flag is up, on any machine */
	UFUNCTION(BlueprintPure, Category="Ceremony")
	float GetSecondsLeft() const;

	/** How the last ceremony ended and how long ago (s) on this machine; false when none ended yet */
	bool GetLastEnd(EFlagCeremonyEnd& OutEnd, float& OutAge) const;

	/** Where the villagers gather around (server): the spot the officiant started from */
	FVector GetGatherCenter() const { return GatherCenter; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Stones, with their collision */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Cairn;

	/** Pole, out of the stones once the first ceremony started */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Pole;

	/** Flag, slid up the pole */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Flag;

	/** The ceremony as the server sees it */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	FFlagCeremonyState State;

	/** Server: where the officiant started (the gathering centre, the reference of the stillness check) */
	FVector GatherCenter = FVector::ZeroVector;

	/** Server: checks of the officiant during the raising */
	FTimerHandle CheckTimer;

	/** Local players whose ceremony key is mapped */
	TArray<TWeakObjectPtr<ULocalPlayer>> Mapped;

	FTimerHandle LocalTimer;

	/** Ends seen on this machine, and when the last one was */
	uint8 SeenEndCount = 0;
	double LocalEndTime = -1.0;

	UFUNCTION()
	void OnRep_Rules();

	UFUNCTION()
	void OnRep_State();

	/** Meshes and their places from the rules */
	void ApplyRules();

	/** Pole and flag as the state says, the tick on while the flag rises */
	void ApplyState();

	/** Bottom of the flag above the foot of the pole now (cm) */
	float GetFlagHeight() const;

	/** Server: the officiant is still there, alive and still; the flag is up at the end */
	void CheckOfficiant();

	/** Server: the ceremony ends this way */
	void Finish(EFlagCeremonyEnd End);

	/** Server: a blow to the officiant */
	UFUNCTION()
	void OnOfficiantHealthChanged(float NewHealth, float Delta);

	/** Server: the village of the site from the spawner, when not given */
	void ResolveVillage();

	/** Server clock as every machine sees it */
	double GetServerNow() const;

	/** Maps the ceremony key for the local players standing in the area while it can start */
	void CheckLocalPlayers();

	/** Adds or removes the mapping of one local player */
	void SetMapped(ULocalPlayer* Player, bool bMapped);
};
