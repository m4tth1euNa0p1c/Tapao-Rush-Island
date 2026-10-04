// Copyright Epic Games, Inc. All Rights Reserved.

#include "monke_island.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"

class FMonkeIslandModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FMonkeIslandModule::LogBuildVersion);
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);
		FDefaultGameModuleImpl::ShutdownModule();
	}

private:
	void LogBuildVersion()
	{
		FString Version;
		if (GConfig)
		{
			GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
		}
		if (Version.IsEmpty() || Version.Len() > 96 || Version.Contains(TEXT("\n")) || Version.Contains(TEXT("\r"))
			|| Version.Contains(TEXT("/")) || Version.Contains(TEXT("\\")))
		{
			Version = TEXT("unversioned");
		}
		UE_LOG(Logmonke_island, Display, TEXT("Crownfall build=%s"), *Version);
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMonkeIslandModule, monke_island, "monke_island");

DEFINE_LOG_CATEGORY(Logmonke_island)
