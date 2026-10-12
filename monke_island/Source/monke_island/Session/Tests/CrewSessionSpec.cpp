// monke_island

#include "Misc/AutomationTest.h"
#include "Session/CrewSessionRules.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Session
// Who may join the host's session, and the name the server keeps (T-315; cahier §16, §24.2, §24.5; review T-269 M4, m6)
BEGIN_DEFINE_SPEC(FCrewSessionSpec, "monke_island.Session",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	/** A string from code points written as numbers: no invisible character (zero-width, bidirectional) in this source */
	static FString FromCodePoints(std::initializer_list<uint32> CodePoints)
	{
		FString Out;
		for (const uint32 CodePoint : CodePoints)
		{
			if (sizeof(TCHAR) == 2 && CodePoint > 0xFFFFu)
			{
				Out.AppendChar(static_cast<TCHAR>(0xD800u + ((CodePoint - 0x10000u) >> 10)));
				Out.AppendChar(static_cast<TCHAR>(0xDC00u + ((CodePoint - 0x10000u) & 0x3FFu)));
			}
			else
			{
				Out.AppendChar(static_cast<TCHAR>(CodePoint));
			}
		}
		return Out;
	}

	FString Emoji() const
	{
		// U+1F600 as a UTF-16 pair, whatever the compiler does with the literal
		FString Out;
		if constexpr (sizeof(TCHAR) == 2)
		{
			Out.AppendChar(static_cast<TCHAR>(0xD83D));
			Out.AppendChar(static_cast<TCHAR>(0xDE00));
		}
		else
		{
			Out.AppendChar(static_cast<TCHAR>(0x1F600));
		}
		return Out;
	}
END_DEFINE_SPEC(FCrewSessionSpec)

void FCrewSessionSpec::Define()
{
	Describe("Gate", [this]()
	{
		It("lets anyone in at the lobby and on a map before any launch, within the places", [this]()
		{
			const FCrewRoster Roster;
			TestEqual(TEXT("lobby"), Roster.Evaluate(TEXT("id:C"), true, false, false), ECrewJoinRefusal::None);
			TestEqual(TEXT("island before the launch (PIE)"), Roster.Evaluate(TEXT("id:C"), false, false, false), ECrewJoinRefusal::None);
			TestEqual(TEXT("full"), Roster.Evaluate(TEXT("id:C"), true, false, true), ECrewJoinRefusal::SessionFull);
		});

		It("takes only the crew on the maps of a launched expedition, anyone at the lobby", [this]()
		{
			FCrewRoster Roster;
			const TArray<FString> Crew = { TEXT("id:A"), TEXT("id:B") };
			Roster.Launch(Crew);
			TestTrue(TEXT("launched"), Roster.bExpeditionLaunched);
			TestEqual(TEXT("stranger on the island"), Roster.Evaluate(TEXT("id:C"), false, false, false), ECrewJoinRefusal::ExpeditionInProgress);
			TestEqual(TEXT("crew on the island"), Roster.Evaluate(TEXT("id:A"), false, false, false), ECrewJoinRefusal::None);
			TestEqual(TEXT("crew, but no place left"), Roster.Evaluate(TEXT("id:B"), false, false, true), ECrewJoinRefusal::SessionFull);
			TestEqual(TEXT("stranger at the lobby between two islands"), Roster.Evaluate(TEXT("id:C"), true, false, false), ECrewJoinRefusal::None);
		});

		It("never lets a spectator in, even at the lobby", [this]()
		{
			FCrewRoster Roster;
			TestEqual(TEXT("lobby"), Roster.Evaluate(TEXT("id:A"), true, true, false), ECrewJoinRefusal::SpectatorOnly);
			Roster.Launch(TArray<FString>{ TEXT("id:A") });
			TestEqual(TEXT("crew asking to spectate"), Roster.Evaluate(TEXT("id:A"), false, true, false), ECrewJoinRefusal::SpectatorOnly);
		});

		It("keeps the crew across the travels of the session, from island to island", [this]()
		{
			FCrewRoster Roster;
			Roster.Launch(TArray<FString>{ TEXT("id:A"), TEXT("id:B") });
			// the boot map travels to the island
			Roster.NoteServerTravel(TEXT("Lvl_Raraka"));
			TestTrue(TEXT("island reached by the travel"), Roster.BeginServerWorld(TEXT("Lvl_Raraka")));
			TestEqual(TEXT("B reconnects on the island"), Roster.Evaluate(TEXT("id:B"), false, false, false), ECrewJoinRefusal::None);
			TestEqual(TEXT("a stranger does not"), Roster.Evaluate(TEXT("id:C"), false, false, false), ECrewJoinRefusal::ExpeditionInProgress);
			// end of the day: back to the boot map, then the next island; D joins at the boot map between the two
			Roster.NoteServerTravel(TEXT("L_Boot"));
			TestTrue(TEXT("boot map reached by the travel"), Roster.BeginServerWorld(TEXT("l_boot")));
			TestTrue(TEXT("still launched"), Roster.bExpeditionLaunched);
			Roster.Launch(TArray<FString>{ TEXT("id:A"), TEXT("id:D") });
			Roster.NoteServerTravel(TEXT("L_Frag_Raraka_01"));
			TestTrue(TEXT("next island"), Roster.BeginServerWorld(TEXT("L_Frag_Raraka_01")));
			TestEqual(TEXT("D, who left with the crew"), Roster.Evaluate(TEXT("id:D"), false, false, false), ECrewJoinRefusal::None);
			TestEqual(TEXT("B, who missed the boot map, stays known"), Roster.Evaluate(TEXT("id:B"), false, false, false), ECrewJoinRefusal::None);
			TestEqual(TEXT("C, never in"), Roster.Evaluate(TEXT("id:C"), false, false, false), ECrewJoinRefusal::ExpeditionInProgress);
		});

		It("starts a new session in a world no travel of the session goes to", [this]()
		{
			FCrewRoster Roster;
			Roster.Launch(TArray<FString>{ TEXT("id:A") });
			TestFalse(TEXT("hosted from the menu, or Recommencer"), Roster.BeginServerWorld(TEXT("L_Boot")));
			TestFalse(TEXT("nothing launched"), Roster.bExpeditionLaunched);
			TestFalse(TEXT("nobody known"), Roster.IsMember(TEXT("id:A")));

			Roster.Launch(TArray<FString>{ TEXT("id:A") });
			Roster.NoteServerTravel(TEXT("Lvl_Raraka"));
			TestFalse(TEXT("a travel elsewhere"), Roster.BeginServerWorld(TEXT("L_Boot")));
			TestFalse(TEXT("forgotten"), Roster.bExpeditionLaunched || Roster.IsMember(TEXT("id:A")));

			Roster.Launch(TArray<FString>{ TEXT("id:A") });
			Roster.Reset();
			TestFalse(TEXT("reset: back to a menu"), Roster.bExpeditionLaunched || Roster.IsMember(TEXT("id:A")));
			TestTrue(TEXT("no travel pending"), Roster.PendingTravelMap.IsEmpty());
		});

		It("takes into the crew a player let in before the launch who logs in after it, not a player at the lobby", [this]()
		{
			FCrewRoster Roster;
			Roster.NoteLogin(TEXT("id:G"), false);
			TestFalse(TEXT("before any launch"), Roster.IsMember(TEXT("id:G")));
			Roster.Launch(TArray<FString>{ TEXT("id:A") });
			Roster.NoteLogin(TEXT("id:E"), false);
			Roster.NoteLogin(TEXT("id:F"), true);
			TestTrue(TEXT("logged in on the island"), Roster.IsMember(TEXT("id:E")));
			TestFalse(TEXT("logged in at the lobby"), Roster.IsMember(TEXT("id:F")));
		});

		It("never takes an empty key into the crew", [this]()
		{
			FCrewRoster Roster;
			Roster.Launch(TArray<FString>{ FString() });
			Roster.NoteLogin(FString(), false);
			TestEqual(TEXT("nobody"), Roster.Members.Num(), 0);
			TestEqual(TEXT("refused"), Roster.Evaluate(FString(), false, false, false), ECrewJoinRefusal::ExpeditionInProgress);
		});

		It("keys a player by its unique id, else by its name", [this]()
		{
			TestEqual(TEXT("id"), CrewSession::MakeCrewKey(TEXT("HOST-1234"), TEXT("Bob")), FString(TEXT("id:HOST-1234")));
			TestEqual(TEXT("name"), CrewSession::MakeCrewKey(FString(), TEXT("Bob")), FString(TEXT("name:Bob")));
			TestTrue(TEXT("neither"), CrewSession::MakeCrewKey(FString(), FString()).IsEmpty());
			TestNotEqual(TEXT("an id never matches a name"), CrewSession::MakeCrewKey(TEXT("Bob"), FString()), CrewSession::MakeCrewKey(FString(), TEXT("Bob")));
		});
	});

	Describe("Travel", [this]()
	{
		It("reads the map of a travel URL or a package, without options, object nor PIE prefix", [this]()
		{
			TestEqual(TEXT("URL"), CrewSession::MapShortName(TEXT("/Game/MonkeIsland/Maps/L_Boot.L_Boot?listen?MaxPlayers=4")), FString(TEXT("L_Boot")));
			TestEqual(TEXT("island URL"), CrewSession::MapShortName(TEXT("/Game/MonkeIsland/Maps/Lvl_Raraka?Island=FRG_02?listen")), FString(TEXT("Lvl_Raraka")));
			TestEqual(TEXT("PIE package"), CrewSession::MapShortName(TEXT("/Game/MonkeIsland/Maps/UEDPIE_0_Lvl_Raraka")), FString(TEXT("Lvl_Raraka")));
			TestEqual(TEXT("short name"), CrewSession::MapShortName(TEXT("L_Frag_Raraka_01")), FString(TEXT("L_Frag_Raraka_01")));
			TestTrue(TEXT("options alone"), CrewSession::MapShortName(TEXT("?restart")).IsEmpty());
		});
	});

	Describe("PlayerName", [this]()
	{
		It("trims the spaces and turns a run of them into one", [this]()
		{
			TestEqual(TEXT("spaces"), CrewSession::SanitizePlayerName(TEXT("   Jean    Bart  "), 20), FString(TEXT("Jean Bart")));
			TestEqual(TEXT("wide spaces"), CrewSession::SanitizePlayerName(TEXT("Jean") + FromCodePoints({ 0xA0, 0x3000 }) + TEXT("Bart"), 20), FString(TEXT("Jean Bart")));
		});

		It("removes control and invisible characters", [this]()
		{
			TestEqual(TEXT("line breaks and tab"), CrewSession::SanitizePlayerName(TEXT("Bob\r\n\tby"), 20), FString(TEXT("Bobby")));
			TestEqual(TEXT("bidirectional override"), CrewSession::SanitizePlayerName(TEXT("Ad") + FromCodePoints({ 0x202E }) + TEXT("min"), 20), FString(TEXT("Admin")));
			TestEqual(TEXT("zero width"), CrewSession::SanitizePlayerName(TEXT("A") + FromCodePoints({ 0x200B }) + TEXT("d") + FromCodePoints({ 0x2060 }) + TEXT("min") + FromCodePoints({ 0xFEFF }), 20), FString(TEXT("Admin")));
			TestEqual(TEXT("C1 and DEL"), CrewSession::SanitizePlayerName(FromCodePoints({ 'A', 0x85, 'B', 0x7F, 'C' }), 20), FString(TEXT("ABC")));
			TestTrue(TEXT("nothing printable"), CrewSession::SanitizePlayerName(FromCodePoints({ 0x200B, 0x09, 0x20, 0x202E }), 20).IsEmpty());
		});

		It("keeps at most MaxChars characters, never a space at the end", [this]()
		{
			TestEqual(TEXT("cut at 20"), CrewSession::SanitizePlayerName(FString::ChrN(30, TEXT('a')), 20), FString::ChrN(20, TEXT('a')));
			TestEqual(TEXT("no space at the cut"), CrewSession::SanitizePlayerName(TEXT("abcdefghijklmnopqrs tuv"), 20), FString(TEXT("abcdefghijklmnopqrs")));
			TestTrue(TEXT("0 keeps nothing"), CrewSession::SanitizePlayerName(TEXT("Bob"), 0).IsEmpty());
		});

		It("keeps accents, counts a character beyond the basic plane as one and never splits it", [this]()
		{
			TestEqual(TEXT("accents"), CrewSession::SanitizePlayerName(TEXT("Éloïse Çà"), 20), FString(TEXT("Éloïse Çà")));
			FString TwentyOne;
			FString Twenty;
			for (int32 Index = 0; Index < 21; ++Index)
			{
				TwentyOne += Emoji();
				if (Index < 20)
				{
					Twenty += Emoji();
				}
			}
			TestEqual(TEXT("21 emoji -> 20"), CrewSession::SanitizePlayerName(TwentyOne, 20), Twenty);
		});

		It("drops half of a surrogate pair and piles of combining marks", [this]()
		{
			if constexpr (sizeof(TCHAR) == 2)
			{
				FString Broken(TEXT("A"));
				Broken.AppendChar(static_cast<TCHAR>(0xD800));
				Broken += TEXT("B");
				Broken.AppendChar(static_cast<TCHAR>(0xDC00));
				TestEqual(TEXT("unpaired halves"), CrewSession::SanitizePlayerName(Broken, 20), FString(TEXT("AB")));
			}
			TestEqual(TEXT("two marks at most"), CrewSession::SanitizePlayerName(FromCodePoints({ 'e', 0x301, 0x301, 0x301, 0x301, 0x301, 'x' }), 20), FromCodePoints({ 'e', 0x301, 0x301, 'x' }));
			TestEqual(TEXT("no mark without a letter"), CrewSession::SanitizePlayerName(FromCodePoints({ 0x301, 'a', ' ', 0x301, 'b' }), 20), FString(TEXT("a b")));
		});
	});

	Describe("Messages", [this]()
	{
		It("tells the refused player why, in French", [this]()
		{
			TestTrue(TEXT("expedition"), CrewSession::GetRefusalMessage(ECrewJoinRefusal::ExpeditionInProgress, 4).ToString().StartsWith(TEXT("Expédition en cours")));
			TestTrue(TEXT("full, with the places"), CrewSession::GetRefusalMessage(ECrewJoinRefusal::SessionFull, 4).ToString().Contains(TEXT("4")));
			TestFalse(TEXT("spectator"), CrewSession::GetRefusalMessage(ECrewJoinRefusal::SpectatorOnly, 4).IsEmpty());
			TestTrue(TEXT("none"), CrewSession::GetRefusalMessage(ECrewJoinRefusal::None, 4).IsEmpty());
		});

		It("shows the joining player the host's message, the engine's refusals in French, and the cause of a failure", [this]()
		{
			const FString HostMessage = CrewSession::GetRefusalMessage(ECrewJoinRefusal::ExpeditionInProgress, 4).ToString();
			TestEqual(TEXT("host message kept"), CrewSession::DescribeJoinFailure(EJoinFailureKind::Refused, HostMessage).ToString(), HostMessage);
			TestTrue(TEXT("engine full"), CrewSession::DescribeJoinFailure(EJoinFailureKind::Refused, TEXT("Server full.")).ToString().Contains(TEXT("complète")));
			TestFalse(TEXT("engine generic"), CrewSession::DescribeJoinFailure(EJoinFailureKind::Refused, TEXT("Pending Connection Failed.")).ToString().Contains(TEXT("Pending")));
			TestFalse(TEXT("empty"), CrewSession::DescribeJoinFailure(EJoinFailureKind::Refused, FString()).IsEmpty());
			TestTrue(TEXT("version"), CrewSession::DescribeJoinFailure(EJoinFailureKind::Version, TEXT("whatever")).ToString().StartsWith(TEXT("Version")));
			TestFalse(TEXT("timeout"), CrewSession::DescribeJoinFailure(EJoinFailureKind::Timeout, FString()).IsEmpty());
		});

		It("filters and bounds a message from the network", [this]()
		{
			const FText Shown = CrewSession::DescribeJoinFailure(EJoinFailureKind::Refused, FString(TEXT("Bad")) + FromCodePoints({ 0x0D, 0x0A, 0x202E }) + TEXT("host ") + FString::ChrN(500, TEXT('x')));
			TestFalse(TEXT("no control character"), Shown.ToString().Contains(FromCodePoints({ 0x0A })) || Shown.ToString().Contains(FromCodePoints({ 0x202E })));
			TestTrue(TEXT("bounded"), Shown.ToString().Len() <= CrewSession::MaxFailureMessageChars);
		});
	});
}

#endif
