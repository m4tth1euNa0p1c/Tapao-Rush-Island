// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

/** Log category of the Tidemark editor tools (import, bake, lint, commandlet). */
DECLARE_LOG_CATEGORY_EXTERN(LogTidemarkEditor, Log, All);

/** Editor module of the Tidemark plugin: import, first-wet bake, lint panel and commandlet. Reads the runtime, never the game. */
class TIDEMARKEDITOR_API FTidemarkEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface
};
