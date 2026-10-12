// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VillageDoorsComponent.generated.h"

class APawn;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UVillageHouseKit;

/**
 *  Sliding doors of the village houses built from a kit (T-186, docs/features/villages-japonais.md), on AVillageSpawner.
 *  Every machine adds the same houses in the same order and builds their panels as instanced meshes that collide
 *  (movable, WorldDynamic): a closed panel is a wall. The server looks at every pawn near the houses and opens a doorway
 *  while one is near it, closing it CloseDelay after the last left; the open doorways replicate. A client also opens
 *  at once the doorways near its own pawn, so its predicted movement never walks into a door the server opened.
 *  Each machine slides the panels toward their state, eased, and ticks only while one moves.
 *  A house whose kit waits for the flag (bSealedUntilFlag, the shrine) keeps its doors shut for everyone until the
 *  server unseals its village (the flag ceremony is over); they then open for good.
 */
UCLASS(ClassGroup=(Village))
class MONKE_ISLAND_API UVillageDoorsComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UVillageDoorsComponent();

	/** Adds the panels of a house built from a kit; every machine adds the same houses in the same order (the doorway
	 *  indices replicate) */
	void AddHouse(UVillageHouseKit* Kit, const FTransform& House, int32 Village);

	/** Builds the panels added so far and starts watching the pawns; bBlockCamera: a closed door stops the camera */
	void FinishBuild(bool bBlockCamera);

	/** Houses with doors */
	int32 GetHouseCount() const { return Houses.Num(); }

	/** Doorways of every house */
	int32 GetDoorwayCount() const { return DoorwayHouse.Num(); }

	/** Sliding panels of every house */
	int32 GetPanelCount() const { return Panels.Num(); }

	/** True when the doorway is open on this machine */
	bool IsDoorwayOpen(int32 Doorway) const { return Shown.IsValidIndex(Doorway) && Shown[Doorway]; }

	/** Server: the flag of this village is up: the doors of its sealed houses open for good */
	void UnsealVillage(int32 Village);

	/** True once the flag of this village is up (every machine) */
	bool IsVillageUnsealed(int32 Village) const { return UnsealedVillages.Contains(Village); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** A house with doors */
	struct FDoorHouse
	{
		/** Index in Kits */
		int32 Kit = INDEX_NONE;
		/** Placement of the house */
		FTransform Transform;
		/** Middle of its shell (world, 2D) */
		FVector2D Center = FVector2D::ZeroVector;
		/** Farthest a pawn near one of its doorways can be from Center (cm) */
		double Reach = 0.0;
		/** Index of its first doorway */
		int32 FirstDoorway = 0;
		/** Its village */
		int32 Village = INDEX_NONE;
		/** Shut until its village is unsealed */
		bool bSealed = false;
	};

	/** One panel of one house */
	struct FDoorPanel
	{
		int32 House = 0;
		/** Index in the Panels of the kit */
		int32 KitPanel = 0;
		/** Index in PanelComponents */
		int32 Component = 0;
		/** Instance in its component */
		int32 Instance = 0;
		/** Share of its travel slid (0 closed) */
		float Alpha = 0.0f;
		/** Share it slides toward */
		float Target = 0.0f;
	};

	/** Kits of the houses */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVillageHouseKit>> Kits;

	/** Mesh of each panel component (held by the kits) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> ComponentMeshes;

	/** Instanced panels, one per village and panel mesh */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PanelComponents;

	/** Doorways the server holds open (sorted indices) */
	UPROPERTY(ReplicatedUsing=OnRep_OpenDoorways)
	TArray<int32> OpenDoorways;

	/** Villages whose flag is up: their sealed houses stand open */
	UPROPERTY(ReplicatedUsing=OnRep_UnsealedVillages)
	TArray<int32> UnsealedVillages;

	TArray<FDoorHouse> Houses;

	TArray<FDoorPanel> Panels;

	/** House of each doorway */
	TArray<int32> DoorwayHouse;

	/** Panels of each doorway */
	TArray<TArray<int32>> DoorwayPanels;

	/** Component of each village and panel mesh while the houses are added */
	TMap<TPair<int32, UStaticMesh*>, int32> ComponentOf;

	/** Instances of each component waiting for FinishBuild */
	TArray<TArray<FTransform>> PendingInstances;

	/** Last time a watched pawn was near each doorway (world seconds, < 0 never): every pawn on the server, the own
	 *  pawns on a client */
	TArray<double> LastNear;

	/** Doorways open on the server (from OpenDoorways on a client) */
	TBitArray<> ServerOpen;

	/** Doorways near the own pawns of a client */
	TBitArray<> LocalOpen;

	/** Doorways open on this machine */
	TBitArray<> Shown;

	/** Panels sliding */
	TArray<int32> Moving;

	/** Houses by cell of CellSize */
	TMap<FIntPoint, TArray<int32>> Grid;

	/** Side of a grid cell: the largest reach of a house (cm) */
	double CellSize = 1000.0;

	FTimerHandle CheckTimer;

	bool bBuilt = false;

	UFUNCTION()
	void OnRep_OpenDoorways();

	UFUNCTION()
	void OnRep_UnsealedVillages();

	/** Looks at the pawns near the doors: the server at every pawn, a client at its own */
	void CheckPawns();

	/** Flags the doorways a pawn stands near */
	void MarkNear(const APawn& Pawn, TBitArray<>& Near) const;

	/** Shown state of every doorway from the server's and the local one; starts the slides that change */
	void RefreshShown();

	/** World placement of a panel at its current Alpha */
	FTransform PanelTransform(const FDoorPanel& Panel) const;
};
