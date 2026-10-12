// monke_island

#include "Flood/FloodSimulationActor.h"
#include "Flood/FloodRules.h"
#include "Flood/FloodSchedule.h"
#include "Flood/FloodSeaRiseSubsystem.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatStartSubsystem.h"
#include "DayNight/DayNightCycleActor.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Expedition/Core/ExpeditionRules.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Net/UnrealNetwork.h"
#include "UObject/Package.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

FFloodActorRank FFloodActorRank::Of(const AActor& Actor)
{
	FFloodActorRank Rank;
	const ULevel* Level = Actor.GetLevel();
	Rank.bPersistentLevel = Level && Level->IsPersistentLevel();
	Rank.ActorName = Actor.GetFName().ToString();
	Rank.LevelName = Level ? UWorld::RemovePIEPrefix(Level->GetOutermost()->GetName()) : FString();
	return Rank;
}

bool FFloodActorRank::IsBefore(const FFloodActorRank& Other) const
{
	if (bPersistentLevel != Other.bPersistentLevel)
	{
		return bPersistentLevel;
	}
	if (const int32 ByName = ActorName.Compare(Other.ActorName, ESearchCase::CaseSensitive); ByName != 0)
	{
		return ByName < 0;
	}
	return LevelName.Compare(Other.LevelName, ESearchCase::CaseSensitive) < 0;
}

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
	bGridSetup = false;
	bGroundReady = false;
	// The grid is set up once the clock is known (in expedition mode on the fragment played, T-103)
	ResolveDuplicates();
}

bool AFloodSimulationActor::ResolveDuplicates()
{
	// One flood per world: two simulations would cost twice and only one feeds the water surface. Once at BeginPlay,
	// never per frame. The order is a property of the actors (FFloodActorRank), not of their BeginPlay order, so every
	// machine keeps the same one, even when a streamed level brings a better ranked actor late.
	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}
	AFloodSimulationActor* Kept = this;
	FFloodActorRank KeptRank = FFloodActorRank::Of(*this);
	for (TActorIterator<AFloodSimulationActor> It(World); It; ++It)
	{
		AFloodSimulationActor* Other = *It;
		if (Other == this || !IsValid(Other) || Other->bDuplicate)
		{
			continue;
		}
		const FFloodActorRank OtherRank = FFloodActorRank::Of(*Other);
		if (OtherRank.IsBefore(KeptRank))
		{
			Kept = Other;
			KeptRank = OtherRank;
		}
	}
	if (Kept != this)
	{
		DisableAsDuplicate(*Kept);
		return false;
	}
	for (TActorIterator<AFloodSimulationActor> It(World); It; ++It)
	{
		AFloodSimulationActor* Other = *It;
		if (Other != this && IsValid(Other) && !Other->bDuplicate && Other->HasActorBegunPlay())
		{
			Other->DisableAsDuplicate(*this);
		}
	}
	return true;
}

void AFloodSimulationActor::DisableAsDuplicate(const AFloodSimulationActor& Kept)
{
	if (bDuplicate)
	{
		return;
	}
	bDuplicate = true;
	UE_LOG(Logmonke_island, Warning, TEXT("%s (%s): another flood actor, %s (%s), is kept in this world, this one is disabled; "
		"keep a single AFloodSimulationActor per world"), *GetName(), *GetNameSafe(GetLevel() ? GetLevel()->GetOutermost() : nullptr),
		*Kept.GetName(), *GetNameSafe(Kept.GetLevel() ? Kept.GetLevel()->GetOutermost() : nullptr));

	SetActorTickEnabled(false);
	if (AExpeditionGameState* GameState = Expedition.Get())
	{
		GameState->OnSubmersionEnded.RemoveDynamic(this, &AFloodSimulationActor::HandleSubmersionEnded);
	}
	Expedition.Reset();
	if (UIslandWaterSubsystem* WaterSubsystem = UIslandWaterSubsystem::Get(this))
	{
		WaterSubsystem->SetSurfaceProvider(this, nullptr);
	}
	for (UProceduralMeshComponent* Chunk : Chunks)
	{
		if (Chunk)
		{
			Chunk->DestroyComponent();
		}
	}
	Chunks.Reset();
	ChunkFlat.Reset();
	if (Apron)
	{
		Apron->DestroyComponent();
		Apron = nullptr;
	}
	Grid = FFloodGrid();
	OutsideSea = FFloodOutsideSea();
	bGridSetup = false;
	bGroundReady = false;
	bStarted = false;
}

void AFloodSimulationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFloodSimulationActor, GridCenter);
	DOREPLIFETIME(AFloodSimulationActor, bHasGridCenter);
}

bool AFloodSimulationActor::ChooseGridCenter(FVector& OutCenter, FString& OutSource) const
{
	const UBoatStartSubsystem* BoatStart = UBoatStartSubsystem::Get(this);
	if (BoatStart && !BoatStart->HasResolved())
	{
		// the boat start picks the fragment once the world has begun play
		return false;
	}
	if (const ABoatActor* Boat = BoatStart ? BoatStart->GetArrivalBoat() : nullptr)
	{
		// the bow of the arrival boat points to the beach of the fragment
		const FVector Bow = FVector(Boat->GetActorForwardVector().X, Boat->GetActorForwardVector().Y, 0.0).GetSafeNormal();
		OutCenter = Boat->GetActorLocation() + Bow * Rules->ArrivalToGridCenterCm;
		OutSource = FString::Printf(TEXT("arrival boat of %s"), *BoatStart->GetArrivalFragmentId().ToString());
		return true;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr)
		{
			OutCenter = Pawn->GetActorLocation();
			OutSource = FString::Printf(TEXT("pawn %s (no arrival boat)"), *Pawn->GetName());
			return true;
		}
	}
	return false;
}

void AFloodSimulationActor::SetupGrid(const FVector& Center, int32 Cells, float CellSizeCm)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(AFloodSimulationActor::SetupGrid);

	bGridSetup = true;
	CellCm = CellSizeCm;
	WaterMaterial = Rules->WaterMaterial.LoadSynchronous();

	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	double SeaLevelCm = Water ? Water->GetSeaLevel() : 0.0;
	// A late joiner may set the grid up after the global sea started rising: the grid wants the level before the rise
	if (const UFloodSeaRiseSubsystem* SeaRise = UFloodSeaRiseSubsystem::Get(this); SeaRise && SeaRise->IsDrivingSea())
	{
		SeaLevelCm -= SeaRise->GetHeldRiseCm();
	}
	BaseSeaLevel = static_cast<float>(SeaLevelCm / 100.0);

	Grid.Init(Cells, Cells, CellCm / 100.0f);
	Origin = FVector2D(Center.X, Center.Y) - FVector2D(0.5 * Cells * CellCm);
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
	ChunkFlat.Init(0, ChunksX * ChunksY);
	ChunkCursor = 0;
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
	UE_LOG(Logmonke_island, Log, TEXT("%s: flood grid of %d x %d cells of %.1f m (%.2f km) centred on X=%.0f Y=%.0f, %d chunks"),
		*GetName(), Cells, Cells, CellCm / 100.0f, Cells * CellCm / 100000.0f, Center.X, Center.Y, Chunks.Num());
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

	if (!Rules || bDuplicate)
	{
		return;
	}
	if (!bGridSetup)
	{
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
		if (ClockSource == EHazardClockSource::Expedition && Rules->bLocalGridInExpedition)
		{
			// T-103: the water rises on the fragment played, wherever the boat landed, never at the spawn
			if (!bHasGridCenter)
			{
				if (!HasAuthority())
				{
					// a client waits for the centre chosen by the server
					return;
				}
				FString Source;
				if (!ChooseGridCenter(GridCenter, Source))
				{
					return;
				}
				bHasGridCenter = true;
				ForceNetUpdate();
				UE_LOG(Logmonke_island, Log, TEXT("%s: flood grid laid on the fragment played, from the %s"), *GetName(), *Source);
			}
			SetupGrid(GridCenter, Rules->ExpeditionGridCells, Rules->ExpeditionCellSizeCm);
		}
		else
		{
			SetupGrid(GetActorLocation(), Rules->GridCells, Rules->CellSizeCm);
		}
	}
	if (!bGroundReady)
	{
		ReadGroundBatch();
		return;
	}

	Simulate(GetFloodSeconds());

	if (!bStarted)
	{
		return;
	}
	UpdateApron();
	MeshTimer -= DeltaSeconds;
	if (MeshTimer <= 0.0f || ChunkCursor != 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(AFloodSimulationActor::BuildChunks);
		const int32 Total = Chunks.Num();
		// a round of rebuilds is spread over the frames: at most ChunksPerFrame chunks and MeshBudgetMs per frame
		const double Deadline = FPlatformTime::Seconds() + Rules->MeshBudgetMs / 1000.0;
		for (int32 Count = 0; Count < Rules->ChunksPerFrame && Total > 0; ++Count)
		{
			BuildChunk(ChunkCursor);
			ChunkCursor = (ChunkCursor + 1) % Total;
			if (ChunkCursor == 0 || FPlatformTime::Seconds() >= Deadline)
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
	TRACE_CPUPROFILER_EVENT_SCOPE(AFloodSimulationActor::ReadGroundBatch);
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
		SetupOutsideSea();
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

	TRACE_CPUPROFILER_EVENT_SCOPE(AFloodSimulationActor::Simulate);
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
	if (!bStarted || !Rules || bDuplicate)
	{
		return false;
	}
	const int32 X = FMath::FloorToInt32((Location.X - Origin.X) / CellCm);
	const int32 Y = FMath::FloorToInt32((Location.Y - Origin.Y) / CellCm);
	const bool bInGrid = X >= 0 && Y >= 0 && X < Grid.GetSizeX() && Y < Grid.GetSizeY();
	bool bWet = false;
	double GridSurfaceCm = 0.0;
	double GroundCm = 0.0;
	if (bInGrid)
	{
		const int32 I = Grid.Index(X, Y);
		bWet = Grid.Depth[I] * 100.0f >= Rules->MinDrawDepthCm;
		GridSurfaceCm = Grid.Surface(I) * 100.0;
		GroundCm = Grid.Ground[I] * 100.0;
	}
	// T-141: outside the grid the sea of the fragment takes the same rise (no step at the border) and fades far away;
	// without an outside sea (no expedition, or bRaiseOutsideGrid off) only the wet cells of the grid answer
	double SurfaceCm = 0.0;
	if (!OutsideSea.Surface(FVector2D(Location.X, Location.Y), bInGrid, bWet, GridSurfaceCm, GroundCm,
		BaseSeaLevel * 100.0, HeldRiseCm, SurfaceCm))
	{
		return false;
	}
	OutSurfaceZ = static_cast<float>(SurfaceCm);
	return true;
}

void AFloodSimulationActor::SetupOutsideSea()
{
	OutsideSea = FFloodOutsideSea();
	if (ClockSource != EHazardClockSource::Expedition || !Rules->bRaiseOutsideGrid || !Grid.IsValid())
	{
		return;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(AFloodSimulationActor::SetupOutsideSea);

	// the land of the fragment: every cell whose ground stands above the sea, read by the same traces on every machine
	FBox2D Land(ForceInit);
	for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetSizeX(); ++X)
		{
			if (Grid.Ground[Grid.Index(X, Y)] > BaseSeaLevel)
			{
				Land += FVector2D(Origin.X + (X + 0.5) * CellCm, Origin.Y + (Y + 0.5) * CellCm);
			}
		}
	}
	const FBox2D GridBox(Origin, Origin + FVector2D(Grid.GetSizeX() * CellCm, Grid.GetSizeY() * CellCm));
	OutsideSea = FFloodOutsideSea::Make(GridBox, Land, Rules->OutsideFullRiseMarginCm, Rules->OutsideFadeCm, Rules->EdgeBlendCm);
	UE_LOG(Logmonke_island, Log, TEXT("%s: the sea of the fragment rises outside the grid up to %.2f km past its border (land %s, margin %.2f km, fade %.2f km); "
		"farther the map keeps its sea level"), *GetName(), OutsideSea.GetReachCm() / 100000.0, Land.bIsValid ? TEXT("found") : TEXT("none"),
		Rules->OutsideFullRiseMarginCm / 100000.0f, Rules->OutsideFadeCm / 100000.0f);
	BuildApron();
}

void AFloodSimulationActor::BuildApron()
{
	if (!OutsideSea.IsValid() || Grid.GetSizeX() < 2 || Grid.GetSizeY() < 2)
	{
		return;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(AFloodSimulationActor::BuildApron);

	// the apron starts where the chunks end (the centres of the border cells) and its lattice lands on that edge exactly
	const FVector2D Inner0 = Origin + FVector2D(0.5 * CellCm);
	const FVector2D InnerSize((Grid.GetSizeX() - 1) * CellCm, (Grid.GetSizeY() - 1) * CellCm);
	const int32 KX = FMath::Max(1, FMath::RoundToInt32(InnerSize.X / Rules->ApronCellCm));
	const int32 KY = FMath::Max(1, FMath::RoundToInt32(InnerSize.Y / Rules->ApronCellCm));
	const FVector2D Step(InnerSize.X / KX, InnerSize.Y / KY);
	const double Reach = OutsideSea.GetReachCm() + CellCm;
	const int32 RX = FMath::CeilToInt32(Reach / Step.X);
	const int32 RY = FMath::CeilToInt32(Reach / Step.Y);
	const int32 VX = KX + 2 * RX + 1;
	const int32 VY = KY + 2 * RY + 1;
	const FVector ActorLocation = GetActorLocation();

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<double> Weights;
	Vertices.Reserve(VX * VY);
	Normals.Reserve(VX * VY);
	UVs.Reserve(VX * VY);
	Weights.Reserve(VX * VY);
	for (int32 J = 0; J < VY; ++J)
	{
		for (int32 I = 0; I < VX; ++I)
		{
			const FVector2D Point = Inner0 + FVector2D((I - RX) * Step.X, (J - RY) * Step.Y);
			// Z in units of rise: the component is scaled in Z by the rise, so the mesh is built once
			const double Weight = OutsideSea.RiseWeight(Point);
			Weights.Add(Weight);
			Vertices.Add(FVector(Point.X - ActorLocation.X, Point.Y - ActorLocation.Y, Weight));
			Normals.Add(FVector::UpVector);
			UVs.Add(FVector2D(Point.X / 1000.0, Point.Y / 1000.0));
		}
	}
	TArray<int32> Triangles;
	for (int32 J = 0; J < VY - 1; ++J)
	{
		for (int32 I = 0; I < VX - 1; ++I)
		{
			const int32 GI = I - RX;
			const int32 GJ = J - RY;
			if (GI >= 0 && GI < KX && GJ >= 0 && GJ < KY)
			{
				// over the grid: the chunks draw the flood
				continue;
			}
			const int32 A = J * VX + I;
			const int32 B = A + 1;
			const int32 C = A + VX;
			const int32 D = C + 1;
			if (Weights[A] <= 0.0 && Weights[B] <= 0.0 && Weights[C] <= 0.0 && Weights[D] <= 0.0)
			{
				// the sea of the map, untouched
				continue;
			}
			// both windings: the surface is seen from above and from under water
			Triangles.Append({A, C, B, B, C, D, A, B, C, B, D, C});
		}
	}
	if (Triangles.IsEmpty())
	{
		return;
	}

	Apron = NewObject<UProceduralMeshComponent>(this, TEXT("FloodApron"));
	Apron->SetupAttachment(RootComponent);
	Apron->SetUsingAbsoluteRotation(true);
	Apron->SetUsingAbsoluteScale(true);
	Apron->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Apron->SetCastShadow(false);
	Apron->SetVisibility(false);
	Apron->RegisterComponent();
	Apron->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
	if (WaterMaterial)
	{
		Apron->SetMaterial(0, WaterMaterial);
	}
	ApronRiseCm = -1.0;
	UE_LOG(Logmonke_island, Log, TEXT("%s: apron of %d x %d vertices (%.0f m cells) around the grid"), *GetName(), VX, VY, Step.X / 100.0);
}

void AFloodSimulationActor::UpdateApron()
{
	if (!Apron)
	{
		return;
	}
	const double Rise = HeldRiseCm;
	if (ApronRiseCm >= 0.0 && FMath::Abs(Rise - ApronRiseCm) < 0.5)
	{
		return;
	}
	ApronRiseCm = Rise;
	const bool bVisible = Rise >= Rules->ApronMinRiseCm;
	if (Apron->IsVisible() != bVisible)
	{
		Apron->SetVisibility(bVisible);
	}
	if (!bVisible)
	{
		return;
	}
	// sunk a little where the rise fades out (no flicker against the sea of the map), exactly on the rise near the grid
	const double Sink = Rules->ApronSinkCm;
	Apron->SetRelativeLocation(FVector(0.0, 0.0, BaseSeaLevel * 100.0 - Sink - GetActorLocation().Z));
	Apron->SetWorldScale3D(FVector(1.0, 1.0, FMath::Max(0.01, Rise + Sink)));
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
	const float MinDepth = Rules->MinDrawDepthCm / 100.0f;
	// open sea is drawn by the sea surface itself, at its current level (it rises with the expedition, T-103)
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	const float MinSurface = FMath::Max(BaseSeaLevel, Water ? Water->GetSeaLevel() / 100.0f : BaseSeaLevel) + MinDepth;
	const FVector ActorLocation = GetActorLocation();

	auto IsWet = [&](int32 I)
	{
		return Grid.Depth[I] > MinDepth && Grid.Surface(I) > MinSurface;
	};

	// Open sea and drowned land: every cell under water at the sea level, one flat quad moved with the sea
	// (a transform update) instead of a rebuilt mesh (new render data and body setup each time)
	const float Sea = BaseSeaLevel + static_cast<float>(HeldRiseCm / 100.0);
	const float FlatTolerance = Rules->FlatChunkToleranceCm / 100.0f;
	bool bFlat = true;
	for (int32 GY = Y0; GY <= Y1 && bFlat; ++GY)
	{
		for (int32 GX = X0; GX <= X1; ++GX)
		{
			const int32 I = Grid.Index(GX, GY);
			if (!IsWet(I) || FMath::Abs(Grid.Surface(I) - Sea) > FlatTolerance)
			{
				bFlat = false;
				break;
			}
		}
	}
	if (bFlat)
	{
		if (!ChunkFlat[ChunkIndex])
		{
			ChunkFlat[ChunkIndex] = 1;
			const double LX0 = Origin.X + (X0 + 0.5) * CellCm;
			const double LY0 = Origin.Y + (Y0 + 0.5) * CellCm;
			const double LX1 = Origin.X + (X1 + 0.5) * CellCm;
			const double LY1 = Origin.Y + (Y1 + 0.5) * CellCm;
			const TArray<FVector> Corners = {
				FVector(LX0 - ActorLocation.X, LY0 - ActorLocation.Y, 0.0), FVector(LX1 - ActorLocation.X, LY0 - ActorLocation.Y, 0.0),
				FVector(LX0 - ActorLocation.X, LY1 - ActorLocation.Y, 0.0), FVector(LX1 - ActorLocation.X, LY1 - ActorLocation.Y, 0.0) };
			const TArray<FVector> FlatNormals = { FVector::UpVector, FVector::UpVector, FVector::UpVector, FVector::UpVector };
			const TArray<FVector2D> FlatUVs = {
				FVector2D(LX0 / 1000.0, LY0 / 1000.0), FVector2D(LX1 / 1000.0, LY0 / 1000.0),
				FVector2D(LX0 / 1000.0, LY1 / 1000.0), FVector2D(LX1 / 1000.0, LY1 / 1000.0) };
			// both windings: the surface is seen from above and from under water
			const TArray<int32> FlatTriangles = { 0, 2, 1, 1, 2, 3, 0, 1, 2, 1, 3, 2 };
			Chunk->CreateMeshSection_LinearColor(0, Corners, FlatTriangles, FlatNormals, FlatUVs, TArray<FLinearColor>(), TArray<FProcMeshTangent>(), false);
			if (WaterMaterial)
			{
				Chunk->SetMaterial(0, WaterMaterial);
			}
		}
		const FVector Height(0.0, 0.0, Sea * 100.0 - ActorLocation.Z);
		if (!Chunk->GetRelativeLocation().Equals(Height, 0.1))
		{
			Chunk->SetRelativeLocation(Height);
		}
		return;
	}
	if (ChunkFlat[ChunkIndex])
	{
		ChunkFlat[ChunkIndex] = 0;
		Chunk->SetRelativeLocation(FVector::ZeroVector);
	}

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
