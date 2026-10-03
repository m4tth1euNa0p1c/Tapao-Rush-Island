// monke_island

#include "Footsteps/FootstepRules.h"

UFootstepRules::UFootstepRules()
{
	// the boat and the village buildings are planks, the shrine and lanterns stone, the reef coral
	auto Add = [this](EFootstepSurface Surface, std::initializer_list<const TCHAR*> Words)
	{
		FFootstepKeywords& Entry = Keywords.AddDefaulted_GetRef();
		Entry.Surface = Surface;
		for (const TCHAR* Word : Words)
		{
			Entry.Words.Add(Word);
		}
	};
	Add(EFootstepSurface::Wood, { TEXT("Barque"), TEXT("Boat"), TEXT("Oar"), TEXT("Minka"), TEXT("Kura"), TEXT("Engawa"), TEXT("Torii"), TEXT("Wood"), TEXT("Plank"), TEXT("Deck"), TEXT("Pier"), TEXT("Fence"), TEXT("NoticeBoard"), TEXT("FireTower"), TEXT("Crate") });
	Add(EFootstepSurface::Coral, { TEXT("Coral"), TEXT("Rubble"), TEXT("Feo"), TEXT("Reef") });
	Add(EFootstepSurface::Litter, { TEXT("Litter"), TEXT("Frond"), TEXT("Leaf"), TEXT("Grass"), TEXT("Bush") });
	Add(EFootstepSurface::Stone, { TEXT("Stone"), TEXT("Rock"), TEXT("Shrine"), TEXT("Lantern"), TEXT("Well"), TEXT("Concrete"), TEXT("Cistern") });
	Add(EFootstepSurface::SandDry, { TEXT("Sand"), TEXT("Beach") });
}
