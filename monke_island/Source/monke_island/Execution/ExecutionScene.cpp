// monke_island

#include "Execution/ExecutionScene.h"
#include "Execution/ExecutionMath.h"
#include "Execution/ExecutionRules.h"
#include "Fort/FortSoldier.h"
#include "Galleon/GalleonActor.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "ExecutionScene"

namespace
{
	/** The uniforms of the squad when the rules name none (the French garrison) */
	const TCHAR* const DefaultSquad[] = {
		TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Grenadier.BP_BeanFR_Grenadier_C"),
		TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Fusilier_Bicorne.BP_BeanFR_Fusilier_Bicorne_C"),
		TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Fusilier_1767.BP_BeanFR_Fusilier_1767_C"),
		TEXT("/Game/MonkeIsland/Characters/BeanFR/Blueprints/BP_BeanFR_Sergent.BP_BeanFR_Sergent_C"),
	};

	/** A 1 m cube centred on its pivot, stretched into a plank when the rules name no mesh */
	const TCHAR* const DefaultPlankMesh = TEXT("/Engine/BasicShapes/Cube.Cube");

	float HalfHeightOf(const APawn* Pawn)
	{
		const ACharacter* Character = Cast<ACharacter>(Pawn);
		return Character && Character->GetCapsuleComponent() ? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 50.0f;
	}
}

AExecutionScene::AExecutionScene()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	RulesPath = FSoftObjectPath(TEXT("/Game/MonkeIsland/Galleon/Execution/DA_ExecutionRules.DA_ExecutionRules"));

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Plank = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plank"));
	Plank->SetupAttachment(Root);
	Plank->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Plank->SetCanEverAffectNavigation(false);
	Plank->SetVisibility(false);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bConstrainAspectRatio = false;
}

void AExecutionScene::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AExecutionScene, StartTime);
	DOREPLIFETIME(AExecutionScene, Condemned);
}

AExecutionScene* AExecutionScene::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AExecutionScene> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

AExecutionScene* AExecutionScene::Begin(UWorld* World, AGalleonActor* Galleon)
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}
	if (AExecutionScene* Existing = Get(World))
	{
		return Existing;
	}
	FTransform Where = FTransform::Identity;
	if (Galleon && Galleon->GetHull())
	{
		Where = Galleon->GetHull()->GetComponentTransform();
		Where.SetScale3D(FVector::OneVector);
	}
	else if (const AGameStateBase* GameState = World->GetGameState())
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			if (const APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr)
			{
				Where = FTransform(FRotator(0.0, Pawn->GetActorRotation().Yaw, 0.0), Pawn->GetActorLocation() - FVector(0.0, 0.0, 250.0));
				break;
			}
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AExecutionScene* Scene = World->SpawnActor<AExecutionScene>(AExecutionScene::StaticClass(), Where, Params);
	if (Scene && Galleon && Galleon->GetHull())
	{
		// on the hull that carries the players (T-317: it follows the water, only the visual copy of the ship rolls)
		Scene->AttachToComponent(Galleon->GetHull(), FAttachmentTransformRules::KeepWorldTransform);
	}
	UE_LOG(Logmonke_island, Log, TEXT("Execution: the crew missed the quota of its contract, the squad is called%s"),
		Galleon ? TEXT(" on the galleon") : TEXT(" (no galleon: where the crew stands)"));
	return Scene;
}

const UExecutionRules* AExecutionScene::GetRules() const
{
	if (!Rules && !bRulesLoadTried)
	{
		bRulesLoadTried = true;
		Rules = Cast<UExecutionRules>(RulesPath.TryLoad());
		if (!Rules)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Execution: no DA_ExecutionRules at '%s', the class defaults are used"), *RulesPath.ToString());
		}
	}
	return Rules ? Rules.Get() : GetDefault<UExecutionRules>();
}

void AExecutionScene::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		const AGameStateBase* GameState = GetWorld()->GetGameState();
		StartTime = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	}
}

void AExecutionScene::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Drums)
	{
		Drums->Stop();
	}
	Super::EndPlay(EndPlayReason);
}

FTransform AExecutionScene::Frame() const
{
	FTransform Result = GetActorTransform();
	Result.SetScale3D(FVector::OneVector);
	return Result;
}

float AExecutionScene::GetSceneTime() const
{
	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (StartTime < 0.0 || !GameState)
	{
		return -1.0f;
	}
	return static_cast<float>(GameState->GetServerWorldTimeSeconds() - StartTime);
}

bool AExecutionScene::IsShowing() const
{
	const UExecutionRules* R = GetRules();
	const float Time = GetSceneTime();
	return Time >= 0.0f && ExecutionMath::PhaseAt(Time, R->PlaceAtSeconds, R->EndAtSeconds) == ExecutionMath::EPhase::Scene;
}

bool AExecutionScene::IsOver() const
{
	const UExecutionRules* R = GetRules();
	const float Time = GetSceneTime();
	return Time >= 0.0f && ExecutionMath::PhaseAt(Time, R->PlaceAtSeconds, R->EndAtSeconds) == ExecutionMath::EPhase::Over;
}

FText AExecutionScene::GetTitle(float& OutAlpha) const
{
	const UExecutionRules* R = GetRules();
	const float Time = GetSceneTime();
	OutAlpha = 0.0f;
	if (Time < 0.0f)
	{
		return FText::GetEmpty();
	}
	if (Time < R->PlaceAtSeconds)
	{
		OutAlpha = ExecutionMath::TitleAlpha(Time, 0.2f, FMath::Min(R->TitleSeconds, R->PlaceAtSeconds - 0.2f));
		return LOCTEXT("QuotaMissed", "Quota non atteint");
	}
	const float VerdictAt = R->FireAtSeconds + R->VolleySpreadSeconds + 1.5f;
	OutAlpha = ExecutionMath::TitleAlpha(Time, VerdictAt, FMath::Max(R->EndAtSeconds - VerdictAt, 0.5f));
	return OutAlpha > 0.0f ? LOCTEXT("CampaignOver", "La campagne s'achève ici") : FText::GetEmpty();
}

bool AExecutionScene::IsCondemned(const APawn* Pawn) const
{
	return Pawn && Condemned.ContainsByPredicate([Pawn](const TObjectPtr<APawn>& Item) { return Item.Get() == Pawn; });
}

void AExecutionScene::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UExecutionRules* R = GetRules();
	const float Time = GetSceneTime();
	if (Time < 0.0f)
	{
		return;
	}
	if (HasAuthority() && !bPlaced && Time >= R->PlaceAtSeconds)
	{
		PlaceEveryone();
	}
	if (!bPlankBuilt && Time >= R->PlaceAtSeconds - R->FadeSeconds)
	{
		BuildPlank();
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdatePresentation();
		HoldLocalCondemned();
	}
}

void AExecutionScene::BuildPlank()
{
	bPlankBuilt = true;
	const UExecutionRules* R = GetRules();
	UStaticMesh* Mesh = R->PlankMesh ? R->PlankMesh.Get() : LoadObject<UStaticMesh>(nullptr, DefaultPlankMesh);
	if (!Mesh)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Execution: no plank mesh, the crew stands in the air"));
		return;
	}
	Plank->SetStaticMesh(Mesh);
	if (R->PlankMaterial)
	{
		for (int32 Slot = 0; Slot < Plank->GetNumMaterials(); ++Slot)
		{
			Plank->SetMaterial(Slot, R->PlankMaterial);
		}
	}
	// the mesh stretched to the size of the board, its top at PlankStart, running out along PlankDirection
	const FBox Box = Mesh->GetBoundingBox();
	const FVector MeshSize = Box.GetSize();
	const FVector Scale(R->PlankSize.X / FMath::Max(MeshSize.X, 1.0), R->PlankSize.Y / FMath::Max(MeshSize.Y, 1.0), R->PlankSize.Z / FMath::Max(MeshSize.Z, 1.0));
	const FVector Along = R->PlankDirection.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : R->PlankDirection.GetSafeNormal2D();
	const FQuat Rotation = Along.ToOrientationQuat();
	const FVector Centre = R->PlankStart + Along * (0.5 * R->PlankSize.X) - FVector(0.0, 0.0, 0.5 * R->PlankSize.Z);
	Plank->SetRelativeTransform(FTransform(Rotation, Centre - Rotation.RotateVector(Box.GetCenter() * Scale), Scale));
	Plank->SetVisibility(true);
}

FTransform AExecutionScene::CondemnedSpot(int32 Index, float HalfHeight) const
{
	const UExecutionRules* R = GetRules();
	const FTransform Hull = Frame();
	const FVector Feet = Hull.TransformPosition(ExecutionMath::PlankSpot(R->PlankStart, R->PlankDirection, R->FirstSpotCm, R->SpotSpacingCm, R->PlankSize.X, Index));
	// walking the plank: the back to the squad, the face to the sea (and to the camera)
	const FVector SquadAt = Hull.TransformPosition(R->SquadCentre);
	const float Yaw = static_cast<float>((Feet - SquadAt).GetSafeNormal2D().Rotation().Yaw);
	return FTransform(FRotator(0.0f, Yaw, 0.0f), Feet + FVector(0.0, 0.0, HalfHeight + 4.0));
}

void AExecutionScene::PlaceEveryone()
{
	bPlaced = true;
	BuildPlank();
	const AGameStateBase* GameState = GetWorld()->GetGameState();
	TArray<APawn*> Living;
	if (GameState)
	{
		for (const APlayerState* PlayerState : GameState->PlayerArray)
		{
			APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
			const UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
			if (Pawn && (!Health || !Health->IsDead()))
			{
				Living.Add(Pawn);
			}
		}
	}
	Condemned.Reset();
	for (int32 Index = 0; Index < Living.Num(); ++Index)
	{
		APawn* Pawn = Living[Index];
		const FTransform Spot = CondemnedSpot(Index, HalfHeightOf(Pawn));
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			Character->GetCharacterMovement()->StopMovementImmediately();
		}
		Pawn->TeleportTo(Spot.GetLocation(), Spot.Rotator(), false, true);
		if (AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Pawn))
		{
			Islander->SetForceThirdPerson(true);
		}
		Condemned.Add(Pawn);
	}
	ForceNetUpdate();
	SpawnSquad();
	UE_LOG(Logmonke_island, Log, TEXT("Execution: %d of the crew on the plank, %d soldier(s) on deck"), Condemned.Num(), Squad.Num());
}

void AExecutionScene::SpawnSquad()
{
	const UExecutionRules* R = GetRules();
	TArray<UClass*> Classes;
	for (const TSubclassOf<AFortSoldier>& Class : R->SquadClasses)
	{
		if (Class)
		{
			Classes.Add(Class.Get());
		}
	}
	if (Classes.IsEmpty())
	{
		for (const TCHAR* Path : DefaultSquad)
		{
			if (UClass* Class = LoadClass<AFortSoldier>(nullptr, Path))
			{
				Classes.Add(Class);
			}
		}
	}
	if (Classes.IsEmpty())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Execution: no soldier class, no squad"));
		return;
	}
	const FTransform Hull = Frame();
	const FVector Along = Hull.TransformVectorNoScale(R->PlankDirection.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : R->PlankDirection.GetSafeNormal2D());
	const FVector Facing = R->PlankDirection.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : R->PlankDirection.GetSafeNormal2D();
	const double Now = GetWorld()->GetTimeSeconds();
	const float Time = GetSceneTime();
	for (int32 Index = 0; Index < Classes.Num(); ++Index)
	{
		const AFortSoldier* Defaults = GetDefault<AFortSoldier>(Classes[Index]);
		const float HalfHeight = Defaults && Defaults->GetCapsuleComponent() ? Defaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 55.0f;
		const FVector Feet = Hull.TransformPosition(ExecutionMath::SquadSpot(R->SquadCentre, Facing, R->SquadSpacingCm, Index, Classes.Num()));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AFortSoldier* Soldier = GetWorld()->SpawnActor<AFortSoldier>(Classes[Index], Feet + FVector(0.0, 0.0, HalfHeight + 5.0),
			FRotator(0.0, Along.Rotation().Yaw, 0.0), Params);
		if (!Soldier)
		{
			continue;
		}
		Squad.Add(Soldier);
		const int32 Victim = ExecutionMath::VictimOf(Index, Condemned.Num());
		APawn* Target = Condemned.IsValidIndex(Victim) ? Condemned[Victim].Get() : nullptr;
		const FVector Shot = Target ? (Target->GetActorLocation() - Feet).GetSafeNormal2D() : Along;
		const FVector Push = Shot * R->PushSpeed + FVector(0.0, 0.0, R->PushUpSpeed);
		const float Spread = FMath::FRandRange(0.0f, R->VolleySpreadSeconds);
		Soldier->StartFiringSquad(Target, Now + (R->AimAtSeconds - Time), Now + (R->FireAtSeconds + Spread - Time), Push);
	}
}

void AExecutionScene::UpdatePresentation()
{
	const UExecutionRules* R = GetRules();
	const float Time = GetSceneTime();
	// the glide of the camera, from the place time to the end, eased
	const float Span = FMath::Max(R->EndAtSeconds - R->PlaceAtSeconds, 0.1f);
	const float Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, FMath::Clamp((Time - R->PlaceAtSeconds) / Span, 0.0f, 1.0f), 2.0f);
	const FTransform Hull = Frame();
	const FVector Eye = Hull.TransformPosition(FMath::Lerp(R->CameraStart, R->CameraEnd, Alpha));
	const FVector Look = Hull.TransformPosition(R->CameraLookAt);
	Camera->SetFieldOfView(R->CameraFieldOfView);
	Camera->SetWorldLocationAndRotation(Eye, (Look - Eye).Rotation());

	// the drums roll from the place time to the volley
	if (R->DrumSound && !bDrumsDone)
	{
		if (!Drums && Time >= R->PlaceAtSeconds && Time < R->FireAtSeconds)
		{
			Drums = UGameplayStatics::SpawnSoundAttached(R->DrumSound, Root);
		}
		if (Drums && Time >= R->FireAtSeconds)
		{
			Drums->FadeOut(0.15f, 0.0f);
			bDrumsDone = true;
		}
	}
}

void AExecutionScene::HoldLocalCondemned()
{
	const UExecutionRules* R = GetRules();
	const float Time = GetSceneTime();
	if (Time < R->PlaceAtSeconds || Time >= R->FireAtSeconds)
	{
		return;
	}
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	const int32 Index = Pawn ? Condemned.IndexOfByPredicate([Pawn](const TObjectPtr<APawn>& Item) { return Item.Get() == Pawn; }) : INDEX_NONE;
	if (Index == INDEX_NONE)
	{
		return;
	}
	if (AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Pawn))
	{
		Islander->SetForceThirdPerson(true);
	}
	// facing the sea: this machine owns its pawn's rotation
	const FTransform Spot = CondemnedSpot(Index, HalfHeightOf(Pawn));
	Pawn->SetActorRotation(Spot.Rotator());
}

#undef LOCTEXT_NAMESPACE
