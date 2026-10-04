// monke_island

#include "Camps/CampRules.h"
#include "Engine/StaticMesh.h"

namespace
{
	/** Maquette (Blender, metres, the bean faces -Y) -> camp frame (cm): (x, -y, z) * 100, yaw = -yaw */
	FTransform FromMaquette(double X, double Y, double Z, double YawDeg)
	{
		return FTransform(FRotator(0.0, -YawDeg, 0.0), FVector(X * 100.0, -Y * 100.0, Z * 100.0));
	}

	FCampPiece Piece(ECampPieceKind Kind, double X, double Y, double YawDeg)
	{
		FCampPiece Out;
		Out.Kind = Kind;
		Out.Transform = FromMaquette(X, Y, 0.0, YawDeg);
		return Out;
	}
}

UCampRules::UCampRules()
{
	// campement-pecheurs.blend, placements of the scene (scripts/props/camp_textures.py notes)
	Pieces = {
		Piece(ECampPieceKind::Hut, -2.1, 1.5, 0.0),
		Piece(ECampPieceKind::Shelter, 2.7, 1.6, -12.6),
		Piece(ECampPieceKind::Campfire, 0.25, -0.85, 0.0),
		Piece(ECampPieceKind::LogSeat, -0.6, -0.95, 10.0),
		Piece(ECampPieceKind::LogSeat, 1.05, -1.4, -25.0),
		Piece(ECampPieceKind::DryingRack, 4.1, -0.85, 20.1),
	};
	// tools: rod, trap, net, basket in this order (classes filled by scripts/props/import_camp.py)
	for (const FTransform& Spot : { FromMaquette(1.85, -0.05, 0.0, -31.5), FromMaquette(1.45, 0.95, 0.0, 51.6),
		FromMaquette(3.3, 1.05, 0.0, 17.2), FromMaquette(-1.25, -0.55, 0.0, 22.9) })
	{
		FCampItem& Item = Tools.AddDefaulted_GetRef();
		Item.Transform = Spot;
	}
	// one fish on the shelter table (0.595 m high), two by the fire
	FishSpots = { FromMaquette(2.761, 1.873, 0.6, 4.6), FromMaquette(0.75, -1.15, 0.0, 70.0), FromMaquette(-0.2, -1.4, 0.0, -40.0) };
}

UStaticMesh* UCampRules::GetPieceMesh(ECampPieceKind Kind) const
{
	switch (Kind)
	{
	case ECampPieceKind::Hut:			return HutMesh;
	case ECampPieceKind::Shelter:		return ShelterMesh;
	case ECampPieceKind::Campfire:		return CampfireMesh;
	case ECampPieceKind::LogSeat:		return LogSeatMesh;
	case ECampPieceKind::DryingRack:	return DryingRackMesh;
	}
	return nullptr;
}
