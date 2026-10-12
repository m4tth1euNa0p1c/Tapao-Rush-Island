// monke_island

#include "Creatures/TikiBlood.h"
#include "Creatures/TikiBloodSpecies.h"
#include "Creatures/TikiRules.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Animation/Skeleton.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkinnedAsset.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

namespace TikiBloodParams
{
	/** Niagara user parameters and decal material parameters read by the blood assets (documented on UTikiRules) */
	static const FName Color(TEXT("BloodColor"));
	static const FName Direction(TEXT("Direction"));
	static const FName Duration(TEXT("Duration"));
	static const FName GrowStartTime(TEXT("GrowStartTime"));
	static const FName GrowDuration(TEXT("GrowDuration"));
}

namespace TikiBloodSystems
{
	/** Kinds of blood systems, for UTikiBloodSubsystem::SystemOf */
	static constexpr int32 Impact = 0;
	static constexpr int32 SliceSpray = 1;
	static constexpr int32 Chunks = 2;

	/** Species table loaded when DefaultGame.ini does not say otherwise */
	static const TCHAR* DefaultSpeciesRulesPath = TEXT("/Game/MonkeIsland/VFX/Blood/DA_BloodSpecies.DA_BloodSpecies");
}

UTikiBloodSubsystem::UTikiBloodSubsystem()
{
	SpeciesRulesPath = FSoftObjectPath(TikiBloodSystems::DefaultSpeciesRulesPath);
}

UTikiBloodSubsystem* UTikiBloodSubsystem::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}
	return World->GetSubsystem<UTikiBloodSubsystem>();
}

void UTikiBloodSubsystem::Deinitialize()
{
	for (TPair<TWeakObjectPtr<const UTikiRules>, TSharedPtr<FStreamableHandle>>& Load : Loads)
	{
		if (Load.Value.IsValid())
		{
			Load.Value->CancelHandle();
		}
	}
	Loads.Empty();
	for (TSharedPtr<FStreamableHandle>* Handle : { &SpeciesLoad, &SpeciesSystemsLoad })
	{
		if (Handle->IsValid())
		{
			(*Handle)->CancelHandle();
			Handle->Reset();
		}
	}
	Decals.Empty();
	Super::Deinitialize();
}

void UTikiBloodSubsystem::Preload(const UTikiRules* Rules)
{
	if (!Rules || !Rules->bBloodEnabled)
	{
		return;
	}
	RequestSpecies();
	if (Loads.Contains(Rules))
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const FSoftObjectPath& Path : { Rules->MusketImpactSystem.ToSoftObjectPath(), Rules->SliceSpraySystem.ToSoftObjectPath(),
		Rules->ChunksSystem.ToSoftObjectPath(), Rules->PoolDecalMaterial.ToSoftObjectPath() })
	{
		if (!Path.IsNull())
		{
			Paths.Add(Path);
		}
	}
	for (const TSoftObjectPtr<UMaterialInterface>& Splat : Rules->SplatDecalMaterials)
	{
		if (!Splat.IsNull())
		{
			Paths.Add(Splat.ToSoftObjectPath());
		}
	}
	// kept even when empty: one request per rules asset; an asset still loading is skipped by the spawns (no hitch)
	Loads.Add(Rules, Paths.IsEmpty() ? nullptr : UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths));
	if (Paths.IsEmpty())
	{
		UE_LOG(Logmonke_island, Log, TEXT("Tiki blood: %s has no blood asset set, no blood"), *GetNameSafe(Rules));
	}
}

void UTikiBloodSubsystem::RequestSpecies()
{
	if (bSpeciesRequested)
	{
		return;
	}
	bSpeciesRequested = true;
	if (SpeciesRulesPath.IsNull())
	{
		return;
	}
	// the table first (a few soft references), then the systems of its species, all in the background
	SpeciesLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(SpeciesRulesPath, FStreamableDelegate::CreateWeakLambda(this, [this]()
	{
		SpeciesRules = Cast<UTikiBloodSpeciesRules>(SpeciesRulesPath.ResolveObject());
		if (!SpeciesRules)
		{
			UE_LOG(Logmonke_island, Log, TEXT("Tiki blood: no species table at %s, every body bleeds the blood of its rules"), *SpeciesRulesPath.ToString());
			return;
		}
		TArray<FSoftObjectPath> Systems;
		for (const FTikiBloodSpecies& Species : SpeciesRules->Species)
		{
			for (const TSoftObjectPtr<UNiagaraSystem>* System : { &Species.MusketImpactSystem, &Species.SliceSpraySystem, &Species.ChunksSystem })
			{
				if (!System->IsNull())
				{
					Systems.AddUnique(System->ToSoftObjectPath());
				}
			}
		}
		UE_LOG(Logmonke_island, Log, TEXT("Tiki blood: %d species (%s), %d systems of their own"), SpeciesRules->Species.Num(),
			*GetNameSafe(SpeciesRules), Systems.Num());
		SpeciesBySkeleton.Reset();
		if (!Systems.IsEmpty())
		{
			SpeciesSystemsLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Systems);
		}
	}));
}

int32 UTikiBloodSubsystem::ResolveSpecies(const AActor* Source, const USceneComponent* Body, const FVector& Location)
{
	if (!SpeciesRules)
	{
		return INDEX_NONE;
	}
	bool bBody = false;
	int32 Species = INDEX_NONE;
	// a skinned mesh of the body: true when its skeleton belongs to a listed species
	auto Match = [this, &bBody, &Species](const USkinnedMeshComponent* Skinned) -> bool
	{
		const USkinnedAsset* Asset = Skinned ? Skinned->GetSkinnedAsset() : nullptr;
		const USkeleton* Skeleton = Asset ? Asset->GetSkeleton() : nullptr;
		if (!Skeleton)
		{
			return false;
		}
		bBody = true;
		if (const int32* Known = SpeciesBySkeleton.Find(Skeleton))
		{
			Species = *Known;
		}
		else
		{
			Species = TikiBloodSpecies::Find(SpeciesRules->Species, FSoftObjectPath(Skeleton));
			SpeciesBySkeleton.Add(Skeleton, Species);
		}
		return Species != INDEX_NONE;
	};
	// the bleeding component first, then the character's mesh, then any skinned mesh of the actor whose skeleton is
	// listed (a corpse: its body, not the weapon it holds)
	if (!Match(Cast<USkinnedMeshComponent>(Body)))
	{
		const AActor* Owner = Source ? Source : (Body ? Body->GetOwner() : nullptr);
		const ACharacter* Character = Cast<ACharacter>(Owner);
		if (Owner && !(Character && Match(Character->GetMesh())))
		{
			const TInlineComponentArray<USkinnedMeshComponent*> Skinned(Owner);
			for (const USkinnedMeshComponent* Component : Skinned)
			{
				if (Match(Component))
				{
					break;
				}
			}
		}
	}
	if (bBody)
	{
		WoundFrame = GFrameCounter;
		WoundLocation = Location;
		WoundSpecies = Species;
		return Species;
	}
	// no body (chunks of a burst): the wound bled here in this frame
	return TikiBloodSpecies::IsSameWound(GFrameCounter, Location, WoundFrame, WoundLocation, SpeciesRules->SameWoundRadius) ? WoundSpecies : INDEX_NONE;
}

FLinearColor UTikiBloodSubsystem::ColorOf(const UTikiRules* Rules, int32 Species) const
{
	if (SpeciesRules && SpeciesRules->Species.IsValidIndex(Species))
	{
		return SpeciesRules->Species[Species].Color;
	}
	return Rules ? Rules->BloodColor : FLinearColor::Red;
}

UNiagaraSystem* UTikiBloodSubsystem::SystemOf(const UTikiRules* Rules, int32 Species, int32 Kind) const
{
	auto Pick = [Kind](const TSoftObjectPtr<UNiagaraSystem>& InImpact, const TSoftObjectPtr<UNiagaraSystem>& InSpray,
		const TSoftObjectPtr<UNiagaraSystem>& InChunks) -> const TSoftObjectPtr<UNiagaraSystem>&
	{
		return Kind == TikiBloodSystems::Impact ? InImpact : Kind == TikiBloodSystems::SliceSpray ? InSpray : InChunks;
	};
	if (SpeciesRules && SpeciesRules->Species.IsValidIndex(Species))
	{
		const FTikiBloodSpecies& Blood = SpeciesRules->Species[Species];
		const TSoftObjectPtr<UNiagaraSystem>& Own = Pick(Blood.MusketImpactSystem, Blood.SliceSpraySystem, Blood.ChunksSystem);
		if (!Own.IsNull())
		{
			// still loading: skipped, never drawn in the wrong colour
			return Own.Get();
		}
	}
	return Rules ? Pick(Rules->MusketImpactSystem, Rules->SliceSpraySystem, Rules->ChunksSystem).Get() : nullptr;
}

bool UTikiBloodSubsystem::ConsumeBudget(const UTikiRules* Rules, const FVector& Location)
{
	if (!Rules || !Rules->bBloodEnabled)
	{
		return false;
	}
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	// nobody sees blood that far (the first local player's camera; none, e.g. a listen server without a view: spawn)
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(World, 0);
		Camera && Rules->BloodCullDistance > 0.0f && FVector::DistSquared(Camera->GetCameraLocation(), Location) > FMath::Square(Rules->BloodCullDistance))
	{
		return false;
	}
	if (BudgetFrame != GFrameCounter)
	{
		BudgetFrame = GFrameCounter;
		BudgetUsed = 0;
	}
	if (BudgetUsed >= Rules->MaxBloodSpawnsPerFrame)
	{
		return false;
	}
	++BudgetUsed;
	return true;
}

UNiagaraComponent* UTikiBloodSubsystem::SpawnSystem(UNiagaraSystem* System, const FLinearColor& Color, const FVector& Location, const FVector& Direction)
{
	if (!System)
	{
		return nullptr;
	}
	const FVector Along = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	// the system's +X follows the direction; the Direction parameter carries it in the world for the modules that read it
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, Along.Rotation(), FVector(1.0f),
		true, true, ENCPoolMethod::AutoRelease, true);
	if (Effect)
	{
		Effect->SetVariableLinearColor(TikiBloodParams::Color, Color);
		Effect->SetVariableVec3(TikiBloodParams::Direction, Along);
	}
	return Effect;
}

void UTikiBloodSubsystem::SpawnHit(const UTikiRules* Rules, const FVector& Location, const FVector& Direction, const AActor* Ignore, bool bSplat)
{
	if (!ConsumeBudget(Rules, Location))
	{
		return;
	}
	const int32 Species = ResolveSpecies(Ignore, nullptr, Location);
	SpawnSystem(SystemOf(Rules, Species, TikiBloodSystems::Impact), ColorOf(Rules, Species), Location, Direction);
	if (bSplat)
	{
		SpawnSplat(Rules, Location, Ignore);
	}
}

void UTikiBloodSubsystem::SpawnSliceSpray(const UTikiRules* Rules, USceneComponent* Body, FName Bone, const FVector& Location, const FVector& Direction)
{
	if (!Rules || !Body)
	{
		return;
	}
	const int32 Species = ResolveSpecies(Body->GetOwner(), Body, Location);
	UNiagaraSystem* System = SystemOf(Rules, Species, TikiBloodSystems::SliceSpray);
	if (!System || !ConsumeBudget(Rules, Location))
	{
		return;
	}
	const FVector Along = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	// not pooled: it is stopped by a timer, a pooled component could be reused by then
	UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Body, Bone, Location, Along.Rotation(),
		EAttachLocation::KeepWorldPosition, true, true, ENCPoolMethod::None, true);
	if (!Effect)
	{
		return;
	}
	Effect->SetVariableLinearColor(TikiBloodParams::Color, ColorOf(Rules, Species));
	Effect->SetVariableVec3(TikiBloodParams::Direction, Along);
	Effect->SetVariableFloat(TikiBloodParams::Duration, Rules->SliceSprayDuration);
	// a looping jet stops emitting after its time, its last drops fall, then it destroys itself
	FTimerHandle Stop;
	GetWorld()->GetTimerManager().SetTimer(Stop, FTimerDelegate::CreateWeakLambda(Effect, [Effect]() { Effect->Deactivate(); }),
		FMath::Max(0.1f, Rules->SliceSprayDuration), false);
}

void UTikiBloodSubsystem::SpawnChunks(const UTikiRules* Rules, const FVector& Location, const FVector& Direction, const AActor* Source)
{
	if (!ConsumeBudget(Rules, Location))
	{
		return;
	}
	const int32 Species = ResolveSpecies(Source, nullptr, Location);
	SpawnSystem(SystemOf(Rules, Species, TikiBloodSystems::Chunks), ColorOf(Rules, Species), Location, Direction);
}

bool UTikiBloodSubsystem::TraceGround(const UTikiRules* Rules, const FVector& Location, const AActor* Ignore, FVector& OutPoint, FVector& OutNormal) const
{
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiBloodGround), false, Ignore);
	// no blood on the water (a water plane with a collision)
	if (const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this))
	{
		for (const TWeakObjectPtr<AActor>& Surface : Water->GetWaterSurfaces())
		{
			if (const AActor* SurfaceActor = Surface.Get())
			{
				Params.AddIgnoredActor(SurfaceActor);
			}
		}
	}
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0.0, 0.0, 20.0), Location - FVector(0.0, 0.0, Rules->GroundTraceDistance), Objects, Params))
	{
		return false;
	}
	// under water the decal would show through it: none
	if (const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this); Water && Water->GetWaterSurfaceAt(Hit.ImpactPoint) > Hit.ImpactPoint.Z + 2.0)
	{
		return false;
	}
	OutPoint = Hit.ImpactPoint;
	OutNormal = Hit.ImpactNormal;
	return true;
}

void UTikiBloodSubsystem::SpawnSplat(const UTikiRules* Rules, const FVector& Location, const AActor* Ignore)
{
	if (!Rules || Rules->SplatDecalMaterials.IsEmpty())
	{
		return;
	}
	UMaterialInterface* Material = Rules->SplatDecalMaterials[FMath::RandRange(0, Rules->SplatDecalMaterials.Num() - 1)].Get();
	FVector Point, Normal;
	if (Material && ConsumeBudget(Rules, Location) && TraceGround(Rules, Location, Ignore, Point, Normal))
	{
		const int32 Species = ResolveSpecies(Ignore, nullptr, Location);
		SpawnDecal(Rules, Material, ColorOf(Rules, Species), Point, Normal,
			FMath::FRandRange(Rules->SplatSizeMin, FMath::Max(Rules->SplatSizeMin, Rules->SplatSizeMax)), Rules->SplatLifetime);
	}
}

void UTikiBloodSubsystem::SpawnPool(const UTikiRules* Rules, const FVector& Location, const AActor* Ignore, float AgeSeconds)
{
	UMaterialInterface* Material = Rules ? Rules->PoolDecalMaterial.Get() : nullptr;
	FVector Point, Normal;
	if (!Material || AgeSeconds >= Rules->PoolLifetime || !ConsumeBudget(Rules, Location) || !TraceGround(Rules, Location, Ignore, Point, Normal))
	{
		return;
	}
	const int32 Species = ResolveSpecies(Ignore, nullptr, Location);
	UDecalComponent* Decal = SpawnDecal(Rules, Material, ColorOf(Rules, Species), Point, Normal, Rules->PoolSize, Rules->PoolLifetime - AgeSeconds);
	if (UMaterialInstanceDynamic* Pool = Decal ? Cast<UMaterialInstanceDynamic>(Decal->GetDecalMaterial()) : nullptr)
	{
		// the material grows the pool from these (no tick); world time, the same as the Time node
		Pool->SetScalarParameterValue(TikiBloodParams::GrowStartTime, static_cast<float>(GetWorld()->GetTimeSeconds()) - AgeSeconds + Rules->PoolGrowDelay);
		Pool->SetScalarParameterValue(TikiBloodParams::GrowDuration, Rules->PoolGrowDuration);
	}
}

UDecalComponent* UTikiBloodSubsystem::SpawnDecal(const UTikiRules* Rules, UMaterialInterface* Material, const FLinearColor& Color, const FVector& Point,
	const FVector& Normal, float Size, float Lifetime)
{
	if (Rules->MaxActiveDecals <= 0)
	{
		return nullptr;
	}
	// the decal projects along its +X, into the ground; a random turn about it so two splats differ
	FRotator Rotation = FRotationMatrix::MakeFromX(-Normal).Rotator();
	Rotation.Roll = FMath::FRandRange(0.0f, 360.0f);
	const FVector Extent(Rules->DecalDepth * 0.5f, Size * 0.5f, Size * 0.5f);
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(GetWorld(), Material, Extent, Point, Rotation, 0.0f);
	if (!Decal)
	{
		return nullptr;
	}
	if (UMaterialInstanceDynamic* Instance = Decal->CreateDynamicMaterialInstance())
	{
		Instance->SetVectorParameterValue(TikiBloodParams::Color, Color);
	}
	// fades out at the end of its life (DecalLifetimeOpacity in the material), then the engine destroys it
	Decal->SetFadeOut(FMath::Max(0.0f, Lifetime - Rules->DecalFadeDuration), FMath::Min(Rules->DecalFadeDuration, FMath::Max(0.1f, Lifetime)), false);

	Decals.RemoveAll([](const TWeakObjectPtr<UDecalComponent>& Old) { return !Old.IsValid(); });
	Decals.Add(Decal);
	while (Decals.Num() > FMath::Max(0, Rules->MaxActiveDecals))
	{
		if (UDecalComponent* Oldest = Decals[0].Get())
		{
			Oldest->DestroyComponent();
		}
		Decals.RemoveAt(0);
	}
	return Decal;
}
