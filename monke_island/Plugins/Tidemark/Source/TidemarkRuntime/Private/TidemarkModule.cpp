// Tidemark plugin

#include "Tidemark/TidemarkModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogTidemark);
DEFINE_LOG_CATEGORY(LogTidemarkNet);

#define LOCTEXT_NAMESPACE "FTidemarkRuntimeModule"

void FTidemarkRuntimeModule::StartupModule()
{
	UE_LOG(LogTidemark, Log, TEXT("Tidemark runtime module started"));
}

void FTidemarkRuntimeModule::ShutdownModule()
{
	UE_LOG(LogTidemark, Log, TEXT("Tidemark runtime module shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTidemarkRuntimeModule, TidemarkRuntime)
