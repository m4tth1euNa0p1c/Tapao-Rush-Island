// monke_island

#include "Flood/FloodSimulationActor.h"
#include "Flood/FloodRules.h"
#include "Flood/FloodSchedule.h"
#include "DayNight/DayNightCycleActor.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

AFloodSimulationActor::AFloodSimulationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(1.0f);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AFloodSimulationActor::BeginPlay()
{
	Super::BeginPlay();

	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s has no DA_FloodRules: using the default rules"), *GetName());
		Rules = NewObject<UFloodRules>(this);
	}
	WaterMaterial = Rules->WaterMaterial.LoadSynchronous();

	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	BaseSeaLevel = (Water ? Water->GetSeaLevel() : 0.0f) / 100.0f;

	const float CellCm = Rules->CellSizeCm;
	Grid.Init(Rules->GridCells, Rules->GridCells, CellCm / 100.0f);
	const FVector Center = GetActorLocation();
	Origin = FVector2D(Center.X, Center.Y) - FVector2D(0.5 * Rules->GridCells * CellCm);
	TraceCursor = 0;
	bGroundReady = false;

	// the sea surfaces are planes, not ground
	IgnoredSurfaces.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(UIslandWaterSubsystem::WaterSurfaceTag))
		{
			IgnoredSurfaces.Add(*It);
		}
	}

	ChunksX = FMath::DivideAndRoundUp(Grid.GetSizeX() - 1, Rules->ChunkCells);
	ChunksY = FMath::DivideAndRoundUp(Grid.GetSizeY() - 1, Rules->ChunkCells);
	Chunks.Reset();
	for (int32 Index = 0; Index < ChunksX * ChunksY; ++Index)
	{
		UProceduralMeshComponent* Chunk = NewObject<UProceduralMeshComponent>(this, *FString::Printf(TEXT("FloodChunk_%d"), Index));
		Chunk->SetupAttachment(RootComponent);
		Chunk->SetUsingAbsoluteRotation(true);
		Chunk->SetUsingAbsoluteScale(true);
		Chunk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Chunk->SetCastShadow(false);
		Chunk->bUseAsyncCooking = true;
		Chunk->RegisterComponent();
		Chunks.Add(Chunk);
	}

	if (UIslandWaterSubsystem* WaterSubsystem = UIslandWaterSubsystem::Get(this))
	{
		TWeakObjectPtr<AFloodSimulationActor> WeakThis(this);
		WaterSubsystem->SetSurfaceProvider(this, [WeakThis](const FVector& Location, float& OutSurfaceZ)
		{
			const AFloodSimulationActor* Flood = WeakThis.Get();
			return Flood && Flood->GetFloodSurfaceAt(Location, OutSurfaceZ);
		});
	}
}

void AFloodSimulationActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AExpeditionGameState* GameState = Expedition.Get())
	{
		GameState->OnSubmersionEnded.RemoveDynamic(this, &AFloodSimulationActor::HandleSubmersionEnded);
	}
	Expedition.Reset();
	if (UIslandWaterSubsystem* WaterSubsystem = UIslandWaterSubsystem::Get(this))
	{
		WaterSubsystem->SetSurfaceProvider(this, nullptr);
	}
	Super::EndPlay(EndPlayReason);
}

void AFloodSimulationActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Rules)
	{
		return;
	}
	if (!bGroundReady)
	{
		ReadGroundBatch();
		return;
	}

	if (!ResolveClock())
	{
		// A client waits for the replicated game state before choosing its clock
		return;
	}
	if (ClockSource == EHazardClockSource::Disabled)
	{
		SetActorTickEnabled(false);
		return;
	}

	Simulate(GetFloodSeconds());

	if (!bStarted)
	{
		return;
	}
	MeshTimer -= DeltaSeconds;
	if (MeshTimer <= 0.0f || ChunkCursor != 0)
	{
		const int32 Total = Chunks.Num();
		for (int32 Count = 0; Count < Rules->ChunksPerFrame && Total > 0; ++Count)
		{
			BuildChunk(ChunkCursor);
			ChunkCursor = (ChunkCursor + 1) % Total;
			if (ChunkCursor == 0)
			{
				break;
			}
		}
		if (ChunkCursor == 0)
		{
			MeshTimer = Rules->MeshUpdateInterval;
		}
	}
}

double AFloodSimulationActor::GetElapsedSeconds() const
{
	if (const ADayNightCycleActor* Clock = ADayNightCycleActor::Get(this))
	{
		return Clock->GetElapsedSeconds();
	}
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

bool AFloodSimulationActor::ResolveClock()
{
	if (ClockSource != EHazardClockSource::Undecided)
	{
		return true;
	}
	ClockSource = HazardClock::ResolveForWorld(Rules->ClockMode, GetWorld());
	if (ClockSource == EHazardClockSource::Undecided)
	{
		return false;
	}

	HazardClock::LogDecision(this, TEXT("the rising water"), Rules->ClockMode, ClockSource);
	if (ClockSource == EHazardClockSource::Expedition)
	{
		AExpeditionGameState* GameState = AExpeditionGameState::Get(this);
		check(GameState && GameState->Rules);
		Expedition = GameState;
		GameState->OnSubmersionEnded.AddUniqueDynamic(this, &AFloodSimulationActor::HandleSubmersionEnded);
		// A late joiner may arrive after the end of the submersion: the water is already up
		bRiseComplete = GameState->GetPhase() == EExpeditionPhase::Resolved;
		UE_LOG(Logmonke_island, Log, TEXT("%s: the sea rises at the expedition deadline, %.0f cm in %.0f s (DA_ExpeditionRules); DA_FloodRules timing ignored"),
			*GetName(), GameState->Rules->MaxWaterRiseCm, GameState->Rules->SubmersionDuration);
	}
	return true;
}

double AFloodSimulationActor::GetFloodSeconds() const
{
	switch (ClockSource)
	{
	case EHazardClockSource::Expedition:
	{
		const AExpeditionGameState* GameState = Expedition.Get();
		if (!GameState)
		{
			return -1.0;
		}
		const FExpeditionClockState& State = GameState->GetClockState();
		const double Now = GameState->GetServerWorldTimeSeconds();
		// The server owns the clock (no bonus once its time passed the deadline); a client waits for the replicated phase
		const bool bConfirmed = GameState->GetPhase() >= EExpeditionPhase::Submerging
			|| (GameState->HasAuthority() && State.bStarted && Now >= State.FloodStartTime);
		return FloodSchedule::ExpeditionFloodSeconds(State, Now, bConfirmed);
	}
	case EHazardClockSource::DayNight:
		return FloodSchedule::DayNightFloodSeconds(*Rules, GetElapsedSeconds());
	default:
		return -1.0;
	}
}

double AFloodSimulationActor::ComputeRiseCm(double FloodSeconds) const
{
	switch (ClockSource)
	{
	case EHazardClockSource::Expedition:
	{
		const AExpeditionGameState* GameState = Expedition.Get();
		if (!GameState || !GameState->Rules)
		{
			return 0.0;
		}
		return FloodSchedule::ExpeditionRiseCm(GameState->GetClockState(), FloodSeconds, FExpeditionClockParams::FromRules(*GameState->Rules));
	}
	case EHazardClockSource::DayNight:
		return FloodSchedule::DayNightRiseCm(*Rules, FloodSeconds);
	default:
		return 0.0;
	}
}

void AFloodSimulationActor::HandleSubmersionEnded()
{
	if (bRiseComplete)
	{
		return;
	}
	bRiseComplete = true;
	UE_LOG(Logmonke_island, Log, TEXT("%s: end of the submersion (expedition clock) at +%.0f cm: the sea keeps rising (PostSubmersionRiseCmPerSecond)"), *GetName(), HeldRiseCm);
}

float AFloodSimulationActor::RiseAt(const UFloodRules& InRules, float Seconds)
{
	return static_cast<float>(FloodSchedule::DayNightRiseCm(InRules, Seconds));
}

float AFloodSimulationActor::GetCurrentRiseCm() const
{
	return bStarted ? static_cast<float>(HeldRiseCm) : 0.0f;
}

void AFloodSimulationActor::ReadGroundBatch()
{
	UWorld* World = GetWorld();
	const int32 Count = Grid.GetSizeX() * Grid.GetSizeY();
	if (!World || Count == 0)
	{
		bGroundReady = true;
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FloodGround), true, this);
	for (const TWeakObjectPtr<AActor>& Surface : IgnoredSurfaces)
	{
		if (const AActor* SurfaceActor = Surface.Get())
		{
			Params.AddIgnoredActor(SurfaceActor);
		}
	}

	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const float CellCm = Rules->CellSizeCm;
	const int32 End = FMath::Min(Count, TraceCursor + Rules->TracesPerFrame);
	for (; TraceCursor < End; ++TraceCursor)
	{
		const int32 X = TraceCursor % Grid.GetSizeX();
		const int32 Y = TraceCursor / Grid.GetSizeX();
		const double WX = Origin.X + (X + 0.5) * CellCm;
		const double WY = Origin.Y + (Y + 0.5) * CellCm;
		double Top = Rules->TraceTopCm;
		float GroundCm = Rules->TraceBottomCm;
		// skip vegetation (instanced meshes): only the terrain and the built ground count
		for (int32 Attempt = 0; Attempt < 4; ++Attempt)
		{
			FHitResult Hit;
			if (!World->LineTraceSingleByObjectType(Hit, FVector(WX, WY, Top), FVector(WX, WY, Rules->TraceBottomCm), Objects, Params))
			{
				break;
			}
			if (Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
			{
				Top = Hit.ImpactPoint.Z - 5.0;
				continue;
			}
			GroundCm = static_cast<float>(Hit.ImpactPoint.Z);
			break;
		}
		Grid.Ground[TraceCursor] = GroundCm / 100.0f;
	}

	if (TraceCursor >= Count)
	{
		bGroundReady = true;
		UE_LOG(Logmonke_island, Log, TEXT("%s read the ground: %d x %d cells of %.1f m"), *GetName(), Grid.GetSizeX(), Grid.GetSizeY(), Grid.GetCellSize());
	}
}

void AFloodSimulationActor::Simulate(double FloodSeconds)
{
	if (FloodSeconds < 0.0)
	{
		return;
	}
	if (!bStarted)
	{
		Grid.SetupSea(BaseSeaLevel, Rules->OpenSeaDepthCm / 100.0f);
		bStarted = true;
		StepsDone = 0;
		HeldRiseCm = 0.0;
		MeshTimer = 0.0f;
		if (ClockSource == EHazardClockSource::Expedition)
		{
			const AExpeditionGameState* GameState = Expedition.Get();
			UE_LOG(Logmonke_island, Log, TEXT("%s: the sea starts rising at the expedition deadline (%.2f s server time)"), *GetName(),
				GameState ? GameState->GetFloodStartTime() : -1.0);
		}
		else
		{
			UE_LOG(Logmonke_island, Log, TEXT("%s: the sea starts rising on the day / night clock (%.0f cm in %.0f s)"), *GetName(), Rules->RiseCm, Rules->RiseSeconds);
		}
	}

	const double Dt = Rules->StepSeconds;
	const int64 Target = static_cast<int64>(FloodSeconds / Dt);
	for (int32 Step = 0; Step < Rules->MaxStepsPerFrame && StepsDone < Target; ++Step)
	{
		++StepsDone;
		// The sea target only ever rises, whatever the clock says (late replication, rounding)
		HeldRiseCm = FloodSchedule::HoldRise(HeldRiseCm, ComputeRiseCm(StepsDone * Dt));
		const float Sea = BaseSeaLevel + static_cast<float>(HeldRiseCm / 100.0);
		Grid.Step(static_cast<float>(Dt), Sea, Rules->Gravity, Rules->FluxDamping);
	}

	if (!bRiseComplete && ClockSource == EHazardClockSource::DayNight && StepsDone * Dt >= Rules->RiseSeconds)
	{
		bRiseComplete = true;
		UE_LOG(Logmonke_island, Log, TEXT("%s: full rise reached on the day / night clock (+%.0f cm)"), *GetName(), HeldRiseCm);
	}
}

bool AFloodSimulationActor::GetFloodSurfaceAt(const FVector& Location, float& OutSurfaceZ) const
{
	if (!bStarted || !Rules)
	{
		return false;
	}
	const float CellCm = Rules->CellSizeCm;
	const int32 X = FMath::FloorToInt32((Location.X - Origin.X) / CellCm);
	const int32 Y = FMath::FloorToInt32((Location.Y - Origin.Y) / CellCm);
	if (X < 0 || Y < 0 || X >= Grid.GetSizeX() || Y >= Grid.GetSizeY())
	{
		return false;
	}
	const int32 I = Grid.Index(X, Y);
	if (Grid.Depth[I] * 100.0f < Rules->MinDrawDepthCm)
	{
		return false;
	}
	OutSurfaceZ = Grid.Surface(I) * 100.0f;
	return true;
}

void AFloodSimulationActor::BuildChunk(int32 ChunkIndex)
{
	UProceduralMeshComponent* Chunk = Chunks.IsValidIndex(ChunkIndex) ? Chunks[ChunkIndex].Get() : nullptr;
	if (!Chunk)
	{
		return;
	}

	const int32 N = Rules->ChunkCells;
	const int32 X0 = (ChunkIndex % ChunksX) * N;
	const int32 Y0 = (ChunkIndex / ChunksX) * N;
	const int32 X1 = FMath::Min(X0 + N, Grid.GetSizeX() - 1);
	const int32 Y1 = FMath::Min(Y0 + N, Grid.GetSizeY() - 1);
	const int32 W = X1 - X0 + 1;
	const int32 H = Y1 - Y0 + 1;
	const float CellCm = Rules->CellSizeCm;
	const float MinDepth = Rules->MinDrawDepthCm / 100.0f;
	// open sea at its normal level is drawn by the sea surface itself
	const float MinSurface = BaseSeaLevel + MinDepth;
	const FVector ActorLocation = GetActorLocation();

	auto IsWet = [&](int32 I)
	{
		return Grid.Depth[I] > MinDepth && Grid.Surface(I) > MinSurface;
	};

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<int32> VertexOf;
	VertexOf.Init(INDEX_NONE, W * H);

	auto VertexFor = [&](int32 LX, int32 LY) -> int32
	{
		int32& Slot = VertexOf[LY * W + LX];
		if (Slot != INDEX_NONE)
		{
			return Slot;
		}
		const int32 GX = X0 + LX;
		const int32 GY = Y0 + LY;
		const int32 I = Grid.Index(GX, GY);
		float Z = Grid.Surface(I);
		if (!IsWet(I))
		{
			// dry corner of a shore quad: take the highest wet neighbour, the ground hides the overlap
			float Best = -FLT_MAX;
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					const int32 NX = GX + DX;
					const int32 NY = GY + DY;
					if (NX >= 0 && NY >= 0 && NX < Grid.GetSizeX() && NY < Grid.GetSizeY() && IsWet(Grid.Index(NX, NY)))
					{
						Best = FMath::Max(Best, Grid.Surface(Grid.Index(NX, NY)));
					}
				}
			}
			Z = Best > -FLT_MAX ? FMath::Min(Best, Grid.Ground[I]) : Grid.Ground[I];
		}
		const double WX = Origin.X + (GX + 0.5) * CellCm;
		const double WY = Origin.Y + (GY + 0.5) * CellCm;
		Slot = Vertices.Add(FVector(WX - ActorLocation.X, WY - ActorLocation.Y, Z * 100.0 - ActorLocation.Z));
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D(WX / 1000.0, WY / 1000.0));
		return Slot;
	};

	for (int32 LY = 0; LY < H - 1; ++LY)
	{
		for (int32 LX = 0; LX < W - 1; ++LX)
		{
			const int32 I00 = Grid.Index(X0 + LX, Y0 + LY);
			const int32 I10 = I00 + 1;
			const int32 I01 = I00 + Grid.GetSizeX();
			const int32 I11 = I01 + 1;
			if (!IsWet(I00) && !IsWet(I10) && !IsWet(I01) && !IsWet(I11))
			{
				continue;
			}
			const int32 A = VertexFor(LX, LY);
			const int32 B = VertexFor(LX + 1, LY);
			const int32 C = VertexFor(LX, LY + 1);
			const int32 D = VertexFor(LX + 1, LY + 1);
			// both windings: the surface is seen from above and from under water
			Triangles.Append({A, C, B, B, C, D, A, B, C, B, D, C});
		}
	}

	if (Triangles.Num() == 0)
	{
		if (Chunk->GetNumSections() > 0)
		{
			Chunk->ClearAllMeshSections();
		}
		return;
	}
	Chunk->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	if (WaterMaterial)
	{
		Chunk->SetMaterial(0, WaterMaterial);
	}
}

#if !UE_BUILD_SHIPPING

static FAutoConsoleCommandWithWorld GFloodNowCommand(
	TEXT("monke.FloodNow"),
	TEXT("Jumps the day / night clock to 5 s before the rising water (server, day / night fallback only)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ADayNightCycleActor* Clock = ADayNightCycleActor::Get(World);
		for (TActorIterator<AFloodSimulationActor> It(World); It && Clock; ++It)
		{
			if (It->GetClockSource() == EHazardClockSource::Expedition)
			{
				UE_LOG(Logmonke_island, Warning, TEXT("monke.FloodNow: %s follows the expedition clock, the day / night jump does not move the water"), *It->GetName());
			}
			else if (It->Rules)
			{
				Clock->SetElapsedSeconds(FMath::Max(0.0f, It->Rules->StartSeconds - 5.0f));
			}
			break;
		}
	}));

#endif
