// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"
#include "MachineGunRules.generated.h"

class UCameraShakeBase;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

/**
 *  Tunable rules of the bean's crank machine gun, a heavy belt-fed gun of 1918 with a water jacket (DA_MachineGunRules;
 *  owner 2026-10-10, T-273; model and clips T-272). Holding fire turns the crank: the cadence climbs from MinCadenceRpm
 *  to MaxCadenceRpm over SpinUpSeconds and the rounds leave one by one from the belt (BeltCapacity) until the trigger
 *  is let go, the belt runs dry or the barrel overheats. Every round heats the barrel (HeatPerRound); it cools once the
 *  fire stops; at OverheatHeat the fire is cut, steam vents and the barrel glows until it cooled to OverheatRecoverHeat.
 *  The reload swaps the ammunition box and feeds a new belt (Reload clip, ReloadSeconds). Heavy: the carrier walks and
 *  runs slower while it is in hand. The base rules keep the shot itself (Damage per round, falloff, tracers, flash).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UMachineGunRules : public UFirearmRules
{
	GENERATED_BODY()

public:

	UMachineGunRules();

	// ---------------------------------------------------------------- belt

	/** Rounds on a belt (a full ammunition box) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1", ClampMax="250"))
	int32 BeltCapacity = 100;

	/** Rounds a player carries for the gun in all, the belt in the gun included */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="1"))
	int32 MaxRounds = 300;

	/** Rounds at spawn (the belt filled first, at most MaxRounds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ammo", meta=(ClampMin="0"))
	int32 StartRounds = 200;

	/** Fire on an empty belt with rounds in reserve starts the reload */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoReloadOnEmptyFire = true;

	/** The belt running dry during a burst starts the reload after FireEnd (with rounds in reserve) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	bool bAutoReloadWhenBeltRunsOut = true;

	/** Length of the reload (s; the Reload clip is stretched to it, 0 = the clip's own length) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload", meta=(ClampMin="0", Units="s"))
	float ReloadSeconds = 5.0f;

	/** Event of the Reload clip where the new belt is in the feed (the rounds count from there) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Reload")
	FName ReloadLoadedEvent = TEXT("BeltIn");

	// ---------------------------------------------------------------- crank

	/** Cadence of the first turns of the crank (rounds per minute) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crank", meta=(ClampMin="60", ClampMax="1200"))
	float MinCadenceRpm = 450.0f;

	/** Cadence at full crank speed (rounds per minute) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crank", meta=(ClampMin="60", ClampMax="1200"))
	float MaxCadenceRpm = 600.0f;

	/** Time the crank takes from rest to full speed (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crank", meta=(ClampMin="0", Units="s"))
	float SpinUpSeconds = 0.4f;

	/** Time the crank takes to stop once let go (s; fire again before that and the cadence goes on from there) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crank", meta=(ClampMin="0", Units="s"))
	float SpinDownSeconds = 0.35f;

	/** Rounds per turn of the crank: one FireLoop clip (its play rate follows MaxCadenceRpm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crank", meta=(ClampMin="1", ClampMax="12"))
	int32 RoundsPerCrankTurn = 4;

	/** The gun fires while its carrier runs (off: running lets go of the trigger, the gun fires again at a walk) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crank")
	bool bCanFireWhileSprinting = false;

	// ---------------------------------------------------------------- heat

	/** Heat a round gives the barrel (1 = overheated from cold: 0.0135 overheats after 75 rounds held, 7.5 s, before
	 *  the belt runs dry; bursts of 2 s with 1 s pauses empty a belt without overheating) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0", ClampMax="1"))
	float HeatPerRound = 0.0135f;

	/** Heat lost per second once the fire stopped */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0"))
	float CoolingPerSecond = 0.18f;

	/** The barrel starts cooling this long after the last round (s): no cooling during a burst */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0", Units="s"))
	float CoolingDelaySeconds = 0.35f;

	/** Heat at which the gun overheats: the fire is cut, steam vents (Overheat clip) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0.05", ClampMax="1"))
	float OverheatHeat = 1.0f;

	/** An overheated gun fires again once cooled down to this heat */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0", ClampMax="1"))
	float OverheatRecoverHeat = 0.45f;

	/** Scalar parameter of the glowing materials (EmissiveStrength of the project's baked master: the jacket's emissive
	 *  texture is the glow mask of a hot barrel, T-272); a material without it does not glow */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat")
	FName HeatParameterName = TEXT("EmissiveStrength");

	/** Value of that parameter at full glow (0 when cold) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0"))
	float HeatParameterMax = 25.0f;

	/** Material slots of the weapon that glow: a slot whose name contains one of these (M_MG_Jacket); empty = every slot
	 *  (the other parts share the master but have no glow mask: they would glow grey) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat")
	TArray<FString> HeatMaterialSlots = { TEXT("Jacket") };

	/** The barrel starts to glow above this heat (the material parameter goes from 0 there to HeatParameterMax at full
	 *  heat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0", ClampMax="0.99"))
	float GlowStartHeat = 0.45f;

	/** Light of the glowing jacket on the hands and the ground at full heat (0 = none) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0"))
	float HeatLightIntensity = 2500.0f;

	/** See HeatLightIntensity */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat")
	FLinearColor HeatLightColor = FLinearColor(1.0f, 0.28f, 0.06f);

	/** See HeatLightIntensity (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0", Units="cm"))
	float HeatLightRadius = 140.0f;

	/** Weapon bone or socket on the water jacket around the barrel (the glow light, the vent of an overheat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat")
	FName JacketSocket = TEXT("socket_jacket");

	/** Weapon bone or socket of the steam valve of the jacket (the wisps of a hot gun) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat")
	FName SteamCapSocket = TEXT("socket_steam");

	/** Wisps of steam out of the cap above this heat (NS_MG_Steam, restarted while hot; none = no wisps) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat")
	TObjectPtr<UNiagaraSystem> SteamSystem;

	/** Heat above which the cap smokes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat", meta=(ClampMin="0", ClampMax="1"))
	float SteamStartHeat = 0.6f;

	/** Burst of steam of an overheat, at the cap and the jacket (NS_MG_SteamVent; none = SteamSystem) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat")
	TObjectPtr<UNiagaraSystem> SteamVentSystem;

	/** Event of the Overheat clip where the steam vents (at the start of the overheat when the clip has none) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Heat")
	FName SteamVentEvent = TEXT("SteamVent");

	// ---------------------------------------------------------------- spread and recoil

	/** Spread added by each round of a burst (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0", Units="deg"))
	float BloomPerRound = 0.1f;

	/** Most spread a burst adds (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0", Units="deg"))
	float MaxBloomDegrees = 2.5f;

	/** Spread the bloom loses per second without a round (deg/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0"))
	float BloomRecoveryPerSecond = 5.0f;

	/** The bloom on the move, times this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0"))
	float MovingBloomScale = 1.8f;

	/** The view climbs this much per round (deg; the carrier pulls down to keep the gun on target) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0", Units="deg"))
	float RecoilPitchPerRound = 0.17f;

	/** Sideways drift of the view per round (deg: a random walk, either side) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0", Units="deg"))
	float RecoilYawPerRound = 0.09f;

	/** Recoil of the view on the move, times this */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0"))
	float MovingRecoilScale = 1.6f;

	/** How fast the recoil reaches the view (1/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="1"))
	float RecoilApplySpeed = 22.0f;

	/** Shake of the carrier's camera at every round (UMachineGunFireCameraShake by default, none when empty) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil")
	TSubclassOf<UCameraShakeBase> FireCameraShake;

	/** Scale of that shake standing */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0"))
	float FireShakeScale = 1.0f;

	/** Scale of that shake on the move */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recoil", meta=(ClampMin="0"))
	float MovingShakeScale = 1.5f;

	// ---------------------------------------------------------------- carry

	/** Walking speed of the carrier while the gun is in hand (share of its own) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(ClampMin="0.1", ClampMax="1"))
	float WalkSpeedFactor = 0.72f;

	/** Running speed of the carrier while the gun is in hand (share of its own) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Carry", meta=(ClampMin="0.1", ClampMax="1"))
	float SprintSpeedFactor = 0.68f;

	/** Weapon bones of the loaded belt (its pockets and cartridges, belt_01..17 of T-272), hidden while the belt is empty
	 *  and no reload plays (the empty belt out of the gun stays) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Belt")
	TArray<FName> LoadedBeltBones;

	/** Ammunition box let go during the reload, falling from the gun (SM_Ammo_MGBox; none = it vanishes with its bone) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Belt")
	TObjectPtr<UStaticMesh> DroppedBoxMesh;

	/** Event of the Reload clip where the box is let go */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Belt")
	FName DroppedBoxEvent = TEXT("BoxOut");

	/** Weapon bone or socket the box falls from */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Belt")
	FName DroppedBoxSocket = TEXT("socket_box");

	/** Time the dropped box stays on the ground (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Belt", meta=(ClampMin="0.1", Units="s"))
	float DroppedBoxSeconds = 8.0f;

	// ---------------------------------------------------------------- brass

	/** Spent case thrown out at every round (SM_MG_Casing; none = no cases) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass")
	TObjectPtr<UStaticMesh> CasingMesh;

	/** Belt link thrown out with every round (SM_MG_BeltLink; none = a cloth belt, no links) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass")
	TObjectPtr<UStaticMesh> LinkMesh;

	/** Weapon bone or socket the cases leave from (the ejection port under the receiver) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass")
	FName EjectSocket = TEXT("socket_eject");

	/** Weapon bone or socket a little way out along the ejection: the cases fly from EjectSocket toward it (without it,
	 *  along EjectSocket's X axis; without either, to the carrier's right) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass")
	FName EjectDirectionSocket = TEXT("socket_eject_dir");

	/** Speed of a case out of the gun (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass", meta=(ClampMin="0", Units="cm/s"))
	float CasingSpeedCmps = 260.0f;

	/** Speed of a link (cm/s; it drops more than it flies) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass", meta=(ClampMin="0", Units="cm/s"))
	float LinkSpeedCmps = 120.0f;

	/** Cone around the ejection axis (deg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass", meta=(ClampMin="0", ClampMax="90", Units="deg"))
	float CasingSpreadDegrees = 16.0f;

	/** Time a case or a link stays before it disappears (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass", meta=(ClampMin="0.1", Units="s"))
	float CasingLifeSeconds = 2.5f;

	/** Cases in flight or on the ground at once per gun (the oldest one is reused beyond) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass", meta=(ClampMin="0", ClampMax="64"))
	int32 MaxCasings = 24;

	/** Links at once per gun */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass", meta=(ClampMin="0", ClampMax="64"))
	int32 MaxLinks = 12;

	/** Share of the speed a case keeps when it bounces off the ground (0: it stays where it lands) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass", meta=(ClampMin="0", ClampMax="0.9"))
	float CasingBounce = 0.3f;

	/** Clink of a case landing (SC_MG_CasingLand) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass")
	TObjectPtr<USoundBase> CasingLandSound;

	/** Two clinks of the same gun at least this far apart (s; the stream of brass stays a stream) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Brass", meta=(ClampMin="0", Units="s"))
	float CasingSoundMinInterval = 0.06f;

	// ---------------------------------------------------------------- sounds

	/** Clatter of the crank and the feed, looped while the crank turns; its pitch follows the cadence (none = silent) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Sounds")
	TObjectPtr<USoundBase> MechanismLoopSound;

	/** The crank engaging at the start of a burst */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Sounds")
	TObjectPtr<USoundBase> BurstStartSound;

	/** The echo of the place and the crank stopping at the end of a burst */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Sounds")
	TObjectPtr<USoundBase> BurstTailSound;

	/** Pitch of the mechanism loop at the first turns of the crank (1 at full speed; MinCadenceRpm / MaxCadenceRpm keeps
	 *  its clatter on the rounds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Sounds", meta=(ClampMin="0.2", ClampMax="2"))
	float MechanismPitchAtSpinUp = 0.75f;

	/** Hiss and ping of the barrel overheating (at the start of the overheat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Sounds")
	TObjectPtr<USoundBase> OverheatSound;

	/** Steam simmering in the jacket, looped while the gun is hot (volume with the heat above SteamStartHeat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Sounds")
	TObjectPtr<USoundBase> SteamLoopSound;

	/** Volume of the simmering at full heat */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visuals|Sounds", meta=(ClampMin="0"))
	float SteamLoopVolume = 0.8f;
};
