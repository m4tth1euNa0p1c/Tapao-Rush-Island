// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FlagCeremonyRules.generated.h"

class UInputMappingContext;
class UStaticMesh;

/**
 *  Flag ceremony of a village (owner requests 2026-10-07, cahier §6.2 "Pacifier une zone"), DA_FlagCeremonyRules:
 *  on the stone pile in the middle of the square, F raises the British flag while the officiant sounds the trumpet;
 *  the villagers leave their routine and stand in a ring around the officiant, those of the houses bringing the
 *  Tidal Relic of their house, which they lay around the officiant once the flag is up. The zone is then pacified
 *  (bonus of the expedition, once) and the shrine of the village opens.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UFlagCeremonyRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Stone pile the pole stands in, with its collision (SM_Temple_Debris_StonePile) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site")
	TObjectPtr<UStaticMesh> CairnMesh;

	/** Pole, pivot at its foot on its axis (SM_FlagCeremony_Pole) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site")
	TObjectPtr<UStaticMesh> PoleMesh;

	/** Flag, pivot at the bottom of its hoist on the pole axis, the fly along +X (SM_FlagCeremony_Flag) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site")
	TObjectPtr<UStaticMesh> FlagMesh;

	/** Foot of the pole from the site (cm): in the middle of the stones */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site")
	FVector PoleOffset = FVector(0.0f, 0.0f, 5.0f);

	/** Bottom of the flag above the foot of the pole when the ceremony starts (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site", meta=(ClampMin="0", Units="cm"))
	float FlagStartHeight = 90.0f;

	/** Bottom of the flag above the foot of the pole once raised (cm): its top under the gilded ball */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Site", meta=(ClampMin="0", Units="cm"))
	float FlagEndHeight = 345.0f;

	/** Ceremony area around the stones (cm): a player standing there may start it with F */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony", meta=(ClampMin="50", Units="cm"))
	float Radius = 450.0f;

	/** A player more than this above or below the site is not in the area (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony", meta=(ClampMin="0", Units="cm"))
	float HalfHeight = 300.0f;

	/** Time the flag takes to go up (s): 60 by the owner's request of 2026-10-07; the sermon of cahier §6.2 is 20 s */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony", meta=(ClampMin="1", Units="s"))
	float RitualSeconds = 60.0f;

	/** The officiant may not move farther than this from where he started (cm, cahier §6.2: 10 cm against jitter) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony", meta=(ClampMin="0", Units="cm"))
	float MoveTolerance = 10.0f;

	/** A blow taken by the officiant interrupts (cahier §6.2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony")
	bool bInterruptOnDamage = true;

	/** Seconds between two server checks of the officiant */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony", meta=(ClampMin="0.02", Units="s"))
	float CheckInterval = 0.1f;

	/** The pacification gives the bonus of the zone to the expedition (cahier §6.1, once per zone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony")
	bool bGrantZoneBonus = true;

	/** Houses whose kit waits for the flag (the shrine) open their doors for good once it is up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ceremony")
	bool bOpenSealedHouses = true;

	/** Villagers stand this far around the officiant (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villagers", meta=(ClampMin="100", Units="cm"))
	float GatherRadius = 380.0f;

	/** They lay their relics this far around the officiant once the flag is up (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villagers", meta=(ClampMin="60", Units="cm"))
	float OfferRadius = 150.0f;

	/** No villager stands and no relic lies closer than this to the middle of the stones (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villagers", meta=(ClampMin="0", Units="cm"))
	float CairnClearance = 120.0f;

	/** Speed of the villagers hurrying to the ceremony (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villagers", meta=(ClampMin="50", Units="cm/s"))
	float GatherSpeed = 300.0f;

	/** Time they stay after laying their relic, cheering, before they go back home (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villagers", meta=(ClampMin="0", Units="s"))
	float CelebrateSeconds = 8.0f;

	/** Villages within this distance of the site also come, beside its own (cm); 0: its village only */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villagers", meta=(ClampMin="0", Units="cm"))
	float NeighbourVillageRadius = 0.0f;

	/** Context added for a local player standing in an idle area: F to the ceremony action, above the interaction
	 *  (IMC_FlagCeremony; the action is bound by UTrumpetEmoteComponent) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputMappingContext> CeremonyMapping;

	/** Its priority: above the interaction (2) and the emotes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	int32 MappingPriority = 10;

	/** Seconds between two looks at the local pawns */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(ClampMin="0.02", Units="s"))
	float LocalCheckInterval = 0.1f;

	/** Prompt next to the key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FText PromptText = NSLOCTEXT("FlagCeremony", "Prompt", "Hisser les couleurs");

	/** Title over the progress bar */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FText TitleText = NSLOCTEXT("FlagCeremony", "Title", "Cérémonie des couleurs");

	/** Reminder for the officiant under the bar */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FText HoldStillText = NSLOCTEXT("FlagCeremony", "HoldStill", "Ne bougez pas");

	/** Message once the flag is up (owner 2026-10-07: the British crown claims the land) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FText DoneText = NSLOCTEXT("FlagCeremony", "Done", "La Couronne britannique revendique ces terres");

	/** Line under DoneText with the reward ({0}: ClaimReward) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FText RewardText = NSLOCTEXT("FlagCeremony", "Reward", "+{0} couronnes");

	/** Crowns paid into the crew's treasury when the flag is up (owner 2026-10-07: "+1000"); 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Reward", meta=(ClampMin="0"))
	int32 ClaimReward = 1000;

	/** Message when the ceremony stops before the end */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FText InterruptedText = NSLOCTEXT("FlagCeremony", "Interrupted", "Cérémonie interrompue");

	/** Players this close to the site see the progress (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="0", Units="cm"))
	float HudRadius = 4000.0f;

	/** Time an end message stays on screen (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD", meta=(ClampMin="0", Units="s"))
	float MessageSeconds = 5.0f;
};
