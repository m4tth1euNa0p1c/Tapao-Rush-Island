// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** Main log category of the Tidemark runtime (grid, solver, rules, query, render bridge). */
DECLARE_LOG_CATEGORY_EXTERN(LogTidemark, Log, All);

/** Log category of the synchronisation layer (clock, edit journal, hash checks, snapshots). */
DECLARE_LOG_CATEGORY_EXTERN(LogTidemarkNet, Log, All);

/** Runtime module of the Tidemark plugin. Holds no state: the authoritative simulation lives in the world subsystem. */
class TIDEMARKRUNTIME_API FTidemarkRuntimeModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
