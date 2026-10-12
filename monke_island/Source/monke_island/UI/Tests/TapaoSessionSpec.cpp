// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "UI/Tapao/TapaoUISkin.h"
#include "UI/Tapao/TapaoUISubsystem.h"
#include "UObject/UObjectGlobals.h"

// T-297 (M4 and m5 of docs/audit/revue-securite-T269.md): a game alone never becomes a server open to the local network,
// a hosted game takes the places of DA_TapaoUISkin, and REJOINDRE only opens host[:port].
// Run with: ./scripts/ue.ps1 test -Filter monke_island.UI.Session
BEGIN_DEFINE_SPEC(FTapaoSessionSpec, "monke_island.UI.Session",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTapaoSessionSpec)

void FTapaoSessionSpec::Define()
{
	Describe("PlayOptions", [this]()
	{
		It("never listens for a game alone, also when the end screen starts it again (M4)", [this]()
		{
			TestEqual(TEXT("SOLO"), UTapaoUISubsystem::MakePlayOptions(false, 4), FString());
			const TOptional<FString> Restart = UTapaoUISubsystem::MakeRestartOptions(NM_Standalone, 4);
			TestTrue(TEXT("the host of a game alone starts it again"), Restart.IsSet());
			TestEqual(TEXT("without listening"), Restart.IsSet() ? Restart.GetValue() : FString(TEXT("unset")), FString());
		});

		It("keeps a hosted game hosted, with its places", [this]()
		{
			const TOptional<FString> Restart = UTapaoUISubsystem::MakeRestartOptions(NM_ListenServer, 4);
			TestEqual(TEXT("hosted again"), Restart.IsSet() ? Restart.GetValue() : FString(), FString(TEXT("listen?MaxPlayers=4")));
			TestEqual(TEXT("CRÉER UNE PARTIE"), UTapaoUISubsystem::MakePlayOptions(true, 4), FString(TEXT("listen?MaxPlayers=4")));
			TestEqual(TEXT("at most 16"), UTapaoUISubsystem::MakePlayOptions(true, 99), FString(TEXT("listen?MaxPlayers=16")));
			TestEqual(TEXT("at least the host"), UTapaoUISubsystem::MakePlayOptions(true, 0), FString(TEXT("listen?MaxPlayers=1")));
		});

		It("lets only the host start an island again", [this]()
		{
			TestFalse(TEXT("client"), UTapaoUISubsystem::MakeRestartOptions(NM_Client, 4).IsSet());
		});

		It("ships 4 places for a hosted game (cahier §16)", [this]()
		{
			TestEqual(TEXT("class default"), GetDefault<UTapaoUISkin>()->MaxPlayers, 4);
			const UTapaoUISkin* Asset = LoadObject<UTapaoUISkin>(nullptr, TEXT("/Game/MonkeIsland/UI/Tapao/DA_TapaoUISkin.DA_TapaoUISkin"));
			if (Asset)
			{
				TestTrue(TEXT("DA_TapaoUISkin keeps 1 to 4 places"), Asset->MaxPlayers >= 1 && Asset->MaxPlayers <= 4);
			}
			else
			{
				AddInfo(TEXT("DA_TapaoUISkin not found: only the class default is checked"));
			}
		});
	});

	Describe("JoinAddress", [this]()
	{
		It("opens an IPv4 address or a host name, with an optional port", [this]()
		{
			TestTrue(TEXT("loopback"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("127.0.0.1")));
			TestTrue(TEXT("LAN with port"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("192.168.1.20:7777")));
			TestTrue(TEXT("host name"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("pc-de-matthieu.local")));
		});

		It("refuses URL options, spaces, a bad port and an address too long (m5)", [this]()
		{
			TestFalse(TEXT("empty"), UTapaoUISubsystem::IsValidJoinAddress(FString()));
			TestFalse(TEXT("URL option"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("192.168.1.20?listen")));
			TestFalse(TEXT("URL option after the port"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("192.168.1.20:7777?Name=X")));
			TestFalse(TEXT("space"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("192.168.1 .20")));
			TestFalse(TEXT("port out of range"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("192.168.1.20:99999")));
			TestFalse(TEXT("port zero"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("192.168.1.20:0")));
			TestFalse(TEXT("empty port"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("192.168.1.20:")));
			TestFalse(TEXT("dash first"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("-host")));
			TestFalse(TEXT("path"), UTapaoUISubsystem::IsValidJoinAddress(TEXT("/Game/MonkeIsland/Maps/L_Boot")));
			TestFalse(TEXT("2 000 characters (FName stops at 1 023)"), UTapaoUISubsystem::IsValidJoinAddress(FString::ChrN(2000, TEXT('a'))));
		});
	});
}

#endif
