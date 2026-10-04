// monke_island

#include "Boats/BoatStartSubsystem.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatStart.h"
#include "Boats/BoatStartRules.h"
#include "Boats/BoatSubsystem.h"
#include "Boats/ExpeditionArrivalPoint.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"

UBoatStartSubsystem* UBoatStartSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBoatStartSubsystem>() : nullptr;
}

bool UBoatStartSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UBoatStartSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() == NM_Client)
	{
		// the start is decided by the server; clients see the boat and the seats through replication
		bResolved = true;
		return;
	}

	Rules = StartRulesAsset.IsNull() ? nullptr : StartRulesAsset.LoadSynchronous();
	if (!Rules)
	{
		if (!StartRulesAsset.IsNull())
		{
			UE_LOG(Logmonke_island, Warning, TEXT("BoatStart: %s not found, using the default start rules"), *StartRulesAsset.ToString());
		}
		Rules = GetDefault<UBoatStartRules>();
	}

	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &UBoatStartSubsystem::OnPostLogin);
	ResolveTimer = InWorld.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UBoatStartSubsystem::Resolve));
}

FDelegateHandle UBoatStartSubsystem::CallOrRegisterOnArrivalResolved(FOnBoatArrivalResolved::FDelegate&& Delegate)
{
	if (bArrivalNotified)
	{
		Delegate.ExecuteIfBound(ArrivalFragmentId, ArrivalBoatTransform);
		return FDelegateHandle();
	}
	return OnArrivalResolved.Add(MoveTemp(Delegate));
}

void UBoatStartSubsystem::NotifyArrivalResolved(FName FragmentId, const FTransform& BoatTransform)
{
	if (bArrivalNotified)
	{
		return;
	}
	bArrivalNotified = true;
	ArrivalFragmentId = FragmentId;
	ArrivalBoatTransform = BoatTransform;
	OnArrivalResolved.Broadcast(FragmentId, BoatTransform);
}

void UBoatStartSubsystem::Deinitialize()
{
	OnArrivalResolved.Clear();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResolveTimer);
	}
	FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	PostLoginHandle.Reset();
	for (const TPair<TWeakObjectPtr<AController>, FDelegateHandle>& Watch : PawnWatches)
	{
		if (AController* Controller = Watch.Key.Get())
		{
			Controller->GetOnNewPawnNotifier().Remove(Watch.Value);
		}
	}
	PawnWatches.Reset();
	HandledControllers.Reset();
	Super::Deinitialize();
}

FString UBoatStartSubsystem::GetWorldOptions() const
{
	const UWorld* World = GetWorld();
	if (const AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr)
	{
		if (!GameMode->OptionsString.IsEmpty())
		{
			return GameMode->OptionsString;
		}
	}
	FString Options;
	if (World)
	{
		for (const FString& Option : World->URL.Op)
		{
			Options += TEXT("?") + Option;
		}
	}
	return Options;
}

bool UBoatStartSubsystem::SetArrivalOverride(const FTransform& BoatTransform, FName FragmentId)
{
	if (bResolved)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("BoatStart: arrival override for %s ignored, the start is already decided"), *FragmentId.ToString());
		return false;
	}
	if (BoatTransform.ContainsNaN())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("BoatStart: invalid arrival override for %s ignored"), *FragmentId.ToString());
		return false;
	}
	ArrivalOverride = FTransform(FRotator(0.0f, BoatTransform.Rotator().Yaw, 0.0f), BoatTransform.GetLocation());
	ArrivalOverrideFragmentId = FragmentId;
	return true;
}

void UBoatStartSubsystem::ClearArrivalOverride()
{
	if (!bResolved)
	{
		ArrivalOverride.Reset();
		ArrivalOverrideFragmentId = NAME_None;
	}
}

void UBoatStartSubsystem::Resolve()
{
	bResolved = true;
	if (!GetWorld() || !Rules)
	{
		return;
	}
	if (!Rules->bEnabled)
	{
		UE_LOG(Logmonke_island, Log, TEXT("BoatStart: disabled in the start rules, players start on a PlayerStart"));
		return;
	}

	FTransform PointTransform;
	FName FragmentId;
	FString Source;
	if (ChooseArrival(PointTransform, FragmentId, Source))
	{
		SpawnArrivalBoat(PointTransform, FragmentId, Source);
	}
}

bool UBoatStartSubsystem::ChooseArrival(FTransform& OutPointTransform, FName& OutFragmentId, FString& OutSource) const
{
	UWorld* World = GetWorld();

	// 1. the expedition data (island definition, Start anchorage) wins
	if (ArrivalOverride.IsSet())
	{
		OutPointTransform = ArrivalOverride.GetValue();
		OutFragmentId = ArrivalOverrideFragmentId;
		OutSource = TEXT("override");
		return true;
	}

	// 2. / 3. every arrival point of the map, in a stable order (fragment, then actor name) so a seed is reproducible
	TArray<AExpeditionArrivalPoint*> Points;
	for (TActorIterator<AExpeditionArrivalPoint> It(World); It; ++It)
	{
		Points.Add(*It);
	}
	if (Points.IsEmpty())
	{
		UE_LOG(Logmonke_island, Log, TEXT("BoatStart: no arrival override nor ExpeditionArrivalPoint in %s, players start on a PlayerStart"), *World->GetMapName());
		return false;
	}
	Points.Sort([](const AExpeditionArrivalPoint& A, const AExpeditionArrivalPoint& B)
	{
		if (A.FragmentId != B.FragmentId)
		{
			return A.FragmentId.LexicalLess(B.FragmentId);
		}
		return A.GetFName().LexicalLess(B.GetFName());
	});

	// fragment and seed from the URL (untrusted: sanitised)
	const FString Options = GetWorldOptions();
	FName Wanted;
	if (UGameplayStatics::HasOption(Options, BoatStart::IslandOption))
	{
		const FString Raw = UGameplayStatics::ParseOption(Options, BoatStart::IslandOption);
		Wanted = BoatStart::SanitizeFragmentId(Raw, Rules->MaxFragmentIdLength);
		if (Wanted.IsNone())
		{
			UE_LOG(Logmonke_island, Warning, TEXT("BoatStart: ignoring the invalid ?%s= option (%d characters)"), BoatStart::IslandOption, Raw.Len());
		}
	}
	int32 Seed = 0;
	if (!BoatStart::ParseSeed(UGameplayStatics::ParseOption(Options, BoatStart::SeedOption), Seed))
	{
		Seed = Rules->ArrivalSeed != 0 ? Rules->ArrivalSeed : static_cast<int32>(FPlatformTime::Cycles() & 0x7fffffff);
	}

	TArray<FName> Ids;
	Ids.Reserve(Points.Num());
	for (const AExpeditionArrivalPoint* Point : Points)
	{
		Ids.Add(Point->FragmentId);
	}
	const int32 Index = BoatStart::ChooseArrivalIndex(Ids, Wanted, Seed, Rules->bRandomArrivalWhenIslandUnknown);
	if (Index == INDEX_NONE)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("BoatStart: no arrival point for island %s among %d points, players start on a PlayerStart"), *Wanted.ToString(), Points.Num());
		return false;
	}
	const AExpeditionArrivalPoint* Point = Points[Index];
	if (!Wanted.IsNone() && Point->FragmentId != Wanted)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("BoatStart: island %s has no arrival point, random point of %s instead"), *Wanted.ToString(), *Point->FragmentId.ToString());
	}
	OutPointTransform = Point->GetBoatTransform();
	OutFragmentId = Point->FragmentId;
	OutSource = FString::Printf(TEXT("%s, seed %d"), *Point->GetName(), Seed);
	return true;
}

bool UBoatStartSubsystem::SpawnArrivalBoat(const FTransform& PointTransform, FName FragmentId, const FString& Source)
{
	UWorld* World = GetWorld();
	FTransform BoatTransform;
	if (!FindWaterSpot(PointTransform, BoatTransform))
	{
		UE_LOG(Logmonke_island, Warning, TEXT("BoatStart: arrival of %s (%s) has no water %.0f cm deep within %.0f m behind it, players start on a PlayerStart"),
			*FragmentId.ToString(), *Source, Rules->MinWaterDepthCm, Rules->SafetySearchMaxCm / 100.0f);
		return false;
	}

	UClass* BoatClass = UBoatSubsystem::LoadBoatClass(Rules->BoatClass);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ABoatActor* Boat = BoatClass ? World->SpawnActor<ABoatActor>(BoatClass, BoatTransform, Params) : nullptr;
	if (!Boat)
	{
		UE_LOG(Logmonke_island, Error, TEXT("BoatStart: could not spawn %s for %s (%s), players start on a PlayerStart"), *GetNameSafe(BoatClass), *FragmentId.ToString(), *Source);
		return false;
	}
	ArrivalBoat = Boat;
	ArrivalFragmentId = FragmentId;
	UE_LOG(Logmonke_island, Log, TEXT("BoatStart: island %s (%s), %s at %s with %d seats"),
		*FragmentId.ToString(), *Source, *Boat->GetName(), *BoatTransform.GetLocation().ToCompactString(), Boat->GetSeatCount());
	// T-141: the island really played, for the expedition game state (Boats/ never includes Expedition/)
	NotifyArrivalResolved(FragmentId, Boat->GetActorTransform());

	// the players already logged in: board now, or as soon as their pawn is spawned
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* Controller = It->Get())
		{
			WatchController(Controller);
			BoardController(Controller);
		}
	}
	return true;
}

bool UBoatStartSubsystem::FindWaterSpot(const FTransform& PointTransform, FTransform& OutBoatTransform) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(World);
	const FVector Forward = PointTransform.GetUnitAxis(EAxis::X);
	const FVector Back = -FVector(Forward.X, Forward.Y, 0.0).GetSafeNormal();
	const FCollisionObjectQueryParams Ground(ECC_WorldStatic);
	const float Step = FMath::Max(10.0f, Rules->SafetySearchStepCm);

	for (float Distance = 0.0f; Distance <= Rules->SafetySearchMaxCm; Distance += Step)
	{
		const FVector Spot = PointTransform.GetLocation() + Back * Distance;
		const float Surface = Water ? Water->GetWaterSurfaceAt(Spot) : 0.0f;
		FHitResult Hit;
		const FVector Top(Spot.X, Spot.Y, Surface + 3000.0f);
		const FVector Bottom(Spot.X, Spot.Y, Surface - 5000.0f);
		const float GroundZ = World->LineTraceSingleByObjectType(Hit, Top, Bottom, Ground) ? static_cast<float>(Hit.ImpactPoint.Z) : Surface - 5000.0f;
		if (Surface - GroundZ < Rules->MinWaterDepthCm)
		{
			continue;
		}
		if (Distance > 0.0f)
		{
			UE_LOG(Logmonke_island, Log, TEXT("BoatStart: arrival point in shallow water, boat moved back %.0f m"), Distance / 100.0f);
		}
		OutBoatTransform = FTransform(PointTransform.GetRotation(), FVector(Spot.X, Spot.Y, Surface + Rules->SpawnHeightAboveWaterCm));
		return true;
	}
	return false;
}

void UBoatStartSubsystem::OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	// before Resolve, the players present are boarded by Resolve itself
	if (!bResolved || !GameMode || GameMode->GetWorld() != GetWorld() || !NewPlayer)
	{
		return;
	}
	if (!ArrivalBoat.IsValid() || !Rules || !Rules->bBoardLateJoiners)
	{
		return;
	}
	WatchController(NewPlayer);
	BoardController(NewPlayer);
}

void UBoatStartSubsystem::WatchController(AController* Controller)
{
	if (!Controller || HandledControllers.Contains(Controller) || PawnWatches.Contains(Controller))
	{
		return;
	}
	TWeakObjectPtr<AController> WeakController(Controller);
	const FDelegateHandle Handle = Controller->GetOnNewPawnNotifier().AddWeakLambda(this, [this, WeakController](APawn* NewPawn)
	{
		UWorld* World = GetWorld();
		if (!NewPawn || !World)
		{
			return;
		}
		// the notification comes in the middle of Possess: board on the next tick
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
		{
			BoardController(WeakController.Get());
		}));
	});
	PawnWatches.Add(WeakController, Handle);
}

void UBoatStartSubsystem::StopWatching(AController* Controller)
{
	FDelegateHandle Handle;
	if (Controller && PawnWatches.RemoveAndCopyValue(Controller, Handle))
	{
		Controller->GetOnNewPawnNotifier().Remove(Handle);
	}
}

void UBoatStartSubsystem::BoardController(AController* Controller)
{
	if (!Controller || !Rules || HandledControllers.Contains(Controller))
	{
		return;
	}
	ACharacter* Character = Cast<ACharacter>(Controller->GetPawn());
	if (!Character)
	{
		// not spawned yet: the pawn watch boards it later
		return;
	}
	HandledControllers.Add(Controller);
	StopWatching(Controller);

	ABoatActor* Boat = ArrivalBoat.Get();
	if (!IsValid(Boat) || Boat->IsSunk())
	{
		UE_LOG(Logmonke_island, Log, TEXT("BoatStart: the arrival boat is gone, %s starts on a PlayerStart"), *Controller->GetName());
		return;
	}
	UBoatSubsystem::EnsureRider(Character);

	const FRotator Facing(0.0f, Boat->GetActorRotation().Yaw, 0.0f);
	TArray<bool> Occupied;
	Occupied.Reserve(Boat->GetSeatCount());
	for (int32 Index = 0; Index < Boat->GetSeatCount(); ++Index)
	{
		Occupied.Add(Boat->IsSeatOccupied(Index));
	}
	const int32 Seat = BoatStart::ChooseSeat(Occupied, Rules->SeatOrder);
	bool bSeated = false;
	if (Seat != INDEX_NONE)
	{
		// next to the seat first: SitCharacter checks the reach, then snaps the pawn onto the socket
		const FVector SeatLocation = Boat->GetHull()->GetSocketLocation(Boat->GetSeatSocket(Seat)) + Boat->GetActorUpVector() * Boat->GetSeatedCapsuleOffset();
		Character->SetActorLocationAndRotation(SeatLocation, Facing, false, nullptr, ETeleportType::TeleportPhysics);
		bSeated = Boat->SitCharacter(Character, Seat);
	}
	if (!bSeated)
	{
		// every seat taken: standing on the floor, the boat carries the weight (PersonMassKg)
		const FVector Spot = Boat->GetActorTransform().TransformPosition(BoatStart::StandingSpot(Rules->StandingSpots, StandingCount++));
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_Falling);
		}
		Character->SetActorLocationAndRotation(Spot, Facing, false, nullptr, ETeleportType::TeleportPhysics);
	}
	if (Rules->bFaceBow)
	{
		Controller->SetControlRotation(Facing);
		if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
		{
			PlayerController->ClientSetRotation(Facing, true);
		}
	}

	const TCHAR* Place = !bSeated ? TEXT("standing") : (Boat->IsRower(Character) ? TEXT("at the oars") : TEXT("seated"));
	UE_LOG(Logmonke_island, Log, TEXT("BoatStart: %s boards %s %s (seat %d)"), *Character->GetName(), *Boat->GetName(), Place, bSeated ? Seat : INDEX_NONE);
}
