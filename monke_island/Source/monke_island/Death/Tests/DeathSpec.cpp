// monke_island

#include "Misc/AutomationTest.h"
#include "Death/DeathRules.h"
#include "Death/DeathSpectatorComponent.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Player/IslanderHealthComponent.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/PlayerState.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Death
BEGIN_DEFINE_SPEC(FDeathSpec, "monke_island.Death",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	/** A class entry of the rules */
	struct FEntry
	{
		UClass* Class;
		const TCHAR* Text;
	};

	/** Rules with only these class entries (the defaults' texts otherwise) */
	UDeathRules* MakeRules(const TArray<FEntry>& Entries, UClass* TypedClass = nullptr, const TCHAR* TypedText = nullptr)
	{
		UDeathRules* Rules = NewObject<UDeathRules>(GetTransientPackage());
		Rules->SourceTexts.Reset();
		for (const FEntry& Entry : Entries)
		{
			FDeathSourceText Text;
			Text.Source = Entry.Class;
			Text.Text = FText::FromString(Entry.Text);
			Rules->SourceTexts.Add(Text);
		}
		if (TypedClass)
		{
			FDeathSourceText Text;
			Text.Source = TypedClass;
			Text.DamageType = UDamageType::StaticClass();
			Text.Text = FText::FromString(TypedText);
			Rules->SourceTexts.Add(Text);
		}
		return Rules;
	}

	/** A death record of a world blow of these classes */
	static FDeathRecord WorldBlow(UClass* Killer, UClass* Causer = nullptr, UClass* DamageType = nullptr)
	{
		FDeathRecord Record;
		Record.Time = 1.0f;
		Record.KillerClass = Killer;
		Record.CauserClass = Causer ? Causer : Killer;
		Record.DamageType = DamageType;
		return Record;
	}

END_DEFINE_SPEC(FDeathSpec)

void FDeathSpec::Define()
{
	Describe("Screen", [this]()
	{
		It("shows NAUFRAGÉ only once the whole crew is dead (solo: at the death)", [this]()
		{
			// the crew is lost as the server counts it (AExpeditionGameState::IsWipe), the screen follows
			const bool bSoloWiped = AExpeditionGameState::IsWipe(1, 0, 0);
			TestTrue(TEXT("solo: the death wipes the crew"), bSoloWiped);
			TestEqual(TEXT("solo: NAUFRAGÉ"), UDeathSpectatorComponent::DecideScreen(true, bSoloWiped, false, false), EDeathScreen::Wreck);

			const bool bOneOfTwo = AExpeditionGameState::IsWipe(1, 1, 0);
			TestFalse(TEXT("two players, one dead: the crew lives"), bOneOfTwo);
			TestEqual(TEXT("one of two dead: the death screen, no NAUFRAGÉ"), UDeathSpectatorComponent::DecideScreen(true, bOneOfTwo, false, false), EDeathScreen::Dead);
			TestEqual(TEXT("one of two dead, a teammate followed"), UDeathSpectatorComponent::DecideScreen(true, bOneOfTwo, false, true), EDeathScreen::Following);

			const bool bBothDead = AExpeditionGameState::IsWipe(2, 0, 0);
			TestTrue(TEXT("the last one dies: the crew is lost"), bBothDead);
			TestEqual(TEXT("the spectator sees NAUFRAGÉ"), UDeathSpectatorComponent::DecideScreen(true, bBothDead, false, true), EDeathScreen::Wreck);

			TestFalse(TEXT("dead while a teammate still loads: no wipe"), AExpeditionGameState::IsWipe(1, 0, 1));
			TestFalse(TEXT("three dead of four: no wipe"), AExpeditionGameState::IsWipe(3, 1, 0));
		});

		It("shows nothing to the living, nor over the end of a day the others survived", [this]()
		{
			TestEqual(TEXT("alive"), UDeathSpectatorComponent::DecideScreen(false, false, false, false), EDeathScreen::None);
			TestEqual(TEXT("alive at the end of the day"), UDeathSpectatorComponent::DecideScreen(false, false, true, false), EDeathScreen::None);
			TestEqual(TEXT("dead, the crew sails off: the departure scene"), UDeathSpectatorComponent::DecideScreen(true, false, true, true), EDeathScreen::None);
		});
	});

	Describe("Follow", [this]()
	{
		It("follows the first living teammate, then the next and the previous, round the end", [this]()
		{
			const TArray<int32> Ids = { 256, 257, 259 };
			TestEqual(TEXT("first follow, forward"), UDeathSpectatorComponent::PickFollowIndex(Ids, INDEX_NONE, 1), 0);
			TestEqual(TEXT("first follow, backward"), UDeathSpectatorComponent::PickFollowIndex(Ids, INDEX_NONE, -1), 2);
			TestEqual(TEXT("next"), UDeathSpectatorComponent::PickFollowIndex(Ids, 256, 1), 1);
			TestEqual(TEXT("next of the last: the first"), UDeathSpectatorComponent::PickFollowIndex(Ids, 259, 1), 0);
			TestEqual(TEXT("previous"), UDeathSpectatorComponent::PickFollowIndex(Ids, 259, -1), 1);
			TestEqual(TEXT("previous of the first: the last"), UDeathSpectatorComponent::PickFollowIndex(Ids, 256, -1), 2);
		});

		It("goes on in order when the followed teammate is gone", [this]()
		{
			// 257 died: he is no candidate any more
			const TArray<int32> Ids = { 256, 259 };
			TestEqual(TEXT("next after the dead one"), UDeathSpectatorComponent::PickFollowIndex(Ids, 257, 1), 1);
			TestEqual(TEXT("previous before the dead one"), UDeathSpectatorComponent::PickFollowIndex(Ids, 257, -1), 0);
		});

		It("stays on the only teammate and follows nobody when nobody lives", [this]()
		{
			TestEqual(TEXT("the only one, forward"), UDeathSpectatorComponent::PickFollowIndex({ 300 }, 300, 1), 0);
			TestEqual(TEXT("the only one, backward"), UDeathSpectatorComponent::PickFollowIndex({ 300 }, 300, -1), 0);
			TestEqual(TEXT("nobody"), UDeathSpectatorComponent::PickFollowIndex({}, 300, 1), static_cast<int32>(INDEX_NONE));
		});
	});

	Describe("Record", [this]()
	{
		It("keeps the cause the blow gave", [this]()
		{
			const FDeathRecord Fall = UIslanderHealthComponent::MakeDeathRecord(EDeathCause::Fall, nullptr, nullptr, nullptr, 12.0f);
			TestEqual(TEXT("fall"), Fall.Cause, EDeathCause::Fall);
			TestTrue(TEXT("set"), Fall.IsSet());
			TestNull(TEXT("no killer"), Fall.KillerClass.Get());
			TestNull(TEXT("no player"), Fall.KillerPlayer.Get());
			TestFalse(TEXT("alive: unset"), FDeathRecord().IsSet());
		});

		It("names the pawn behind the blow, or the actor that dealt it", [this]()
		{
			const FDeathRecord ByPawn = UIslanderHealthComponent::MakeDeathRecord(EDeathCause::Unknown, GetDefault<ACharacter>(), nullptr,
				GetDefault<UDamageType>(), 3.0f);
			TestEqual(TEXT("unknown: told by the classes"), ByPawn.Cause, EDeathCause::Unknown);
			TestEqual(TEXT("the pawn itself"), ByPawn.KillerClass.Get(), ACharacter::StaticClass());
			TestEqual(TEXT("the causer"), ByPawn.CauserClass.Get(), ACharacter::StaticClass());
			TestEqual(TEXT("the damage type"), ByPawn.DamageType.Get(), UDamageType::StaticClass());
			TestNull(TEXT("no player holds it"), ByPawn.KillerPlayer.Get());

			const FDeathRecord ByProp = UIslanderHealthComponent::MakeDeathRecord(EDeathCause::Unknown, GetDefault<AStaticMeshActor>(), nullptr, nullptr, 3.0f);
			TestEqual(TEXT("no pawn behind: the actor"), ByProp.KillerClass.Get(), AStaticMeshActor::StaticClass());
		});
	});

	Describe("Cause", [this]()
	{
		It("tells the fall, the drowning and the deluge", [this]()
		{
			const UDeathRules* Rules = GetDefault<UDeathRules>();
			for (const EDeathCause Cause : { EDeathCause::Fall, EDeathCause::Drowning, EDeathCause::Flood, EDeathCause::Coconut, EDeathCause::Shark })
			{
				FDeathRecord Record;
				Record.Time = 1.0f;
				Record.Cause = Cause;
				const FText* Expected = Rules->CauseTexts.Find(Cause);
				if (TestNotNull(*FString::Printf(TEXT("a text for %s"), *UEnum::GetValueAsString(Cause)), Expected))
				{
					TestEqual(*UEnum::GetValueAsString(Cause), Rules->DescribeDeath(Record, nullptr).ToString(), Expected->ToString());
				}
			}
		});

		It("names the teammate who struck, or tells one's own blow", [this]()
		{
			const UDeathRules* Rules = GetDefault<UDeathRules>();
			FDeathRecord Record;
			Record.Time = 1.0f;
			Record.Cause = EDeathCause::Player;
			// the teammate left the game: "un équipier"
			const FString Gone = Rules->DescribeDeath(Record, nullptr).ToString();
			TestTrue(TEXT("a teammate who left"), Gone.Contains(Rules->TeammateName.ToString()));
			// one's own grenade
			APlayerState* Self = GetMutableDefault<APlayerState>();
			Record.KillerPlayer = Self;
			TestEqual(TEXT("one's own blow"), Rules->DescribeDeath(Record, Self).ToString(), Rules->SelfKillText.ToString());
		});

		It("tells a creature, a soldier or a hazard by its class, the most precise entry first", [this]()
		{
			const UDeathRules* Rules = MakeRules({ { APawn::StaticClass(), TEXT("pawn") }, { ACharacter::StaticClass(), TEXT("character") } });
			TestEqual(TEXT("the nearest parent wins"), Rules->DescribeDeath(WorldBlow(ACharacter::StaticClass()), nullptr).ToString(), FString(TEXT("character")));
			TestEqual(TEXT("a subclass of a listed class"), Rules->DescribeDeath(WorldBlow(ADefaultPawn::StaticClass()), nullptr).ToString(), FString(TEXT("pawn")));
			TestEqual(TEXT("nothing listed"), Rules->DescribeDeath(WorldBlow(AStaticMeshActor::StaticClass()), nullptr).ToString(), Rules->UnknownText.ToString());
			// what dealt the blow (a projectile) before who was behind it
			const UDeathRules* WithProp = MakeRules({ { ACharacter::StaticClass(), TEXT("character") }, { AStaticMeshActor::StaticClass(), TEXT("prop") } });
			TestEqual(TEXT("the causer first"), WithProp->DescribeDeath(WorldBlow(ACharacter::StaticClass(), AStaticMeshActor::StaticClass()), nullptr).ToString(),
				FString(TEXT("prop")));
		});

		It("prefers the entry of the blow's damage type", [this]()
		{
			const UDeathRules* Rules = MakeRules({ { ACharacter::StaticClass(), TEXT("any") } }, ACharacter::StaticClass(), TEXT("typed"));
			TestEqual(TEXT("typed blow"), Rules->DescribeDeath(WorldBlow(ACharacter::StaticClass(), nullptr, UDamageType::StaticClass()), nullptr).ToString(),
				FString(TEXT("typed")));
			TestEqual(TEXT("blow without a type"), Rules->DescribeDeath(WorldBlow(ACharacter::StaticClass()), nullptr).ToString(), FString(TEXT("any")));
		});

		It("knows the creatures, the soldiers and the hazards of the island by their native classes", [this]()
		{
			const UDeathRules* Rules = GetDefault<UDeathRules>();
			const TCHAR* Classes[] = {
				TEXT("/Script/monke_island.GiantCrab"), TEXT("/Script/monke_island.SharkActor"), TEXT("/Script/monke_island.TikiWarrior"),
				TEXT("/Script/monke_island.FortSoldier"), TEXT("/Script/monke_island.FallingCoconutActor"), TEXT("/Script/monke_island.FortMoat"),
				TEXT("/Script/monke_island.ExpeditionGameState") };
			for (const TCHAR* Path : Classes)
			{
				const UClass* Class = FindObject<UClass>(nullptr, Path);
				if (TestNotNull(Path, Class))
				{
					const FDeathSourceText* Entry = Rules->FindSourceText(Class, UDamageType::StaticClass());
					TestTrue(*FString::Printf(TEXT("%s has its cause"), Path), Entry && !Entry->Text.IsEmpty());
				}
			}
			const UClass* Crab = FindObject<UClass>(nullptr, TEXT("/Script/monke_island.GiantCrab"));
			const FDeathSourceText* CrabEntry = Crab ? Rules->FindSourceText(Crab, nullptr) : nullptr;
			TestTrue(TEXT("Tupa Nui"), CrabEntry && CrabEntry->Text.ToString().Contains(TEXT("Tupa Nui")));
		});
	});

	Describe("Camera", [this]()
	{
		It("stands behind the followed teammate, above him, along his view", [this]()
		{
			const UDeathRules* Rules = GetDefault<UDeathRules>();
			const FVector Pawn(1000.0, 2000.0, 300.0);
			const FVector Camera = Rules->FollowCameraLocation(Pawn, 0.0f);
			const FVector Pivot = Rules->FollowPivot(Pawn);
			TestTrue(TEXT("behind (yaw 0 looks along +X)"), Camera.X < Pawn.X);
			TestEqual(TEXT("in line"), Camera.Y, Pawn.Y, 0.01);
			TestTrue(TEXT("above the pivot when looking down"), Rules->FollowPitch >= 0.0f || Camera.Z > Pivot.Z);
			TestEqual(TEXT("at the distance"), FVector::Dist(Camera, Pivot), static_cast<double>(Rules->FollowDistance), 0.01);
			const FVector Turned = Rules->FollowCameraLocation(Pawn, 90.0f);
			TestTrue(TEXT("yaw 90 looks along +Y: behind is -Y"), Turned.Y < Pawn.Y);
		});
	});
}

#endif
