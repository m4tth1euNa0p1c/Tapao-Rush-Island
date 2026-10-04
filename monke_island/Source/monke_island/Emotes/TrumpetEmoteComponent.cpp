// monke_island

#include "Emotes/TrumpetEmoteComponent.h"
#include "Emotes/TrumpetEmoteRules.h"
#include "Boats/BoatRiderComponent.h"
#include "Character/Bean/BeanPuppetComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
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
	DOREPLIFETIME(UTrumpetEmoteComponent, PlayCount);
}

void UTrumpetEmoteComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_TrumpetEmoteRules, no trumpet emote"), *GetNameSafe(GetOwner()));
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
	if (Sound)
	{
		Sound->Stop();
	}
	if (Trumpet)
	{
		Trumpet->DestroyComponent();
		Trumpet = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UTrumpetEmoteComponent::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (bInputBound || !Pawn || !Rules || !Rules->EmoteAction)
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
	Input->BindAction(Rules->EmoteAction, ETriggerEvent::Started, this, &UTrumpetEmoteComponent::PlayFanfare);
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		if (Rules->MappingContext)
		{
			Subsystem->AddMappingContext(Rules->MappingContext, Rules->MappingPriority);
		}
	}
	bInputBound = true;
}

void UTrumpetEmoteComponent::PlayFanfare()
{
	if (!Rules || IsPlaying())
	{
		return;
	}
	if (GetOwner()->HasAuthority())
	{
		ServerPlayFanfare_Implementation();
	}
	else
	{
		ServerPlayFanfare();
	}
}

bool UTrumpetEmoteComponent::CanPlay() const
{
	const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner());
	if (!Rules || !Islander || Islander->IsRagdoll() || IsPlaying())
	{
		return false;
	}
	if (GetWorld()->GetTimeSeconds() - LastServerStart < Rules->Cooldown)
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

void UTrumpetEmoteComponent::ServerPlayFanfare_Implementation()
{
	if (!CanPlay())
	{
		return;
	}
	LastServerStart = GetWorld()->GetTimeSeconds();
	// held with both hands: the item in hand is put away, and taken back at the end
	PutAwaySlot = INDEX_NONE;
	if (UIslanderInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UIslanderInventoryComponent>(); Inventory && Inventory->GetActiveSlot() >= 0)
	{
		PutAwaySlot = Inventory->GetActiveSlot();
		Inventory->SelectSlot(PutAwaySlot);
	}
	++PlayCount;
	StartLocal();
	GetWorld()->GetTimerManager().SetTimer(EndTimer, this, &UTrumpetEmoteComponent::FinishOnServer, GetTotal(), false);
}

void UTrumpetEmoteComponent::FinishOnServer()
{
	UIslanderInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UIslanderInventoryComponent>();
	if (Inventory && PutAwaySlot != INDEX_NONE && Inventory->GetActiveSlot() < 0)
	{
		Inventory->SelectSlot(PutAwaySlot);
	}
	PutAwaySlot = INDEX_NONE;
}

void UTrumpetEmoteComponent::OnRep_PlayCount()
{
	StartLocal();
}

void UTrumpetEmoteComponent::StartLocal()
{
	if (!Rules)
	{
		return;
	}
	LocalStart = GetWorld()->GetTimeSeconds();
	bSounded = false;
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
	return Rules ? Rules->EquipSeconds + Rules->FanfareSeconds + Rules->UnequipSeconds : 0.0f;
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
	if (Time < Rules->FanfareSeconds)
	{
		OutElapsed = Time;
		OutDuration = Rules->FanfareSeconds;
		return ETrumpetPhase::Fanfare;
	}
	OutElapsed = Time - Rules->FanfareSeconds;
	OutDuration = Rules->UnequipSeconds;
	return ETrumpetPhase::Unequip;
}

void UTrumpetEmoteComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	const ETrumpetPhase Phase = GetPhase(Elapsed, Duration);
	if (Phase == ETrumpetPhase::Fanfare && !bSounded && Rules->FanfareSound)
	{
		bSounded = true;
		Sound = UGameplayStatics::SpawnSoundAttached(Rules->FanfareSound, Trumpet ? static_cast<USceneComponent*>(Trumpet) : GetOwner()->GetRootComponent(),
			NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, true, 1.0f, 1.0f, Elapsed, Rules->SoundAttenuation);
	}
	UpdateTrumpet();
	if (Phase == ETrumpetPhase::None)
	{
		LocalStart = -1.0;
		SetComponentTickEnabled(false);
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
	const bool bAtMouth = Phase == ETrumpetPhase::Fanfare
		|| (Phase == ETrumpetPhase::Equip && Elapsed >= Rules->AtMouthTime)
		|| (Phase == ETrumpetPhase::Unequip && Elapsed < Rules->LeaveMouthTime);
	// component space bones carry the FBX root scale (x100): only location and rotation are used
	const FTransform BoneCS = Body->GetBoneTransformByName(Rules->GripBone, EBoneSpaces::ComponentSpace);
	Trumpet->SetRelativeTransform((bAtMouth ? Rules->GripInHand : Rules->CarryInHand) * FTransform(BoneCS.GetRotation(), BoneCS.GetLocation()));
}
