// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"
#include "TrumpetEmoteRules.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UTexture2D;

/**
 *  Herald trumpet of the bean (maquettes bean-trumpet / bean-trumpet-anim of the second PC): the trumpet comes out in
 *  the right hand, plays its loop and goes away. The item in hand is put away for the time it plays (the trumpet is held
 *  with both hands) and taken back after. Two uses:
 *  - the trumpet call (key A, owner request 2026-10-11, T-333): a military bugle call of CallSeconds in all that gives
 *    the stamina back, then CallCooldown before the next one; its icon sits bottom left in the HUD;
 *  - the flag ceremony (key F on a flag site, AFlagCeremonySite): the trumpet plays for the whole ritual.
 *  The free fanfare of the key V (no cooldown) was removed by T-333: one trumpet, on A.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UTrumpetEmoteRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Trumpet with its banner (SM_Bean_Trumpet) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	TObjectPtr<UStaticMesh> TrumpetMesh;

	/** Bone holding the trumpet */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	FName GripBone = TEXT("hand_r");

	/** Trumpet at the mouth, relative to GripBone (maquette attach_trumpet.json, trumpet_in_hand_r) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	FTransform GripInHand = FTransform(FRotator(-167.57, 54.86, -39.25), FVector(-3.33, -7.21, 3.55));

	/** Trumpet carried while it comes out and goes away (trumpet_carry_hand_r) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trumpet")
	FTransform CarryInHand = FTransform(FRotator(-112.91, 55.86, 32.55), FVector(-0.44, -12.29, 1.64));

	/** Times of the clips (A_Bean_Trumpet_Equip / Unequip, 24 fps); the play loop (A_Bean_Trumpet_Play_Loop) fills the
	 *  time in between */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.05", Units="s"))
	float EquipSeconds = 0.792f;

	/** See EquipSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.05", Units="s"))
	float UnequipSeconds = 0.792f;

	/** In the equip clip, the trumpet goes from the carry grip to the mouth (AN_Trumpet_AtMouth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float AtMouthTime = 0.625f;

	/** In the unequip clip, the trumpet leaves the mouth for the carry grip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float LeaveMouthTime = 0.2f;

	/** The trumpet call lasts this long in all, the equip and the put away included (owner 2026-10-11: 10 s); the play
	 *  loop fills the time between them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Call", meta=(ClampMin="2", Units="s"))
	float CallSeconds = 10.0f;

	/** Time between two calls, counted from the start of the last one (owner 2026-10-11: every minute) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Call", meta=(ClampMin="0", Units="s"))
	float CallCooldown = 60.0f;

	/** Stamina a call gives back, as a share of the bean's MaxStamina (1: a full bar), spread over CallStaminaSeconds
	 *  from the start of the play loop; it adds to the bean's own recovery and stops when the call is broken */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Call", meta=(ClampMin="0", ClampMax="4"))
	float CallStaminaFraction = 1.0f;

	/** Time over which the stamina comes back, from the start of the play loop (0: the whole loop) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Call", meta=(ClampMin="0", Units="s"))
	float CallStaminaSeconds = 0.0f;

	/** Military bugle call sounded once at the start of the play loop (SW_Trumpet_Call: "Assembly", U.S. Army, public
	 *  domain; scripts/audio/trumpet_call_prepare.py); heard by everyone near through SoundAttenuation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Call")
	TObjectPtr<USoundBase> CallSound;

	/** A broken call (knocked out, in the water, an item taken in hand) fades its bugle call out over this time */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Call", meta=(ClampMin="0", Units="s"))
	float CallSoundFadeSeconds = 0.3f;

	/** Picture of the call in the HUD, bottom left (T_Icon_TrumpetCall, scripts/ui/trumpet_call_icon.py) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Call")
	TObjectPtr<UTexture2D> CallIcon;

	/** Fanfare of the flag ceremony, at the start of its loop and every RitualFanfareInterval (SW_Trumpet_Fanfare,
	 *  scripts/props/trumpet_sounds.py) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> FanfareSound;

	/** Attenuation of the trumpet (heard far) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	/** Key of the trumpet call (owner 2026-10-11: A on his AZERTY keyboard, where ZQSD walk): scripts/editor/
	 *  trumpet_call_ue.py maps it to CallAction in MappingContext, the HUD writes it on the icon */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	FKey CallKey = EKeys::A;

	/** Action of the trumpet call (IA_TrumpetCall); it consumes its key, so the A of IA_Move in IMC_Default (QWERTY
	 *  left) no longer walks while the mapping is on */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> CallAction;

	/** Mapping context of the trumpet (IMC_TrumpetEmote: CallKey -> CallAction), added for the local player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Priority of the mapping context (above IMC_Default, whose A walks left) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	int32 MappingPriority = 1;

	/** Legacy: action of the free fanfare of the key V, removed by T-333 and no longer bound (scripts/props/
	 *  import_trumpet.py still fills it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> EmoteAction;

	/** Flag ceremony key (IA_FlagCeremony, F in IMC_FlagCeremony while the player stands in the area of a flag site,
	 *  AFlagCeremonySite): asks the server to start the ceremony there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> CeremonyAction;

	/** Requests of a player (call, ceremony) the server takes at once; then RequestsPerSecond (rate limit, SEC-03): a
	 *  player who mashes the key stays under it, a modified client's flood does not */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1"))
	float RequestBurst = 4.0f;

	/** Requests given back each second once RequestBurst is spent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.1"))
	float RequestsPerSecond = 2.0f;

	/** During a flag ceremony the trumpet plays its loop (A_Bean_Trumpet_Play_Loop); the fanfare sounds at its start,
	 *  then every this many seconds (0: once) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony", meta=(ClampMin="0", Units="s"))
	float RitualFanfareInterval = 15.0f;
};
