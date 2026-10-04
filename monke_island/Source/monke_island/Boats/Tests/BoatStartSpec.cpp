// monke_island

#include "Misc/AutomationTest.h"
#include "Boats/BoatStart.h"
#include "Boats/BoatStartRules.h"
#include "Boats/BoatStartSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "Kismet/GameplayStatics.h"

#if WITH_AUTOMATION_TESTS

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Boats.Start
BEGIN_DEFINE_SPEC(FBoatStartSpec, "monke_island.Boats.Start",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	TArray<FName> Ids;
END_DEFINE_SPEC(FBoatStartSpec)

void FBoatStartSpec::Define()
{
	// T-141 (REC finding 8): the island really played reaches the expedition through an event, Boats/ stays independent
	Describe("ArrivalEvent", [this]()
	{
		It("tells the fragment and the boat transform once, to listeners before and after the event", [this]()
		{
			TStrongObjectPtr<UBoatStartSubsystem> BoatStart(NewObject<UBoatStartSubsystem>());
			const FTransform Boat(FRotator(0.0f, 35.0f, 0.0f), FVector(1200.0, -800.0, 20.0));
			int32 Calls = 0;
			FName Seen;
			FVector SeenLocation = FVector::ZeroVector;
			BoatStart->CallOrRegisterOnArrivalResolved(FOnBoatArrivalResolved::FDelegate::CreateLambda(
				[&Calls, &Seen, &SeenLocation](FName FragmentId, const FTransform& Transform)
				{
					++Calls;
					Seen = FragmentId;
					SeenLocation = Transform.GetLocation();
				}));
			TestEqual(TEXT("nothing before the arrival"), Calls, 0);

			BoatStart->NotifyArrivalResolved(TEXT("FRG_Raraka_07"), Boat);
			BoatStart->NotifyArrivalResolved(TEXT("FRG_Raraka_02"), FTransform::Identity);
			TestEqual(TEXT("fired once"), Calls, 1);
			TestEqual(TEXT("fragment"), Seen, FName(TEXT("FRG_Raraka_07")));
			TestTrue(TEXT("boat transform"), SeenLocation.Equals(Boat.GetLocation()));
			TestEqual(TEXT("arrival fragment kept"), BoatStart->GetArrivalFragmentId(), FName(TEXT("FRG_Raraka_07")));

			FName Late;
			const FDelegateHandle Handle = BoatStart->CallOrRegisterOnArrivalResolved(FOnBoatArrivalResolved::FDelegate::CreateLambda(
				[&Late](FName FragmentId, const FTransform&) { Late = FragmentId; }));
			TestEqual(TEXT("a late listener is called right away"), Late, FName(TEXT("FRG_Raraka_07")));
			TestFalse(TEXT("and not registered"), Handle.IsValid());
		});
	});

	BeforeEach([this]()
	{
		// points as UBoatStartSubsystem sorts them: by fragment, two points for the second fragment
		Ids = {FName(TEXT("FRG_Raraka_01")), FName(TEXT("FRG_Raraka_02")), FName(TEXT("FRG_Raraka_02")), FName(TEXT("FRG_Raraka_03"))};
	});

	Describe("ArrivalPoint", [this]()
	{
		It("takes the point of the fragment named by ?Island=", [this]()
		{
			for (int32 Seed = 1; Seed <= 20; ++Seed)
			{
				TestEqual(TEXT("FRG_Raraka_03"), BoatStart::ChooseArrivalIndex(Ids, TEXT("FRG_Raraka_03"), Seed, true), 3);
				TestEqual(TEXT("FRG_Raraka_01"), BoatStart::ChooseArrivalIndex(Ids, TEXT("FRG_Raraka_01"), Seed, false), 0);
			}
		});

		It("picks among the points of the fragment when it has several", [this]()
		{
			bool bSeen[2] = {false, false};
			for (int32 Seed = 1; Seed <= 64; ++Seed)
			{
				const int32 Index = BoatStart::ChooseArrivalIndex(Ids, TEXT("FRG_Raraka_02"), Seed, false);
				TestTrue(TEXT("a point of FRG_Raraka_02"), Index == 1 || Index == 2);
				if (Index == 1 || Index == 2)
				{
					bSeen[Index - 1] = true;
				}
			}
			TestTrue(TEXT("both points used over 64 seeds"), bSeen[0] && bSeen[1]);
		});

		It("is reproducible for a seed and random without ?Island=", [this]()
		{
			TSet<int32> Seen;
			for (int32 Seed = 1; Seed <= 64; ++Seed)
			{
				const int32 Index = BoatStart::ChooseArrivalIndex(Ids, NAME_None, Seed, true);
				TestEqual(TEXT("same seed, same point"), BoatStart::ChooseArrivalIndex(Ids, NAME_None, Seed, true), Index);
				TestTrue(TEXT("valid index"), Ids.IsValidIndex(Index));
				Seen.Add(Index);
			}
			TestTrue(TEXT("several points drawn over 64 seeds"), Seen.Num() >= 3);
		});

		It("falls back to a random point for an unknown island only when allowed", [this]()
		{
			TestTrue(TEXT("random allowed"), Ids.IsValidIndex(BoatStart::ChooseArrivalIndex(Ids, TEXT("FRG_Nowhere"), 7, true)));
			TestEqual(TEXT("random refused"), BoatStart::ChooseArrivalIndex(Ids, TEXT("FRG_Nowhere"), 7, false), INDEX_NONE);
		});

		It("gives no point (so no boat) when the map has none", [this]()
		{
			const TArray<FName> None;
			TestEqual(TEXT("no option"), BoatStart::ChooseArrivalIndex(None, NAME_None, 3, true), INDEX_NONE);
			TestEqual(TEXT("with option"), BoatStart::ChooseArrivalIndex(None, TEXT("FRG_Raraka_01"), 3, true), INDEX_NONE);
		});
	});

	Describe("UrlOptions", [this]()
	{
		It("reads the island and the seed from the travel URL", [this]()
		{
			const FString Options = TEXT("?listen?Island=FRG_Raraka_02?Seed=1234");
			TestEqual(TEXT("island"), BoatStart::SanitizeFragmentId(UGameplayStatics::ParseOption(Options, BoatStart::IslandOption), 64), FName(TEXT("FRG_Raraka_02")));
			int32 Seed = 0;
			TestTrue(TEXT("seed parsed"), BoatStart::ParseSeed(UGameplayStatics::ParseOption(Options, BoatStart::SeedOption), Seed));
			TestEqual(TEXT("seed"), Seed, 1234);
		});

		It("rejects an untrusted island id that is not a plain identifier", [this]()
		{
			TestTrue(TEXT("empty"), BoatStart::SanitizeFragmentId(TEXT(""), 64).IsNone());
			TestTrue(TEXT("path"), BoatStart::SanitizeFragmentId(TEXT("../../Game/Maps"), 64).IsNone());
			TestTrue(TEXT("spaces"), BoatStart::SanitizeFragmentId(TEXT("FRG Raraka"), 64).IsNone());
			TestTrue(TEXT("too long"), BoatStart::SanitizeFragmentId(FString::ChrN(65, TEXT('A')), 64).IsNone());
			TestEqual(TEXT("valid"), BoatStart::SanitizeFragmentId(TEXT(" FRG_Raraka-01 "), 64), FName(TEXT("FRG_Raraka-01")));
		});

		It("rejects a seed that is not an integer", [this]()
		{
			int32 Seed = 5;
			TestFalse(TEXT("absent"), BoatStart::ParseSeed(TEXT(""), Seed));
			TestFalse(TEXT("text"), BoatStart::ParseSeed(TEXT("abc"), Seed));
			TestFalse(TEXT("decimal"), BoatStart::ParseSeed(TEXT("1.5"), Seed));
			TestFalse(TEXT("overflow"), BoatStart::ParseSeed(TEXT("99999999999"), Seed));
			TestTrue(TEXT("negative"), BoatStart::ParseSeed(TEXT("-42"), Seed));
			TestEqual(TEXT("negative value"), Seed, -42);
		});
	});

	Describe("Seats", [this]()
	{
		It("puts the first player on the first seat of the order (the oars), then the others", [this]()
		{
			const TArray<int32> Order = {2, 0, 1, 3};
			TArray<bool> Occupied = {false, false, false, false};
			TArray<int32> Given;
			for (int32 Player = 0; Player < 4; ++Player)
			{
				const int32 Seat = BoatStart::ChooseSeat(Occupied, Order);
				Given.Add(Seat);
				if (Occupied.IsValidIndex(Seat))
				{
					Occupied[Seat] = true;
				}
			}
			TestTrue(TEXT("seats in order 2, 0, 1, 3"), Given == TArray<int32>({2, 0, 1, 3}));
		});

		It("leaves the players beyond the seats standing", [this]()
		{
			const UBoatStartRules* Rules = GetDefault<UBoatStartRules>();
			TArray<bool> Occupied = {false, false};
			int32 Standing = 0;
			for (int32 Player = 0; Player < 4; ++Player)
			{
				const int32 Seat = BoatStart::ChooseSeat(Occupied, Rules->SeatOrder);
				if (Seat == INDEX_NONE)
				{
					++Standing;
					continue;
				}
				TestFalse(TEXT("seat free"), Occupied[Seat]);
				Occupied[Seat] = true;
			}
			TestEqual(TEXT("2 seats, 4 players: 2 standing"), Standing, 2);
			TestTrue(TEXT("standing spots defined"), Rules->StandingSpots.Num() > 0);
			TestFalse(TEXT("two standing players on different spots"),
				BoatStart::StandingSpot(Rules->StandingSpots, 0).Equals(BoatStart::StandingSpot(Rules->StandingSpots, 1)));
		});

		It("skips taken or unknown seats of the order and falls back to any free seat", [this]()
		{
			const TArray<bool> Occupied = {true, false, true};
			TestEqual(TEXT("order with a bad index"), BoatStart::ChooseSeat(Occupied, TArray<int32>({7, 0, 2})), 1);
			TestEqual(TEXT("empty order"), BoatStart::ChooseSeat(Occupied, TArray<int32>()), 1);
			TestEqual(TEXT("boat without seats"), BoatStart::ChooseSeat(TArray<bool>(), TArray<int32>({0})), INDEX_NONE);
		});
	});
}

#endif
