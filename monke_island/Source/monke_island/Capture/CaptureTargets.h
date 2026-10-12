// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Capture/CaptureTypes.h"

class AActor;
class ACharacter;
class APawn;
class UCaptureRules;

/**
 *  What the capture knows of the mobs it takes (T-336): the only place of the Capture domain that reads the French
 *  soldiers (Fort/) and the Veilleurs (Creatures/). The dependency goes one way, Capture -> Fort / Creatures: the mobs
 *  know nothing of the capture (no line added to FortSoldier or TikiCreature).
 */
namespace CaptureTargets
{
	/** Kind of a pawn: Soldier (AFortSoldier), Watcher (ATikiCreature), Other */
	MONKE_ISLAND_API ECaptiveKind KindOf(const AActor* Actor);

	/** Down: on the ground without a leg, dragging itself (a soldier or a Veilleur that lost a leg) */
	MONKE_ISLAND_API bool IsDown(const AActor* Actor);

	/** Share of health left (1 without health), and whether it lives */
	MONKE_ISLAND_API float HealthFractionOf(const AActor* Actor, bool& bOutAlive);

	/** True for a pawn the rules let cuff: of a capturable kind, alive, never held by a player */
	MONKE_ISLAND_API bool IsCapturable(const APawn* Pawn, const UCaptureRules& Rules);

	/** Server: the look of the prisoner that replaces Original (mesh, materials, size, lost bones, clips) */
	MONKE_ISLAND_API void CopyLook(ACharacter* Original, const UCaptureRules& Rules, FCapturePrisonerLook& OutLook);
}
