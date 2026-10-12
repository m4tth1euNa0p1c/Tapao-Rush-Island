// monke_island

#include "Emotes/TrumpetEmoteComponent.h"
#include "Emotes/TrumpetCallMath.h"
#include "Emotes/TrumpetEmoteRules.h"
#include "Boats/BoatRiderComponent.h"
#include "Ceremony/FlagCeremonySite.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Player/IslanderMovementComponent.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UTrumpetEmoteComponent::UTrumpetEmoteComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// after the puppet built its pose: the trumpet follows the right hand
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(true);
}

void UTrumpetEmoteComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UTrumpetEmoteComponent, PlayState);
	// only the owner's HUD shows the cooldown
	DOREPLIFETIME_CONDITION(UTrumpetEmoteComponent, CallStartTime, COND_OwnerOnly);
}

void UTrumpetEmoteComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_TrumpetEmoteRules, no trumpet"), *GetNameSafe(GetOwner()));
		return;
	}
	TryBindInput();
}

void UTrumpetEmoteComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EndTimer);
		World->GetTimerManager().ClearTimer(BindTimer);
	}
	if (IsValid(Sound))
	{
		Sound->Stop();
	}
	if (IsValid(CallSoundComponent))
	{
		CallSoundComponent->Stop();
	}
	if (Trumpet)
	{
		Trumpet->DestroyComponent();
		Trumpet = nullptr;
	}
	SetPlayView(false, false);
	Super::EndPlay(EndPlayReason);
}

void UTrumpetEmoteComponent::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (bInputBound || !Pawn || !Rules)
	{
		return;
	}
	UEnhancedInputComponent* Input = Pawn->IsLocallyControlled() ? Cast<UEnhancedInputComponent>(Pawn->InputComponent) : nullptr;
	const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	if (!Input || !PlayerController)
	{
		// the controller and the input arrive after BeginPlay on clients; remote pawns never get them
		if ((!Pawn->GetController() || Pawn->IsLocallyControlled()) && ++BindAttempts < 60)
		{
			GetWorld()->GetTimerManager().SetTimer(BindTimer, this, &UTrumpetEmoteComponent::TryBindInput, 0.25f, false);
		}
		return;
	}
	// the call (key A); the free fanfare of the key V is gone (T-333): EmoteAction is no longer bound
	if (Rules->CallAction)
	{
		Input->BindAction(Rules->CallAction, ETriggerEvent::Started, this, &UTrumpetEmoteComponent::RequestCall);
	}
	else
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: DA_TrumpetEmoteRules has no CallAction (IA_TrumpetCall), no trumpet call: "
			"the editor action of scripts/editor/trumpet_call_ue.py sets it up"), *GetNameSafe(Pawn));
	}
	if (Rules->CeremonyAction)
	{
		Input->BindAction(Rules->CeremonyAction, ETriggerEvent::Started, this, &UTrumpetEmoteComponent::RequestCeremony);
	}
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
	if (Subsystem && Rules->MappingContext)
	{
		Subsystem->AddMappingContext(Rules->MappingContext, Rules->MappingPriority);
	}
	bInputBound = true;
}

void UTrumpetEmoteComponent::RequestCall()
{
	// nothing to ask while this machine sees the call cannot start (the server checks it all again)
	if (!CanCallNow())
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		ServerRequestCall_Implementation();
	}
	else
	{
		ServerRequestCall();
	}
}

void UTrumpetEmoteComponent::ServerRequestCall_Implementation()
{
	if (!Rules || !ConsumeRequest(TEXT("trumpet call")))
	{
		return;
	}
	const double Now = GetServerNow();
	const bool bPlaying = IsPlaying();
	const bool bFree = CanPlayNow();
	if (bPlaying || !bFree || !TrumpetCallMath::IsReady(Now, CallStartTime, Rules->CallCooldown))
	{
		// a refusal of the rules (security.md §2): logged, nothing happens, no disconnection
		UE_LOG(Logmonke_island, Warning, TEXT("%s: trumpet call refused (%s)"), *GetNameSafe(GetOwner()),
			bPlaying ? TEXT("the trumpet already plays") : !bFree ? TEXT("the bean cannot play now") : TEXT("cooldown not over"));
		return;
	}
	CallStartTime = Now;
	StartPlay(ETrumpetPlayKind::Call, TrumpetCallMath::LoopSeconds(Rules->CallSeconds, Rules->EquipSeconds, Rules->UnequipSeconds));
	UE_LOG(Logmonke_island, Log, TEXT("%s: trumpet call (%.1f s, the next one in %.0f s)"), *GetNameSafe(GetOwner()), Rules->CallSeconds,
		Rules->CallCooldown);
}

void UTrumpetEmoteComponent::RequestCeremony()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AFlagCeremonySite* Site = Pawn ? AFlagCeremonySite::FindAt(this, Pawn->GetActorLocation()) : nullptr;
	// the area maps the key only while the ceremony can start: nothing to ask otherwise
	if (!Site || !Site->CanStart(Pawn))
	{
		return;
	}
	if (Pawn->HasAuthority())
	{
		ServerRequestCeremony_Implementation(Site);
	}
	else
	{
		ServerRequestCeremony(Site);
	}
}

void UTrumpetEmoteComponent::ServerRequestCeremony_Implementation(AFlagCeremonySite* Site)
{
	// the site checks the area, the officiant and its own state again
	if (IsValid(Site) && ConsumeRequest(TEXT("flag ceremony")))
	{
		Site->TryStart(Cast<APawn>(GetOwner()));
	}
}

bool UTrumpetEmoteComponent::ConsumeRequest(const TCHAR* What)
{
	const UWorld* World = GetWorld();
	if (!World || !Rules)
	{
		return false;
	}
	if (RequestLimiter.Consume(World->GetTimeSeconds(), Rules->RequestBurst, Rules->RequestsPerSecond))
	{
		return true;
	}
	UE_LOG(Logmonke_island, Verbose, TEXT("%s: %s refused, more than %.0f requests at once (%d refused so far)"), *GetNameSafe(GetOwner()),
		What, Rules->RequestBurst, RequestLimiter.Refused);
	return false;
}

bool UTrumpetEmoteComponent::StartRitual(float Seconds)
{
	if (!GetOwner()->HasAuthority() || !CanPlayNow() || !(Seconds > 0.0f))
	{
		return false;
	}
	StartPlay(ETrumpetPlayKind::Ceremony, Seconds);
	return true;
}

void UTrumpetEmoteComponent::StopRitual()
{
	if (GetOwner()->HasAuthority() && IsPlaying() && LocalKind == ETrumpetPlayKind::Ceremony)
	{
		CutPlay();
	}
}

bool UTrumpetEmoteComponent::CanPlayNow() const
{
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	if (!Rules || !Islander || Islander->IsRagdoll())
	{
		return false;
	}
	if (const UIslanderHealthComponent* Health = Islander->GetHealthComponent(); Health && Health->IsDead())
	{
		return false;
	}
	if (const UBoatRiderComponent* Rider = Islander->FindComponentByClass<UBoatRiderComponent>(); Rider && Rider->IsSeated())
	{
		return false;
	}
	const UCharacterMovementComponent* Move = Islander->GetCharacterMovement();
	return !(Move && Move->IsSwimming());
}

void UTrumpetEmoteComponent::StartPlay(ETrumpetPlayKind Kind, float LoopSeconds)
{
	// held with both hands: the item in hand is put away, and taken back at the end (a play still running put it away
	// already: it comes back at the end of this one)
	if (PutAwaySlot == INDEX_NONE)
	{
		if (UIslanderInventoryComponent* Items = GetOwner()->FindComponentByClass<UIslanderInventoryComponent>(); Items && Items->GetActiveSlot() >= 0)
		{
			PutAwaySlot = Items->GetActiveSlot();
			Items->SelectSlot(PutAwaySlot);
		}
	}
	++PlayState.Id;
	PlayState.Kind = Kind;
	PlayState.LoopSeconds = FMath::Max(LoopSeconds, 0.0f);
	PlayState.StartTime = GetServerNow();
	PlayState.CutAfter = -1.0f;
	GetOwner()->ForceNetUpdate();
	StartLocal(0.0f);
	ScheduleFinish();
}

void UTrumpetEmoteComponent::CutPlay()
{
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	const ETrumpetPhase Phase = GetPhase(Elapsed, Duration);
	// only the equip and the loop are cut short: the put away runs to its end
	if (PlayState.CutAfter >= 0.0f || (Phase != ETrumpetPhase::Equip && Phase != ETrumpetPhase::Ritual))
	{
		return;
	}
	PlayState.CutAfter = FMath::Max(GetElapsed(), 0.0f);
	GetOwner()->ForceNetUpdate();
	StopLocalLoop();
	ScheduleFinish();
}

void UTrumpetEmoteComponent::CheckCallOnServer()
{
	if (!Inventory.IsValid())
	{
		Inventory = GetOwner()->FindComponentByClass<UIslanderInventoryComponent>();
	}
	// knocked out, dead, in the water, seated, or an item taken in hand (a slot key): the call breaks, the trumpet goes away
	const bool bItemInHand = Inventory.IsValid() && Inventory->GetActiveSlot() >= 0;
	if (bItemInHand || !CanPlayNow())
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: trumpet call broken (%s)"), *GetNameSafe(GetOwner()),
			bItemInHand ? TEXT("an item taken in hand") : TEXT("the bean can no longer play"));
		CutPlay();
	}
}

void UTrumpetEmoteComponent::UpdateCallStamina()
{
	// stamina runs on the server and on the owning client (UIslanderMovementComponent): the call gives it back on both,
	// from the same replicated play, so their movement predictions keep agreeing; the other machines only show it
	if (GetOwnerRole() < ROLE_AutonomousProxy || !Rules || LocalStart < 0.0)
	{
		return;
	}
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UIslanderMovementComponent* Move = Character ? Cast<UIslanderMovementComponent>(Character->GetCharacterMovement()) : nullptr;
	if (!Move)
	{
		return;
	}
	// up to the end of the loop on this machine (sooner when the call was broken)
	const float Elapsed = FMath::Min(GetElapsed(), static_cast<float>(LocalLoopEnd - LocalStart));
	const float Given = TrumpetCallMath::StaminaGiven(Elapsed, Rules->EquipSeconds, PlayState.LoopSeconds, Rules->CallStaminaSeconds,
		Rules->CallStaminaFraction * Move->MaxStamina);
	if (Given > CallStaminaGiven)
	{
		Move->RestoreStamina(Given - CallStaminaGiven);
		CallStaminaGiven = Given;
	}
}

void UTrumpetEmoteComponent::OnRep_PlayState(const FTrumpetPlay& PreviousPlay)
{
	if (PlayState.Id != PreviousPlay.Id)
	{
		// a new play, dated on the server: a pawn seen late (it came into relevancy) joins it where it is, or skips it
		// once it is over
		const float Late = static_cast<float>(GetServerNow() - PlayState.StartTime);
		if (PlayState.Kind != ETrumpetPlayKind::None && Late < TotalOf(PlayState))
		{
			StartLocal(Late);
		}
		return;
	}
	if (PlayState.CutAfter >= 0.0f && PreviousPlay.CutAfter < 0.0f)
	{
		StopLocalLoop();
	}
}

void UTrumpetEmoteComponent::StopLocalLoop()
{
	if (LocalKind == ETrumpetPlayKind::None || LocalStart < 0.0 || !Rules)
	{
		return;
	}
	// where the server cut it, in the time of this machine (the local play is dated on the server's start); now otherwise
	const double CutAt = PlayState.CutAfter >= 0.0f ? LocalStart + PlayState.CutAfter : GetWorld()->GetTimeSeconds();
	LocalLoopEnd = FMath::Max(LocalStart + Rules->EquipSeconds, FMath::Min(LocalLoopEnd, CutAt));
	// the bugle call stops with the trumpet leaving the mouth
	if (IsValid(CallSoundComponent))
	{
		CallSoundComponent->FadeOut(Rules->CallSoundFadeSeconds, 0.0f);
	}
}

void UTrumpetEmoteComponent::ScheduleFinish()
{
	GetWorld()->GetTimerManager().SetTimer(EndTimer, this, &UTrumpetEmoteComponent::FinishOnServer, FMath::Max(GetTotal() - GetElapsed(), 0.01f), false);
}

void UTrumpetEmoteComponent::FinishOnServer()
{
	UIslanderInventoryComponent* Items = GetOwner()->FindComponentByClass<UIslanderInventoryComponent>();
	if (Items && PutAwaySlot != INDEX_NONE && Items->GetActiveSlot() < 0)
	{
		Items->SelectSlot(PutAwaySlot);
	}
	PutAwaySlot = INDEX_NONE;
}

void UTrumpetEmoteComponent::StartLocal(float Late)
{
	if (!Rules || PlayState.Kind == ETrumpetPlayKind::None)
	{
		return;
	}
	// a call replaced by a ceremony: its bugle call stops
	if (IsValid(CallSoundComponent))
	{
		CallSoundComponent->FadeOut(Rules->CallSoundFadeSeconds, 0.0f);
	}
	CallSoundComponent = nullptr;
	LocalKind = PlayState.Kind;
	LocalStart = GetWorld()->GetTimeSeconds() - FMath::Max(Late, 0.0f);
	bSounded = false;
	CallStaminaGiven = 0.0f;
	LocalLoopEnd = LocalStart + Rules->EquipSeconds + PlayState.LoopSeconds;
	if (PlayState.CutAfter >= 0.0f)
	{
		LocalLoopEnd = FMath::Max(LocalStart + Rules->EquipSeconds, FMath::Min(LocalLoopEnd, LocalStart + PlayState.CutAfter));
	}
	NextRitualFanfare = LocalStart + Rules->EquipSeconds;
	// seen from behind; the ceremony also holds the bean still until the trumpet goes away
	SetPlayView(true, LocalKind == ETrumpetPlayKind::Ceremony);
	if (!Puppet.IsValid())
	{
		Puppet = GetOwner()->FindComponentByClass<UBeanPuppetComponent>();
	}
	if (!Trumpet && Puppet.IsValid())
	{
		Trumpet = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("EmoteTrumpet"));
		Trumpet->SetStaticMesh(Rules->TrumpetMesh);
		Trumpet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Trumpet->SetCanEverAffectNavigation(false);
		// rides on the puppet component (scale 1), placed every frame from the hand
		Trumpet->SetupAttachment(Puppet.Get());
		Trumpet->SetHiddenInGame(true);
		Trumpet->RegisterComponent();
		AddTickPrerequisiteComponent(Puppet.Get());
	}
	SetComponentTickEnabled(true);
}

float UTrumpetEmoteComponent::GetElapsed() const
{
	return LocalStart < 0.0 || !GetWorld() ? 0.0f : static_cast<float>(GetWorld()->GetTimeSeconds() - LocalStart);
}

float UTrumpetEmoteComponent::GetTotal() const
{
	return Rules && LocalStart >= 0.0 ? static_cast<float>(LocalLoopEnd - LocalStart) + Rules->UnequipSeconds : 0.0f;
}

float UTrumpetEmoteComponent::TotalOf(const FTrumpetPlay& InPlay) const
{
	if (!Rules)
	{
		return 0.0f;
	}
	const float Loop = InPlay.CutAfter >= 0.0f ? FMath::Clamp(InPlay.CutAfter - Rules->EquipSeconds, 0.0f, InPlay.LoopSeconds) : InPlay.LoopSeconds;
	return Rules->EquipSeconds + Loop + Rules->UnequipSeconds;
}

double UTrumpetEmoteComponent::GetServerNow() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}

ETrumpetPhase UTrumpetEmoteComponent::GetPhase(float& OutElapsed, float& OutDuration) const
{
	OutElapsed = 0.0f;
	OutDuration = 0.0f;
	if (!IsPlaying())
	{
		return ETrumpetPhase::None;
	}
	float Time = GetElapsed();
	if (Time < Rules->EquipSeconds)
	{
		OutElapsed = Time;
		OutDuration = Rules->EquipSeconds;
		return ETrumpetPhase::Equip;
	}
	Time -= Rules->EquipSeconds;
	// the play loop until its end on this machine
	const float Loop = static_cast<float>(LocalLoopEnd - LocalStart) - Rules->EquipSeconds;
	if (Time < Loop)
	{
		OutElapsed = Time;
		OutDuration = Loop;
		return ETrumpetPhase::Ritual;
	}
	OutElapsed = Time - Loop;
	OutDuration = Rules->UnequipSeconds;
	return ETrumpetPhase::Unequip;
}

bool UTrumpetEmoteComponent::HasCall() const
{
	return Rules && Rules->CallAction;
}

float UTrumpetEmoteComponent::GetCallCooldownLeft() const
{
	return Rules ? TrumpetCallMath::CooldownLeft(GetServerNow(), CallStartTime, Rules->CallCooldown) : 0.0f;
}

float UTrumpetEmoteComponent::GetCallCooldownFraction() const
{
	return Rules ? TrumpetCallMath::CooldownFraction(GetServerNow(), CallStartTime, Rules->CallCooldown) : 0.0f;
}

float UTrumpetEmoteComponent::GetCallFractionLeft() const
{
	if (!IsCallPlaying())
	{
		return -1.0f;
	}
	const float Total = GetTotal();
	return Total > 0.0f ? FMath::Clamp(1.0f - GetElapsed() / Total, 0.0f, 1.0f) : 0.0f;
}

bool UTrumpetEmoteComponent::CanCallNow() const
{
	return HasCall() && !IsPlaying() && CanPlayNow() && GetCallCooldownLeft() <= 0.0f;
}

FText UTrumpetEmoteComponent::GetCallKeyText() const
{
	if (!CallKeyName.IsEmpty())
	{
		return CallKeyName;
	}
	// the key the local player has mapped to the call, once the input subsystem has built its mappings; the rules' key
	// until then
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Input = Controller ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer()) : nullptr;
	if (Input && Rules && Rules->CallAction)
	{
		const TArray<FKey> Keys = Input->QueryKeysMappedToAction(Rules->CallAction);
		if (Keys.Num() > 0)
		{
			CallKeyName = Keys[0].GetDisplayName(false);
			return CallKeyName;
		}
	}
	return Rules && Rules->CallKey.IsValid() ? Rules->CallKey.GetDisplayName(false) : FText::GetEmpty();
}

UTexture2D* UTrumpetEmoteComponent::GetCallIcon() const
{
	return Rules ? Rules->CallIcon.Get() : nullptr;
}

void UTrumpetEmoteComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	const ETrumpetPhase Phase = GetPhase(Elapsed, Duration);
	USceneComponent* SoundParent = Trumpet ? static_cast<USceneComponent*>(Trumpet) : GetOwner()->GetRootComponent();
	if (Phase == ETrumpetPhase::Ritual && LocalKind == ETrumpetPlayKind::Call)
	{
		// the bugle call once, from the start of the loop (from where it is for a pawn seen late); the fanfare of the
		// ceremony stands in until the call has its own sound
		USoundBase* CallSound = Rules->CallSound ? Rules->CallSound.Get() : Rules->FanfareSound.Get();
		if (!bSounded && CallSound)
		{
			bSounded = true;
			CallSoundComponent = UGameplayStatics::SpawnSoundAttached(CallSound, SoundParent, NAME_None, FVector::ZeroVector,
				EAttachLocation::KeepRelativeOffset, true, 1.0f, 1.0f, Elapsed, Rules->SoundAttenuation);
		}
	}
	else if (Phase == ETrumpetPhase::Ritual && Rules->FanfareSound)
	{
		// a ceremony: the fanfare at the start of the loop, then now and then
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextRitualFanfare)
		{
			NextRitualFanfare = Rules->RitualFanfareInterval > 0.0f ? Now + Rules->RitualFanfareInterval : TNumericLimits<double>::Max();
			Sound = UGameplayStatics::SpawnSoundAttached(Rules->FanfareSound, SoundParent, NAME_None, FVector::ZeroVector,
				EAttachLocation::KeepRelativeOffset, true, 1.0f, 1.0f, 0.0f, Rules->SoundAttenuation);
		}
	}
	if (LocalKind == ETrumpetPlayKind::Call)
	{
		UpdateCallStamina();
		if (GetOwner()->HasAuthority() && (Phase == ETrumpetPhase::Equip || Phase == ETrumpetPhase::Ritual))
		{
			CheckCallOnServer();
		}
	}
	UpdateTrumpet();
	// the player's own view comes back as the trumpet goes away
	if ((bViewForced || bHoldStill) && Phase != ETrumpetPhase::Equip && Phase != ETrumpetPhase::Ritual)
	{
		SetPlayView(false, false);
	}
	if (Phase == ETrumpetPhase::None)
	{
		LocalStart = -1.0;
		LocalKind = ETrumpetPlayKind::None;
		SetComponentTickEnabled(false);
	}
}

void UTrumpetEmoteComponent::SetPlayView(bool bForceThirdPerson, bool bHold)
{
	AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	if (bForceThirdPerson != bViewForced)
	{
		bViewForced = bForceThirdPerson;
		// every machine: the view mode also sets how the body turns, which the movement replication follows. A boat seat
		// holds the third person on its own and gives the view back when the rider stands up
		const UBoatRiderComponent* Rider = Islander ? Islander->FindComponentByClass<UBoatRiderComponent>() : nullptr;
		if (Islander && (bForceThirdPerson || !(Rider && Rider->IsSeated())))
		{
			Islander->SetForceThirdPerson(bForceThirdPerson);
		}
	}
	if (bHold != bHoldStill)
	{
		bHoldStill = bHold;
		// the owner's controller ignores the movement keys (a stack: one hold, one release)
		if (APlayerController* Controller = Islander && Islander->IsLocallyControlled() ? Cast<APlayerController>(Islander->GetController()) : nullptr)
		{
			Controller->SetIgnoreMoveInput(bHold);
		}
	}
}

void UTrumpetEmoteComponent::UpdateTrumpet()
{
	UBeanPuppetComponent* Body = Puppet.Get();
	if (!Trumpet || !Body)
	{
		return;
	}
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	const ETrumpetPhase Phase = GetPhase(Elapsed, Duration);
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	const bool bShow = Phase != ETrumpetPhase::None && !(Islander && Islander->IsRagdoll()) && Body->GetBoneIndex(Rules->GripBone) != INDEX_NONE;
	if (Trumpet->bHiddenInGame == bShow)
	{
		Trumpet->SetHiddenInGame(!bShow, true);
	}
	if (!bShow)
	{
		return;
	}
	const bool bAtMouth = Phase == ETrumpetPhase::Fanfare || Phase == ETrumpetPhase::Ritual
		|| (Phase == ETrumpetPhase::Equip && Elapsed >= Rules->AtMouthTime)
		|| (Phase == ETrumpetPhase::Unequip && Elapsed < Rules->LeaveMouthTime);
	// component space bones carry the FBX root scale (x100): only location and rotation are used
	const FTransform BoneCS = Body->GetBoneTransformByName(Rules->GripBone, EBoneSpaces::ComponentSpace);
	Trumpet->SetRelativeTransform((bAtMouth ? Rules->GripInHand : Rules->CarryInHand) * FTransform(BoneCS.GetRotation(), BoneCS.GetLocation()));
}
