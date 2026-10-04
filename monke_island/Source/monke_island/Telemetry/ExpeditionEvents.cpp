#include "Telemetry/ExpeditionEvents.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Player/IslanderHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"
#include "monke_island.h"

FString ExpeditionEvents::SafeBuildToken(const FString& Value)
{
	if (Value.IsEmpty() || Value.Len() > 96)
	{
		return TEXT("unversioned");
	}
	for (const TCHAR C : Value)
	{
		if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9')
			|| C == '.' || C == '_' || C == '-' || C == '+'))
		{
			return TEXT("unversioned");
		}
	}
	return Value;
}

FString ExpeditionEvents::BuildVersion()
{
	FString Value;
	if (GConfig)
	{
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Value, GGameIni);
	}
	return SafeBuildToken(Value);
}

FString ExpeditionEvents::RulesHash(const UExpeditionRules& Rules)
{
	TArray<FString> Fields;
	for (TFieldIterator<FProperty> It(Rules.GetClass()); It; ++It)
	{
		const FProperty* Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) || Property->HasAnyPropertyFlags(CPF_Transient)
			|| !Property->GetOwnerClass()->IsChildOf(UExpeditionRules::StaticClass()))
		{
			continue;
		}
		FString Value;
		if (const FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			const float Number = FloatProperty->GetPropertyValue_InContainer(&Rules);
			uint32 Bits;
			FMemory::Memcpy(&Bits, &Number, sizeof(Bits));
			Value = FString::Printf(TEXT("%08x"), Bits);
		}
		else
		{
			Property->ExportText_InContainer(0, Value, &Rules, nullptr, nullptr, PPF_None);
		}
		Fields.Add(FString::Printf(TEXT("%s:%s:%d:%s"), *Property->GetName(), *Property->GetCPPType(), Value.Len(), *Value));
	}
	Fields.Sort();
	return FMD5::HashAnsiString(*(TEXT("expedition-rules-v1\n") + FString::Join(Fields, TEXT("\n"))));
}

void ExpeditionEvents::LogDeath(const UIslanderHealthComponent& Health, EDeathCause Cause)
{
	const AActor* Owner = Health.GetOwner();
	UWorld* World = Health.GetWorld();
	if (!Owner || !Owner->HasAuthority() || !World)
	{
		return;
	}
	const APawn* Pawn = Cast<APawn>(Owner);
	const bool bPlayer = Pawn && Pawn->IsPlayerControlled();
	int32 Alive = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const APawn* Player = Controller ? Controller->GetPawn() : nullptr;
		const UIslanderHealthComponent* PlayerHealth = Player ? Player->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
		Alive += PlayerHealth && !PlayerHealth->IsDead() ? 1 : 0;
	}
	const AExpeditionGameState* State = AExpeditionGameState::Get(Owner);
	const double Elapsed = State && State->GetExpeditionId().IsValid()
		? FMath::Max(0.0, State->GetServerWorldTimeSeconds() - State->GetExpeditionStartTime()) : 0.0;
	const FString Id = State ? State->GetExpeditionId().ToString() : TEXT("none");
	const FString Phase = State ? UEnum::GetValueAsString(State->GetPhase()) : TEXT("unknown");
	UE_LOG(Logmonke_island, Log, TEXT("Expedition %s: died schema=1 cause=%s t=%.2f phase=%s alive=%d entity=%s"),
		*Id, DeathCauseName(Cause), Elapsed, *Phase, Alive, bPlayer ? TEXT("player") : TEXT("creature"));
}
