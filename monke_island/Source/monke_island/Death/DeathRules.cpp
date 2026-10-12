// monke_island

#include "Death/DeathRules.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerState.h"

#define LOCTEXT_NAMESPACE "DeathRules"

UDeathRules::UDeathRules()
{
	// what the blow told (cahier §19: a short cause)
	CauseTexts.Add(EDeathCause::Fall, LOCTEXT("Fall", "Tombé de trop haut"));
	CauseTexts.Add(EDeathCause::Drowning, LOCTEXT("Drowning", "Noyé"));
	CauseTexts.Add(EDeathCause::Coconut, LOCTEXT("Coconut", "Assommé par une noix de coco"));
	CauseTexts.Add(EDeathCause::Tiki, LOCTEXT("Warrior", "Terrassé par un guerrier de l'île"));
	CauseTexts.Add(EDeathCause::Shark, LOCTEXT("Shark", "Dévoré par un requin"));
	CauseTexts.Add(EDeathCause::Flood, LOCTEXT("Flood", "Emporté par le déluge"));

	// who struck, by class: soft paths, nothing of the creatures is loaded for this; the uniforms of the French soldiers
	// (BP_BeanFR_*) and the mobs of the island (characters-tiki.md) before their native classes
	auto Source = [this](const TCHAR* ClassPath, const FText& Text)
	{
		FDeathSourceText Entry;
		Entry.Source = TSoftClassPtr<AActor>(FSoftObjectPath(ClassPath));
		Entry.Text = Text;
		SourceTexts.Add(Entry);
	};
	Source(TEXT("/Script/monke_island.GiantCrab"), LOCTEXT("GiantCrab", "Écrasé par Tupa Nui"));
	Source(TEXT("/Script/monke_island.SharkActor"), LOCTEXT("SharkClass", "Dévoré par un requin"));
	Source(TEXT("/Script/monke_island.TikiCreature"), LOCTEXT("IslandCreature", "Terrassé par un gardien de l'île"));
	Source(TEXT("/Script/monke_island.TikiWarrior"), LOCTEXT("IslandWarrior", "Terrassé par un guerrier de l'île"));
	Source(TEXT("/Game/MonkeIsland/Characters/CocoWarrior/Blueprints/BP_TikiCocoWarrior.BP_TikiCocoWarrior_C"),
		LOCTEXT("CocoWarrior", "Assommé par un Guerrier Coco"));
	Source(TEXT("/Game/MonkeIsland/Characters/CrabGuardian/Blueprints/BP_TikiCrabGuardian.BP_TikiCrabGuardian_C"),
		LOCTEXT("CrabGuardian", "Écrasé par un Gardien Crabe"));
	Source(TEXT("/Game/MonkeIsland/Characters/AshWatcher/Blueprints/BP_TikiAshWatcher.BP_TikiAshWatcher_C"),
		LOCTEXT("AshWatcher", "Terrassé par un Veilleur des Cendres"));
	Source(TEXT("/Script/monke_island.FortSoldier"), LOCTEXT("FrenchSoldier", "Abattu par un soldat français"));
	const FText Fusilier = LOCTEXT("FrenchFusilier", "Abattu par un fusilier français");
	Source(TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_CFM_Fusilier.BP_BeanFR_CFM_Fusilier_C"), Fusilier);
	Source(TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Fusilier_1767.BP_BeanFR_Fusilier_1767_C"), Fusilier);
	Source(TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Fusilier_Bicorne.BP_BeanFR_Fusilier_Bicorne_C"), Fusilier);
	Source(TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Grenadier.BP_BeanFR_Grenadier_C"),
		LOCTEXT("FrenchGrenadier", "Abattu par un grenadier français"));
	Source(TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Sergent.BP_BeanFR_Sergent_C"),
		LOCTEXT("FrenchSergeant", "Abattu par un sergent français"));
	Source(TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Officier_Marine.BP_BeanFR_Officier_Marine_C"),
		LOCTEXT("FrenchOfficer", "Abattu par un officier de marine français"));
	Source(TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Chasseur_Shako.BP_BeanFR_Chasseur_Shako_C"),
		LOCTEXT("FrenchChasseur", "Abattu par un chasseur français"));
	Source(TEXT("/Script/monke_island.FallingCoconutActor"), LOCTEXT("CoconutClass", "Assommé par une noix de coco"));
	Source(TEXT("/Script/monke_island.FortMoat"), LOCTEXT("Moat", "Noyé dans les douves"));
	Source(TEXT("/Script/monke_island.ExpeditionGameState"), LOCTEXT("FloodClass", "Emporté par le déluge"));

	PlayerKillText = LOCTEXT("PlayerKill", "Tué par {Killer}");
	TeammateName = LOCTEXT("Teammate", "un équipier");
	SelfKillText = LOCTEXT("SelfKill", "Victime de sa propre maladresse");
	UnknownText = LOCTEXT("Unknown", "Mort dans des circonstances obscures");
}

FText UDeathRules::DescribeDeath(const FDeathRecord& Record, const APlayerState* Victim) const
{
	// a player's blow: the teammate is named, or the dead player struck himself
	if (Record.Cause == EDeathCause::Player)
	{
		if (Record.KillerPlayer && Record.KillerPlayer == Victim)
		{
			return SelfKillText;
		}
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Killer"), Record.KillerPlayer ? FText::FromString(Record.KillerPlayer->GetPlayerName()) : TeammateName);
		return FText::Format(PlayerKillText, Arguments);
	}
	// what the blow told, then who dealt it (a weapon or a projectile is more precise), then who was behind it
	if (const FText* Text = CauseTexts.Find(Record.Cause); Text && !Text->IsEmpty())
	{
		return *Text;
	}
	if (const FDeathSourceText* Entry = FindSourceText(Record.CauserClass.Get(), Record.DamageType.Get()))
	{
		return Entry->Text;
	}
	if (const FDeathSourceText* Entry = FindSourceText(Record.KillerClass.Get(), Record.DamageType.Get()))
	{
		return Entry->Text;
	}
	return UnknownText;
}

const FDeathSourceText* UDeathRules::FindSourceText(const UClass* Class, const UClass* InDamageType) const
{
	if (!Class)
	{
		return nullptr;
	}
	const FDeathSourceText* Best = nullptr;
	int32 BestSteps = MAX_int32;
	bool bBestTyped = false;
	for (const FDeathSourceText& Entry : SourceTexts)
	{
		// a class that is not loaded cannot be a parent of a loaded one: nothing is loaded here
		const UClass* EntryClass = Entry.Source.Get();
		if (!EntryClass || Entry.Text.IsEmpty())
		{
			continue;
		}
		const bool bTyped = !Entry.DamageType.IsNull();
		if (bTyped)
		{
			const UClass* EntryDamage = Entry.DamageType.Get();
			if (!EntryDamage || !InDamageType || !InDamageType->IsChildOf(EntryDamage))
			{
				continue;
			}
		}
		int32 Steps = 0;
		const UClass* Parent = Class;
		while (Parent && Parent != EntryClass)
		{
			Parent = Parent->GetSuperClass();
			++Steps;
		}
		if (!Parent)
		{
			continue;
		}
		if (Steps < BestSteps || (Steps == BestSteps && bTyped && !bBestTyped))
		{
			Best = &Entry;
			BestSteps = Steps;
			bBestTyped = bTyped;
		}
	}
	return Best;
}

FVector UDeathRules::FollowPivot(const FVector& PawnLocation) const
{
	return PawnLocation + FVector(0.0, 0.0, FollowHeight);
}

FVector UDeathRules::FollowCameraLocation(const FVector& PawnLocation, float Yaw) const
{
	return FollowPivot(PawnLocation) - FRotator(FollowPitch, Yaw, 0.0f).Vector() * FollowDistance;
}

#undef LOCTEXT_NAMESPACE
