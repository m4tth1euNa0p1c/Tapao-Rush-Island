// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Run/RunSaveGame.h"
#include "Run/RunStateComponent.h"
#include "Run/RunSubsystem.h"
#include "Run/SaveSlotFile.h"
#include "UObject/Package.h"

// R-03 (T-293) and R-41 (T-311) of docs/audit/revue-reseau-T268.md. The subsystem is made without Initialize and reads a
// transient folder: the tests never touch Saved/SaveGames.
// Run with: ./scripts/ue.ps1 test -Filter monke_island.Run.Subsystem
BEGIN_DEFINE_SPEC(FRunSubsystemSpec, "monke_island.Run.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	UGameInstance* GameInstance = nullptr;
	URunSubsystem* Run = nullptr;
	FString Folder;

	/** A loadout of Count items */
	static FRunLoadout MakeLoadout(int32 Count)
	{
		FRunLoadout Loadout;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FRunLoadoutItem& Item = Loadout.Items.AddDefaulted_GetRef();
			Item.Slot = Index;
			Item.Definition = TSoftObjectPtr<UInventoryItemDefinition>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Weapons/Musket/DA_Item_Musket.DA_Item_Musket")));
		}
		return Loadout;
	}

END_DEFINE_SPEC(FRunSubsystemSpec)

void FRunSubsystemSpec::Define()
{
	BeforeEach([this]()
	{
		// no game instance Init and no subsystem Initialize: nothing reads the real slots
		GameInstance = NewObject<UGameInstance>(GEngine);
		GameInstance->AddToRoot();
		Run = NewObject<URunSubsystem>(GameInstance);
		Run->AddToRoot();
		Folder = FPaths::AutomationTransientDir() / TEXT("RunSubsystemSpec");
		IFileManager::Get().DeleteDirectory(*Folder, /*RequireExists*/ false, /*Tree*/ true);
		IFileManager::Get().MakeDirectory(*Folder, /*Tree*/ true);
		Run->SlotDirectory = Folder;
	});

	AfterEach([this]()
	{
		Run->RemoveFromRoot();
		Run = nullptr;
		GameInstance->RemoveFromRoot();
		GameInstance = nullptr;
		IFileManager::Get().DeleteDirectory(*Folder, /*RequireExists*/ false, /*Tree*/ true);
	});

	Describe("ClientRead", [this]()
	{
		It("keeps a run only on a machine that owns it", [this]()
		{
			TestFalse(TEXT("client"), URunSubsystem::KeepsRunOn(NM_Client));
			TestTrue(TEXT("alone"), URunSubsystem::KeepsRunOn(NM_Standalone));
			TestTrue(TEXT("host"), URunSubsystem::KeepsRunOn(NM_ListenServer));
			TestTrue(TEXT("dedicated server"), URunSubsystem::KeepsRunOn(NM_DedicatedServer));
		});

		It("never reads the slot on a client, and sets the menu's run aside, untouched, when it joins a host", [this]()
		{
			// the run a solo game left on this machine
			URunSaveGame* Saved = NewObject<URunSaveGame>(GetTransientPackage());
			Saved->State.bActive = true;
			Saved->State.DaysPlayed = 6;
			Saved->State.Treasury = 900;
			const TValueOrError<MonkeSaveSlot::FSlotPaths, ESaveSlotError> Paths = MonkeSaveSlot::MakePaths(Folder, TEXT("Run"));
			if (!TestTrue(TEXT("transient slot"), Paths.HasValue()))
			{
				return;
			}
			TestEqual(TEXT("solo run on disk"), MonkeSaveSlot::Write(Paths.GetValue(), *Saved, URunSaveGame::CurrentVersion, FDateTime::Now()), ESaveSlotError::None);
			TArray<uint8> Before;
			FFileHelper::LoadFileToArray(Before, *Paths.GetValue().Main);

			// a client started straight into a host's game (PIE client): nothing is read
			Run->SyncWithNetMode(NM_Client);
			TestFalse(TEXT("not read on a client"), Run->bLoaded);
			TestFalse(TEXT("no run on a client"), Run->GetState().bActive);
			TestEqual(TEXT("no campaign day on a client"), Run->GetCampaignDay(), 0);

			// the main menu of this machine reads its own run
			Run->SyncWithNetMode(NM_Standalone);
			TestTrue(TEXT("read in the menu"), Run->GetState().bActive);
			TestEqual(TEXT("its campaign day"), Run->GetCampaignDay(), 7);

			// then it joins a host: its run is set aside in memory, never saved
			Run->SyncWithNetMode(NM_Client);
			TestFalse(TEXT("set aside in the host's game"), Run->GetState().bActive);
			TestEqual(TEXT("the day shown is not its own"), Run->GetCampaignDay(), 0);
			Run->SaveRun();
			TArray<uint8> After;
			FFileHelper::LoadFileToArray(After, *Paths.GetValue().Main);
			TestTrue(TEXT("its file is untouched"), Before == After);

			// back in its menu, its run is there again
			Run->SyncWithNetMode(NM_Standalone);
			TestEqual(TEXT("its treasury again"), Run->GetState().Treasury, 900);
		});

		It("tells every machine the host's run through the game state (campaign day as the server counts it)", [this]()
		{
			const UWorld::InitializationValues Init = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreateNavigation(false).CreateAISystem(false);
			UWorld* World = UWorld::CreateWorld(EWorldType::Game, /*bInformEngineOfWorld*/ false, NAME_None, nullptr, /*bAddToRoot*/ true,
				ERHIFeatureLevel::Num, &Init);
			AActor* Owner = World ? World->SpawnActor<AActor>() : nullptr;
			URunStateComponent* Component = Owner ? NewObject<URunStateComponent>(Owner) : nullptr;
			if (TestNotNull(TEXT("component"), Component))
			{
				Component->RegisterComponent();
				TestFalse(TEXT("no run before the server says so"), Component->IsRunActive());
				TestEqual(TEXT("no campaign day before"), Component->GetCampaignDay(), 0);
				FRunContractState State;
				State.bActive = true;
				State.DaysPlayed = 4;
				State.Quota = 5500;
				Component->SetFromContract(State, 3, 3, FText::FromString(TEXT("Zone")), FRunDayReport());
				TestTrue(TEXT("a run is under way"), Component->IsRunActive());
				TestEqual(TEXT("campaign day as URunSubsystem::GetCampaignDay"), Component->GetCampaignDay(), 5);
			}
			if (World)
			{
				World->DestroyWorld(/*bInformEngineOfWorld*/ false);
			}
		});
	});

	Describe("Loadout", [this]()
	{
		It("hands the kept equipment out once on an island", [this]()
		{
			Run->Loadouts.Add(TEXT("A"), MakeLoadout(2));
			const FRunLoadout* First = Run->ClaimLoadout(TEXT("A"));
			TestTrue(TEXT("restored at the start of the island"), First && First->Items.Num() == 2);
			TestNull(TEXT("a second pawn of the same player gets the default kit"), Run->ClaimLoadout(TEXT("A")));
			TestNull(TEXT("FindLoadout hands it out once too"), Run->FindLoadout(TEXT("A")));
			TestNull(TEXT("nothing kept for another player"), Run->ClaimLoadout(TEXT("B")));
		});

		It("keeps what a leaving player carries, without what he gave away (no duplication)", [this]()
		{
			Run->Loadouts.Add(TEXT("A"), MakeLoadout(2));
			TestNotNull(TEXT("restored"), Run->ClaimLoadout(TEXT("A")));
			// A gives one of his two items to B, then leaves with the other
			const FRunLoadout LeftWith = MakeLoadout(1);
			Run->RecordLoadout(TEXT("A"), &LeftWith);
			const FRunLoadout* Back = Run->ClaimLoadout(TEXT("A"));
			TestTrue(TEXT("back on the island, he takes what he left with"), Back && Back->Items.Num() == 1);
			TestNull(TEXT("once"), Run->ClaimLoadout(TEXT("A")));
		});

		It("forgets the equipment of a player lost on the island", [this]()
		{
			Run->Loadouts.Add(TEXT("A"), MakeLoadout(2));
			Run->RecordLoadout(TEXT("A"), nullptr);
			TestFalse(TEXT("nothing kept"), Run->Loadouts.Contains(TEXT("A")));
			TestNull(TEXT("the default kit"), Run->ClaimLoadout(TEXT("A")));
		});

		It("loses at the end of the day what was handed out to a player gone unseen, and keeps the one of a player who never came", [this]()
		{
			Run->Loadouts.Add(TEXT("A"), MakeLoadout(2));
			Run->Loadouts.Add(TEXT("C"), MakeLoadout(1));
			TestNotNull(TEXT("A restored"), Run->ClaimLoadout(TEXT("A")));
			Run->ForgetHandedOutAbsent(TSet<FString>{ TEXT("B") });
			TestFalse(TEXT("A's equipment went with his pawn"), Run->Loadouts.Contains(TEXT("A")));
			TestTrue(TEXT("C keeps his for the island he comes to"), Run->Loadouts.Contains(TEXT("C")));
		});
	});
}

#endif
