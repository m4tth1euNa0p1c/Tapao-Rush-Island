#include "Misc/AutomationTest.h"
#include "Telemetry/ExpeditionEvents.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS
BEGIN_DEFINE_SPEC(FExpeditionEventsSpec, "monke_island.Telemetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FExpeditionEventsSpec)

void FExpeditionEventsSpec::Define()
{
	It("names every death cause with a stable token", [this]()
	{
		const TCHAR* Expected[] = { TEXT("unknown"), TEXT("fall"), TEXT("drowning"), TEXT("coconut"), TEXT("tiki"), TEXT("shark") };
		for (uint8 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
		{
			TestEqual(TEXT("cause"), FString(DeathCauseName(static_cast<EDeathCause>(Index))), FString(Expected[Index]));
		}
		TestEqual(TEXT("invalid"), FString(DeathCauseName(static_cast<EDeathCause>(255))), FString(TEXT("unknown")));
	});
	It("maps engine damage types without depending on hazard actors", [this]()
	{
		TestEqual(TEXT("tiki"), GetDefault<UTikiDamageType>()->Cause, EDeathCause::Tiki);
		TestEqual(TEXT("shark"), GetDefault<USharkDamageType>()->Cause, EDeathCause::Shark);
	});
	It("rejects personal paths and log control characters in build tokens", [this]()
	{
		TestEqual(TEXT("valid"), ExpeditionEvents::SafeBuildToken(TEXT("0.1.0+abc123")), FString(TEXT("0.1.0+abc123")));
		for (const FString& Value : { FString(), FString(TEXT("C:/Users/Alice/build")), FString(TEXT("v1\nUser: Alice")), FString::ChrN(97, 'a') })
		{
			TestEqual(TEXT("rejected"), ExpeditionEvents::SafeBuildToken(Value), FString(TEXT("unversioned")));
		}
	});
	It("hashes values independently of asset names", [this]()
	{
		TStrongObjectPtr<UExpeditionRules> A(NewObject<UExpeditionRules>());
		TStrongObjectPtr<UExpeditionRules> B(NewObject<UExpeditionRules>());
		TestEqual(TEXT("same values"), ExpeditionEvents::RulesHash(*A), ExpeditionEvents::RulesHash(*B));
		TestEqual(TEXT("length"), ExpeditionEvents::RulesHash(*A).Len(), 32);
	});
	It("changes the hash for each editable rules field", [this]()
	{
		TStrongObjectPtr<UExpeditionRules> Rules(NewObject<UExpeditionRules>());
		const FString Original = ExpeditionEvents::RulesHash(*Rules);
		int32 Checked = 0;
		for (TFieldIterator<FProperty> It(UExpeditionRules::StaticClass(), EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_Edit)) { continue; }
			Rules.Reset(NewObject<UExpeditionRules>());
			if (const FFloatProperty* Property = CastField<FFloatProperty>(*It))
			{
				Property->SetPropertyValue_InContainer(Rules.Get(), Property->GetPropertyValue_InContainer(Rules.Get()) + 1.0f);
			}
			else if (const FIntProperty* Property = CastField<FIntProperty>(*It))
			{
				Property->SetPropertyValue_InContainer(Rules.Get(), Property->GetPropertyValue_InContainer(Rules.Get()) + 1);
			}
			else if (const FBoolProperty* Property = CastField<FBoolProperty>(*It))
			{
				Property->SetPropertyValue_InContainer(Rules.Get(), !Property->GetPropertyValue_InContainer(Rules.Get()));
			}
			else if (It->GetFName() == TEXT("RestartMap"))
			{
				Rules->RestartMap = FSoftObjectPath(TEXT("/Game/Test.Test"));
			}
			else { AddError(TEXT("New rules property needs a hash sensitivity test")); continue; }
			TestNotEqual(*It->GetName(), ExpeditionEvents::RulesHash(*Rules), Original);
			++Checked;
		}
		TestTrue(TEXT("all current fields covered"), Checked >= 11);
	});
}
#endif
