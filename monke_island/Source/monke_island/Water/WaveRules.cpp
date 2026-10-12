// monke_island

#include "Water/WaveRules.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"

UWaveRules::UWaveRules()
{
	// a lagoon chop on a long swell from the trade winds (east-south-east), about 40 cm from trough to crest at most:
	// long trains carry the height, short ones the sparkle; directions spread so no pattern shows
	Waves = {
		FIslandGerstnerWave(200.0f, 2400.0f, 14.0f, 0.35f, 0.0f),
		FIslandGerstnerWave(175.0f, 1300.0f, 9.0f, 0.40f, 1.3f),
		FIslandGerstnerWave(225.0f, 800.0f, 6.0f, 0.45f, 2.1f),
		FIslandGerstnerWave(190.0f, 520.0f, 4.0f, 0.50f, 4.0f),
		FIslandGerstnerWave(250.0f, 330.0f, 2.5f, 0.50f, 0.7f),
		FIslandGerstnerWave(150.0f, 210.0f, 1.6f, 0.50f, 5.2f),
	};
	// ripples shorter than 2 m are left to the detail normal maps of the material
	WaveParameters = TSoftObjectPtr<UMaterialParameterCollection>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Environment/Water/MPC_Waves.MPC_Waves")));
	SurfaceMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Environment/Water/M_Water_Ocean.M_Water_Ocean")));
}
