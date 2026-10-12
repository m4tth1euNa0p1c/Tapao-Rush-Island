// monke_island

#include "Capture/CaptureRules.h"
#include "GameFramework/Pawn.h"

UCaptureRules::UCaptureRules()
{
	// by their native classes (no include of the mob domains here): a Blueprint of the list may be added in the asset
	CapturableClasses = {
		TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Script/monke_island.FortSoldier"))),
		TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Script/monke_island.TikiCreature")))
	};
	// The villagers (ATikiVillager, a subclass of ATikiCreature) are capturable since the owner's decision of 2026-10-11
	// (T-336), which lifts the T-139 exclusion; the boss stays out
	NeverCapturableClasses = {
		TSoftClassPtr<APawn>(FSoftObjectPath(TEXT("/Script/monke_island.GiantCrab")))
	};
	CuffBones = { TEXT("hand_r"), TEXT("hand_l"), TEXT("lowerarm_r"), TEXT("spine_02") };
	UseKeys = { EKeys::F };

	PlacePromptText = NSLOCTEXT("Capture", "PlacePrompt", "{0} : poser la cage (2 min)");
	PlacingText = NSLOCTEXT("Capture", "Placing", "Pose de la cage…");
	CuffPromptText = NSLOCTEXT("Capture", "CuffPrompt", "Maintenir {0} : menotter le {1}");
	CuffingText = NSLOCTEXT("Capture", "Cuffing", "Menottage…");
	HandcuffsInUseText = NSLOCTEXT("Capture", "HandcuffsInUse", "Les menottes sont au poignet de votre prisonnier");
	LeadingText = NSLOCTEXT("Capture", "Leading", "Prisonnier en laisse ({0}) : amenez-le à une cage sur la plage");
	CapturedText = NSLOCTEXT("Capture", "Captured", "Prisonnier menotté ({0}) : amenez-le à une cage sur la plage");
	ReleasedText = NSLOCTEXT("Capture", "Released", "Votre prisonnier ({0}) vous a échappé : il attend, menotté, qu'on reprenne sa chaîne");
	EscapedText = NSLOCTEXT("Capture", "Escaped", "Un prisonnier abandonné ({0}) s'est enfui");
	SoldText = NSLOCTEXT("Capture", "Sold", "{0} a mis un {1} en cage : +{2} c au trésor (trésor : {3} c)");
	CagePlacedText = NSLOCTEXT("Capture", "CagePlaced", "{0} a posé une cage sur la plage (2 min)");
	CageGoneText = NSLOCTEXT("Capture", "CageGone", "Une cage est repartie : ses deux minutes sont écoulées");
	SellPromptText = NSLOCTEXT("Capture", "SellPrompt", "Mettre le prisonnier en cage (+{0} c)");
	TakeChainPromptText = NSLOCTEXT("Capture", "TakeChainPrompt", "Prendre la chaîne du prisonnier");
	SoldierName = NSLOCTEXT("Capture", "Soldier", "soldat");
	WatcherName = NSLOCTEXT("Capture", "Watcher", "Veilleur");
	VillagerName = NSLOCTEXT("Capture", "Villager", "villageois");
	OtherName = NSLOCTEXT("Capture", "Other", "prisonnier");

	RefusalTexts.Add(ECaptureRefusal::NoTarget, NSLOCTEXT("Capture", "NoTarget", "Personne à menotter devant vous"));
	RefusalTexts.Add(ECaptureRefusal::NotCapturable, NSLOCTEXT("Capture", "NotCapturable", "Celui-là ne se met pas aux fers"));
	RefusalTexts.Add(ECaptureRefusal::NotSubdued, NSLOCTEXT("Capture", "NotSubdued", "Pas encore maîtrisé : santé {0} % (il faut {1} % ou moins, ou à terre)"));
	RefusalTexts.Add(ECaptureRefusal::OutOfReach, NSLOCTEXT("Capture", "OutOfReach", "Approchez-vous : trop loin ou hors de vue"));
	RefusalTexts.Add(ECaptureRefusal::HandcuffsInUse, NSLOCTEXT("Capture", "HandcuffsInUseRefusal", "Ces menottes tiennent déjà un prisonnier"));
	RefusalTexts.Add(ECaptureRefusal::AlreadyLeading, NSLOCTEXT("Capture", "AlreadyLeading", "Vous menez déjà un prisonnier"));
	RefusalTexts.Add(ECaptureRefusal::CarrierUnable, NSLOCTEXT("Capture", "CarrierUnable", "Impossible maintenant : debout, sur la terre ferme"));
	RefusalTexts.Add(ECaptureRefusal::NotOnBeach, NSLOCTEXT("Capture", "NotOnBeach", "La cage se pose sur la plage, près de la mer"));
	RefusalTexts.Add(ECaptureRefusal::TooSteep, NSLOCTEXT("Capture", "TooSteep", "Le sol est trop en pente pour la cage"));
	RefusalTexts.Add(ECaptureRefusal::NoRoom, NSLOCTEXT("Capture", "NoRoom", "Pas assez de place pour la cage"));
	RefusalTexts.Add(ECaptureRefusal::CageTooClose, NSLOCTEXT("Capture", "CageTooClose", "Une autre cage est trop proche"));
	RefusalTexts.Add(ECaptureRefusal::TooFast, NSLOCTEXT("Capture", "TooFast", "Doucement…"));
	RefusalTexts.Add(ECaptureRefusal::Busy, NSLOCTEXT("Capture", "Busy", "Quelqu'un s'en occupe déjà"));
}

bool UCaptureRules::IsCapturableClass(const UClass* Class) const
{
	if (!Class)
	{
		return false;
	}
	// the classes of the lists are native or loaded with this asset: Get() never loads anything
	for (const TSoftClassPtr<APawn>& Never : NeverCapturableClasses)
	{
		if (const UClass* Excluded = Never.Get(); Excluded && Class->IsChildOf(Excluded))
		{
			return false;
		}
	}
	for (const TSoftClassPtr<APawn>& Allowed : CapturableClasses)
	{
		if (const UClass* Included = Allowed.Get(); Included && Class->IsChildOf(Included))
		{
			return true;
		}
	}
	return false;
}

FText UCaptureRules::GetUseKeyText() const
{
	for (const FKey& Key : UseKeys)
	{
		if (Key.IsValid())
		{
			return Key.GetDisplayName();
		}
	}
	return FText::GetEmpty();
}

FText UCaptureRules::GetKindName(ECaptiveKind Kind) const
{
	switch (Kind)
	{
	case ECaptiveKind::Soldier:	return SoldierName;
	case ECaptiveKind::Watcher:	return WatcherName;
	case ECaptiveKind::Villager:	return VillagerName;
	default:					return OtherName;
	}
}

FText UCaptureRules::GetRefusalText(ECaptureRefusal Reason, float HealthFraction) const
{
	const FText* Text = Reason == ECaptureRefusal::None ? nullptr : RefusalTexts.Find(Reason);
	if (!Text)
	{
		return FText::GetEmpty();
	}
	if (Reason == ECaptureRefusal::NotSubdued)
	{
		const int32 Health = FMath::RoundToInt32(FMath::Clamp(FMath::IsFinite(HealthFraction) ? HealthFraction : 1.0f, 0.0f, 1.0f) * 100.0f);
		const int32 Threshold = FMath::RoundToInt32(FMath::Clamp(SubduedHealthFraction, 0.0f, 1.0f) * 100.0f);
		return FText::Format(*Text, FText::AsNumber(Health), FText::AsNumber(Threshold));
	}
	return *Text;
}
