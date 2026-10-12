// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Creatures/TikiCombat.h"
#include "TikiShredDemo.generated.h"

class UAnimSequenceBase;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTikiRules;
struct FStreamableHandle;

/** One baked shredding demo of a mob (T-168): SK_Demo_<Mob>_<Clip> of the combat-dismemberment pack, its own skeleton
 *  of fragments (root, frag_<Part>) and its baked animation that replays the Blender detachment exactly */
USTRUCT(BlueprintType)
struct FTikiShredDemoClip
{
	GENERATED_BODY()

	/** Reaction this demo replaces (Musket_Burst, a Sabre_* cut) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	ETikiCombatClip Clip = ETikiCombatClip::None;

	/** Mesh of the fragments (SK_Demo_<Mob>_<Clip>, Characters/<Mob>/ShredDemos), loaded in the background */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	/** Baked animation of the fragments, on the demo's own skeleton (imported with the mesh) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	TSoftObjectPtr<UAnimSequenceBase> Anim;

	/** Time of the contact in the animation (s): frame 12 at 30 fps in the pack; the demo starts there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo", meta=(ClampMin="0", Units="s"))
	float ContactTime = 0.4f;

	/** Fragment bones that leave the body (frag_<Part>, the pack's detached_parts): a jet of blood at the contact,
	 *  a splat where they land */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	TArray<FName> SeveredBones;
};

/**
 *  Baked shredding demos of a mob (DA_<Mob>_ShredDemoSet, T-168). When a lethal reaction has a demo here, the mob and
 *  its corpse are replaced by the demo played once at the mob's place, its fragments held on the ground, then faded
 *  (ATikiShredDemo). Filled by scripts/character/set_shred_demos.py after scripts/character/import_shred_demos.py.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UTikiShredDemoSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** The demos, one per clip */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	TArray<FTikiShredDemoClip> Demos;

	/** Fragment of the held weapon in the demos (frag_Weapon of the pack): hidden when the mob lets its weapon fall as a
	 *  pickup at its death (DroppedWeaponClass, T-161), so the weapon is not seen twice; None keeps it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShredDemo")
	FName WeaponFragmentBone = TEXT("frag_Weapon");

	/** Demo of a clip with a mesh and an animation set, nullptr when none */
	const FTikiShredDemoClip* Find(ETikiCombatClip Clip) const;

	/** Bit (1 << clip) set for each clip with a usable demo */
	uint32 AvailableMask() const;

	/** Starts loading the meshes, the animations and the burst effect of the rules in the background (once; nothing on
	 *  a dedicated server) */
	void Preload(const UObject* WorldContext, const UTikiRules* Rules);

private:

	/** Background load of the demos */
	TSharedPtr<FStreamableHandle> PreloadHandle;
};

/** Pure rules of the shredding demos, tested without a world */
namespace TikiShredDemo
{
	/** What decides whether a lethal blow is replaced by a demo, and which one */
	struct FChoice
	{
		/** DA_TikiRules bShredDemos */
		bool bEnabled = false;
		/** Lethal reaction picked by T-150 (None when the mob has no reaction clip for the blow) */
		ETikiCombatClip Clip = ETikiCombatClip::None;
		/** UTikiShredDemoSet::AvailableMask of the mob, 0 without a set */
		uint32 DemoMask = 0;
		/** The blow cuts (cutlass move with bCut) */
		bool bBlowCuts = false;
		/** DA_TikiRules bShredDemoSabreNeedsCut: a sabre demo only for a blow that cuts */
		bool bSabreNeedsCut = false;
		/** Bones cut off the body before this blow (a demo shows the whole body) */
		int32 CutsBefore = 0;
		/** A gun shot (musket, pistol, shotgun) */
		bool bFirearm = false;
		/** Distance of the shot (cm), 0 when unknown */
		float Distance = 0.0f;
		/** Close range of the weapon (cm, the shotgun's 5 m), 0 for a weapon without one */
		float CloseDistance = 0.0f;
		/** DA_TikiRules bCloseShotBurstDemo: a lethal shot within CloseDistance always takes the Musket_Burst demo */
		bool bCloseShotBurstDemo = true;
	};

	/** Clips a demo can replace: the burst and the four sabre cuts (an impact or a fall keeps its reaction) */
	inline bool IsShredClip(ETikiCombatClip Clip)
	{
		return Clip == ETikiCombatClip::MusketBurst || TikiCombat::IsSabre(Clip);
	}

	/** A lethal shot within the close range of its weapon (the shotgun under 5 m) */
	inline bool IsCloseShot(const FChoice& Choice)
	{
		return Choice.bFirearm && Choice.CloseDistance > 0.0f && Choice.Distance > 0.0f && Choice.Distance <= Choice.CloseDistance;
	}

	/** Demo replacing a lethal blow: Musket_Burst for a close shot (whatever reaction T-150 picked), else the demo of
	 *  the picked reaction (the burst, the sabre cut of the blow's sense). None keeps the T-150 / T-155 / T-167
	 *  behaviour (no demo for the mob, a body already cut, a sabre blow that does not cut when the rules want it) */
	inline ETikiCombatClip Choose(const FChoice& Choice)
	{
		if (!Choice.bEnabled || Choice.CutsBefore > 0)
		{
			return ETikiCombatClip::None;
		}
		if (Choice.bCloseShotBurstDemo && IsCloseShot(Choice) && TikiCombat::Has(Choice.DemoMask, ETikiCombatClip::MusketBurst))
		{
			return ETikiCombatClip::MusketBurst;
		}
		const bool bCutOk = !TikiCombat::IsSabre(Choice.Clip) || !Choice.bSabreNeedsCut || Choice.bBlowCuts;
		return IsShredClip(Choice.Clip) && TikiCombat::Has(Choice.DemoMask, Choice.Clip) && bCutOk ? Choice.Clip : ETikiCombatClip::None;
	}

	/** Fragment a demo hides: the weapon's, when the mob drops its weapon as a pickup (T-161: the pickup is the weapon,
	 *  the demo does not throw a second one); None otherwise (a villager's basket flies with the body) */
	inline FName HiddenFragment(FName WeaponFragmentBone, bool bDropsWeapon)
	{
		return bDropsWeapon ? WeaponFragmentBone : NAME_None;
	}

	/** Time the animation starts at on a machine: the contact, plus the age of the reaction there (a client receives
	 *  it later), within the animation */
	inline float StartTime(float ContactTime, float Age, float Length)
	{
		return FMath::Clamp(ContactTime + FMath::Max(0.0f, Age), 0.0f, FMath::Max(0.0f, Length));
	}

	/** Yaw of the demo actor (degrees): the victim faces where the blow came from, as in the pack where it is turned
	 *  180 degrees toward the bean; FallbackYaw (the mob's) without a horizontal blow */
	inline float FacingYaw(const FVector& BlowDirection, float FallbackYaw)
	{
		const FVector Flat = BlowDirection.GetSafeNormal2D();
		return Flat.IsNearlyZero() ? FallbackYaw : FRotator::NormalizeAxis(static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-Flat.Y, -Flat.X))));
	}

	/** Phase of a demo Elapsed seconds after it started at StartTime */
	enum class EPhase : uint8
	{
		/** The baked animation plays */
		Play,
		/** The last frame is held on the ground */
		Hold,
		/** It fades (End = Fade) */
		Fade,
		/** Gone (Fade) */
		Done,
		/** Kept as pieces (End = Keep) */
		Kept
	};

	inline EPhase Phase(float Elapsed, float StartTime, float Length, float HoldTime, float FadeTime, bool bKeep)
	{
		const float PlayLeft = FMath::Max(0.0f, Length - StartTime);
		if (Elapsed < PlayLeft)
		{
			return EPhase::Play;
		}
		if (Elapsed < PlayLeft + HoldTime)
		{
			return EPhase::Hold;
		}
		if (bKeep)
		{
			return EPhase::Kept;
		}
		return Elapsed < PlayLeft + HoldTime + FadeTime ? EPhase::Fade : EPhase::Done;
	}

	/** Share of the fade done (0 at its start, 1 at its end) */
	inline float FadeAlpha(float Elapsed, float StartTime, float Length, float HoldTime, float FadeTime)
	{
		const float FadeStart = FMath::Max(0.0f, Length - StartTime) + HoldTime;
		return FadeTime <= 0.0f ? (Elapsed >= FadeStart ? 1.0f : 0.0f) : FMath::Clamp((Elapsed - FadeStart) / FadeTime, 0.0f, 1.0f);
	}
}

/**
 *  Baked shredding demo of a dead tiki (T-168), purely cosmetic and local: every machine but a dedicated server spawns
 *  its own from the replicated combat reaction (FTikiCombatReactionState::bShredDemo). The fragments play the baked
 *  animation once from the contact, the blood of T-153 leaves the severed fragments, the last frame is held on the
 *  ground for ShredDemoHoldTime, then they fade (or stay as pieces). No collision, no tick: timers only.
 */
UCLASS(NotBlueprintable)
class MONKE_ISLAND_API ATikiShredDemo : public AActor
{
	GENERATED_BODY()

public:

	ATikiShredDemo();

	/**
	 *  Spawns a demo at a mob's place: ground point under FeetLocation, yaw from the blow (FacingYaw), the mob mesh's
	 *  Scale, the animation from its contact plus Age (s since the server started the reaction). nullptr on a dedicated
	 *  server, without a world or when the demo's assets cannot load. Ignore: the mob, left out of the ground trace.
	 *  bFreshBlood: the blood of the contact is spawned (false for a reaction seen late). HiddenBone: a fragment left
	 *  out (TikiShredDemo::HiddenFragment, the weapon dropped as a pickup), None for none.
	 */
	static ATikiShredDemo* Spawn(UWorld* World, UTikiRules* Rules, const FTikiShredDemoClip& Demo, const FVector& FeetLocation, float Yaw,
		const FVector& Scale, float Age, const AActor* Ignore, bool bFreshBlood, FName HiddenBone = NAME_None);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:

	/** The fragments, playing the baked animation */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> Fragments;

private:

	/** Rules read for the hold, the fade and the blood */
	UPROPERTY(Transient)
	TObjectPtr<UTikiRules> Rules;

	/** Fragment bones that leave the body (blood) */
	TArray<FName> SeveredBones;

	/** Animation time it started at, length of the animation (s) */
	float StartTime = 0.0f;
	float Length = 0.0f;

	/** World time it was spawned at */
	double SpawnTime = 0.0;

	/** Location of the fragments before the fade (relative to the actor) */
	FVector RestRelativeLocation = FVector::ZeroVector;

	FTimerHandle LandTimer;
	FTimerHandle EndTimer;
	FTimerHandle FadeTimer;

	/** Jets out of the severed fragments, chunks at the body (contact) */
	void SpawnContactBlood();

	/** Splats where the severed fragments lie (end of the animation) */
	void SpawnLandBlood();

	/** End of the hold: fade, or kept as pieces */
	void EndHold();

	/** Starts the fade (sink and Fade parameter), then destroys */
	void StartFade();

	/** One step of the fade */
	void StepFade();

	/** Demos kept as pieces (Keep), oldest first, by world */
	static TArray<TWeakObjectPtr<ATikiShredDemo>> Kept;
};
