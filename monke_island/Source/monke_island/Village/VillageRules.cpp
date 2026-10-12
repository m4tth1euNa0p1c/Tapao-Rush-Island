// monke_island

#include "Village/VillageRules.h"
#include "Village/VillageAmbienceActor.h"
#include "Village/VillageHouseKit.h"

namespace
{
	FVillageClearMargins Margins(float Palm, float Young, float Bush, float Fallen, float Ground)
	{
		FVillageClearMargins Out;
		Out.Palm = Palm;
		Out.Young = Young;
		Out.Bush = Bush;
		Out.Fallen = Fallen;
		Out.Ground = Ground;
		return Out;
	}

	FVillagePlantRule PlantRule(const TCHAR* Prefix, EVillagePlant Plant)
	{
		FVillagePlantRule Out;
		Out.MeshPrefix = Prefix;
		Out.Plant = Plant;
		return Out;
	}
}

UVillageRules::UVillageRules()
{
	AmbienceClass = AVillageAmbienceActor::StaticClass();
	ClearMeshPaths = { TEXT("/Game/MonkeIsland/Environment/Vegetation/"), TEXT("/Game/MonkeIsland/Environment/Props/") };
	// the tables of scripts/map/village_clearing.py (CATEGORIES, MARGIN_CM)
	PlantRules = {
		PlantRule(TEXT("SM_CoconutPalm_Fallen"), EVillagePlant::Fallen),
		PlantRule(TEXT("SM_CoconutPalm_Juvenile"), EVillagePlant::Young),
		PlantRule(TEXT("SM_CoconutPalm_Young"), EVillagePlant::Young),
		PlantRule(TEXT("SM_CoconutPalm_Sprout"), EVillagePlant::Ground),
		PlantRule(TEXT("SM_CoconutPalm_"), EVillagePlant::Palm),
		PlantRule(TEXT("SM_Pandanus_Young"), EVillagePlant::Young),
		PlantRule(TEXT("SM_Naupaka"), EVillagePlant::Bush),
		PlantRule(TEXT("SM_MikiMiki"), EVillagePlant::Bush),
		PlantRule(TEXT("SM_Tahinu"), EVillagePlant::Bush),
		PlantRule(TEXT("SM_Pandanus"), EVillagePlant::Bush),
	};
	BuildingMargins = Margins(300.0f, 350.0f, 300.0f, 500.0f, 100.0f);
	SmallMargins = Margins(80.0f, 240.0f, 240.0f, 300.0f, 40.0f);
	PlazaMargins = Margins(150.0f, 250.0f, 250.0f, 400.0f, 50.0f);
	RoadMargins = Margins(80.0f, 150.0f, 150.0f, 300.0f, 30.0f);
	PathMargins = Margins(80.0f, 150.0f, 150.0f, 300.0f, 30.0f);
}

float UVillageRules::GetMargin(EVillageClearKind Kind, EVillagePlant Plant) const
{
	switch (Kind)
	{
	case EVillageClearKind::Building: return BuildingMargins.For(Plant);
	case EVillageClearKind::Small: return SmallMargins.For(Plant);
	case EVillageClearKind::Plaza: return PlazaMargins.For(Plant);
	case EVillageClearKind::Road: return RoadMargins.For(Plant);
	case EVillageClearKind::Path: return PathMargins.For(Plant);
	default: return 0.0f;
	}
}

float UVillageRules::GetMaxMargin() const
{
	float Max = 0.0f;
	for (const FVillageClearMargins* Table : { &BuildingMargins, &SmallMargins, &PlazaMargins, &RoadMargins, &PathMargins })
	{
		Max = FMath::Max(Max, FMath::Max(FMath::Max(Table->Palm, Table->Young), FMath::Max(FMath::Max(Table->Bush, Table->Fallen), Table->Ground)));
	}
	return Max;
}

EVillagePlant UVillageRules::GetPlant(const FString& MeshName) const
{
	for (const FVillagePlantRule& Rule : PlantRules)
	{
		if (!Rule.MeshPrefix.IsEmpty() && MeshName.StartsWith(Rule.MeshPrefix))
		{
			return Rule.Plant;
		}
	}
	return EVillagePlant::Ground;
}

bool UVillageRules::IsClearable(const FString& MeshPath) const
{
	for (const FString& Path : ClearMeshPaths)
	{
		if (!Path.IsEmpty() && MeshPath.StartsWith(Path))
		{
			return true;
		}
	}
	return false;
}

UVillageHouseKit* UVillageRules::FindHouseKit(const UStaticMesh* Mesh) const
{
	if (!Mesh)
	{
		return nullptr;
	}
	for (UVillageHouseKit* Kit : HouseKits)
	{
		// without its shell the kit would add its panels to a house that already has them
		if (Kit && Kit->SourceMesh == Mesh && Kit->ShellMesh)
		{
			return Kit;
		}
	}
	return nullptr;
}
