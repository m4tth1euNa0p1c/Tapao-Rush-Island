// monke_island

#include "Camps/CampSite.h"
#include "Camps/CampRules.h"
#include "monke_island.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ACampSite::ACampSite()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;
	Tags.Add(TEXT("TikiQuietZone"));

	QuietZone = CreateDefaultSubobject<USphereComponent>(TEXT("QuietZone"));
	QuietZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	QuietZone->SetSphereRadius(1100.0f);
	RootComponent = QuietZone;
}

void ACampSite::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ACampSite, Rules, COND_InitialOnly);
	DOREPLIFETIME_CONDITION(ACampSite, CampSeed, COND_InitialOnly);
}

void ACampSite::BeginPlay()
{
	Super::BeginPlay();
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_CampRules, empty camp"), *GetName());
		return;
	}
	QuietZone->SetSphereRadius(Rules->QuietRadius);
	Clearing.Clear(this, GetActorLocation(), Rules->ClearRadius);
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d vegetation instances cleared"), *GetName(), Clearing.Num());
	BuildDecor();
	if (HasAuthority())
	{
		SpawnItems();
	}
}

void ACampSite::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// the vegetation comes back (a camp removed during the game, or the end of a play session in the editor)
	Clearing.Restore();
	if (HasAuthority())
	{
		for (const TWeakObjectPtr<AActor>& Item : Items)
		{
			if (AActor* Alive = Item.Get(); Alive && EndPlayReason == EEndPlayReason::Destroyed)
			{
				Alive->Destroy();
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

FVector ACampSite::GroundAt(const FVector& World) const
{
	TArray<const UPrimitiveComponent*> Ignored;
	for (const TObjectPtr<UStaticMeshComponent>& Piece : DecorPieces)
	{
		Ignored.Add(Piece.Get());
	}
	return FSiteClearing::GroundAt(this, World, Ignored);
}

void ACampSite::BuildDecor()
{
	const FTransform& Camp = GetActorTransform();
	for (const FCampPiece& Piece : Rules->Pieces)
	{
		UStaticMesh* Mesh = Rules->GetPieceMesh(Piece.Kind);
		if (!Mesh)
		{
			continue;
		}
		FTransform World = Piece.Transform * Camp;
		World.SetLocation(GroundAt(World.GetLocation()) + FVector(0.0, 0.0, Piece.Transform.GetLocation().Z));
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Mesh);
		Component->SetupAttachment(RootComponent);
		Component->SetWorldTransform(World);
		Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Component->SetCanEverAffectNavigation(false);
		Component->RegisterComponent();
		DecorPieces.Add(Component);

		if (Piece.Kind == ECampPieceKind::Campfire && Rules->FireLightIntensity > 0.0f)
		{
			FireLight = NewObject<UPointLightComponent>(this);
			FireLight->SetupAttachment(Component, Component->DoesSocketExist(TEXT("fire")) ? FName(TEXT("fire")) : NAME_None);
			FireLight->SetRelativeLocation(FVector(0.0, 0.0, 35.0));
			FireLight->SetIntensityUnits(ELightUnits::Candelas);
			FireLight->SetIntensity(Rules->FireLightIntensity);
			FireLight->SetLightColor(FLinearColor(1.0f, 0.55f, 0.22f));
			FireLight->SetAttenuationRadius(Rules->FireLightRadius);
			FireLight->SetCastShadows(false);
			if (Rules->FireLightFunction)
			{
				FireLight->SetLightFunctionMaterial(Rules->FireLightFunction);
			}
			FireLight->RegisterComponent();
			// the FBX sockets come in with a scale of 100 (memory: ue-mcp-import-pitfalls)
			FireLight->SetWorldScale3D(FVector::OneVector);
		}
	}
}

AActor* ACampSite::SpawnItem(TSubclassOf<AActor> ItemClass, const FTransform& World)
{
	if (!ItemClass)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AActor* Item = GetWorld()->SpawnActor<AActor>(ItemClass, World, Params);
	if (Item)
	{
		Items.Add(Item);
	}
	return Item;
}

void ACampSite::SpawnItems()
{
	const FTransform& Camp = GetActorTransform();
	auto OnGround = [this, &Camp](const FTransform& Local, float Lift)
	{
		FTransform World = Local * Camp;
		World.SetLocation(GroundAt(World.GetLocation()) + FVector(0.0, 0.0, Local.GetLocation().Z + Lift));
		return World;
	};
	for (const FCampItem& Tool : Rules->Tools)
	{
		SpawnItem(Tool.ItemClass, OnGround(Tool.Transform, 6.0f));
	}
	for (const FTransform& Spot : Rules->FishSpots)
	{
		SpawnItem(Rules->FishClass, OnGround(Spot, 4.0f));
	}

	// loot on the floor of the hut, around its sockets
	const UStaticMeshComponent* Hut = nullptr;
	for (const TObjectPtr<UStaticMeshComponent>& Piece : DecorPieces)
	{
		if (Piece && Piece->GetStaticMesh() == Rules->HutMesh)
		{
			Hut = Piece;
			break;
		}
	}
	if (!Hut || Rules->HutLootClasses.IsEmpty())
	{
		return;
	}
	FRandomStream Stream(CampSeed);
	const int32 Count = Stream.RandRange(Rules->HutLootCount.Min, FMath::Max(Rules->HutLootCount.Min, Rules->HutLootCount.Max));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FName Socket = Rules->HutLootSockets.IsEmpty() ? NAME_None : Rules->HutLootSockets[Index % Rules->HutLootSockets.Num()];
		FVector Spot = Socket.IsNone() || !Hut->DoesSocketExist(Socket) ? Hut->GetComponentLocation() + FVector(0.0, 0.0, 45.0)
			: Hut->GetSocketLocation(Socket);
		Spot += FVector(Stream.FRandRange(-1.0f, 1.0f), Stream.FRandRange(-1.0f, 1.0f), 0.0f) * Rules->HutLootSpreadCm + FVector(0.0, 0.0, 8.0);
		const TSubclassOf<AActor> Loot = Rules->HutLootClasses[Stream.RandRange(0, Rules->HutLootClasses.Num() - 1)];
		SpawnItem(Loot, FTransform(FRotator(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f), Spot));
	}
}
