// monke_island

#include "Capture/CaptureTargets.h"
#include "Capture/CaptureRules.h"
#include "Creatures/TikiCreature.h"
#include "Creatures/TikiVillager.h"
#include "Fort/FortSoldier.h"
#include "Fort/FortSoldierRules.h"
#include "Player/IslanderHealthComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	/** Most lost bones replicated for one prisoner (bounded, memory.md MEM-08): a hidden parent hides its children */
	constexpr int32 MaxHiddenBones = 32;
}

ECaptiveKind CaptureTargets::KindOf(const AActor* Actor)
{
	if (Actor && Actor->IsA<AFortSoldier>())
	{
		return ECaptiveKind::Soldier;
	}
	if (Actor && Actor->IsA<ATikiVillager>())
	{
		return ECaptiveKind::Villager;
	}
	if (Actor && Actor->IsA<ATikiCreature>())
	{
		return ECaptiveKind::Watcher;
	}
	return ECaptiveKind::Other;
}

bool CaptureTargets::IsDown(const AActor* Actor)
{
	if (const AFortSoldier* Soldier = Cast<AFortSoldier>(Actor))
	{
		return Soldier->IsCrawling();
	}
	if (const ATikiCreature* Tiki = Cast<ATikiCreature>(Actor))
	{
		return Tiki->IsCrawling();
	}
	return false;
}

float CaptureTargets::HealthFractionOf(const AActor* Actor, bool& bOutAlive)
{
	const UIslanderHealthComponent* Health = Actor ? Actor->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	if (!Health)
	{
		bOutAlive = IsValid(Actor);
		return 1.0f;
	}
	bOutAlive = !Health->IsDead();
	return Health->GetHealthFraction();
}

bool CaptureTargets::IsCapturable(const APawn* Pawn, const UCaptureRules& Rules)
{
	// a player never, whatever pawn it holds
	if (!IsValid(Pawn) || Pawn->IsPlayerControlled() || !Rules.IsCapturableClass(Pawn->GetClass()))
	{
		return false;
	}
	bool bAlive = false;
	HealthFractionOf(Pawn, bAlive);
	return bAlive;
}

void CaptureTargets::CopyLook(ACharacter* Original, const UCaptureRules& Rules, FCapturePrisonerLook& OutLook)
{
	OutLook = FCapturePrisonerLook();
	if (!Original)
	{
		return;
	}
	OutLook.Kind = KindOf(Original);
	OutLook.ActorScale = Original->GetActorScale3D();
	if (const UCapsuleComponent* Capsule = Original->GetCapsuleComponent())
	{
		OutLook.CapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
		OutLook.CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}

	USkeletalMeshComponent* Mesh = Original->GetMesh();
	USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (Mesh && Asset)
	{
		OutLook.Mesh = Asset;
		OutLook.MeshLocation = Mesh->GetRelativeLocation();
		OutLook.MeshRotation = Mesh->GetRelativeRotation();
		OutLook.MeshScale = Mesh->GetRelativeScale3D();
		const int32 NumMaterials = FMath::Min(Mesh->GetNumMaterials(), 16);
		OutLook.Materials.SetNum(NumMaterials);
		for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
		{
			// a material made at runtime (a tint, a flash of a hit) is not an asset other machines can load: its parent
			UMaterialInterface* Material = Mesh->GetMaterial(Slot);
			while (const UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material))
			{
				Material = Dynamic->Parent;
			}
			OutLook.Materials[Slot] = Material && Material->IsAsset() ? Material : nullptr;
		}
		// the bones it lost (a hat knocked off, a limb torn off): the top bone of each hidden branch
		const FReferenceSkeleton& Bones = Asset->GetRefSkeleton();
		for (int32 Bone = 0; Bone < Bones.GetNum() && OutLook.HiddenBones.Num() < MaxHiddenBones; ++Bone)
		{
			const int32 Parent = Bones.GetParentIndex(Bone);
			if (Mesh->IsBoneHidden(Bone) && (Parent == INDEX_NONE || !Mesh->IsBoneHidden(Parent)))
			{
				OutLook.HiddenBones.Add(Bones.GetBoneName(Bone));
			}
		}
	}

	// its own clips: the walk of a soldier of the fort, the locomotion of a Veilleur
	if (const AFortSoldier* Soldier = Cast<AFortSoldier>(Original))
	{
		if (const UFortSoldierRules* SoldierRules = Soldier->Rules)
		{
			OutLook.Idle = SoldierRules->IdleGuard;
			OutLook.Walk = SoldierRules->Walk ? SoldierRules->Walk : SoldierRules->Patrol;
			OutLook.WalkNoSlideSpeed = SoldierRules->WalkNoSlideSpeed;
			OutLook.Crawl = SoldierRules->LeglessCrawlLoop;
			OutLook.CrawlNoSlideSpeed = SoldierRules->CrawlNoSlideSpeed;
		}
		OutLook.bCrawling = Soldier->IsCrawling();
	}
	else if (const ATikiCreature* Tiki = Cast<ATikiCreature>(Original))
	{
		OutLook.Idle = Cast<UAnimSequence>(Tiki->Anims.Idle.Get());
		OutLook.Walk = Cast<UAnimSequence>(Tiki->Anims.Walk.Get());
		if (Tiki->Anims.WalkSpeed > 0.0f)
		{
			OutLook.WalkNoSlideSpeed = Tiki->Anims.WalkSpeed;
		}
		OutLook.Crawl = Cast<UAnimSequence>(Tiki->Anims.LeglessCrawl.Get());
		if (Tiki->Anims.CrawlSpeed > 0.0f)
		{
			OutLook.CrawlNoSlideSpeed = Tiki->Anims.CrawlSpeed;
		}
		OutLook.bCrawling = Tiki->IsCrawling();
	}

	// the captive clips (hands bound, T-337) when they are made for its skeleton: the French soldiers share the bean's
	const USkeleton* Skeleton = Asset ? Asset->GetSkeleton() : nullptr;
	if (Skeleton && Rules.BeanCaptiveIdle && Rules.BeanCaptiveIdle->GetSkeleton() == Skeleton)
	{
		OutLook.Idle = Rules.BeanCaptiveIdle;
		if (Rules.BeanCaptiveWalk && Rules.BeanCaptiveWalk->GetSkeleton() == Skeleton)
		{
			OutLook.Walk = Rules.BeanCaptiveWalk;
			OutLook.WalkNoSlideSpeed = Rules.CaptiveWalkNoSlideSpeed;
		}
		OutLook.bCaptiveClips = true;
	}
	// without a crawl clip a legless prisoner shuffles on with its walk
	if (OutLook.bCrawling && !OutLook.Crawl)
	{
		OutLook.bCrawling = false;
	}
}
