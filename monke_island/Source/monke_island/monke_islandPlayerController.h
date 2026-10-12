// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "monke_islandPlayerController.generated.h"

class UDeathSpectatorComponent;
class UInputMappingContext;
class UUserWidget;
class UVoiceChatComponent;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class MONKE_ISLAND_API Amonke_islandPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	Amonke_islandPlayerController();

	/** Proximity voice chat of this player (T-263): push-to-talk, microphone mode, the "I talk" report to the server */
	UVoiceChatComponent* GetVoiceChat() const { return VoiceChat; }

	/** The engine (at login) and the ToggleSpeaking command go through the voice chat, which keeps the player's mode */
	virtual void ToggleSpeaking(bool bInSpeaking) override;

	/** Death and spectator of this player (T-135): once dead, the view follows a living teammate */
	UDeathSpectatorComponent* GetDeathSpectator() const { return DeathSpectator; }

	/** Server: a dead player who follows a teammate sees the world from him, so what surrounds the teammate stays
	 *  relevant to this client (a dead client sends no camera update); else as the engine does (T-135) */
	virtual void GetPlayerViewPoint(FVector& out_Location, FRotator& out_Rotation) const override;

	/**
	 *  EnableCheats (exec) is refused for a remote player's controller on the host: "serverexec EnableCheats" gave a
	 *  client the host's cheats in development builds (God, Slomo, DestroyAll, Summon, SwitchLevel; security review
	 *  T-269 M3, T-296). See MayUseHostConsole
	 */
	virtual void EnableCheats() override;

	/** No cheat manager for a remote player's controller on the host (the editor gave one to every player), see
	 *  MayUseHostConsole */
	virtual void AddCheats(bool bForce = false) override;

	/**
	 *  This controller may use the host's console, cheats and pause: the host's own (or a client's, on its own machine),
	 *  or any while monke.Net.AllowClientCheats is 1 on the host, for a test between trusted machines (development
	 *  builds only; Shipping has neither console nor cheat manager)
	 */
	bool MayUseHostConsole() const;

	/** Rule of MayUseHostConsole (pure, monke_island.Net.ClientConsole) */
	static bool IsHostConsoleAllowed(bool bLocalController, bool bClientCheatsAllowed);

protected:

	/** The engine sends at most 128 characters (APlayerController::ServerExec): a longer command is a forged packet,
	 *  the client is dropped */
	virtual bool ServerExecRPC_Validate(const FString& Msg) override;

	/** Server: a console command a client sends (serverexec, development builds): run only if MayUseHostConsole */
	virtual void ServerExecRPC_Implementation(const FString& Msg) override;

	/** Server: only the host pauses the game (a client paused everyone, and could in a flood, in development builds;
	 *  the engine already refuses it in Shipping), see MayUseHostConsole */
	virtual void ServerPause_Implementation() override;

	/** Proximity voice chat (T-263, docs/features/chat-vocal.md) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Voice")
	TObjectPtr<UVoiceChatComponent> VoiceChat;

	/** Death and spectator (T-135, docs/features/death-spectator.md): replicated, its requests reach the server */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Death")
	TObjectPtr<UDeathSpectatorComponent> DeathSpectator;


	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

private:

	/** Server: a refused console command, cheat or pause, logged once a second at most (a flood never floods the log) */
	void LogHostConsoleRefusal(const TCHAR* What);

	/** Server: real time of the last refusal logged */
	double LastConsoleRefusalLogTime = -1.0e9;
};
