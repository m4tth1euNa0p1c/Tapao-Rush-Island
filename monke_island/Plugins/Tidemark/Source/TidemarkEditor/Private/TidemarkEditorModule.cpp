// Tidemark plugin

#include "TidemarkEditor/TidemarkEditorModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogTidemarkEditor);

#define LOCTEXT_NAMESPACE "FTidemarkEditorModule"

void FTidemarkEditorModule::StartupModule()
{
	UE_LOG(LogTidemarkEditor, Log, TEXT("Tidemark editor module started"));
}

void FTidemarkEditorModule::ShutdownModule()
{
	UE_LOG(LogTidemarkEditor, Log, TEXT("Tidemark editor module shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTidemarkEditorModule, TidemarkEditor)
