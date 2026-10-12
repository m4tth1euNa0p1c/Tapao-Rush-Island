// monke_island

#include "Village/VillageDoorsComponent.h"
#include "Village/VillageDoorMath.h"
#include "Village/VillageHouseKit.h"
#include "monke_island.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UVillageDoorsComponent::UVillageDoorsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// ticks only while a panel slides
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UVillageDoorsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UVillageDoorsComponent, OpenDoorways);
	DOREPLIFETIME(UVillageDoorsComponent, UnsealedVillages);
}

void UVillageDoorsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void UVillageDoorsComponent::AddHouse(UVillageHouseKit* Kit, const FTransform& House, int32 Village)
{
	if (bBuilt || !Kit || Kit->Doorways.IsEmpty() || Kit->Panels.IsEmpty())
	{
		return;
	}
	int32 KitIndex = Kits.Find(Kit);
	if (KitIndex == INDEX_NONE)
	{
		KitIndex = Kits.Add(Kit);
	}
	const int32 HouseIndex = Houses.AddDefaulted();
	FDoorHouse& Entry = Houses[HouseIndex];
	Entry.Kit = KitIndex;
	Entry.Transform = House;
	Entry.FirstDoorway = DoorwayHouse.Num();
	Entry.Village = Village;
	Entry.bSealed = Kit->bSealedUntilFlag;
	const FBox Bounds = Kit->ShellMesh ? Kit->ShellMesh->GetBoundingBox() : FBox(FVector(-1000.0), FVector(1000.0));
	Entry.Center = FVector2D(House.TransformPosition(Bounds.GetCenter()));
	// a running pawn opens a door from up to MaxLeadDistance away (T-301)
	Entry.Reach = Bounds.GetExtent().Size2D() + Kit->TriggerDepth + Kit->TriggerSideMargin + Kit->MaxLeadDistance;
	for (int32 Index = 0; Index < Kit->Doorways.Num(); ++Index)
	{
		DoorwayHouse.Add(HouseIndex);
		DoorwayPanels.AddDefaulted();
	}
	for (int32 Index = 0; Index < Kit->Panels.Num(); ++Index)
	{
		const FVillageHousePanel& KitPanel = Kit->Panels[Index];
		if (!KitPanel.Mesh || !Kit->Doorways.IsValidIndex(KitPanel.Doorway))
		{
			continue;
		}
		int32& Component = ComponentOf.FindOrAdd(TPair<int32, UStaticMesh*>(Village, KitPanel.Mesh.Get()), INDEX_NONE);
		if (Component == INDEX_NONE)
		{
			Component = ComponentMeshes.Add(KitPanel.Mesh);
			PendingInstances.AddDefaulted();
		}
		const int32 PanelIndex = Panels.AddDefaulted();
		FDoorPanel& Panel = Panels[PanelIndex];
		Panel.House = HouseIndex;
		Panel.KitPanel = Index;
		Panel.Component = Component;
		Panel.Instance = PendingInstances[Component].Num();
		PendingInstances[Component].Add(PanelTransform(Panel));
		DoorwayPanels[Entry.FirstDoorway + KitPanel.Doorway].Add(PanelIndex);
	}
}

void UVillageDoorsComponent::FinishBuild(bool bBlockCamera)
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World)
	{
		return;
	}
	for (int32 Index = 0; Index < ComponentMeshes.Num(); ++Index)
	{
		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Component->SetupAttachment(Owner->GetRootComponent());
		// the panels slide: movable, and WorldDynamic so the floor traces of the totems and the loot pass them
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(ComponentMeshes[Index]);
		Component->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		if (!bBlockCamera)
		{
			Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
		Component->SetCanEverAffectNavigation(false);
		Component->SetGenerateOverlapEvents(false);
		Component->RegisterComponent();
		Component->AddInstances(PendingInstances[Index], false, true, false);
		PanelComponents.Add(Component);
	}
	PendingInstances.Empty();
	ComponentOf.Empty();

	const int32 Count = DoorwayHouse.Num();
	LastNear.Init(-1.0, Count);
	ServerOpen.Init(false, Count);
	LocalOpen.Init(false, Count);
	Shown.Init(false, Count);
	float Interval = 0.0f;
	for (const FDoorHouse& House : Houses)
	{
		CellSize = FMath::Max(CellSize, House.Reach);
		const float KitInterval = FMath::Max(Kits[House.Kit]->CheckInterval, 0.02f);
		Interval = Interval > 0.0f ? FMath::Min(Interval, KitInterval) : KitInterval;
	}
	for (int32 Index = 0; Index < Houses.Num(); ++Index)
	{
		Grid.FindOrAdd(VillageDoorMath::CellOf(Houses[Index].Center, CellSize)).Add(Index);
	}
	// a client may have the server's open doorways before its houses stand
	if (!Owner->HasAuthority())
	{
		OnRep_OpenDoorways();
	}
	if (Count > 0)
	{
		World->GetTimerManager().SetTimer(CheckTimer, this, &UVillageDoorsComponent::CheckPawns, Interval, true);
	}
}

void UVillageDoorsComponent::OnRep_OpenDoorways()
{
	if (!bBuilt)
	{
		// applied by FinishBuild
		return;
	}
	ServerOpen.Init(false, DoorwayHouse.Num());
	for (const int32 Doorway : OpenDoorways)
	{
		if (ServerOpen.IsValidIndex(Doorway))
		{
			ServerOpen[Doorway] = true;
		}
	}
	RefreshShown();
}

void UVillageDoorsComponent::UnsealVillage(int32 Village)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || UnsealedVillages.Contains(Village))
	{
		return;
	}
	UnsealedVillages.Add(Village);
	UnsealedVillages.Sort();
	int32 Sealed = 0;
	for (const FDoorHouse& House : Houses)
	{
		Sealed += House.bSealed && House.Village == Village ? 1 : 0;
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: village %d unsealed, %d sealed houses open their doors"), *GetNameSafe(Owner), Village, Sealed);
	CheckPawns();
	Owner->ForceNetUpdate();
}

void UVillageDoorsComponent::OnRep_UnsealedVillages()
{
	CheckPawns();
}

void UVillageDoorsComponent::CheckPawns()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!bBuilt || !World || !Owner || DoorwayHouse.IsEmpty())
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const bool bServer = Owner->HasAuthority();
	TBitArray<> Near(false, DoorwayHouse.Num());
	if (bServer)
	{
		// players, villagers, mobs: whoever walks to a door finds it opening
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			MarkNear(**It, Near);
		}
	}
	else
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			if (Controller && Controller->IsLocalController() && Controller->GetPawn())
			{
				MarkNear(*Controller->GetPawn(), Near);
			}
		}
	}
	bool bServerChanged = false;
	for (int32 Doorway = 0; Doorway < DoorwayHouse.Num(); ++Doorway)
	{
		if (Near[Doorway])
		{
			LastNear[Doorway] = Now;
		}
		const FDoorHouse& House = Houses[DoorwayHouse[Doorway]];
		const float CloseDelay = Kits[House.Kit]->CloseDelay;
		// a sealed house opens to nobody, then stands open once its village is unsealed
		const bool bOpen = House.bSealed ? UnsealedVillages.Contains(House.Village)
			: VillageDoorMath::ShouldBeOpen(Near[Doorway], LastNear[Doorway], Now, CloseDelay);
		if (!bServer)
		{
			LocalOpen[Doorway] = bOpen;
		}
		else if (ServerOpen[Doorway] != bOpen)
		{
			ServerOpen[Doorway] = bOpen;
			bServerChanged = true;
		}
	}
	if (bServerChanged)
	{
		OpenDoorways.Reset();
		for (TConstSetBitIterator<> It(ServerOpen); It; ++It)
		{
			OpenDoorways.Add(It.GetIndex());
		}
		// the spawner updates once a second otherwise
		Owner->ForceNetUpdate();
	}
	RefreshShown();
}

void UVillageDoorsComponent::MarkNear(const APawn& Pawn, TBitArray<>& Near) const
{
	// a villager hiding at home is hidden without collision
	if (Pawn.IsHidden() || !Pawn.GetActorEnableCollision())
	{
		return;
	}
	float Radius = 0.0f;
	float HalfHeight = 0.0f;
	Pawn.GetSimpleCollisionCylinder(Radius, HalfHeight);
	const FVector Location = Pawn.GetActorLocation();
	const FVector Velocity = Pawn.GetVelocity();
	const FVector2D Point(Location);
	// a moving pawn opens the doors ahead of it (T-301, review R-08); the server sees the pawn of a client half a round
	// trip late, so it opens that much earlier for it (its own pawn and the creatures have no delay)
	float HalfRoundTrip = 0.0f;
	const AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority() && !Pawn.IsLocallyControlled())
	{
		if (const APlayerState* State = Pawn.GetPlayerState())
		{
			HalfRoundTrip = 0.0005f * State->GetPingInMilliseconds();
		}
	}
	const FIntPoint Cell = VillageDoorMath::CellOf(Point, CellSize);
	for (int32 DX = -1; DX <= 1; ++DX)
	{
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			const TArray<int32>* InCell = Grid.Find(Cell + FIntPoint(DX, DY));
			if (!InCell)
			{
				continue;
			}
			for (const int32 HouseIndex : *InCell)
			{
				const FDoorHouse& House = Houses[HouseIndex];
				if (FVector2D::DistSquared(Point, House.Center) > FMath::Square(House.Reach + Radius))
				{
					continue;
				}
				const UVillageHouseKit& Kit = *Kits[House.Kit];
				const FVector Local = House.Transform.InverseTransformPosition(Location);
				// in the same frame and units as Local (a scaled house scales both)
				const FVector LocalVelocity = House.Transform.InverseTransformVector(Velocity);
				const float Lead = VillageDoorMath::LeadSeconds(HalfRoundTrip, Kit.CheckInterval, Kit.SlideSeconds, Kit.LeadSlideShare, Kit.LeadExtraSeconds,
					Kit.MaxLeadPingSeconds);
				for (int32 Index = 0; Index < Kit.Doorways.Num(); ++Index)
				{
					if (VillageDoorMath::IsNearDoorwayAhead(Kit.Doorways[Index], Local, LocalVelocity, Radius, Kit.TriggerDepth, Kit.TriggerSideMargin, Lead,
						Kit.MaxLeadDistance))
					{
						Near[House.FirstDoorway + Index] = true;
					}
				}
			}
		}
	}
}

void UVillageDoorsComponent::RefreshShown()
{
	for (int32 Doorway = 0; Doorway < Shown.Num(); ++Doorway)
	{
		const bool bOpen = ServerOpen[Doorway] || LocalOpen[Doorway];
		if (Shown[Doorway] == bOpen)
		{
			continue;
		}
		Shown[Doorway] = bOpen;
		for (const int32 PanelIndex : DoorwayPanels[Doorway])
		{
			FDoorPanel& Panel = Panels[PanelIndex];
			const float Target = bOpen ? Kits[Houses[Panel.House].Kit]->Panels[Panel.KitPanel].OpenFraction : 0.0f;
			if (Panel.Target != Target)
			{
				Panel.Target = Target;
				Moving.AddUnique(PanelIndex);
			}
		}
	}
	if (!Moving.IsEmpty())
	{
		SetComponentTickEnabled(true);
	}
}

void UVillageDoorsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	for (int32 Index = Moving.Num() - 1; Index >= 0; --Index)
	{
		FDoorPanel& Panel = Panels[Moving[Index]];
		Panel.Alpha = VillageDoorMath::StepToward(Panel.Alpha, Panel.Target, DeltaTime, Kits[Houses[Panel.House].Kit]->SlideSeconds);
		if (UInstancedStaticMeshComponent* Component = PanelComponents.IsValidIndex(Panel.Component) ? PanelComponents[Panel.Component].Get() : nullptr)
		{
			// teleported: a door never pushes, it opens before anyone reaches it and closes once nobody is near
			Component->UpdateInstanceTransform(Panel.Instance, PanelTransform(Panel), true, true, true);
		}
		if (Panel.Alpha == Panel.Target)
		{
			Moving.RemoveAtSwap(Index);
		}
	}
	if (Moving.IsEmpty())
	{
		SetComponentTickEnabled(false);
	}
}

FTransform UVillageDoorsComponent::PanelTransform(const FDoorPanel& Panel) const
{
	const FDoorHouse& House = Houses[Panel.House];
	const FVillageHousePanel& KitPanel = Kits[House.Kit]->Panels[Panel.KitPanel];
	return FTransform(FRotator(0.0f, VillageDoorMath::PanelYaw(KitPanel, Panel.Alpha), 0.0f), VillageDoorMath::PanelLocation(KitPanel, Panel.Alpha))
		* House.Transform;
}
