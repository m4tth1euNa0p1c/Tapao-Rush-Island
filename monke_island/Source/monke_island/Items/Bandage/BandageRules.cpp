// monke_island

#include "Items/Bandage/BandageRules.h"
#include "Player/IslanderHealthComponent.h"

UBandageRules::UBandageRules()
{
	UseKeys = { EKeys::F, EKeys::LeftMouseButton };

	PromptText = NSLOCTEXT("Bandage", "Prompt", "{0} : appliquer un bandage ({1})");
	ApplyingText = NSLOCTEXT("Bandage", "Applying", "Bandage…");
	HealedText = NSLOCTEXT("Bandage", "Healed", "Plaie bandée : santé au maximum");
	FullHealthText = NSLOCTEXT("Bandage", "FullHealth", "Santé au maximum");
	UnableText = NSLOCTEXT("Bandage", "Unable", "Impossible de se bander maintenant");
	InterruptTexts.Add(EBandageInterrupt::Hit, NSLOCTEXT("Bandage", "CutHit", "Bandage interrompu : coup reçu"));
	InterruptTexts.Add(EBandageInterrupt::Sprint, NSLOCTEXT("Bandage", "CutSprint", "Bandage interrompu : course"));
	InterruptTexts.Add(EBandageInterrupt::Jump, NSLOCTEXT("Bandage", "CutJump", "Bandage interrompu : saut"));
	InterruptTexts.Add(EBandageInterrupt::Water, NSLOCTEXT("Bandage", "CutWater", "Bandage interrompu : eau trop profonde"));
	InterruptTexts.Add(EBandageInterrupt::PutAway, NSLOCTEXT("Bandage", "CutPutAway", "Bandage interrompu"));
	InterruptTexts.Add(EBandageInterrupt::Down, NSLOCTEXT("Bandage", "CutDown", "Bandage interrompu"));
}

FText UBandageRules::GetUseKeyText() const
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

FText UBandageRules::GetInterruptText(EBandageInterrupt Interrupt) const
{
	if (const FText* Text = InterruptTexts.Find(Interrupt))
	{
		return *Text;
	}
	return Interrupt == EBandageInterrupt::None ? FText::GetEmpty() : NSLOCTEXT("Bandage", "Cut", "Bandage interrompu");
}

FText UBandageRules::GetRefusalText(EBandageRefusal Refusal) const
{
	switch (Refusal)
	{
	case EBandageRefusal::FullHealth:		return FullHealthText;
	case EBandageRefusal::CarrierUnable:
	case EBandageRefusal::Sprinting:		return UnableText;
	default:								return FText::GetEmpty();
	}
}

float UBandageRules::ComputeHeal(float Health, float MaxHealth) const
{
	// the full maximum, or the set amount, never above MaxHealth
	const float Amount = bHealToMax ? MaxHealth - Health : HealAmount;
	return UIslanderHealthComponent::ComputeHeal(Health, MaxHealth, Amount);
}
