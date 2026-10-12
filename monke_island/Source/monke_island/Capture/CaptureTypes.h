// monke_island

#pragma once

#include "CoreMinimal.h"
#include "CaptureTypes.generated.h"

class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;

/** Capture of prisoners (T-336): cages, handcuffs, prisoners, sales */
MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeCapture, Log, All);

/** Why a capture action is refused (the local check and the server say the same); each has its message in DA_CaptureRules */
UENUM(BlueprintType)
enum class ECaptureRefusal : uint8
{
	/** Nothing against it */
	None,
	/** Nothing to cuff in front of the player */
	NoTarget,
	/** A villager, the giant crab, a player: never */
	NotCapturable,
	/** Healthier than the threshold and standing */
	NotSubdued,
	/** Too far or out of sight */
	OutOfReach,
	/** These handcuffs are on the wrists of a prisoner already */
	HandcuffsInUse,
	/** The player leads a prisoner already */
	AlreadyLeading,
	/** The player cannot do it now: dead, knocked out, swimming, in the air */
	CarrierUnable,
	/** The cage is set on the beach only */
	NotOnBeach,
	/** The ground is too steep */
	TooSteep,
	/** Something stands where the cage would go */
	NoRoom,
	/** Another cage stands too close */
	CageTooClose,
	/** Too many requests at once */
	TooFast,
	/** Someone else is cuffing the target, or a gesture plays already */
	Busy
};

/** Gesture of a player with a capture item, played on every machine */
UENUM(BlueprintType)
enum class ECaptureGesture : uint8
{
	None,
	/** Sets the folded cage down and opens it (A_Bean_Cage_Place) */
	PlaceCage,
	/** Holds the handcuffs on the wrists of the target (A_Bean_Cuff) */
	Cuff
};

/** Where a prisoner is */
UENUM(BlueprintType)
enum class ECaptiveState : uint8
{
	/** On the chain of a jailer, following it */
	Led,
	/** Let go (jailer dead, in the water, too far): still handcuffed, it waits where it stands */
	Abandoned,
	/** Walking into a cage */
	EnteringCage,
	/** Sold, behind the closed door, until it is taken away */
	Caged
};

/** Why a led prisoner was let go (server) */
UENUM(BlueprintType)
enum class ECaptiveRelease : uint8
{
	None,
	/** The jailer left the game or its pawn was replaced */
	JailerGone,
	JailerDead,
	/** The jailer swims */
	JailerInWater,
	/** The chain could not keep up */
	TooFar
};

/** Kind of prisoner, for the texts */
UENUM(BlueprintType)
enum class ECaptiveKind : uint8
{
	Other,
	/** A French soldier of a fort (prisoner for a ransom) */
	Soldier,
	/** A Veilleur (tiki warrior: bounty) */
	Watcher,
	/** A villager (ATikiVillager, capturable since the owner's decision of 2026-10-11) */
	Villager
};

/** A gesture started by the server, replicated as a whole (a new Serial replays it) */
USTRUCT()
struct FCaptureGestureRep
{
	GENERATED_BODY()

	/** The gesture, None once it ended or was cut */
	UPROPERTY()
	ECaptureGesture Gesture = ECaptureGesture::None;

	/** Bumped at each start */
	UPROPERTY()
	uint8 Serial = 0;

	/** Server world time of the start (s) */
	UPROPERTY()
	double StartServerTime = 0.0;

	/** Duration as played (s) */
	UPROPERTY()
	float Seconds = 0.0f;
};

/**
 *  What a prisoner looks like, copied once from the soldier or the Veilleur it replaces (server) and replicated once:
 *  its mesh, materials, place of the mesh, size, the bones it lost, and the clips it walks with
 */
USTRUCT()
struct FCapturePrisonerLook
{
	GENERATED_BODY()

	/** Kind of prisoner (texts) */
	UPROPERTY()
	ECaptiveKind Kind = ECaptiveKind::Other;

	/** Body (the uniform of the soldier, the Veilleur's mesh) */
	UPROPERTY()
	TObjectPtr<USkeletalMesh> Mesh;

	/** Materials by slot, assets only (a material made at runtime gives its parent); empty slots keep the mesh's */
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	/** Mesh placed relative to the capsule (location, rotation, scale) */
	UPROPERTY()
	FVector MeshLocation = FVector::ZeroVector;

	/** See MeshLocation */
	UPROPERTY()
	FRotator MeshRotation = FRotator::ZeroRotator;

	/** See MeshLocation */
	UPROPERTY()
	FVector MeshScale = FVector::OneVector;

	/** Scale of the actor */
	UPROPERTY()
	FVector ActorScale = FVector::OneVector;

	/** Capsule (cm) */
	UPROPERTY()
	float CapsuleRadius = 30.0f;

	/** See CapsuleRadius */
	UPROPERTY()
	float CapsuleHalfHeight = 55.0f;

	/** Bones it lost (a hat knocked off, an arm torn off): hidden on every machine */
	UPROPERTY()
	TArray<FName> HiddenBones;

	/** Standing and walking clips (the captive clips when they fit its skeleton, else its own) */
	UPROPERTY()
	TObjectPtr<UAnimSequence> Idle;

	/** See Idle */
	UPROPERTY()
	TObjectPtr<UAnimSequence> Walk;

	/** Crawling clip of a prisoner without a leg */
	UPROPERTY()
	TObjectPtr<UAnimSequence> Crawl;

	/** Speeds the walk and crawl clips do not slide at (cm/s) */
	UPROPERTY()
	float WalkNoSlideSpeed = 45.0f;

	/** See WalkNoSlideSpeed */
	UPROPERTY()
	float CrawlNoSlideSpeed = 18.0f;

	/** On the ground without a leg: it crawls */
	UPROPERTY()
	bool bCrawling = false;

	/** Idle and Walk are the captive clips (hands bound in the clip): no procedural pose of the arms */
	UPROPERTY()
	bool bCaptiveClips = false;
};

/** One line of the HUD for the local player: a prompt with its key, or a short message */
struct FCaptureHudLine
{
	/** What to read */
	FText Text;

	/** Share of a hold done (0 to 1), < 0 without a bar */
	float Progress = -1.0f;

	/** A refusal or a loss rather than an action */
	bool bWarning = false;
};
