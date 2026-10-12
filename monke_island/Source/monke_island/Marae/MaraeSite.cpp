// monke_island

#include "Marae/MaraeSite.h"
#include "Marae/MaraeRules.h"
#include "monke_island.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AMaraeSite::AMaraeSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	Tags.Add(TEXT("TikiQuietZone"));

	QuietZone = CreateDefaultSubobject<USphereComponent>(TEXT("QuietZone"));
	QuietZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	QuietZone->SetSphereRadius(1500.0f);
	RootComponent = QuietZone;
}

void AMaraeSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AMaraeSite, Rules, COND_InitialOnly);
}

void AMaraeSite::DealArtifacts(const TArray<TSubclassOf<AActor>>& Artifacts)
{
	DealtArtifacts = Artifacts;
	bArtifactsDealt = true;
}

void AMaraeSite::BeginPlay()
{
	Super::BeginPlay();
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_MaraeRules, empty marae"), *GetName());
		return;
	}
	QuietZone->SetSphereRadius(Rules->QuietRadius);
	Clearing.Clear(this, GetActorLocation(), Rules->ClearRadius);
	Build();
	UE_LOG(Logmonke_island, Log, TEXT("%s: marae of %d pieces, %d actors, %d vegetation instances cleared%s"), *GetName(), Pieces.Num(), Spawned.Num(),
		Clearing.Num(), bArtifactsDealt ? *FString::Printf(TEXT(", %d artifacts dealt by the campaign"), DealtArtifacts.Num()) : TEXT(""));
}

void AMaraeSite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Clearing.Restore();
	for (const TWeakObjectPtr<AActor>& Actor : Spawned)
	{
		AActor* Alive = Actor.Get();
		// sacred objects carried away stay theirs; torches and objects left go with the marae
		if (Alive && EndPlayReason == EEndPlayReason::Destroyed && (!Alive->GetIsReplicated() || HasAuthority()))
		{
			Alive->Destroy();
		}
	}
	Spawned.Reset();
	Super::EndPlay(EndPlayReason);
}

void AMaraeSite::Build()
{
	const FTransform& Site = GetActorTransform();
	const FVector Ground = FSiteClearing::GroundAt(this, GetActorLocation(), {});
	for (const FMaraePiece& Piece : Rules->Pieces)
	{
		if (!Piece.Mesh)
		{
			continue;
		}
		FTransform World = Piece.Transform * Site;
		const FVector Base = Piece.bFollowGround ? FSiteClearing::GroundAt(this, World.GetLocation(), {}) : Ground + FVector(0.0, 0.0, Rules->StructureOffsetZ);
		World.SetLocation(FVector(World.GetLocation().X, World.GetLocation().Y, Base.Z + Piece.Transform.GetLocation().Z));
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Piece.Mesh);
		Component->SetupAttachment(RootComponent);
		Component->SetWorldTransform(World);
		if (Piece.bCollision)
		{
			Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		else
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		Component->SetCanEverAffectNavigation(false);
		Component->RegisterComponent();
		Pieces.Add(Component);
	}
	SpawnActors(Ground + FVector(0.0, 0.0, Rules->StructureOffsetZ));
}

void AMaraeSite::SpawnActors(const FVector& Ground)
{
	const FTransform& Site = GetActorTransform();
	for (const FMaraeActorSpot& Spot : Rules->Actors)
	{
		const AActor* Default = Spot.ActorClass ? Spot.ActorClass->GetDefaultObject<AActor>() : nullptr;
		// replicated actors (the sacred objects worth money) come from the server; the torches are built everywhere
		if (!Default || (Default->GetIsReplicated() && !HasAuthority()))
		{
			continue;
		}
		// the pedestal of an artifact the campaign did not deal to this site stays empty
		if (bArtifactsDealt && Rules->ArtifactPool.Contains(Spot.ActorClass) && !DealtArtifacts.Contains(Spot.ActorClass))
		{
			continue;
		}
		FTransform World = Spot.Transform * Site;
		World.SetLocation(FVector(World.GetLocation().X, World.GetLocation().Y, Ground.Z + Spot.Transform.GetLocation().Z));
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AActor* Actor = GetWorld()->SpawnActor<AActor>(Spot.ActorClass, World, Params))
		{
			Spawned.Add(Actor);
		}
	}
}
