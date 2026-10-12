// monke_island

#include "Misc/AutomationTest.h"
#include "Creatures/TikiBloodSpecies.h"

#if WITH_AUTOMATION_TESTS

namespace
{
	/** Species table like DA_BloodSpecies: the bean (red), the coconut warrior (sap), the crab guardian (hemolymph) */
	TArray<FTikiBloodSpecies> MakeSpecies()
	{
		TArray<FTikiBloodSpecies> Species;
		FTikiBloodSpecies& Bean = Species.AddDefaulted_GetRef();
		Bean.Name = TEXT("Bean");
		Bean.Skeletons.Add(TSoftObjectPtr<USkeleton>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton.SK_Bean_Body_Skeleton"))));
		FTikiBloodSpecies& Coco = Species.AddDefaulted_GetRef();
		Coco.Name = TEXT("Coco");
		Coco.Color = FLinearColor(0.28f, 0.10f, 0.012f, 1.0f);
		Coco.Skeletons.Add(TSoftObjectPtr<USkeleton>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/CocoWarrior/Meshes/SK_CocoWarrior_Skeleton.SK_CocoWarrior_Skeleton"))));
		FTikiBloodSpecies& Crab = Species.AddDefaulted_GetRef();
		Crab.Name = TEXT("Crab");
		Crab.Color = FLinearColor(0.015f, 0.09f, 0.30f, 1.0f);
		Crab.Skeletons.Add(TSoftObjectPtr<USkeleton>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/CrabGuardian/Meshes/SK_CrabGuardian_Skeleton.SK_CrabGuardian_Skeleton"))));
		return Species;
	}
}

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Tiki.BloodSpecies
BEGIN_DEFINE_SPEC(FTikiBloodSpeciesSpec, "monke_island.Tiki.BloodSpecies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FTikiBloodSpeciesSpec)

void FTikiBloodSpeciesSpec::Define()
{
	Describe("Species of a body (T-162)", [this]()
	{
		It("is the species that lists the skeleton of its mesh", [this]()
		{
			const TArray<FTikiBloodSpecies> Species = MakeSpecies();
			TestEqual(TEXT("bean"), TikiBloodSpecies::Find(Species, FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton.SK_Bean_Body_Skeleton"))), 0);
			TestEqual(TEXT("coconut warrior"), TikiBloodSpecies::Find(Species, FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/CocoWarrior/Meshes/SK_CocoWarrior_Skeleton.SK_CocoWarrior_Skeleton"))), 1);
			TestEqual(TEXT("crab guardian"), TikiBloodSpecies::Find(Species, FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/CrabGuardian/Meshes/SK_CrabGuardian_Skeleton.SK_CrabGuardian_Skeleton"))), 2);
		});

		It("is none for a skeleton no species lists (a weapon, a new creature): the blood of its rules", [this]()
		{
			const TArray<FTikiBloodSpecies> Species = MakeSpecies();
			TestEqual(TEXT("unlisted"), TikiBloodSpecies::Find(Species, FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/AshWatcher/Meshes/SK_AshWatcher_Skeleton.SK_AshWatcher_Skeleton"))), INDEX_NONE);
			TestEqual(TEXT("no skeleton"), TikiBloodSpecies::Find(Species, FSoftObjectPath()), INDEX_NONE);
			TestEqual(TEXT("no table"), TikiBloodSpecies::Find({}, FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton.SK_Bean_Body_Skeleton"))), INDEX_NONE);
		});

		It("gives every species its own colour, the humans the dark red of the rules", [this]()
		{
			const TArray<FTikiBloodSpecies> Species = MakeSpecies();
			const FLinearColor Red = FTikiBloodSpecies().Color;
			TestTrue(TEXT("default: a dark red"), Red.R > 5.0f * Red.G && Red.R > 5.0f * Red.B && Red.R < 0.5f);
			TestTrue(TEXT("sap is not red"), Species[1].Color.G > 0.2f * Species[1].Color.R);
			TestTrue(TEXT("hemolymph is blue"), Species[2].Color.B > Species[2].Color.R && Species[2].Color.B > Species[2].Color.G);
		});
	});

	Describe("Spray without a body (chunks of a burst)", [this]()
	{
		It("takes the species of the wound bled at the same place in the same frame", [this]()
		{
			const FVector Wound(100.0, 200.0, 50.0);
			TestTrue(TEXT("same frame, close"), TikiBloodSpecies::IsSameWound(42, Wound + FVector(0.0, 120.0, 0.0), 42, Wound, 300.0f));
			TestFalse(TEXT("next frame"), TikiBloodSpecies::IsSameWound(43, Wound, 42, Wound, 300.0f));
			TestFalse(TEXT("too far"), TikiBloodSpecies::IsSameWound(42, Wound + FVector(0.0, 400.0, 0.0), 42, Wound, 300.0f));
		});
	});
}

#endif
