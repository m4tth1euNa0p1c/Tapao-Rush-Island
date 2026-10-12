// monke_island

#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "EditorReimportHandler.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "monke_island.h"

/**
 *  Editor tool run through the automation framework (the MCP can run tests, not console commands):
 *  reimports static meshes in place from new FBX files, so the PCG instances keep their meshes.
 *  Does nothing unless Saved/MonkeTools/reimport_request.json exists:
 *    { "fbx_vertex_colors": "Replace", "assets": [ { "asset": "/Game/...SM_X.SM_X", "fbx": "C:/.../SM_X.fbx" } ] }
 *  The request file is deleted once read. Out of the monke_island.* test tree on purpose.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMonkeReimportVegetationTool, "MonkeTools.ReimportStaticMeshes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMonkeReimportVegetationTool::RunTest(const FString& Parameters)
{
	const FString RequestPath = FPaths::ProjectSavedDir() / TEXT("MonkeTools/reimport_request.json");
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *RequestPath))
	{
		AddInfo(TEXT("No reimport request: nothing to do"));
		return true;
	}
	IFileManager::Get().Delete(*RequestPath);

	TSharedPtr<FJsonObject> Request;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Request) || !Request.IsValid())
	{
		AddError(TEXT("reimport_request.json is not valid JSON"));
		return false;
	}
	const bool bReplaceColors = Request->GetStringField(TEXT("fbx_vertex_colors")) == TEXT("Replace");

	int32 Done = 0;
	const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
	if (!Request->TryGetArrayField(TEXT("assets"), Assets))
	{
		AddError(TEXT("reimport_request.json has no assets"));
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Assets)
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		const FString AssetPath = Entry ? Entry->GetStringField(TEXT("asset")) : FString();
		const FString Fbx = Entry ? Entry->GetStringField(TEXT("fbx")) : FString();
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *AssetPath);
		if (!Mesh || !FPaths::FileExists(Fbx))
		{
			AddError(FString::Printf(TEXT("%s: mesh or FBX missing (%s)"), *AssetPath, *Fbx));
			continue;
		}
		if (UFbxStaticMeshImportData* ImportData = Cast<UFbxStaticMeshImportData>(Mesh->GetAssetImportData()))
		{
			if (bReplaceColors)
			{
				ImportData->VertexColorImportOption = EVertexColorImportOption::Replace;
			}
		}
		FReimportManager::Instance()->UpdateReimportPaths(Mesh, {Fbx});
		const bool bOk = FReimportManager::Instance()->Reimport(Mesh, false, false, Fbx, nullptr, INDEX_NONE, false, true);
		if (bOk)
		{
			Mesh->MarkPackageDirty();
			++Done;
			UE_LOG(Logmonke_island, Display, TEXT("ReimportStaticMeshes: %s <- %s (UV channels %d)"), *AssetPath, *Fbx,
				Mesh->GetNumUVChannels(0));
		}
		else
		{
			AddError(FString::Printf(TEXT("%s: reimport failed"), *AssetPath));
		}
	}
	AddInfo(FString::Printf(TEXT("Reimported %d / %d meshes (save them with AssetTools.save_assets)"), Done, Assets->Num()));
	return Done == Assets->Num();
}

#endif
