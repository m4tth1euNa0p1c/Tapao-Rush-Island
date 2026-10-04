// monke_island

#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Expedition/Islands/ExpeditionIslandDefinition.h"
#include "Expedition/Islands/ExpeditionIslandValidation.h"
#include "Expedition/Islands/FragmentRules.h"
#include "Expedition/Islands/IslandCatalog.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Boats/BoatStartSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/UObjectGlobals.h"

UExpeditionIslandSubsystem* UExpeditionIslandSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UExpeditionIslandSubsystem>() : nullptr;
}

void UExpeditionIslandSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// T-141: the boot map waits for the players of the world left by a travel (a slow client is not left behind).
	// Non seamless: PreLoadMap fires while the old world is still the world of its context; seamless: its own event.
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddWeakLambda(this, [this](const FWorldContext& Context, const FString& /*MapName*/)
	{
		if (Context.OwningGameInstance == GetGameInstance())
		{
			RecordTravelPlayers(Context.World());
		}
	});
	SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddWeakLambda(this, [this](UWorld* World, const FString& /*LevelName*/)
	{
		if (World && World->GetGameInstance() == GetGameInstance())
		{
			RecordTravelPlayers(World);
		}
	});
}

void UExpeditionIslandSubsystem::RecordTravelPlayers(UWorld* World)
{
	AGameModeBase* GameMode = World && World->GetNetMode() != NM_Client ? World->GetAuthGameMode() : nullptr;
	if (!GameMode)
	{
		return;
	}
	TravelPlayerCount = GameMode->GetNumPlayers();
	UE_LOG(LogMonkeIslands, Log, TEXT("Islands: %d player(s) in %s before the travel"), TravelPlayerCount, *World->GetMapName());
}

int32 UExpeditionIslandSubsystem::TakeTravelPlayerCount()
{
	const int32 Count = TravelPlayerCount;
	TravelPlayerCount = 0;
	return Count;
}

bool UExpeditionIslandSubsystem::InjectStartAnchorage(UWorld* World) const
{
	if (!World || World->GetNetMode() == NM_Client)
	{
		return false;
	}
	const FString OptionMatch = FString::Printf(TEXT("%s="), ExpeditionIslands::FragmentOptionKey);
	const FName FromOption = ExpeditionIslands::ParseFragmentOption(World->URL.GetOption(*OptionMatch, nullptr));
	const UExpeditionIslandDefinition* Definition = FromOption.IsNone() ? nullptr : FindLoadedDefinition(FromOption);
	if (!Definition)
	{
		// no ?Island= or its definition is not loaded on this machine: the arrival points of the map are used
		return false;
	}
	const FString WorldPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (Definition->Map.ToSoftObjectPath().GetLongPackageName() != WorldPackage)
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: %s plays %s, not %s: its Start anchorage is not injected"),
			*FromOption.ToString(), *Definition->Map.ToString(), *WorldPackage);
		return false;
	}
	FExpeditionAnchorage Start;
	if (!Definition->FindAnchorageByRole(EAnchorageRole::Start, Start))
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: %s has no Start anchorage, the arrival points of the map are used"), *FromOption.ToString());
		return false;
	}
	UBoatStartSubsystem* BoatStart = UBoatStartSubsystem::Get(World);
	if (!BoatStart || !BoatStart->SetArrivalOverride(Start.Transform, FromOption))
	{
		return false;
	}
	UE_LOG(LogMonkeIslands, Log, TEXT("Islands: boat start of %s on its anchorage %s"), *FromOption.ToString(), *Start.AnchorageId.ToString());
	return true;
}

void UExpeditionIslandSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadMapHandle);
	FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
	PreLoadMapHandle.Reset();
	SeamlessTravelHandle.Reset();
	// Every pending load ends with a cancellation, never in silence (ERR-04)
	if (CatalogState == ECatalogState::Loading)
	{
		FinishCatalog(EFragmentError::LoadCancelled);
	}
	TArray<uint32> PendingIds;
	DefinitionRequests.GetKeys(PendingIds);
	for (const uint32 RequestId : PendingIds)
	{
		FinishDefinition(RequestId, EFragmentError::LoadCancelled, nullptr);
	}

	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearAllTimersForObject(this);
	}
	OnCatalogLoaded.Clear();
	OnSelectionChanged.Clear();

	Super::Deinitialize();
}

const UFragmentRules& UExpeditionIslandSubsystem::GetRules() const
{
	return (Catalog && Catalog->Rules) ? *Catalog->Rules : *GetDefault<UFragmentRules>();
}

FTimerHandle UExpeditionIslandSubsystem::StartTimeout(FTimerDelegate Delegate) const
{
	FTimerHandle Handle;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().SetTimer(Handle, MoveTemp(Delegate), FMath::Max(1.0f, GetRules().LoadTimeoutSeconds), false);
	}
	return Handle;
}

void UExpeditionIslandSubsystem::ClearTimeout(FTimerHandle& Handle) const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		GameInstance->GetTimerManager().ClearTimer(Handle);
	}
	Handle.Invalidate();
}

// ---------------------------------------------------------------------------------------------------------------
// Catalog

void UExpeditionIslandSubsystem::LoadCatalog(FOnIslandCatalogLoaded Callback)
{
	if (CatalogState == ECatalogState::Loaded)
	{
		Callback.ExecuteIfBound(EFragmentError::None, GetOfferedDefinitions());
		return;
	}

	CatalogCallbacks.Add(MoveTemp(Callback));
	if (CatalogState == ECatalogState::Loading)
	{
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		FinishCatalog(EFragmentError::AssetManagerUnavailable);
		return;
	}

	const FPrimaryAssetId CatalogId = UIslandCatalog::GetDefaultCatalogId();
	if (!AssetManager->GetPrimaryAssetPath(CatalogId).IsValid())
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: %s is not registered (Asset Manager, /Game/MonkeIsland/Data/Islands)"), *CatalogId.ToString());
		FinishCatalog(EFragmentError::CatalogNotFound);
		return;
	}

	CatalogState = ECatalogState::Loading;
	CatalogError = EFragmentError::None;
	const uint32 Generation = ++CatalogGeneration;
	CatalogTimeout = StartTimeout(FTimerDelegate::CreateUObject(this, &UExpeditionIslandSubsystem::HandleCatalogTimeout, Generation));

	FAssetManagerLoadParams Params;
	Params.OnComplete = FStreamableDelegateWithHandle::CreateWeakLambda(this, [this, Generation](TSharedPtr<FStreamableHandle>)
	{
		HandleCatalogAssetLoaded(Generation);
	});
	Params.OnCancel = FStreamableDelegateWithHandle::CreateWeakLambda(this, [this, Generation](TSharedPtr<FStreamableHandle>)
	{
		HandleCatalogCancelled(Generation);
	});
	CatalogHandle = AssetManager->LoadPrimaryAsset(CatalogId, TArray<FName>(), MoveTemp(Params));
}

void UExpeditionIslandSubsystem::HandleCatalogAssetLoaded(uint32 Generation)
{
	if (Generation != CatalogGeneration || CatalogState != ECatalogState::Loading)
	{
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	Catalog = AssetManager ? AssetManager->GetPrimaryAssetObject<UIslandCatalog>(UIslandCatalog::GetDefaultCatalogId()) : nullptr;

	TArray<FText> Issues;
	const EFragmentError Error = ExpeditionIslands::ValidateCatalog(Catalog, &Issues);
	for (const FText& Issue : Issues)
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: catalog %s: %s"), *GetNameSafe(Catalog), *Issue.ToString());
	}
	if (Error != EFragmentError::None)
	{
		FinishCatalog(Error);
		return;
	}

	// Well formed, registered and unique entries only; the others were reported above or are reported here
	TArray<FPrimaryAssetId> Ids;
	Ids.Reserve(Catalog->Fragments.Num());
	for (const FPrimaryAssetId& Id : Catalog->Fragments)
	{
		if (Id.PrimaryAssetType != UExpeditionIslandDefinition::PrimaryAssetType || !ExpeditionIslands::IsValidFragmentId(Id.PrimaryAssetName) || Ids.Contains(Id))
		{
			continue;
		}
		if (!AssetManager->GetPrimaryAssetPath(Id).IsValid())
		{
			UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: catalog entry %s is not a registered island definition, skipped"), *Id.ToString());
			continue;
		}
		Ids.Add(Id);
	}
	if (Ids.IsEmpty())
	{
		FinishCatalog(EFragmentError::NoValidFragment);
		return;
	}

	FAssetManagerLoadParams Params;
	Params.OnComplete = FStreamableDelegateWithHandle::CreateWeakLambda(this, [this, Generation, Ids](TSharedPtr<FStreamableHandle>)
	{
		HandleCatalogDefinitionsLoaded(Generation, Ids);
	});
	Params.OnCancel = FStreamableDelegateWithHandle::CreateWeakLambda(this, [this, Generation](TSharedPtr<FStreamableHandle>)
	{
		HandleCatalogCancelled(Generation);
	});
	CatalogHandle = AssetManager->LoadPrimaryAssets(Ids, TArray<FName>(), MoveTemp(Params));
}

void UExpeditionIslandSubsystem::HandleCatalogDefinitionsLoaded(uint32 Generation, TArray<FPrimaryAssetId> Ids)
{
	if (Generation != CatalogGeneration || CatalogState != ECatalogState::Loading)
	{
		return;
	}

	const UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	const UFragmentRules* Rules = Catalog ? Catalog->Rules.Get() : nullptr;
	OfferedDefinitions.Reset();
	for (const FPrimaryAssetId& Id : Ids)
	{
		UExpeditionIslandDefinition* Definition = AssetManager ? AssetManager->GetPrimaryAssetObject<UExpeditionIslandDefinition>(Id) : nullptr;
		TArray<FText> Issues;
		const EFragmentError Error = ExpeditionIslands::ValidateDefinition(Definition, Rules, &Issues);
		if (Error != EFragmentError::None)
		{
			UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: %s (%s) is not offered: %s"), *Id.ToString(), *GetNameSafe(Definition),
				*FString::JoinBy(Issues, TEXT(" "), [](const FText& Issue) { return Issue.ToString(); }));
			continue;
		}
		OfferedDefinitions.Add(Definition);
		LoadedDefinitions.Add(Definition->FragmentId, Definition);
	}

	FinishCatalog(OfferedDefinitions.IsEmpty() ? EFragmentError::NoValidFragment : EFragmentError::None);
}

void UExpeditionIslandSubsystem::HandleCatalogCancelled(uint32 Generation)
{
	if (Generation == CatalogGeneration && CatalogState == ECatalogState::Loading)
	{
		FinishCatalog(EFragmentError::LoadCancelled);
	}
}

void UExpeditionIslandSubsystem::HandleCatalogTimeout(uint32 Generation)
{
	if (Generation == CatalogGeneration && CatalogState == ECatalogState::Loading)
	{
		FinishCatalog(EFragmentError::LoadTimedOut);
	}
}

void UExpeditionIslandSubsystem::FinishCatalog(EFragmentError Error)
{
	ClearTimeout(CatalogTimeout);

	// The state moves first: a cancel answer of the handle below is then ignored
	CatalogState = Error == EFragmentError::None ? ECatalogState::Loaded : ECatalogState::Failed;
	CatalogError = Error;
	if (Error != EFragmentError::None)
	{
		OfferedDefinitions.Reset();
	}

	if (TSharedPtr<FStreamableHandle> Handle = MoveTemp(CatalogHandle))
	{
		if (Error != EFragmentError::None && Handle->IsLoadingInProgress())
		{
			Handle->CancelHandle();
		}
	}

	if (Error == EFragmentError::None)
	{
		UE_LOG(LogMonkeIslands, Log, TEXT("Islands: catalog %s loaded, %d fragment(s) offered"), *GetNameSafe(Catalog), OfferedDefinitions.Num());
	}
	else
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: catalog load failed (%s)"), ExpeditionIslands::LexErrorName(Error));
	}

	const TArray<UExpeditionIslandDefinition*> Offered = GetOfferedDefinitions();
	TArray<FOnIslandCatalogLoaded> Callbacks = MoveTemp(CatalogCallbacks);
	CatalogCallbacks.Reset();
	for (FOnIslandCatalogLoaded& Callback : Callbacks)
	{
		Callback.ExecuteIfBound(Error, Offered);
	}
	OnCatalogLoaded.Broadcast(Error);
}

TArray<UExpeditionIslandDefinition*> UExpeditionIslandSubsystem::GetOfferedDefinitions() const
{
	TArray<UExpeditionIslandDefinition*> Result;
	Result.Reserve(OfferedDefinitions.Num());
	for (const TObjectPtr<UExpeditionIslandDefinition>& Definition : OfferedDefinitions)
	{
		Result.Add(Definition);
	}
	return Result;
}

// ---------------------------------------------------------------------------------------------------------------
// Single definition

void UExpeditionIslandSubsystem::LoadDefinition(FName FragmentId, FOnIslandDefinitionLoaded Callback)
{
	if (!ExpeditionIslands::IsValidFragmentId(FragmentId))
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: definition requested with a malformed fragment id '%s'"), *FragmentId.ToString());
		Callback.ExecuteIfBound(EFragmentError::InvalidFragmentId, nullptr);
		return;
	}
	if (UExpeditionIslandDefinition* Cached = FindLoadedDefinition(FragmentId))
	{
		Callback.ExecuteIfBound(EFragmentError::None, Cached);
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		Callback.ExecuteIfBound(EFragmentError::AssetManagerUnavailable, nullptr);
		return;
	}
	const FPrimaryAssetId Id = UExpeditionIslandDefinition::MakePrimaryAssetId(FragmentId);
	if (!AssetManager->GetPrimaryAssetPath(Id).IsValid())
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: no island definition registered for %s"), *Id.ToString());
		Callback.ExecuteIfBound(EFragmentError::FragmentNotFound, nullptr);
		return;
	}

	const uint32 RequestId = NextRequestId++;
	FDefinitionRequest& Request = DefinitionRequests.Add(RequestId);
	Request.FragmentId = FragmentId;
	Request.Callback = MoveTemp(Callback);
	Request.Timeout = StartTimeout(FTimerDelegate::CreateUObject(this, &UExpeditionIslandSubsystem::HandleDefinitionTimeout, RequestId));

	FAssetManagerLoadParams Params;
	Params.OnComplete = FStreamableDelegateWithHandle::CreateWeakLambda(this, [this, RequestId](TSharedPtr<FStreamableHandle>)
	{
		HandleDefinitionLoaded(RequestId);
	});
	Params.OnCancel = FStreamableDelegateWithHandle::CreateWeakLambda(this, [this, RequestId](TSharedPtr<FStreamableHandle>)
	{
		HandleDefinitionCancelled(RequestId);
	});
	TSharedPtr<FStreamableHandle> Handle = AssetManager->LoadPrimaryAsset(Id, TArray<FName>(), MoveTemp(Params));
	if (FDefinitionRequest* Pending = DefinitionRequests.Find(RequestId))
	{
		Pending->Handle = MoveTemp(Handle);
	}
}

void UExpeditionIslandSubsystem::HandleDefinitionLoaded(uint32 RequestId)
{
	const FDefinitionRequest* Request = DefinitionRequests.Find(RequestId);
	if (!Request)
	{
		return;
	}

	const UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	const FPrimaryAssetId Id = UExpeditionIslandDefinition::MakePrimaryAssetId(Request->FragmentId);
	UExpeditionIslandDefinition* Definition = AssetManager ? AssetManager->GetPrimaryAssetObject<UExpeditionIslandDefinition>(Id) : nullptr;

	TArray<FText> Issues;
	const EFragmentError Error = ExpeditionIslands::ValidateDefinition(Definition, &GetRules(), &Issues);
	if (Error != EFragmentError::None)
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: %s (%s) is not usable: %s"), *Id.ToString(), *GetNameSafe(Definition),
			*FString::JoinBy(Issues, TEXT(" "), [](const FText& Issue) { return Issue.ToString(); }));
		FinishDefinition(RequestId, Error, nullptr);
		return;
	}

	LoadedDefinitions.Add(Definition->FragmentId, Definition);
	FinishDefinition(RequestId, EFragmentError::None, Definition);
}

void UExpeditionIslandSubsystem::HandleDefinitionCancelled(uint32 RequestId)
{
	FinishDefinition(RequestId, EFragmentError::LoadCancelled, nullptr);
}

void UExpeditionIslandSubsystem::HandleDefinitionTimeout(uint32 RequestId)
{
	FinishDefinition(RequestId, EFragmentError::LoadTimedOut, nullptr);
}

void UExpeditionIslandSubsystem::FinishDefinition(uint32 RequestId, EFragmentError Error, UExpeditionIslandDefinition* Definition)
{
	FDefinitionRequest Request;
	if (!DefinitionRequests.RemoveAndCopyValue(RequestId, Request))
	{
		// Already ended (timed out, cancelled or loaded): each load answers once
		return;
	}

	ClearTimeout(Request.Timeout);
	if (Error != EFragmentError::None && Request.Handle.IsValid() && Request.Handle->IsLoadingInProgress())
	{
		Request.Handle->CancelHandle();
	}
	if (Error != EFragmentError::None)
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: load of %s failed (%s)"), *Request.FragmentId.ToString(), ExpeditionIslands::LexErrorName(Error));
	}
	Request.Callback.ExecuteIfBound(Error, Definition);
}

UExpeditionIslandDefinition* UExpeditionIslandSubsystem::FindLoadedDefinition(FName FragmentId) const
{
	const TObjectPtr<UExpeditionIslandDefinition>* Found = LoadedDefinitions.Find(FragmentId);
	return Found ? Found->Get() : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------
// Selection and travel

bool UExpeditionIslandSubsystem::HasSelectionAuthority() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	return !World || World->GetNetMode() != NM_Client;
}

EFragmentError UExpeditionIslandSubsystem::SelectFragment(FName FragmentId)
{
	EFragmentError Error = EFragmentError::None;
	if (!HasSelectionAuthority())
	{
		Error = EFragmentError::NoAuthority;
	}
	else if (!ExpeditionIslands::IsValidFragmentId(FragmentId))
	{
		Error = EFragmentError::InvalidFragmentId;
	}
	else if (CatalogState != ECatalogState::Loaded)
	{
		Error = EFragmentError::CatalogNotLoaded;
	}

	UExpeditionIslandDefinition* Chosen = nullptr;
	if (Error == EFragmentError::None)
	{
		const TObjectPtr<UExpeditionIslandDefinition>* Found = OfferedDefinitions.FindByPredicate([FragmentId](const TObjectPtr<UExpeditionIslandDefinition>& Definition)
		{
			return Definition && Definition->FragmentId == FragmentId;
		});
		Chosen = Found ? Found->Get() : nullptr;
		Error = Chosen ? EFragmentError::None : EFragmentError::NotInCatalog;
	}

	if (Error != EFragmentError::None)
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: selection of '%s' refused (%s)"), *FragmentId.ToString(), ExpeditionIslands::LexErrorName(Error));
		return Error;
	}

	const FName PreviousFragmentId = GetSelectedFragmentId();
	SelectedDefinition = Chosen;
	if (PreviousFragmentId != FragmentId)
	{
		UE_LOG(LogMonkeIslands, Log, TEXT("Islands: fragment %s chosen (%s)"), *FragmentId.ToString(), *GetNameSafe(Chosen));
		OnSelectionChanged.Broadcast(FragmentId, PreviousFragmentId);
	}
	return EFragmentError::None;
}

void UExpeditionIslandSubsystem::ClearSelection()
{
	if (!HasSelectionAuthority() || !SelectedDefinition)
	{
		return;
	}
	const FName PreviousFragmentId = GetSelectedFragmentId();
	SelectedDefinition = nullptr;
	OnSelectionChanged.Broadcast(NAME_None, PreviousFragmentId);
}

FName UExpeditionIslandSubsystem::GetSelectedFragmentId() const
{
	return SelectedDefinition ? SelectedDefinition->FragmentId : NAME_None;
}

EFragmentError UExpeditionIslandSubsystem::GetTravelURL(FString& OutURL) const
{
	OutURL.Reset();
	if (!SelectedDefinition)
	{
		return EFragmentError::NoSelection;
	}
	TValueOrError<FString, EFragmentError> URL = ExpeditionIslands::BuildTravelURL(SelectedDefinition);
	if (URL.HasError())
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: no travel URL for %s (%s)"), *GetSelectedFragmentId().ToString(), ExpeditionIslands::LexErrorName(URL.GetError()));
		return URL.GetError();
	}
	OutURL = URL.StealValue();
	return EFragmentError::None;
}

EFragmentError UExpeditionIslandSubsystem::PickRandomIsland(int32 Seed)
{
	if (!HasSelectionAuthority())
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: random pick refused on a client"));
		return EFragmentError::NoAuthority;
	}
	if (CatalogState != ECatalogState::Loaded)
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: random pick before the catalog is loaded"));
		return EFragmentError::CatalogNotLoaded;
	}

	TArray<FName, TInlineAllocator<16>> Ids;
	for (const TObjectPtr<UExpeditionIslandDefinition>& Definition : OfferedDefinitions)
	{
		if (Definition)
		{
			Ids.Add(Definition->FragmentId);
		}
	}
	const TValueOrError<FName, EFragmentError> Picked = ExpeditionIslands::PickFragmentId(Ids, Seed, LastPickedFragmentId);
	if (Picked.HasError())
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: random pick failed (%s)"), ExpeditionIslands::LexErrorName(Picked.GetError()));
		return Picked.GetError();
	}

	const EFragmentError Error = SelectFragment(Picked.GetValue());
	if (Error == EFragmentError::None)
	{
		UE_LOG(LogMonkeIslands, Log, TEXT("Islands: drew %s with seed %d among %d fragment(s), previous %s"),
			*Picked.GetValue().ToString(), Seed, Ids.Num(), *LastPickedFragmentId.ToString());
		LastPickedFragmentId = Picked.GetValue();
	}
	return Error;
}

EFragmentError UExpeditionIslandSubsystem::TravelToSelectedIsland()
{
	FString URL;
	const EFragmentError Error = GetTravelURL(URL);
	return Error == EFragmentError::None ? TravelToMapURL(URL) : Error;
}

EFragmentError UExpeditionIslandSubsystem::TravelToMapURL(const FString& URL)
{
	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || !GEngine || URL.IsEmpty())
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: travel to '%s' impossible (no world)"), *URL);
		return EFragmentError::TravelFailed;
	}

	switch (World->GetNetMode())
	{
	case NM_Client:
		UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: travel to '%s' refused on a client"), *URL);
		return EFragmentError::NoAuthority;

	case NM_Standalone:
		// Same path as UGameplayStatics::OpenLevel
		UE_LOG(LogMonkeIslands, Log, TEXT("Islands: opening %s"), *URL);
		GEngine->SetClientTravel(World, *URL, TRAVEL_Absolute);
		return EFragmentError::None;

	default:
	{
		// Absolute travel drops the previous options: keep the host listening, and never carry an older Island option
		const FString HostURL = World->GetNetMode() == NM_ListenServer ? URL + TEXT("?listen") : URL;
		UE_LOG(LogMonkeIslands, Log, TEXT("Islands: server travel to %s"), *HostURL);
		return World->ServerTravel(HostURL, true) ? EFragmentError::None : EFragmentError::TravelFailed;
	}
	}
}

FName UExpeditionIslandSubsystem::ResolveFragmentIdForWorld(const UWorld* World) const
{
	if (!World)
	{
		return NAME_None;
	}

	const FString WorldPackage = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	auto PlaysWorld = [&WorldPackage](const UExpeditionIslandDefinition* Definition)
	{
		return Definition && Definition->Map.ToSoftObjectPath().GetLongPackageName() == WorldPackage;
	};

	// 1. The travel URL names the fragment (several definitions may share a map while the fragments are interim)
	const FString OptionMatch = FString::Printf(TEXT("%s="), ExpeditionIslands::FragmentOptionKey);
	const FName FromOption = ExpeditionIslands::ParseFragmentOption(World->URL.GetOption(*OptionMatch, nullptr));
	if (!FromOption.IsNone())
	{
		const UExpeditionIslandDefinition* Known = FindLoadedDefinition(FromOption);
		if (Known && !PlaysWorld(Known))
		{
			UE_LOG(LogMonkeIslands, Warning, TEXT("Islands: URL names %s but its map is %s, not %s"),
				*FromOption.ToString(), *Known->Map.ToString(), *WorldPackage);
		}
		return FromOption;
	}

	// 2. The chosen, then any loaded, definition of this map
	if (PlaysWorld(SelectedDefinition))
	{
		return SelectedDefinition->FragmentId;
	}
	for (const TPair<FName, TObjectPtr<UExpeditionIslandDefinition>>& Pair : LoadedDefinitions)
	{
		if (PlaysWorld(Pair.Value))
		{
			return Pair.Key;
		}
	}

	// 3. The registry tags of the definitions (map opened directly in PIE): nothing is loaded
	if (const UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		TArray<FAssetData> Assets;
		AssetManager->GetPrimaryAssetDataList(UExpeditionIslandDefinition::PrimaryAssetType, Assets);
		for (const FAssetData& Asset : Assets)
		{
			FString MapPath;
			if (!Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(UExpeditionIslandDefinition, Map), MapPath))
			{
				continue;
			}
			MapPath.TrimQuotesInline();
			if (FSoftObjectPath(MapPath).GetLongPackageName() != WorldPackage)
			{
				continue;
			}
			FString FragmentTag;
			Asset.GetTagValue(GET_MEMBER_NAME_CHECKED(UExpeditionIslandDefinition, FragmentId), FragmentTag);
			const FName FragmentId = ExpeditionIslands::ParseFragmentOption(*FragmentTag);
			if (!FragmentId.IsNone())
			{
				return FragmentId;
			}
		}
	}

	UE_LOG(LogMonkeIslands, Verbose, TEXT("Islands: %s is not an expedition fragment"), *WorldPackage);
	return NAME_None;
}
