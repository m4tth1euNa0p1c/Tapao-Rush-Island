// Tidemark plugin

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "TidemarkEditor/Import/CollisionHeightSource.h"

// Run with: ./scripts/ue.ps1 test -Filter Tidemark.Import.Collision
//
// Fixture: a private editor world (physics scene with trace collision, no engine world context) holding
//   - cube A, 10 m wide, top at +200 cm, centred on (0, 0)
//   - cube B, 10 m wide, top at +800 cm, centred on (2000, 0)
//   - an actor tagged TidemarkIgnore above cube A, 3 m wide, top at +400 cm
//   - an instanced foliage component (two instances) above cube B, top at +1000 cm
// Nothing at (-5000, -5000): a missed cell.
// Cells are traced at 400 cm from (-6000, -6000), 30 x 30 cells, so that cell centres fall on (0 + 200k).
//
// Manual recipe when the fixture is fragile on a machine (Chaos flush timing): open L_Frag_Raraka_01, run
// `tidemark.BakeCollision /Game/<Project>/Tidemark/TA_<Map> 600`, expect < 2 min for 14 Nanite tiles and
// a log line "tidemark.BakeCollision: ... baked in N s" with less than 5 % missed cells; then check the asset's grid
// histogram with the verify step of /tidemark-terrain-import.
BEGIN_DEFINE_SPEC(FTidemarkCollisionSpec, "Tidemark.Import.Collision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* World = nullptr;
	UStaticMesh* CubeMesh = nullptr;

	/** Lattice of the fixture: 30 x 30 cells of 400 cm from (-6000, -6000); cell centres on -5800 + 400k. */
	static constexpr int32 LatticeSize = 30;
	static constexpr float LatticeCellCm = 400.0f;
	static const FVector2D LatticeOrigin;

	/** Spawns a static mesh cube of 100 cm scaled to Scale, centred at Location, blocking WorldStatic queries. */
	AStaticMeshActor* SpawnCube(const FVector& Location, const FVector& Scale, const FName& Tag = NAME_None)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, Params);
		if (!Actor)
		{
			return nullptr;
		}
		Actor->SetActorScale3D(Scale);
		if (!Tag.IsNone())
		{
			Actor->Tags.Add(Tag);
		}
		UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
		Component->SetStaticMesh(CubeMesh);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionObjectType(ECC_WorldStatic);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->RecreatePhysicsState();
		return Actor;
	}

	/** Spawns an actor with an instanced static mesh component (foliage-like) of cubes at the given locations. */
	AActor* SpawnFoliage(const TArray<FVector>& Locations)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Actor)
		{
			return nullptr;
		}
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Actor, TEXT("TestFoliage"));
		Actor->SetRootComponent(Component);
		Component->SetStaticMesh(CubeMesh);
		Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionObjectType(ECC_WorldStatic);
		Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->RegisterComponent();
		for (const FVector& Location : Locations)
		{
			Component->AddInstance(FTransform(FRotator::ZeroRotator, Location, FVector(2.0f, 2.0f, 2.0f)), /*bWorldSpace*/ true);
		}
		Component->RecreatePhysicsState();
		return Actor;
	}

	/** Makes new bodies visible to scene queries (Chaos updates its acceleration structure on flush). */
	void FlushPhysics()
	{
		if (FPhysScene* Scene = World ? World->GetPhysicsScene() : nullptr)
		{
			Scene->Flush();
		}
	}

	FTidemarkCollisionSettings MakeSettings() const
	{
		FTidemarkCollisionSettings Settings;
		Settings.BoundsWorld = FBox2D(LatticeOrigin, LatticeOrigin + FVector2D(LatticeSize * LatticeCellCm, LatticeSize * LatticeCellCm));
		Settings.TraceTopCm = 5000.0f;
		Settings.TraceBottomCm = -40000.0f;
		Settings.BatchSize = 256;
		return Settings;
	}
END_DEFINE_SPEC(FTidemarkCollisionSpec)

const FVector2D FTidemarkCollisionSpec::LatticeOrigin(-6000.0, -6000.0);

void FTidemarkCollisionSpec::Define()
{
	BeforeEach([this]()
	{
		CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		TestNotNull(TEXT("engine cube"), CubeMesh);

		World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false, TEXT("TidemarkCollisionSpec"));
		TestNotNull(TEXT("world"), World);
		if (!World || !CubeMesh)
		{
			return;
		}

		// Cube A: 10 m wide, 4 m tall, centre z = 0 => top +200.
		SpawnCube(FVector(0.0, 0.0, 0.0), FVector(10.0f, 10.0f, 4.0f));
		// Cube B: 10 m wide, 4 m tall, centre z = 600 => top +800.
		SpawnCube(FVector(2000.0, 0.0, 600.0), FVector(10.0f, 10.0f, 4.0f));
		// Tagged helper above cube A: 3 m wide (+-150), 1 m tall, centre z = 350 => top +400, bottom +300.
		SpawnCube(FVector(0.0, 0.0, 350.0), FVector(3.0f, 3.0f, 1.0f), TEXT("TidemarkIgnore"));
		// Foliage above cube B: two instances of 2 m cubes, centre z = 900 => top +1000, bottom +800 (touching the ground).
		SpawnFoliage({ FVector(2000.0, 0.0, 900.0), FVector(2200.0, 0.0, 900.0) });
		FlushPhysics();
	});

	AfterEach([this]()
	{
		if (World)
		{
			World->DestroyWorld(false);
			World->RemoveFromRoot();
			World = nullptr;
		}
		CubeMesh = nullptr;
	});

	Describe("TraceCell", [this]()
	{
		It("finds the top of a cube as the highest ground under the max level", [this]()
		{
			if (!World) { return; }
			float HeightCm = 0.0f;
			int32 Ignored = 0;
			int64 Rays = 0;
			TestTrue(TEXT("hit"), FTidemarkCollisionHeightSource::TraceCell(*World, MakeSettings(), -200.0, -200.0, HeightCm, Ignored, Rays));
			TestEqual(TEXT("cube A top"), HeightCm, 200.0f, 1.0f);
			TestEqual(TEXT("one ray"), Rays, static_cast<int64>(1));
			TestEqual(TEXT("no retry"), Ignored, 0);
		});

		It("skips an actor tagged TidemarkIgnore and resumes under it", [this]()
		{
			if (!World) { return; }
			float HeightCm = 0.0f;
			int32 Ignored = 0;
			int64 Rays = 0;
			TestTrue(TEXT("hit"), FTidemarkCollisionHeightSource::TraceCell(*World, MakeSettings(), 0.0, 0.0, HeightCm, Ignored, Rays));
			TestEqual(TEXT("cube A under the helper"), HeightCm, 200.0f, 1.0f);
			TestEqual(TEXT("one ignored hit"), Ignored, 1);
			TestEqual(TEXT("two rays"), Rays, static_cast<int64>(2));
		});

		It("skips instanced foliage and keeps the ground under it", [this]()
		{
			if (!World) { return; }
			float HeightCm = 0.0f;
			int32 Ignored = 0;
			int64 Rays = 0;
			TestTrue(TEXT("hit"), FTidemarkCollisionHeightSource::TraceCell(*World, MakeSettings(), 2000.0, 0.0, HeightCm, Ignored, Rays));
			TestEqual(TEXT("cube B under the foliage"), HeightCm, 800.0f, 1.0f);
			TestEqual(TEXT("one ignored hit"), Ignored, 1);
		});

		It("counts foliage as ground when bIgnoreInstancedFoliage is off", [this]()
		{
			if (!World) { return; }
			FTidemarkCollisionSettings Settings = MakeSettings();
			Settings.bIgnoreInstancedFoliage = false;
			float HeightCm = 0.0f;
			int32 Ignored = 0;
			int64 Rays = 0;
			TestTrue(TEXT("hit"), FTidemarkCollisionHeightSource::TraceCell(*World, Settings, 2000.0, 0.0, HeightCm, Ignored, Rays));
			TestEqual(TEXT("foliage top"), HeightCm, 1000.0f, 1.0f);
			TestEqual(TEXT("no retry"), Ignored, 0);
		});

		It("misses a cell outside every mesh", [this]()
		{
			if (!World) { return; }
			float HeightCm = 0.0f;
			int32 Ignored = 0;
			int64 Rays = 0;
			TestFalse(TEXT("missed"), FTidemarkCollisionHeightSource::TraceCell(*World, MakeSettings(), -5000.0, -5000.0, HeightCm, Ignored, Rays));
			TestEqual(TEXT("one ray"), Rays, static_cast<int64>(1));
		});

		It("gives up after MaxRetries ignored hits", [this]()
		{
			if (!World) { return; }
			// Four tagged slabs stacked above cube A at the probe (100, 100): with MaxRetries = 2 the ray stops before the ground.
			for (int32 Slab = 0; Slab < 4; ++Slab)
			{
				SpawnCube(FVector(100.0, 100.0, 1000.0 + 300.0 * Slab), FVector(2.0f, 2.0f, 1.0f), TEXT("TidemarkIgnore"));
			}
			FlushPhysics();
			FTidemarkCollisionSettings Settings = MakeSettings();
			Settings.MaxRetries = 2;
			float HeightCm = 0.0f;
			int32 Ignored = 0;
			int64 Rays = 0;
			TestFalse(TEXT("exhausted"), FTidemarkCollisionHeightSource::TraceCell(*World, Settings, 100.0, 100.0, HeightCm, Ignored, Rays));
			TestEqual(TEXT("ignored = MaxRetries + 1"), Ignored, 3);
			Settings.MaxRetries = 8;
			TestTrue(TEXT("enough retries"), FTidemarkCollisionHeightSource::TraceCell(*World, Settings, 100.0, 100.0, HeightCm, Ignored, Rays));
			TestEqual(TEXT("ground after the slabs and the helper"), HeightCm, 200.0f, 1.0f);
			TestEqual(TEXT("five ignored hits"), Ignored, 5);
		});
	});

	Describe("Prefetch", [this]()
	{
		It("caches every cell once and answers samples without tracing", [this]()
		{
			if (!World) { return; }
			FTidemarkCollisionHeightSource Source(World, MakeSettings());
			TestFalse(TEXT("not prefetched"), Source.IsPrefetched());
			float Unused = 0.0f;
			TestFalse(TEXT("sample before prefetch fails"), Source.SampleHeightCm(0.0, 0.0, Unused));

			TestTrue(TEXT("prefetch"), Source.Prefetch(LatticeSize, LatticeSize, LatticeOrigin, LatticeCellCm));
			const FTidemarkCollisionBakeStats& Stats = Source.GetStats();
			TestEqual(TEXT("cells"), Stats.NumCells, LatticeSize * LatticeSize);
			TestEqual(TEXT("hit + missed"), Stats.NumHit + Stats.NumMissed, Stats.NumCells);
			TestTrue(TEXT("some hits"), Stats.NumHit > 0);
			TestTrue(TEXT("some misses"), Stats.NumMissed > 0);
			TestTrue(TEXT("retries happened"), Stats.NumRetriedCells > 0);
			TestTrue(TEXT("rays >= cells"), Stats.NumRays >= Stats.NumCells);
			TestFalse(TEXT("not cancelled"), Stats.bCancelled);

			// Cell centre (-5800 + 400k): (200, 200) is k = 15 on both axes, inside cube A (+-500), outside the helper (+-150).
			float HeightCm = 0.0f;
			TestTrue(TEXT("cell (15,15)"), Source.GetCellHeightCm(15, 15, HeightCm));
			TestEqual(TEXT("cube A"), HeightCm, 200.0f, 1.0f);
			// (2200, 200): k = 20, 15 -> cube B (+-500 around 2000), beside the foliage instances (+-100 in Y).
			TestTrue(TEXT("cell (20,15)"), Source.GetCellHeightCm(20, 15, HeightCm));
			TestEqual(TEXT("cube B"), HeightCm, 800.0f, 1.0f);
			// (-5800, -5800): k = 0, 0 -> nothing.
			TestTrue(TEXT("cell (0,0) missed"), Source.IsCellMissed(0, 0));
			TestFalse(TEXT("cell (0,0) has no height"), Source.GetCellHeightCm(0, 0, HeightCm));

			const int64 RaysAfterPrefetch = Stats.NumRays;
			TestTrue(TEXT("sample on cube A"), Source.SampleHeightCm(200.0, 200.0, HeightCm));
			TestEqual(TEXT("sample cube A"), HeightCm, 200.0f, 1.0f);
			TestFalse(TEXT("sample in the void"), Source.SampleHeightCm(-5800.0, -5800.0, HeightCm));
			TestEqual(TEXT("no ray after prefetch"), Source.GetStats().NumRays, RaysAfterPrefetch);
		});

		It("interpolates between cached centres and drops missed neighbours", [this]()
		{
			if (!World) { return; }
			FTidemarkCollisionHeightSource Source(World, MakeSettings());
			TestTrue(TEXT("prefetch"), Source.Prefetch(LatticeSize, LatticeSize, LatticeOrigin, LatticeCellCm));
			float HeightCm = 0.0f;
			// Between (200, 200) on cube A (200 cm) and (600, 200) on cube A too: flat.
			TestTrue(TEXT("flat"), Source.SampleHeightCm(400.0, 200.0, HeightCm));
			TestEqual(TEXT("flat value"), HeightCm, 200.0f, 1.0f);
			// Half-way between cell k=14 (x = -200, on cube A) and cell k=13 (x = -600, off cube A: missed):
			// the missed neighbour is dropped, the sample keeps cube A's height.
			TestTrue(TEXT("edge"), Source.SampleHeightCm(-400.0, 200.0, HeightCm));
			TestEqual(TEXT("edge keeps the ground"), HeightCm, 200.0f, 1.0f);
			// Bounds come from the lattice once prefetched.
			const FBox2D Bounds = Source.GetBoundsWorld();
			TestEqual(TEXT("bounds min"), Bounds.Min, LatticeOrigin);
			TestEqual(TEXT("bounds max x"), Bounds.Max.X, LatticeOrigin.X + LatticeSize * LatticeCellCm, 0.01);
		});

		It("refuses a lattice over the grid budget and an empty one", [this]()
		{
			if (!World) { return; }
			FTidemarkCollisionHeightSource Source(World, MakeSettings());
			TestFalse(TEXT("empty"), Source.Prefetch(0, 10, LatticeOrigin, LatticeCellCm));
			TestFalse(TEXT("too large"), Source.Prefetch(4096, 4096, LatticeOrigin, LatticeCellCm));
			TestFalse(TEXT("still not prefetched"), Source.IsPrefetched());
		});

		It("derives the lattice from the bounds with PrefetchBounds", [this]()
		{
			if (!World) { return; }
			FTidemarkCollisionHeightSource Source(World, MakeSettings());
			TestTrue(TEXT("prefetch bounds"), Source.PrefetchBounds(LatticeCellCm));
			TestEqual(TEXT("size x"), Source.GetSizeX(), LatticeSize);
			TestEqual(TEXT("size y"), Source.GetSizeY(), LatticeSize);
			TestEqual(TEXT("origin"), Source.GetOrigin(), LatticeOrigin);
			TestTrue(TEXT("description names the lattice"), Source.GetSourceDescription().Contains(TEXT("30 x 30")));
		});
	});
}

#endif // WITH_AUTOMATION_TESTS
