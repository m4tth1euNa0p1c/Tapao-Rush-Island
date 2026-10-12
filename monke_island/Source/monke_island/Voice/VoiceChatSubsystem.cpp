// monke_island

#include "Voice/VoiceChatSubsystem.h"
#include "Voice/VoiceChatComponent.h"
#include "Voice/VoiceMath.h"
#include "Voice/VoiceRules.h"
#include "Settings/SettingsSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/VoiceInterface.h"
#include "Net/VoiceConfig.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogMonkeVoice);

#define LOCTEXT_NAMESPACE "VoiceChat"

const TCHAR* UVoiceChatSubsystem::DefaultRulesPath = TEXT("/Game/MonkeIsland/Voice/Data/DA_VoiceRules.DA_VoiceRules");
const FName UVoiceChatSubsystem::VoiceSessionName(TEXT("MonkeVoice"));

namespace
{
	/** Where the voice of a player without a pawn is parked: far beyond any attenuation, so nobody hears it */
	const FVector ParkedVoiceLocation(0.0, 0.0, -1.0e6);

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarVoiceFakeTalk(
		TEXT("monke.Voice.FakeTalk"),
		0,
		TEXT("1: holding the push-to-talk key counts as talking, even without a microphone (the mobs hear it: a noise test ")
		TEXT("alone); 0: off"),
		ECVF_Cheat);

	FAutoConsoleCommandWithWorldAndArgs CmdVoiceTest(
		TEXT("monke.Voice.Test"),
		TEXT("monke.Voice.Test [1|0]: microphone test of this window: your voice comes back to you, nobody else hears it, ")
		TEXT("the mobs neither (toggles without an argument)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			USettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<USettingsSubsystem>() : nullptr;
			if (!Settings)
			{
				UE_LOG(LogMonkeVoice, Warning, TEXT("monke.Voice.Test: no settings in this world (a dedicated server?)"));
				return;
			}
			Settings->SetMicTest(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Settings->IsMicTestOn());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdVoiceDump(
		TEXT("monke.Voice.Dump"),
		TEXT("monke.Voice.Dump: state of the voice chat of this window (session, microphone, talkers) in the log"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (const UVoiceChatSubsystem* Voice = UVoiceChatSubsystem::Get(World))
			{
				Voice->DumpState(*GLog);
			}
		}));
#endif
}

UVoiceChatSubsystem* UVoiceChatSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UVoiceChatSubsystem>() : nullptr;
}

bool UVoiceChatSubsystem::IsFakeTalkOn()
{
#if !UE_BUILD_SHIPPING
	return CVarVoiceFakeTalk.GetValueOnGameThread() != 0;
#else
	return false;
#endif
}

bool UVoiceChatSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UVoiceChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// read once while the level loads, as the other rules of the project
	Rules = LoadObject<UVoiceRules>(nullptr, DefaultRulesPath, nullptr, LOAD_NoWarn);
	if (!Rules)
	{
		UE_LOG(LogMonkeVoice, Warning, TEXT("Voice: %s not found, class defaults used (no push-to-talk key until scripts/editor/voice_setup_ue.py ran)"), DefaultRulesPath);
	}
}

void UVoiceChatSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InWorld.GetTimerManager().SetTimer(UpdateTimer, FTimerDelegate::CreateUObject(this, &UVoiceChatSubsystem::Update),
		FMath::Clamp(GetRules().UpdateIntervalSeconds, 0.02f, 1.0f), true);
	Update();
}

void UVoiceChatSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	if (const TSharedPtr<IOnlineVoice, ESPMode::ThreadSafe> Voice = VoiceInterface.Pin())
	{
		Voice->ClearOnPlayerTalkingStateChangedDelegate_Handle(TalkingHandle);
	}
	if (const TSharedPtr<IOnlineSession, ESPMode::ThreadSafe> Sessions = SessionInterface.Pin())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		// the NULL session ends at once: the engine unregisters the talkers and closes the microphone
		if (bOwnSession && Sessions->GetNamedSession(VoiceSessionName))
		{
			Sessions->DestroySession(VoiceSessionName);
			UE_LOG(LogMonkeVoice, Log, TEXT("Voice: voice session closed"));
		}
	}
	VoiceInterface.Reset();
	SessionInterface.Reset();
	RemoteTalkers.Reset();
	RegisteredIds.Reset();
	TalkingIds.Reset();
	bSessionReady = false;
	bOwnSession = false;
	bLocalTalking = false;
	bSuppressOutgoing = false;
	Super::Deinitialize();
}

const UVoiceRules& UVoiceChatSubsystem::GetRules() const
{
	return Rules ? *Rules : *GetDefault<UVoiceRules>();
}

// ---------------------------------------------------------------- update

void UVoiceChatSubsystem::Update()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	// a dedicated server hears nobody and speaks for nobody: only the server half of the components runs there
	const bool bLocalVoice = World->GetNetMode() != NM_DedicatedServer;
	if (bLocalVoice)
	{
		EnsureVoiceSession();
	}

	UVoiceChatComponent* Local = nullptr;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		UVoiceChatComponent* Voice = Controller ? Controller->FindComponentByClass<UVoiceChatComponent>() : nullptr;
		if (!Voice)
		{
			continue;
		}
		if (Controller->HasAuthority())
		{
			Voice->ServerUpdate(Now);
		}
		if (bLocalVoice && !Local && Controller->IsLocalController())
		{
			Local = Voice;
			Voice->LocalUpdate(Now);
		}
	}
	LocalComponent = Local;

	if (bLocalVoice)
	{
		SyncPlayers();
		UpdateNearbyText();
	}
}

void UVoiceChatSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// before the network flush of this frame: the voice read is dropped (the loopback of the test already took it)
	if (const TSharedPtr<IOnlineVoice, ESPMode::ThreadSafe> Voice = VoiceInterface.Pin())
	{
		Voice->ClearVoicePackets();
	}
}

bool UVoiceChatSubsystem::IsTickable() const
{
	return bSuppressOutgoing && bSessionReady;
}

TStatId UVoiceChatSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UVoiceChatSubsystem, STATGROUP_Tickables);
}

// ---------------------------------------------------------------- session

void UVoiceChatSubsystem::WarnOnce(bool& bWarned, const FString& Message) const
{
	if (!bWarned)
	{
		bWarned = true;
		UE_LOG(LogMonkeVoice, Warning, TEXT("%s"), *Message);
	}
}

void UVoiceChatSubsystem::EnsureVoiceSession()
{
	if (bSessionReady || bSessionPending || bSessionFailed || !GetRules().bVoiceChatEnabled)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || (World->GetNetMode() == NM_Standalone && !NeedsVoiceAlone()))
	{
		return;
	}
	IOnlineSubsystem* OnlineSubsystem = Online::GetSubsystem(World);
	if (!OnlineSubsystem)
	{
		WarnOnce(bWarnedNoSubsystem, TEXT("Voice: no online subsystem, no voice chat"));
		return;
	}
	const IOnlineSessionPtr Sessions = OnlineSubsystem->GetSessionInterface();
	const IOnlineVoicePtr Voice = OnlineSubsystem->GetVoiceInterface();
	if (!Sessions.IsValid() || !Voice.IsValid())
	{
		WarnOnce(bWarnedNoVoice, TEXT("Voice: the engine's voice is off (DefaultEngine.ini: [Voice] bEnabled=true and [OnlineSubsystem] bHasVoiceEnabled=true), no voice chat"));
		return;
	}
	SessionInterface = Sessions;
	if (!VoiceInterface.IsValid())
	{
		VoiceInterface = Voice;
		TalkingHandle = Voice->AddOnPlayerTalkingStateChangedDelegate_Handle(
			FOnPlayerTalkingStateChangedDelegate::CreateUObject(this, &UVoiceChatSubsystem::HandleTalkingStateChanged));
		if (const IOnlineIdentityPtr Identity = OnlineSubsystem->GetIdentityInterface())
		{
			LocalTalkerId = FUniqueNetIdWrapper(Identity->GetUniquePlayerId(0));
		}
	}

	if (Sessions->GetNamedSession(VoiceSessionName))
	{
		bSessionReady = true;
		bOwnSession = true;
		return;
	}
	if (OnlineSubsystem->GetSubsystemName() != NULL_SUBSYSTEM)
	{
		// another platform (L7): its own game session carries the voice
		if (Sessions->GetNumSessions() > 0)
		{
			bSessionReady = true;
			UE_LOG(LogMonkeVoice, Log, TEXT("Voice: voice through the game session of %s"), *OnlineSubsystem->GetSubsystemName().ToString());
		}
		else
		{
			WarnOnce(bWarnedOtherPlatform, FString::Printf(TEXT("Voice: no session on %s yet, no voice chat until one exists"), *OnlineSubsystem->GetSubsystemName().ToString()));
		}
		return;
	}

	// a session of this machine only: not advertised, nobody joins it; the engine's voice runs inside it
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch = true;
	Settings.bShouldAdvertise = false;
	Settings.bUsesPresence = false;
	Settings.bAllowInvites = false;
	Settings.bAllowJoinInProgress = true;
	Settings.NumPublicConnections = 16;
	Settings.NumPrivateConnections = 0;
	CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UVoiceChatSubsystem::HandleCreateSessionComplete));
	bSessionPending = true;
	if (!Sessions->CreateSession(0, VoiceSessionName, Settings) && bSessionPending)
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
		bSessionPending = false;
		bSessionFailed = true;
		UE_LOG(LogMonkeVoice, Warning, TEXT("Voice: the voice session could not be created, no voice chat in this world"));
	}
}

bool UVoiceChatSubsystem::NeedsVoiceAlone() const
{
	UWorld* World = GetWorld();
	const APlayerController* LocalController = GEngine && World ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	if (LocalController && LocalController->GetPawn())
	{
		return true;
	}
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const USettingsSubsystem* Settings = GameInstance ? GameInstance->GetSubsystem<USettingsSubsystem>() : nullptr;
	return Settings && Settings->IsMicTestOn();
}

void UVoiceChatSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionName != VoiceSessionName)
	{
		return;
	}
	if (const TSharedPtr<IOnlineSession, ESPMode::ThreadSafe> Sessions = SessionInterface.Pin())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
	}
	bSessionPending = false;
	bSessionReady = bWasSuccessful;
	bOwnSession = bWasSuccessful;
	bSessionFailed = !bWasSuccessful;
	const UWorld* World = GetWorld();
	if (bWasSuccessful)
	{
		UE_LOG(LogMonkeVoice, Log, TEXT("Voice: voice session ready (net mode %d)"), World ? static_cast<int32>(World->GetNetMode()) : -1);
	}
	else
	{
		UE_LOG(LogMonkeVoice, Warning, TEXT("Voice: the voice session failed, no voice chat in this world"));
	}
}

void UVoiceChatSubsystem::HandleTalkingStateChanged(FUniqueNetIdRef TalkerId, bool bIsTalking)
{
	const FUniqueNetIdWrapper Id(TalkerId);
	if (LocalTalkerId.IsValid() && Id == LocalTalkerId)
	{
		bLocalTalking = bIsTalking;
		return;
	}
	if (bIsTalking)
	{
		TalkingIds.Add(Id);
	}
	else
	{
		TalkingIds.Remove(Id);
	}
}

bool UVoiceChatSubsystem::IsHeardTalking(const APlayerState* PlayerState) const
{
	return PlayerState && PlayerState->GetUniqueId().IsValid() && TalkingIds.Contains(PlayerState->GetUniqueId());
}

EVoiceIndicator UVoiceChatSubsystem::GetLocalIndicator() const
{
	const UVoiceChatComponent* Local = LocalComponent.Get();
	return Local ? Local->GetIndicator() : EVoiceIndicator::None;
}

// ---------------------------------------------------------------- players and talkers

void UVoiceChatSubsystem::SyncPlayers()
{
	UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}
	const APlayerController* LocalController = GEngine ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	const APlayerState* LocalState = LocalController ? LocalController->PlayerState.Get() : nullptr;

	// every player in the voice session: the engine registers the remote talkers (talking state, mutes) from it
	if (const TSharedPtr<IOnlineSession, ESPMode::ThreadSafe> Sessions = bOwnSession ? SessionInterface.Pin() : TSharedPtr<IOnlineSession, ESPMode::ThreadSafe>())
	{
		for (const TObjectPtr<APlayerState>& PlayerState : GameState->PlayerArray)
		{
			if (PlayerState && PlayerState->GetUniqueId().IsValid() && !RegisteredIds.Contains(PlayerState->GetUniqueId()))
			{
				Sessions->RegisterPlayer(VoiceSessionName, *PlayerState->GetUniqueId(), false);
				RegisteredIds.Add(PlayerState->GetUniqueId());
			}
		}
		for (int32 Index = RegisteredIds.Num() - 1; Index >= 0; --Index)
		{
			const bool bStillHere = GameState->PlayerArray.ContainsByPredicate([&Id = RegisteredIds[Index]](const TObjectPtr<APlayerState>& PlayerState)
			{
				return PlayerState && PlayerState->GetUniqueId() == Id;
			});
			if (!bStillHere)
			{
				Sessions->UnregisterPlayer(VoiceSessionName, *RegisteredIds[Index]);
				TalkingIds.Remove(RegisteredIds[Index]);
				RegisteredIds.RemoveAtSwap(Index);
			}
		}
	}

	// a talker per remote player: its voice comes from the head of its pawn, and fades with distance
	RemoteTalkers.RemoveAllSwap([](const FRemoteTalker& Entry) { return !Entry.PlayerState.IsValid(); });
	for (const TObjectPtr<APlayerState>& PlayerState : GameState->PlayerArray)
	{
		if (!PlayerState || PlayerState == LocalState || PlayerState->IsInactive() || PlayerState->IsABot() || !PlayerState->GetUniqueId().IsValid())
		{
			continue;
		}
		FRemoteTalker* Entry = RemoteTalkers.FindByPredicate([&PlayerState](const FRemoteTalker& Talker) { return Talker.PlayerState.Get() == PlayerState; });
		if (!Entry)
		{
			Entry = &RemoteTalkers.AddDefaulted_GetRef();
			Entry->PlayerState = PlayerState.Get();
		}
		UpdateTalker(*Entry);
	}
}

void UVoiceChatSubsystem::UpdateTalker(FRemoteTalker& Entry)
{
	APlayerState* PlayerState = Entry.PlayerState.Get();
	if (!PlayerState)
	{
		return;
	}
	UVOIPTalker* Talker = Entry.Talker.Get();
	USceneComponent* Anchor = Entry.Anchor.Get();
	if (!Talker || !Anchor)
	{
		// components of the player state: they go with it when the player leaves (the talker leaves the engine's table)
		Anchor = NewObject<USceneComponent>(PlayerState, NAME_None, RF_Transient);
		Anchor->SetMobility(EComponentMobility::Movable);
		Anchor->RegisterComponent();
		Talker = NewObject<UVOIPTalker>(PlayerState, NAME_None, RF_Transient);
		Talker->RegisterComponent();
		Talker->Settings.ComponentToAttachTo = Anchor;
		Talker->Settings.AttenuationSettings = GetAttenuation();
		Entry.Talker = Talker;
		Entry.Anchor = Anchor;
		Entry.Pawn = nullptr;
		Entry.bParked = false;
	}

	// the engine keeps one talker per player for the whole process: keep ours there, unless another PIE world of
	// this process holds that player (the editor's several windows in one process share the table)
	const UVOIPTalker* Registered = UVOIPStatics::GetVOIPTalkerForPlayer(PlayerState->GetUniqueId());
	if (Registered != Talker)
	{
		if (!IsValid(Registered) || Registered->GetWorld() == GetWorld())
		{
			Talker->RegisterWithPlayerState(PlayerState);
		}
		else if (!Entry.bWarnedShared)
		{
			Entry.bWarnedShared = true;
			UE_LOG(LogMonkeVoice, Log, TEXT("Voice: a player's voice is placed by another PIE window of this process (several windows, one process): not spatialised here"));
		}
	}

	// the voice on the head of the pawn; without a pawn (dead and gone, or too far to be relevant), out of hearing
	APawn* Pawn = PlayerState->GetPawn();
	if (!IsValid(Pawn))
	{
		Pawn = nullptr;
	}
	if (Pawn)
	{
		if (Pawn != Entry.Pawn.Get() || !Anchor->GetAttachParent())
		{
			FName Socket;
			if (USceneComponent* Head = FindHead(Pawn, Socket))
			{
				Anchor->AttachToComponent(Head, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
				if (Socket.IsNone())
				{
					Anchor->SetRelativeLocation(GetRules().HeadFallbackOffset);
				}
			}
		}
		Entry.bParked = false;
	}
	else if (!Entry.bParked)
	{
		Anchor->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Anchor->SetWorldLocation(ParkedVoiceLocation);
		Entry.bParked = true;
	}
	Entry.Pawn = Pawn;
}

USceneComponent* UVoiceChatSubsystem::FindHead(APawn* Pawn, FName& OutSocket) const
{
	const FName Socket = GetRules().HeadSocketName;
	OutSocket = Socket;
	if (!Socket.IsNone())
	{
		if (const ACharacter* Character = Cast<ACharacter>(Pawn); Character && Character->GetMesh() && Character->GetMesh()->DoesSocketExist(Socket))
		{
			return Character->GetMesh();
		}
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Pawn);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->DoesSocketExist(Socket))
			{
				return Mesh;
			}
		}
	}
	OutSocket = NAME_None;
	return Pawn->GetRootComponent();
}

USoundAttenuation* UVoiceChatSubsystem::GetAttenuation()
{
	const UVoiceRules& Current = GetRules();
	if (Current.AttenuationOverride)
	{
		return Current.AttenuationOverride;
	}
	if (!Attenuation)
	{
		Attenuation = NewObject<USoundAttenuation>(this, NAME_None, RF_Transient);
		Attenuation->Attenuation = VoiceMath::MakeAttenuation(Current.InnerRadiusCm, Current.MaxDistanceCm, Current.DistanceModel, Current.AttenuationAtMaxDb);
	}
	return Attenuation;
}

void UVoiceChatSubsystem::UpdateNearbyText()
{
	UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	const APlayerController* LocalController = GEngine && World ? GEngine->GetFirstLocalPlayerController(World) : nullptr;
	const UVoiceRules& Current = GetRules();

	TArray<TPair<double, FString>, TInlineAllocator<8>> Heard;
	if (GameState && LocalController && Current.MaxNamesShown > 0)
	{
		FVector ListenerLocation;
		FRotator ListenerRotation;
		LocalController->GetPlayerViewPoint(ListenerLocation, ListenerRotation);
		for (const FUniqueNetIdWrapper& Id : TalkingIds)
		{
			const APlayerState* PlayerState = GameState->GetPlayerStateFromUniqueNetId(Id);
			const APawn* Pawn = PlayerState && PlayerState != LocalController->PlayerState ? PlayerState->GetPawn() : nullptr;
			if (!IsValid(Pawn))
			{
				continue;
			}
			const double Distance = FVector::Dist(Pawn->GetActorLocation(), ListenerLocation);
			if (Distance <= Current.MaxDistanceCm)
			{
				Heard.Emplace(Distance, VoiceMath::DisplayName(PlayerState->GetPlayerName(), Current.MaxNameChars));
			}
		}
		Heard.Sort([](const TPair<double, FString>& A, const TPair<double, FString>& B) { return A.Key < B.Key; });
	}

	const int32 Shown = FMath::Min(Heard.Num(), FMath::Max(0, Current.MaxNamesShown));
	bool bSame = NearbyNames.Num() == Heard.Num();
	for (int32 Index = 0; bSame && Index < Heard.Num(); ++Index)
	{
		bSame = NearbyNames[Index] == Heard[Index].Value;
	}
	if (bSame)
	{
		return;
	}
	NearbyNames.Reset(Heard.Num());
	TArray<FText> Lines;
	for (int32 Index = 0; Index < Heard.Num(); ++Index)
	{
		NearbyNames.Add(Heard[Index].Value);
		if (Index < Shown)
		{
			Lines.Add(FText::Format(LOCTEXT("Talking", "{0} parle"), FText::FromString(Heard[Index].Value)));
		}
	}
	if (Heard.Num() > Shown && Shown > 0)
	{
		Lines.Add(FText::Format(LOCTEXT("TalkingMore", "+{0}"), Heard.Num() - Shown));
	}
	NearbyText = FText::Join(FText::FromString(TEXT("\n")), Lines);
}

// ---------------------------------------------------------------- debug

void UVoiceChatSubsystem::DumpState(FOutputDevice& Ar) const
{
	const UWorld* World = GetWorld();
	const UVoiceChatComponent* Local = LocalComponent.Get();
	Ar.Logf(TEXT("Voice: world %s, net mode %d, rules %s"), World ? *World->GetName() : TEXT("none"),
		World ? static_cast<int32>(World->GetNetMode()) : -1, Rules ? TEXT("DA_VoiceRules") : TEXT("class defaults"));
	Ar.Logf(TEXT("Voice: session ready %d (own %d, pending %d, failed %d), voice interface %d, local talker known %d"),
		static_cast<int32>(bSessionReady), static_cast<int32>(bOwnSession), static_cast<int32>(bSessionPending),
		static_cast<int32>(bSessionFailed), static_cast<int32>(VoiceInterface.IsValid()), static_cast<int32>(LocalTalkerId.IsValid()));
	Ar.Logf(TEXT("Voice: local indicator %d, engine hears the local player %d, outgoing voice dropped %d, fake talk %d"),
		static_cast<int32>(Local ? Local->GetIndicator() : EVoiceIndicator::None), static_cast<int32>(bLocalTalking),
		static_cast<int32>(bSuppressOutgoing), static_cast<int32>(IsFakeTalkOn()));
	Ar.Logf(TEXT("Voice: %d player(s) in the session, %d remote talker(s), %d heard talking now"),
		RegisteredIds.Num(), RemoteTalkers.Num(), TalkingIds.Num());
}

#undef LOCTEXT_NAMESPACE
