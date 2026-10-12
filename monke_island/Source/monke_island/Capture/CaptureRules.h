// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InputCoreTypes.h"
#include "Capture/CaptureTypes.h"
#include "CaptureRules.generated.h"

class ACaptureCage;
class ACapturePrisoner;
class APawn;
class UAnimSequence;
class UStaticMesh;

/**
 *  Capture of prisoners (T-336, owner request 2026-10-11, docs/features/capture.md): a cage set on the beach for two
 *  minutes, steel handcuffs that take a subdued French soldier or Veilleur on a chain, the prisoner led to a cage and
 *  sold for 10 000 crowns to the crew's treasury. Every value of the feature (DA_CaptureRules); the meshes and clips of
 *  T-337 (art/export/Props/Capture, art/export/Characters/Capture) are set by scripts/editor/capture_ue.py.
 *  Never the villagers (the village of Motutapu is real; T-139), nor the giant crab, nor a player.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UCaptureRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UCaptureRules();

	// ------------------------------------------------------------------ who

	/** Kinds that can be cuffed (the French soldiers, the Veilleurs and the villagers: AFortSoldier, ATikiCreature and subclasses) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targets")
	TArray<TSoftClassPtr<APawn>> CapturableClasses;

	/** Kinds never cuffed, whatever the list above says (the giant crab, AGiantCrab) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targets")
	TArray<TSoftClassPtr<APawn>> NeverCapturableClasses;

	/** A target is subdued at or under this share of its health ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targets", meta=(ClampMin="0", ClampMax="1"))
	float SubduedHealthFraction = 0.35f;

	/** ... or when it is down (on the ground without a leg, crawling) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Targets")
	bool bDownIsSubdued = true;

	// ------------------------------------------------------------------ cuffing

	/** Reach of the handcuffs from the eyes to the target's body (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cuffing", meta=(ClampMin="0", Units="cm"))
	float CuffReachCm = 200.0f;

	/** The target lies within this angle of the view (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cuffing", meta=(ClampMin="1", ClampMax="180", Units="deg"))
	float CuffAimAngleDeg = 40.0f;

	/** The key is held this long to close the handcuffs (s); released before, nothing happens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cuffing", meta=(ClampMin="0.1", Units="s"))
	float CuffHoldSeconds = 1.5f;

	/** Server: latency tolerance added to the reach when it checks a request (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cuffing", meta=(ClampMin="0", Units="cm"))
	float ServerReachToleranceCm = 60.0f;

	// ------------------------------------------------------------------ prisoner

	/** Class of the prisoner that replaces the cuffed soldier or Veilleur (empty: ACapturePrisoner) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner")
	TSubclassOf<ACapturePrisoner> PrisonerClass;

	/** The prisoner stops this close to its jailer (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="50", Units="cm"))
	float FollowDistanceCm = 180.0f;

	/** Length of the chain (cm): pulled at its end, the prisoner goes as fast as it can */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="60", Units="cm"))
	float ChainLengthCm = 350.0f;

	/** Farther than this from its jailer the prisoner is let go (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="100", Units="cm"))
	float ReleaseDistanceCm = 1200.0f;

	/** Walk of the prisoner when the jailer stands still, and its fastest pace pulled by the chain (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="10", Units="cm/s"))
	float FollowWalkSpeed = 150.0f;

	/** See FollowWalkSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="10", Units="cm/s"))
	float FollowMaxSpeed = 650.0f;

	/** Pace of a prisoner without a leg, dragging itself (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="5", Units="cm/s"))
	float CrawlFollowSpeed = 70.0f;

	/** Seconds between two paths on the navigation mesh (straight walking without one) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="0.05", Units="s"))
	float FollowRepathSeconds = 0.4f;

	/** The prisoner never walks into water deeper than this (cm): it stops at the shore */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="0", Units="cm"))
	float MaxWadeDepthCm = 50.0f;

	/** A prisoner let go flees after this long without a jailer (s, 0 = never); anyone may take its chain meanwhile */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="0", Units="s"))
	float AbandonedEscapeSeconds = 90.0f;

	/** Pace of the prisoner walking into a cage (cm/s), and the longest it takes before it is put inside (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="10", Units="cm/s"))
	float CageEntrySpeed = 110.0f;

	/** See CageEntrySpeed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="0.2", Units="s"))
	float CageEntryMaxSeconds = 3.0f;

	/** The sold prisoner stays in the cage this long, then it is taken away (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner", meta=(ClampMin="0", Units="s"))
	float TakenAwaySeconds = 4.0f;

	/** No blow, shot nor fall hurts a prisoner (the bounty is not lost to a stray ball) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner")
	bool bPrisonerInvulnerable = true;

	/** Captive clips of the bean skeleton (the French soldiers, SK_Bean_Body_Skeleton): hands bound in front */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Animation")
	TObjectPtr<UAnimSequence> BeanCaptiveIdle;

	/** See BeanCaptiveIdle (A_BeanFR_Captive_Walk) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Animation")
	TObjectPtr<UAnimSequence> BeanCaptiveWalk;

	/** Speed the captive walk does not slide at (cm/s; T-337 manifest: 30.7) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Animation", meta=(ClampMin="1", Units="cm/s"))
	float CaptiveWalkNoSlideSpeed = 30.7f;

	/** Walking clip fully played from this speed (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Animation", meta=(ClampMin="1", Units="cm/s"))
	float WalkFullSpeed = 60.0f;

	/** Limits of the play rate of the walk */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Animation", meta=(ClampMin="0.1"))
	float MinPlayRate = 0.6f;

	/** See MinPlayRate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Animation", meta=(ClampMin="0.1"))
	float MaxPlayRate = 3.0f;

	/** Speed of the blend between standing and walking (1/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Animation", meta=(ClampMin="0.1"))
	float WalkBlendSpeed = 6.0f;

	/** Without captive clips for its skeleton (the Veilleurs): both hands pulled together in front of the belly after
	 *  its own clips (two bone IK); 0 leaves its arms as they are */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands", meta=(ClampMin="0", ClampMax="1"))
	float BoundHandsAlpha = 1.0f;

	/** Where the bound hands are, from the pelvis bone: forward, up, and the gap between the wrists (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands", meta=(Units="cm"))
	float BoundHandsForwardCm = 22.0f;

	/** See BoundHandsForwardCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands", meta=(Units="cm"))
	float BoundHandsUpCm = 10.0f;

	/** See BoundHandsForwardCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands", meta=(ClampMin="0", Units="cm"))
	float BoundHandsGapCm = 7.0f;

	/** Bones of the pose of the bound hands (the bean and the mob skeletons share these names) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands")
	FName PelvisBone = TEXT("pelvis");

	/** See PelvisBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands")
	FName UpperArmLBone = TEXT("upperarm_l");

	/** See PelvisBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands")
	FName LowerArmLBone = TEXT("lowerarm_l");

	/** See PelvisBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands")
	FName HandLBone = TEXT("hand_l");

	/** See PelvisBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands")
	FName UpperArmRBone = TEXT("upperarm_r");

	/** See PelvisBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands")
	FName LowerArmRBone = TEXT("lowerarm_r");

	/** See PelvisBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Bound hands")
	FName HandRBone = TEXT("hand_r");

	/** The closed handcuffs ride on the first of these bones the prisoner still has (T-337: once on hand_r, the wrists
	 *  stay together in the captive clips) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Handcuffs")
	TArray<FName> CuffBones;

	/** The closed handcuffs relative to that bone (cm from an unscaled bone; handcuffs_on_captive of the T-337 manifest,
	 *  to check in the editor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prisoner|Handcuffs")
	FTransform CuffsOnBone = FTransform(FRotator(-18.001, -18.379, 131.051), FVector(8.574, -5.949, -2.936));

	// ------------------------------------------------------------------ chain

	/** One link of the chain (SM_Chain_Link: pivot in the middle, long axis +X; the next link is turned 90 deg about X) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain")
	TObjectPtr<UStaticMesh> ChainLinkMesh;

	/** Distance between two links along the chain (cm; T-337: 3.2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain", meta=(ClampMin="0.5", Units="cm"))
	float ChainLinkPitchCm = 3.2f;

	/** Scale of a link */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain")
	FVector ChainLinkScale = FVector::OneVector;

	/** Most links drawn for one chain (pulled taut beyond, they spread) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain", meta=(ClampMin="2", ClampMax="400"))
	int32 MaxChainLinks = 120;

	/** A taut chain still sags this much in its middle (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain", meta=(ClampMin="0", Units="cm"))
	float ChainMinSagCm = 6.0f;

	/** Socket of the closed handcuffs the chain starts from */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain")
	FName CuffsChainSocket = TEXT("chain_anchor");

	/** Bone of the jailer's body the chain ends in, and the middle of its mitten from that bone (cm, turned with the bone;
	 *  leash_hand of the T-337 manifest) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain")
	FName JailerChainBone = TEXT("hand_l");

	/** See JailerChainBone */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain")
	FVector JailerChainOffset = FVector(0.483, -7.583, -0.033);

	/** In first person the jailer sees the chain come into its view at FirstPersonChainOffset (from its camera: X ahead,
	 *  Y right, Z up) rather than at the hand of its body, below the view */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain")
	bool bFirstPersonChainToView = true;

	/** See bFirstPersonChainToView */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Chain")
	FVector FirstPersonChainOffset = FVector(35.0, -22.0, -42.0);

	// ------------------------------------------------------------------ cage

	/** Cage actor set on the beach (BP_CaptureCage; empty: ACaptureCage) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	TSubclassOf<ACaptureCage> CageClass;

	/** Cage set down (SM_Capture_Cage: pivot under the middle of its floor, door on its +X face; sockets prisoner,
	 *  door_hinge, entry) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	TObjectPtr<UStaticMesh> CageMesh;

	/** Its door (SM_Capture_Cage_Door: pivot on the hinges, at the bottom) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	TObjectPtr<UStaticMesh> CageDoorMesh;

	/** How long a cage stays where it was set (s): 2 minutes (owner 2026-10-11) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(ClampMin="1", Units="s"))
	float CageLifetimeSeconds = 120.0f;

	/** When it goes, the cage comes back to the inventory of the one who set it, if there is room ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	bool bReturnCageToPlacer = true;

	/** ... else it lies folded on the ground where it stood (false: lost) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	bool bDropCageWhenNoRoom = true;

	/** The cage is set this far in front of the player (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(ClampMin="50", Units="cm"))
	float PlaceDistanceCm = 150.0f;

	/** Yaw of the cage from the player's (deg): 180 turns its door (+X) toward the player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(Units="deg"))
	float CageYawOffsetDeg = 180.0f;

	/** Half size of the cage set down (cm; SM_Capture_Cage is 120 x 160 x 190) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(Units="cm"))
	FVector CageHalfExtentCm = FVector(60.0, 80.0, 95.0);

	/** The room check starts this high above the ground (cm): a slope never fills it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(ClampMin="0", Units="cm"))
	float ClearanceLiftCm = 30.0f;

	/** No two cages closer than this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(ClampMin="0", Units="cm"))
	float MinCageSpacingCm = 400.0f;

	/** A prisoner led this close to a cage can be put in it with the interaction key on the cage (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(ClampMin="50", Units="cm"))
	float SellRadiusCm = 300.0f;

	/** A prisoner led this close to a cage goes in by itself (cm, 0 = only with the key) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(ClampMin="0", Units="cm"))
	float AutoSellRadiusCm = 0.0f;

	/** Opening of the door (deg, positive yaw opens it outward) and the time it takes (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(Units="deg"))
	float DoorOpenYawDeg = 100.0f;

	/** See DoorOpenYawDeg */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(ClampMin="0.05", Units="s"))
	float DoorSwingSeconds = 0.6f;

	/** Sockets of the cage mesh: hinges of the door, the prisoner's feet inside, the spot in front of the door */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	FName DoorHingeSocket = TEXT("door_hinge");

	/** See DoorHingeSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	FName PrisonerSocket = TEXT("prisoner");

	/** See DoorHingeSocket */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	FName EntrySocket = TEXT("entry");

	/** Box the crosshair finds the cage by (half size, cm): the bars alone are thin */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(Units="cm"))
	FVector InteractionHalfExtentCm = FVector(70.0, 90.0, 100.0);

	/** Time left shown over the cage on every machine */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage")
	bool bShowCageTimer = true;

	/** Height (cm) and size (cm) of that clock */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(Units="cm"))
	float CageTimerHeightCm = 220.0f;

	/** See CageTimerHeightCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cage", meta=(ClampMin="1"))
	float CageTimerSize = 28.0f;

	// ------------------------------------------------------------------ beach

	/** The cage stands on the beach: ground this high above the sea level (cm, UIslandWaterSubsystem) ... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(Units="cm"))
	float MinHeightAboveSeaCm = 0.0f;

	/** See MinHeightAboveSeaCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(Units="cm"))
	float MaxHeightAboveSeaCm = 300.0f;

	/** ... with the sea within this distance (cm): a seabed under the sea level is found around the spot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(ClampMin="100", Units="cm"))
	float MaxShoreDistanceCm = 3000.0f;

	/** A ground this far under the sea level is sea (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(ClampMin="0", Units="cm"))
	float ShoreMinDepthCm = 10.0f;

	/** Directions and rings of the probes that look for the sea around the spot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(ClampMin="1", ClampMax="32"))
	int32 ShoreProbeDirections = 8;

	/** See ShoreProbeDirections */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(ClampMin="1", ClampMax="8"))
	int32 ShoreProbeRings = 3;

	/** Steepest ground the cage stands on (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(ClampMin="0", ClampMax="60", Units="deg"))
	float MaxSlopeDeg = 20.0f;

	/** The ground under the spot is looked for this far above and below it (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(ClampMin="10", Units="cm"))
	float FloorTraceUpCm = 120.0f;

	/** See FloorTraceUpCm */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Beach", meta=(ClampMin="10", Units="cm"))
	float FloorTraceDownCm = 300.0f;

	// ------------------------------------------------------------------ gestures and items in hand

	/** The placing gesture (A_Bean_Cage_Place, 1.5 s) and its moment CagePlaced, when the cage stands (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gestures", meta=(ClampMin="0.1", Units="s"))
	float PlaceGestureSeconds = 1.5f;

	/** See PlaceGestureSeconds (T-337 manifest: 0.933) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gestures", meta=(ClampMin="0", Units="s"))
	float PlaceEventSeconds = 0.933f;

	/** The folded cage leaves the hand at CrateDown of the placing (s; T-337: 0.4) and lies at CrateDownSpot until the
	 *  cage stands (CagePlaced) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gestures", meta=(ClampMin="0", Units="s"))
	float PlaceCrateDownSeconds = 0.4f;

	/** Where the folded cage lies meanwhile, from the player's feet (cm; X ahead, Y right; cage_place.crate_down of T-337) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gestures")
	FTransform CrateDownSpot = FTransform(FRotator(0.0, -90.0, 0.0), FVector(40.0, 4.0, 0.0));

	/** The cuffing clip (A_Bean_Cuff, 1.2 s) is stretched over the hold of the key: its click lands as the server closes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gestures")
	bool bStretchCuffClipToHold = true;

	/** The open handcuffs leave the hand at this share of the cuffing (CuffsClosed: 0.7 of the 1.2 s clip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gestures", meta=(ClampMin="0", ClampMax="1"))
	float CuffsClosedShare = 0.583f;

	/** The movement keys do nothing during a gesture (the player stays where the server checked) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gestures")
	bool bHoldStillDuringGestures = true;

	/** Folded cage in the hand (SM_Capture_Cage_Folded, socket grip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TObjectPtr<UStaticMesh> FoldedCageMesh;

	/** Closed handcuffs, on the prisoner (SM_Steel_Handcuffs, sockets cuff_l, cuff_r, chain_anchor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TObjectPtr<UStaticMesh> HandcuffsMesh;

	/** Open handcuffs in the hand (SM_Steel_Handcuffs_Open, pivot on its socket grip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	TObjectPtr<UStaticMesh> OpenHandcuffsMesh;

	/** Bone of the player's body the items are held by */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	FName HeldBone = TEXT("hand_r");

	/** Socket of a held mesh put on that bone (its grip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	FName HeldGripSocket = TEXT("grip");

	/** The grip socket of the folded cage and of the open handcuffs relative to the bone (cm from an unscaled bone; the
	 *  T-337 manifest, as TorchRules::GripInHand, to check in the editor) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	FTransform FoldedCageInHand = FTransform(FRotator(-95.484, 0.0, 90.0), FVector(-0.483, -7.583, -0.033));

	/** See FoldedCageInHand */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Items")
	FTransform HandcuffsInHand = FTransform(FRotator(-95.484, 0.0, 90.0), FVector(-0.483, -7.583, -0.033));

	// ------------------------------------------------------------------ sale

	/** Crowns paid into the crew's treasury for a prisoner put in a cage (owner 2026-10-11: 10 000) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sale", meta=(ClampMin="0"))
	int32 SalePrice = 10000;

	// ------------------------------------------------------------------ input and network

	/** Keys of the items (the interaction key F): mapped only while they can be used, over the interaction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TArray<FKey> UseKeys;

	/** Priority of their context (above the character's contexts and the inventory's) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	int32 InputPriority = 6;

	/** The owner looks for a target or a spot this often (s): presentation and key, the server decides */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input", meta=(ClampMin="0.02", Units="s"))
	float LocalCheckSeconds = 0.15f;

	/** Server: capture requests (place, cuff, cancel) one player may send at once, and given back each second */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="1"))
	float RequestBurst = 6.0f;

	/** See RequestBurst */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.1"))
	float RequestsPerSecond = 3.0f;

	// ------------------------------------------------------------------ texts

	/** Prompts of the HUD ({0}: the key) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText PlacePromptText;

	/** Shown while the cage is being set down */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText PlacingText;

	/** {0}: the key, {1}: the kind of target */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText CuffPromptText;

	/** Shown while the key is held */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText CuffingText;

	/** Handcuffs on the wrists of a prisoner */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText HandcuffsInUseText;

	/** Leading a prisoner ({0}: the kind) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText LeadingText;

	/** Cuffed ({0}: the kind) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText CapturedText;

	/** The jailer lost its prisoner ({0}: the kind) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText ReleasedText;

	/** A prisoner let go fled ({0}: the kind) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText EscapedText;

	/** A sale, for the whole crew ({0}: who, {1}: the kind, {2}: crowns, {3}: the treasury after it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText SoldText;

	/** A cage set down, for the whole crew ({0}: who) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText CagePlacedText;

	/** A cage gone after its two minutes */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText CageGoneText;

	/** Interaction prompts: on a cage ({0}: crowns), on a prisoner let go */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText SellPromptText;

	/** See SellPromptText */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText TakeChainPromptText;

	/** Names of the kinds of prisoner */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText SoldierName;

	/** See SoldierName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText WatcherName;

	/** See SoldierName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText VillagerName;

	/** See SoldierName */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	FText OtherName;

	/** Why an action is refused ({0}: the health of the target in % for NotSubdued, {1}: the threshold in %) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Texts")
	TMap<ECaptureRefusal, FText> RefusalTexts;

	// ------------------------------------------------------------------ helpers

	/** True when pawns of this class can be cuffed: in CapturableClasses, not in NeverCapturableClasses */
	bool IsCapturableClass(const UClass* Class) const;

	/** Name of the first use key ("F"), empty without one */
	FText GetUseKeyText() const;

	/** Name of a kind of prisoner */
	FText GetKindName(ECaptiveKind Kind) const;

	/** Message of a refusal (empty for None); HealthFraction feeds NotSubdued */
	FText GetRefusalText(ECaptureRefusal Reason, float HealthFraction = 1.0f) const;
};
