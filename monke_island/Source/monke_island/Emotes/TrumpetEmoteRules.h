// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TrumpetEmoteRules.generated.h"

class UInputAction;
class UInputMappingContext;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;

/**
 *  Herald trumpet emote of the bean (key V, maquettes bean-trumpet / bean-trumpet-anim of the second PC): the
 *  trumpet comes out in the right hand, sounds the fanfare and goes away. The item in hand is put away for the
 *  time of the emote (the trumpet is held with both hands) and taken back after.
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

	/** Times of the clips (A_Bean_Trumpet_Equip / Fanfare / Unequip, 24 fps) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.05", Units="s"))
	float EquipSeconds = 0.792f;

	/** See EquipSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.05", Units="s"))
	float FanfareSeconds = 3.5f;

	/** See EquipSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.05", Units="s"))
	float UnequipSeconds = 0.792f;

	/** In the equip clip, the trumpet goes from the carry grip to the mouth (AN_Trumpet_AtMouth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float AtMouthTime = 0.625f;

	/** In the unequip clip, the trumpet leaves the mouth for the carry grip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float LeaveMouthTime = 0.2f;

	/** Time between two fanfares (s, from the start of the last one) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0", Units="s"))
	float Cooldown = 6.0f;

	/** Fanfare, timed on the start of the fanfare clip (SW_Trumpet_Fanfare, scripts/props/trumpet_sounds.py) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> FanfareSound;

	/** Attenuation of the fanfare (heard far) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundAttenuation> SoundAttenuation;

	/** Key of the emote (V) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> EmoteAction;

	/** Mapping context of the emote, added for the local player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	/** Priority of the mapping context */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	int32 MappingPriority = 1;
};
