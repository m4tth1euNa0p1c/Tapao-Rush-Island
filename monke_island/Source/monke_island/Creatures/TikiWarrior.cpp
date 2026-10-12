// monke_island

#include "Creatures/TikiWarrior.h"
#include "Creatures/Hostility.h"
#include "Creatures/TikiBulwarkTactics.h"
#include "Creatures/TikiDirector.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiStalkerTactics.h"
#include "Creatures/TikiTactics.h"
#include "Creatures/TikiTacticsMath.h"
#include "Creatures/TikiWatcherTactics.h"
#include "Player/IslanderHealthComponent.h"
#include "Stealth/StealthRules.h"
#include "Stealth/StealthSubsystem.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/TorchItem.h"
#include "monke_island.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ATikiWarrior::ATikiWarrior()
{
}

float ATikiWarrior::GetRulesHealth() const
{
	// a tank of a crab, a lighter watcher (T-275)
	return Rules ? Rules->WarriorHealth * Rules->GetHealthScale(GetArchetype()) : 80.0f;
}

void ATikiWarrior::BeginPlay()
{
	Super::BeginPlay();
	LastTargetTime = GetWorld()->GetTimeSeconds();
	ApplyArmed();
	// the role of the mob (T-275): its tactics think on the server, its effects load everywhere they show
	if (HasAuthority())
	{
		CreateTactics();
	}
	PreloadTacticEffects();
	NextStalkSoundTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.0f, 3.0f);
}

void ATikiWarrior::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EffectsLoad.Reset();
	Super::EndPlay(EndPlayReason);
}

void ATikiWarrior::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATikiWarrior, bInCombat);
	DOREPLIFETIME_CONDITION(ATikiWarrior, bArmed, COND_InitialOnly);
	DOREPLIFETIME(ATikiWarrior, Tactic);
}

ETikiArchetype ATikiWarrior::GetArchetype() const
{
	if (Archetype != ETikiArchetype::Auto)
	{
		return Archetype;
	}
	// read once the rules are there (a client gets them with the first replication)
	if (ResolvedArchetype == ETikiArchetype::Auto && Rules)
	{
		ResolvedArchetype = TikiTactics::ResolveArchetype(GetClass()->GetName(), Rules->Pack.Archetypes);
	}
	return ResolvedArchetype == ETikiArchetype::Auto ? ETikiArchetype::Brawler : ResolvedArchetype;
}

void ATikiWarrior::CreateTactics()
{
	if (Tactics || !Rules)
	{
		return;
	}
	UClass* TacticsClass = nullptr;
	switch (GetArchetype())
	{
	case ETikiArchetype::Stalker:
		TacticsClass = UTikiStalkerTactics::StaticClass();
		break;
	case ETikiArchetype::Bulwark:
		TacticsClass = UTikiBulwarkTactics::StaticClass();
		break;
	case ETikiArchetype::Watcher:
		TacticsClass = UTikiWatcherTactics::StaticClass();
		break;
	default:
		// a Brawler keeps the classic melee
		return;
	}
	Tactics = NewObject<UTikiTactics>(this, TacticsClass, TEXT("Tactics"));
	Tactics->Bind(this);
	Tactics->RegisterComponent();
	UE_LOG(LogMonkeTactics, Log, TEXT("%s fights as %s"), *GetName(), *UEnum::GetValueAsString(GetArchetype()));
}

void ATikiWarrior::PreloadTacticEffects()
{
	if (!Rules || EffectsLoad.IsValid() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	const auto Add = [&Paths](const TSoftObjectPtr<UNiagaraSystem>& System)
	{
		if (!System.IsNull())
		{
			Paths.AddUnique(System.ToSoftObjectPath());
		}
	};
	switch (GetArchetype())
	{
	case ETikiArchetype::Bulwark:
		Add(Rules->Bulwark.BlockSystem);
		break;
	case ETikiArchetype::Watcher:
		Add(Rules->Watcher.Ash.CloudEffect);
		Add(Rules->Watcher.Ash.EmberEffect);
		break;
	default:
		break;
	}
	if (!Paths.IsEmpty())
	{
		// no sync load: an effect not loaded yet is skipped (MEM-06)
		EffectsLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
	}
}

const FTikiTacticClips* ATikiWarrior::GetTacticClips() const
{
	return Rules ? Rules->GetTacticClips(GetArchetype()) : nullptr;
}

const FTikiTacticSounds* ATikiWarrior::GetTacticSounds() const
{
	return Rules ? Rules->GetTacticSounds(GetArchetype()) : nullptr;
}

UAnimSequenceBase* ATikiWarrior::TacticClip(UAnimSequenceBase* Clip) const
{
	return Clip && IsClipForBody(Clip) ? Clip : nullptr;
}

bool ATikiWarrior::IsCommitted() const
{
	return PendingStrikeTime > 0.0 || (Tactics && Tactics->IsCommitted());
}

void ATikiWarrior::SetTacticRep(const FTikiTacticRep& Rep)
{
	if (Tactic != Rep)
	{
		Tactic = Rep;
		// facing its foe while it moves: the body no longer turns to its movement (the tactics turn it)
		GetCharacterMovement()->bOrientRotationToMovement = !Rep.bStrafe;
	}
}

bool ATikiWarrior::HoldsLitTorch(const APawn* Pawn, bool& bOutRaised)
{
	bOutRaised = false;
	const UIslanderInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	const ATorchItem* Torch = Inventory ? Cast<ATorchItem>(Inventory->GetEquippedItem()) : nullptr;
	if (!Torch || !Torch->IsLit())
	{
		return false;
	}
	bOutRaised = Torch->IsRaised();
	return true;
}

bool ATikiWarrior::IsRevealedByTorch(const AActor* Seen, float Range) const
{
	const UStealthSubsystem* Stealth = UStealthSubsystem::Get(this);
	const ACharacter* Player = Cast<ACharacter>(Seen);
	if (!Rules || !Rules->Pack.bTorchRevealsAtNight || !Stealth || !Player || !IsLivingPlayer(Player))
	{
		return false;
	}
	bool bRaised = false;
	const float Sight = Range > 0.0f ? FMath::Min(Range, Rules->Pack.TorchNightSightCm) : Rules->Pack.TorchNightSightCm;
	const float Distance = static_cast<float>(FVector::Dist(Player->GetActorLocation(), GetActorLocation()));
	return TikiTactics::TorchReveals(Stealth->IsNight(), Player->bIsCrouched, HoldsLitTorch(Player, bRaised), Distance, Sight);
}

bool ATikiWarrior::CanSpot(const AActor* Seen, float Range) const
{
	// the stealth first (T-262); at night the flame of a lit torch shows a standing player (T-275)
	return Super::CanSpot(Seen, Range) || IsRevealedByTorch(Seen, Range);
}

void ATikiWarrior::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// the breath and the clicks of a stalker circling its foe, near the local camera only (T-275)
	if (GetNetMode() == NM_DedicatedServer || IsDead() || (Tactic.State != ETikiTacticState::Stalk && Tactic.State != ETikiTacticState::Encircle))
	{
		return;
	}
	const FTikiTacticSounds* Sounds = GetTacticSounds();
	const double Now = GetWorld()->GetTimeSeconds();
	if (!Sounds || Sounds->Stalk.IsEmpty() || Now < NextStalkSoundTime)
	{
		return;
	}
	NextStalkSoundTime = Now + FMath::FRandRange(Sounds->StalkIntervalMin, FMath::Max(Sounds->StalkIntervalMin, Sounds->StalkIntervalMax));
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (Camera && FVector::DistSquared(Camera->GetCameraLocation(), GetActorLocation()) < FMath::Square(Sounds->StalkSoundDistance))
	{
		PlayTacticSound(Sounds->Stalk);
	}
}

UAnimSequenceBase* ATikiWarrior::GetIdleClip() const
{
	// on guard with a foe, or behind its claw guard (T-275)
	return (bInCombat || Tactic.bGuardUp) && Anims.IdleCombat ? Anims.IdleCombat.Get() : Anims.Idle.Get();
}

UAnimSequenceBase* ATikiWarrior::GetDirectionalClip(float Speed, float& OutClipSpeed) const
{
	// facing its foe while it moves (T-275): a side step, a walk backward; forward, the walk and run as before
	const FTikiTacticClips* Clips = Tactic.bStrafe ? GetTacticClips() : nullptr;
	if (!Clips)
	{
		return nullptr;
	}
	UAnimSequenceBase* Clip = nullptr;
	switch (TikiTactics::StrafeClipFor(GetVelocity(), GetActorForwardVector()))
	{
	case TikiTactics::EStrafeClip::Back:
		Clip = Clips->WalkBack;
		break;
	case TikiTactics::EStrafeClip::Left:
		Clip = Clips->StrafeLeft;
		break;
	case TikiTactics::EStrafeClip::Right:
		Clip = Clips->StrafeRight;
		break;
	default:
		break;
	}
	Clip = TacticClip(Clip);
	if (!Clip)
	{
		return nullptr;
	}
	const float Authored = Clip == Clips->WalkBack ? Clips->WalkBackClipSpeed : Clips->StrafeClipSpeed;
	OutClipSpeed = Authored > 0.0f ? Authored : (Anims.WalkSpeed > 0.0f ? Anims.WalkSpeed : (Rules ? Rules->WalkClipSpeed : 110.0f));
	return Clip;
}

bool ATikiWarrior::IsLocomotionClip(const UAnimSequenceBase* Clip) const
{
	if (Super::IsLocomotionClip(Clip))
	{
		return true;
	}
	const FTikiTacticClips* Clips = Clip ? GetTacticClips() : nullptr;
	return Clips && (Clip == Clips->StrafeLeft || Clip == Clips->StrafeRight || Clip == Clips->WalkBack);
}

void ATikiWarrior::PlayLocalAction(ETikiAction Action, uint8 Variant)
{
	const FTikiTacticClips* Clips = GetTacticClips();
	const FTikiTacticSounds* Sounds = GetTacticSounds();
	if (!Clips || !Sounds || !Rules)
	{
		Super::PlayLocalAction(Action, Variant);
		return;
	}
	// the gesture of a feint or a howl, the clip of the signature move: the role's, else the warrior's
	UAnimSequenceBase* Gesture = TacticClip(Clips->Gesture);
	if (!Gesture)
	{
		Gesture = Anims.Taunt ? Anims.Taunt.Get() : Anims.Alert.Get();
	}
	UAnimSequenceBase* Signature = TacticClip(Clips->Signature);
	if (!Signature)
	{
		Signature = Anims.Attack;
	}
	switch (Action)
	{
	case ETikiAction::Alert:
		// the voice's growl and the role's call over it
		Super::PlayLocalAction(Action, Variant);
		if (CanPlayAction())
		{
			PlayTacticSound(Sounds->Alert);
		}
		return;
	case ETikiAction::Feint:
		if (CanPlayAction())
		{
			PlayTacticSound(Sounds->Feint);
			PlayOneShot(Gesture, Rules->Stalker.FeintDashSeconds + Rules->Stalker.FeintBackSeconds);
		}
		return;
	case ETikiAction::HeavyAttack:
		if (CanPlayAction())
		{
			PlayTacticSound(Sounds->Windup);
			PlayVoice(Voice.Attack);
			PlayOneShot(Signature, Rules->Stalker.HeavyWindup + 0.45f, Clips->SignatureRate);
		}
		return;
	case ETikiAction::Grab:
		if (CanPlayAction())
		{
			PlayTacticSound(Sounds->Windup);
			PlayOneShot(Signature, Rules->Bulwark.GrabWindup + 0.4f, Clips->SignatureRate);
		}
		return;
	case ETikiAction::GuardBreak:
		if (CanPlayAction())
		{
			PlayTacticSound(Sounds->GuardBreak);
			PlayVoice(Voice.Hurt);
			PlayOneShot(Anims.Stagger ? Anims.Stagger.Get() : Anims.Hit.Get(), 0.0f);
		}
		return;
	case ETikiAction::Howl:
		if (CanPlayAction())
		{
			PlayTacticSound(Sounds->Howl, true);
			PlayOneShot(Gesture, Rules->Watcher.HowlSeconds);
		}
		return;
	case ETikiAction::Cast:
		if (CanPlayAction())
		{
			PlayTacticSound(Sounds->Windup);
			PlayOneShot(Signature, Rules->Watcher.CastWindup + 0.35f, Clips->SignatureRate);
		}
		return;
	case ETikiAction::Flee:
		if (CanPlayAction())
		{
			if (Sounds->Flee.IsEmpty())
			{
				PlayVoice(Voice.Flee.IsEmpty() ? Voice.Hurt : Voice.Flee);
			}
			else
			{
				PlayTacticSound(Sounds->Flee);
			}
		}
		return;
	default:
		Super::PlayLocalAction(Action, Variant);
		return;
	}
}

void ATikiWarrior::PlayTacticSound(const TArray<TObjectPtr<USoundBase>>& Sounds, bool bFar, const FVector* Location) const
{
	const FTikiTacticSounds* Set = GetTacticSounds();
	// the sound of an action received long after it started stays silent (T-306)
	if (Sounds.IsEmpty() || !Set || GetNetMode() == NM_DedicatedServer || (Rules && ActionAge > Rules->ActionLateSoundSeconds))
	{
		return;
	}
	USoundBase* Sound = Sounds[FMath::RandRange(0, Sounds.Num() - 1)];
	if (!Sound)
	{
		return;
	}
	USoundAttenuation* Attenuation = bFar && Set->FarAttenuation ? Set->FarAttenuation.Get() : (Set->Attenuation ? Set->Attenuation.Get() : Voice.Attenuation.Get());
	const float Pitch = FMath::FRandRange(0.95f, 1.05f);
	if (Location)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, *Location, Set->Volume, Pitch, 0.0f, Attenuation);
	}
	else
	{
		UGameplayStatics::SpawnSoundAttached(Sound, GetMesh(), NAME_None, FVector(0.0f, 0.0f, 70.0f), EAttachLocation::KeepRelativeOffset, true,
			Set->Volume, Pitch, 0.0f, Attenuation);
	}
}

void ATikiWarrior::MulticastTacticCue_Implementation(uint8 Cue, FVector_NetQuantize Location)
{
	const FTikiTacticSounds* Sounds = GetTacticSounds();
	if (!Sounds || !Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FVector Where(Location);
	switch (static_cast<ETikiTacticCue>(Cue))
	{
	case ETikiTacticCue::Block:
		// the claw parries: its clip, the clang, and the sparks where the blow hit it
		if (const FTikiTacticClips* Clips = GetTacticClips())
		{
			PlayOneShot(TacticClip(Clips->Block), 0.0f);
		}
		PlayTacticSound(Sounds->Block, false, &Where);
		if (UNiagaraSystem* Sparks = Rules->Bulwark.BlockSystem.Get())
		{
			const FVector Out = (Where - GetActorLocation()).GetSafeNormal(UE_SMALL_NUMBER, GetActorForwardVector());
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Sparks, Where, Out.Rotation(), FVector(Rules->Bulwark.BlockSystemScale));
		}
		break;
	case ETikiTacticCue::Seize:
		PlayTacticSound(Sounds->Seize, false, &Where);
		break;
	default:
		break;
	}
}

float ATikiWarrior::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// the claw guard takes a blow from the front first (T-275): blocked, nothing reaches the body (no blood, no limb,
	// no reaction); what gets through is a plain blow on its health
	if (HasAuthority() && Tactics && DamageAmount > 0.0f && !IsDead())
	{
		float Left = DamageAmount;
		if (Tactics->FilterDamage(Left, DamageEvent, EventInstigator, DamageCauser))
		{
			UIslanderHealthComponent* Life = GetHealth();
			if (Left <= 0.0f || !Life)
			{
				return 0.0f;
			}
			const float Dealt = Life->ApplyHealthDamage(Left, DamageCauser);
			if (!IsDead())
			{
				APawn* Attacker = EventInstigator ? EventInstigator->GetPawn() : nullptr;
				OnDamaged(Attacker ? Attacker : PawnOfCauser(DamageCauser), Dealt);
			}
			return Dealt;
		}
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ATikiWarrior::HearHowl(const FVector& Where, APawn* Suspect)
{
	if (!Rules || IsDead() || bBurrowing || Target.IsValid() || !IsInReachOfPost(Where))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	// it runs there; it attacks only if it then sees the foe (the stealth holds, T-262)
	Lead.Hunt(Suspect, Where, Now);
	LastTargetTime = Now;
	UE_LOG(LogMonkeTactics, Log, TEXT("%s hears the howl: runs to look at %s"), *GetName(), *Where.ToCompactString());
}

bool ATikiWarrior::CanStartStrike(double Now) const
{
	return Rules && Now - LastAttackTime >= Rules->AttackCooldown;
}

void ATikiWarrior::BeginStrike(APawn* Victim, double Now)
{
	if (!Victim || !Rules)
	{
		return;
	}
	GetCharacterMovement()->StopMovementImmediately();
	FaceToward(Victim->GetActorLocation());
	LastAttackTime = Now;
	PendingStrikeTime = Now + Rules->AttackWindup;
	PlayAction(ETikiAction::Attack, static_cast<uint8>(FMath::RandRange(0, Anims.AttackVariants.Num())));
}

void ATikiWarrior::SetArmed(bool bInArmed)
{
	bArmed = bInArmed && Rules && Rules->Musket.Mesh;
	ShotsLeft = bArmed ? FMath::Max(Rules->Musket.Shots, 0) : 0;
	ReloadedTime = 0.0;
}

void ATikiWarrior::OnRep_Armed()
{
	ApplyArmed();
}

void ATikiWarrior::ApplyArmed()
{
	if (!bArmed || MusketMesh || !Rules || !Rules->Musket.Mesh)
	{
		return;
	}
	const FTikiMusketRules& Musket = Rules->Musket;
	MusketMesh = NewObject<UStaticMeshComponent>(this, TEXT("TikiMusket"));
	MusketMesh->SetStaticMesh(Musket.Mesh);
	MusketMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	MusketMesh->SetupAttachment(GetMesh(), Musket.Bone);
	// the musket lies along its own weapon (the weapon bone: the spear's axis), held by the hand; the bones of the tiki
	// carry a scale of 100, so the grip is placed in the world and expressed against the hand, scale included
	const FTransform Hand = GetMesh()->GetSocketTransform(Musket.Bone, RTS_World);
	const bool bWeaponBone = !Musket.HiddenWeaponBone.IsNone() && GetMesh()->GetBoneIndex(Musket.HiddenWeaponBone) != INDEX_NONE;
	const FTransform Holder = bWeaponBone ? GetMesh()->GetSocketTransform(Musket.HiddenWeaponBone, RTS_World) : Hand;
	const FTransform InWorld = Musket.Grip * FTransform(Holder.GetRotation(), Holder.GetLocation());
	MusketMesh->SetRelativeTransform(InWorld.GetRelativeTransform(Hand));
	MusketMesh->RegisterComponent();
	// its own spear or club is put away while it carries the musket
	if (bWeaponBone)
	{
		GetMesh()->HideBoneByName(Musket.HiddenWeaponBone, PBO_None);
	}
}

FVector ATikiWarrior::GetMuzzle() const
{
	if (MusketMesh && Rules)
	{
		return MusketMesh->GetComponentTransform().TransformPosition(Rules->Musket.MuzzleOffset);
	}
	return GetActorLocation() + GetActorForwardVector() * 60.0f + FVector(0.0f, 0.0f, 40.0f);
}

bool ATikiWarrior::HasLineOfShot(const APawn* Victim) const
{
	if (!Victim)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiMusketLine), false, this);
	Params.AddIgnoredActor(Victim);
	return !GetWorld()->LineTraceTestByChannel(GetMuzzle(), Victim->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f), ECC_Visibility, Params);
}

void ATikiWarrior::ShootNow()
{
	const FTikiMusketRules& Musket = Rules->Musket;
	APawn* Victim = Target.Get();
	ShotsLeft = FMath::Max(0, ShotsLeft - 1);
	ReloadedTime = GetWorld()->GetTimeSeconds() + Musket.ReloadSeconds;
	const FVector Muzzle = GetMuzzle();
	const FVector Aim = Victim ? Victim->GetActorLocation() + FVector(0.0f, 0.0f, 30.0f) : Muzzle + GetActorForwardVector() * Musket.MaxRangeCm;
	FVector Direction = (Aim - Muzzle).GetSafeNormal();
	float Chance = 0.0f;
	bool bHit = false;
	if (Victim && Hostility::IsLivingFoe(this, Victim) && HasLineOfShot(Victim))
	{
		// the chance falls with the distance, from HitChanceNear to HitChanceFar
		Chance = FMath::GetMappedRangeValueClamped(FVector2f(Musket.HitChanceNearCm, FMath::Max(Musket.HitChanceFarCm, Musket.HitChanceNearCm + 1.0f)),
			FVector2f(Musket.HitChanceNear, Musket.HitChanceFar), static_cast<float>(FVector::Dist(Muzzle, Aim)));
		bHit = FMath::FRand() < Chance;
	}
	if (bHit)
	{
		const FHitResult Hit(Victim, nullptr, Aim, -Direction);
		UGameplayStatics::ApplyPointDamage(Victim, Musket.Damage, Direction, Hit, GetController(), this, nullptr);
	}
	else
	{
		// the ball passes beside the target
		const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
		const FVector Miss = Aim + Side * (FMath::FRandRange(-1.0f, 1.0f) * Musket.MissSpreadCm)
			+ FVector(0.0f, 0.0f, FMath::FRandRange(-0.3f, 0.6f) * Musket.MissSpreadCm);
		Direction = (Miss - Muzzle).GetSafeNormal();
	}
	MulticastMusketShot(Muzzle, Direction);
	UE_LOG(Logmonke_island, Log, TEXT("TikiWarrior %s: fires at %s, %s (chance %.0f %%), %d ball(s) left"), *GetName(), Victim ? *Victim->GetName() : TEXT("nothing"),
		bHit ? TEXT("hit") : TEXT("missed"), Chance * 100.0f, ShotsLeft);
}

void ATikiWarrior::MulticastMusketShot_Implementation(FVector_NetQuantize Muzzle, FVector_NetQuantizeNormal Direction)
{
	if (!Rules || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FTikiMusketRules& Musket = Rules->Musket;
	if (Musket.ShotSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Musket.ShotSound, Muzzle, 1.0f, 1.0f, 0.0f, Musket.ShotAttenuation);
	}
	if (!Musket.FlashMesh)
	{
		return;
	}
	if (!MusketFlash)
	{
		MusketFlash = NewObject<UStaticMeshComponent>(this, TEXT("TikiMusketFlash"));
		MusketFlash->SetStaticMesh(Musket.FlashMesh);
		MusketFlash->SetMaterial(0, Musket.FlashMaterial);
		MusketFlash->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		MusketFlash->SetCastShadow(false);
		MusketFlash->SetAbsolute(true, true, true);
		MusketFlash->RegisterComponent();
	}
	// the flame of a 1 m sphere stretched along the shot, its back at the muzzle
	const FVector Forward = FVector(Direction).GetSafeNormal();
	MusketFlash->SetWorldLocationAndRotation(FVector(Muzzle) + Forward * (50.0 * Musket.FlashScale.X), Forward.Rotation());
	MusketFlash->SetWorldScale3D(Musket.FlashScale);
	MusketFlash->SetVisibility(true);
	GetWorldTimerManager().SetTimer(FlashTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (MusketFlash)
		{
			MusketFlash->SetVisibility(false);
		}
	}), FMath::Max(Musket.FlashSeconds, 0.01f), false);
}

void ATikiWarrior::InitWarrior(ETikiWarriorRole InRole, APawn* InTarget, const FVector& InLeapDirection)
{
	WarriorRole = InRole;
	Target = InTarget;
	LeapDirection = InLeapDirection.GetSafeNormal2D();
	LastTargetTime = GetWorld()->GetTimeSeconds();
	// called against a player (an alarm, an ambush): it knows where that player stands now
	MarkSeen(InTarget);
}

void ATikiWarrior::Engage(APawn* InTarget)
{
	if (Hostility::IsLivingFoe(this, InTarget) && !IsDead())
	{
		Target = InTarget;
		LastTargetTime = GetWorld()->GetTimeSeconds();
		MarkSeen(InTarget);
	}
}

void ATikiWarrior::HearNoise(const FStealthNoise& Noise)
{
	if (!IsDead() && Lead.TakeNoise(Noise))
	{
		LastTargetTime = GetWorld()->GetTimeSeconds();
		UE_LOG(LogMonkeStealth, Log, TEXT("TikiWarrior %s: heard %s at %s, goes to look"), *GetName(), *GetNameSafe(Noise.Instigator.Get()),
			*Noise.Location.ToCompactString());
	}
}

void ATikiWarrior::MarkSeen(const AActor* Foe)
{
	if (Foe)
	{
		TargetSeenAt = Foe->GetActorLocation();
		TargetSeenTime = GetWorld()->GetTimeSeconds();
	}
}

bool ATikiWarrior::TrackTarget(APawn* Victim, double Now)
{
	bSeesTarget = CanSpot(Victim, Rules->LoseRadius);
	if (bSeesTarget)
	{
		MarkSeen(Victim);
		return true;
	}
	// its noises tell where it is (a player running at night): followed there
	const UStealthRules& StealthRules = UStealthSubsystem::RulesOf(this);
	FStealthNoise Noise;
	if (Listen(StealthRules.WarriorHearingScale, Noise) && Noise.Instigator.Get() == Victim)
	{
		TargetSeenAt = Noise.Location;
		TargetSeenTime = Now;
		Lead.NoiseTime = FMath::Max(Lead.NoiseTime, Noise.Time);
		return true;
	}
	return Now - TargetSeenTime <= StealthRules.LoseSightGraceSeconds;
}

bool ATikiWarrior::IsInReachOfPost(const FVector& Location) const
{
	const bool bHasPost = (WarriorRole == ETikiWarriorRole::Guardian || WarriorRole == ETikiWarriorRole::Ambusher) && PostRadius > 0.0f;
	return !bHasPost || FVector::Dist2D(Location, Post) <= Rules->LoseRadius;
}

void ATikiWarrior::Investigate(double Now)
{
	const UStealthRules& StealthRules = UStealthSubsystem::RulesOf(this);
	if (!Lead.bArrived)
	{
		// after a foe it hunted it runs, to a noise it walks (an alert walk); a place it cannot reach ends the way
		const float Left = MoveToward(Lead.Location, Lead.bHunting ? Rules->WarriorRunSpeed : Rules->WarriorWalkSpeed);
		if (Left > StealthRules.ArriveRadiusCm && !UpdateStuck(true))
		{
			return;
		}
		Lead.bArrived = true;
		NextSearchTurnTime = Now;
		GetCharacterMovement()->StopMovementImmediately();
	}
	// there: it looks around, toward a new direction now and then
	if (Now >= NextSearchTurnTime)
	{
		NextSearchTurnTime = Now + StealthRules.SearchTurnSeconds;
		const float Angle = FMath::FRandRange(0.0f, 2.0f * UE_PI);
		FaceToward(GetActorLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 200.0f);
	}
}

void ATikiWarrior::Burrow()
{
	if (bBurrowing || IsDead())
	{
		return;
	}
	bBurrowing = true;
	PendingStrikeTime = 0.0;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlayAction(ETikiAction::Hit);
	SetLifeSpan(2.0f);
}

void ATikiWarrior::ServerThink(float DeltaSeconds)
{
	if (bBurrowing)
	{
		// sinks into the sand; the replicated movement carries it to the clients
		AddActorWorldOffset(FVector(0.0f, 0.0f, -80.0f * DeltaSeconds));
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();

	// leap out of the bush toward the target
	if (!LeapDirection.IsNearlyZero())
	{
		PlayAction(ETikiAction::Emerge);
		LaunchCharacter(LeapDirection * Rules->EmergeLeapSpeed + FVector(0.0f, 0.0f, Rules->EmergeLeapSpeed), true, true);
		LeapDirection = FVector::ZeroVector;
		return;
	}

	// blow in progress: it lands after the visible preparation
	if (PendingStrikeTime > 0.0)
	{
		if (Now >= PendingStrikeTime)
		{
			PendingStrikeTime = 0.0;
			StrikeNow();
		}
		return;
	}
	// aimed shot in progress: it follows its target, then fires (T-214)
	if (PendingShotTime > 0.0)
	{
		if (IsMaimed() || IsDead())
		{
			PendingShotTime = 0.0;
		}
		else if (Now >= PendingShotTime)
		{
			PendingShotTime = 0.0;
			ShootNow();
		}
		else if (const APawn* Aimed = Target.Get())
		{
			FaceToward(Aimed->GetActorLocation());
		}
		return;
	}
	// a move of its role that runs to its end: a heavy blow, a grab, a cast, a howl, a flight (T-275)
	if (Tactics && Tactics->ThinkBusy(Now, DeltaSeconds))
	{
		return;
	}
	if (IsActionPlaying())
	{
		return;
	}

	// keep or find a target; a guardian also gives up a player who led it too far from its village
	const bool bHasPost = (WarriorRole == ETikiWarriorRole::Guardian || WarriorRole == ETikiWarriorRole::Ambusher) && PostRadius > 0.0f;
	const bool bWasInCombat = bInCombat;
	APawn* Victim = Target.Get();
	// its prey down: a victory; out of reach in the water: a taunt from the shore
	if (Victim && !Hostility::IsLivingFoe(this, Victim))
	{
		PlayAction(ETikiAction::Victory);
	}
	else if (Victim && IsOutOfReachInWater(Victim) && Now - LastTauntTime >= Rules->TauntInterval)
	{
		LastTauntTime = Now;
		FaceToward(Victim->GetActorLocation());
		PlayAction(ETikiAction::Taunt);
	}
	if (Victim && (!Hostility::IsLivingFoe(this, Victim) || IsOutOfReachInWater(Victim) || FVector::Dist(Victim->GetActorLocation(), GetActorLocation()) > Rules->LoseRadius
		|| (bHasPost && FVector::Dist2D(Victim->GetActorLocation(), Post) > Rules->LoseRadius)))
	{
		Victim = nullptr;
		Target.Reset();
	}
	// a foe it no longer sees (a crouched player, the night: T-262) is followed a moment, then looked for where it was
	// seen or heard last
	if (Victim && !TrackTarget(Victim, Now))
	{
		UE_LOG(LogMonkeStealth, Log, TEXT("TikiWarrior %s: lost sight of %s, looks for it at %s"), *GetName(), *Victim->GetName(), *TargetSeenAt.ToCompactString());
		Lead.Hunt(Victim, TargetSeenAt, Now);
		Victim = nullptr;
		Target.Reset();
	}
	if (!Victim)
	{
		// the nearest living foe it sees, whatever its side (a player, a fort soldier, the giant crab); a defender only
		// takes back the one it hunts
		APawn* Spotted = nullptr;
		if (WarriorRole != ETikiWarriorRole::Defender)
		{
			Spotted = FindNearestFoe(Rules->AggroRadius, true);
		}
		else if (APawn* Hunted = Cast<APawn>(Lead.Suspect.Get()); Lead.bActive && Hostility::IsLivingFoe(this, Hunted) && CanSpot(Hunted, Rules->AggroRadius))
		{
			Spotted = Hunted;
		}
		if (Spotted && !IsOutOfReachInWater(Spotted))
		{
			Victim = Spotted;
			Target = Victim;
			bSeesTarget = true;
			MarkSeen(Victim);
			Lead.Clear();
			// a growl when it spots a foe, not every time it finds it again
			if (Now - LastRoarTime >= Rules->AggroVoiceCooldown)
			{
				LastRoarTime = Now;
				PlayAction(ETikiAction::Alert);
			}
		}
	}
	// no foe in sight: it listens; a noise of a player sends it there to look (T-262); a defender only follows the one
	// it hunts
	if (!Victim)
	{
		const bool bWasLooking = Lead.bActive;
		FStealthNoise Noise;
		if (Listen(UStealthSubsystem::RulesOf(this).WarriorHearingScale, Noise) && IsInReachOfPost(Noise.Location)
			&& (WarriorRole != ETikiWarriorRole::Defender || (bWasLooking && Noise.Instigator == Lead.Suspect)) && Lead.TakeNoise(Noise))
		{
			FaceToward(Noise.Location);
			if (!bWasLooking)
			{
				UE_LOG(LogMonkeStealth, Log, TEXT("TikiWarrior %s: hears %s at %.0f m, goes to look"), *GetName(), *GetNameSafe(Noise.Instigator.Get()),
					FVector::Dist(Noise.Location, GetActorLocation()) / 100.0);
				if (Now - LastRoarTime >= Rules->AggroVoiceCooldown)
				{
					LastRoarTime = Now;
					PlayAction(ETikiAction::Alert);
				}
			}
		}
	}

	bInCombat = Victim != nullptr;
	// its role hears of a foe after a time without one: spotted, struck by it, called against it (T-275)
	if (Tactics && Victim && !bWasInCombat)
	{
		Tactics->OnEngaged(Victim);
	}
	if (!Victim)
	{
		if (Tactics)
		{
			Tactics->OnNoFoe();
		}
		// something to look for: it goes there and looks around, then forgets (T-262)
		if (Lead.IsActive(Now, UStealthSubsystem::RulesOf(this).NoiseMemorySeconds))
		{
			Investigate(Now);
			return;
		}
		if (Lead.bActive)
		{
			UE_LOG(LogMonkeStealth, Log, TEXT("TikiWarrior %s: found nobody, forgets"), *GetName());
			Lead.Clear();
		}
		// a defender with nobody left to punish goes back into the ground, an ambusher into its bush
		if ((WarriorRole == ETikiWarriorRole::Defender && Now - LastTargetTime > 8.0)
			|| (WarriorRole == ETikiWarriorRole::Ambusher && Now - LastTargetTime > Rules->AmbushIdleTime))
		{
			Burrow();
			return;
		}
		// a guardian stays by its bush: it wanders around its post and walks back when it strayed
		if (bHasPost && TikiMath::IsOffPost(GetActorLocation(), Post, PostRadius))
		{
			IdleGoal = Post;
		}
		if (IdleGoal.IsZero() || MoveToward(IdleGoal, Rules->WarriorWalkSpeed) < 80.0f || UpdateStuck(true))
		{
			const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
			IdleGoal = bHasPost ? TikiMath::PointOnRing(Post, Angle, FMath::Sqrt(FMath::FRand()), 0.0f, PostRadius)
				: TikiMath::PointOnRing(GetActorLocation(), Angle, FMath::FRand(), 300.0f, 1200.0f);
		}
		return;
	}

	LastTargetTime = Now;
	const float Distance = static_cast<float>(FVector::Dist2D(Victim->GetActorLocation(), GetActorLocation()));
	// armed, with balls left, the arm holding the musket on: it shoots from a distance, standing to reload (T-214);
	// only at a foe it sees (T-262)
	const FTikiMusketRules& Musket = Rules->Musket;
	if (bSeesTarget && bArmed && ShotsLeft > 0 && !IsMaimed() && Distance >= Musket.MinRangeCm && Distance <= Musket.MaxRangeCm)
	{
		// its musket first: no guard nor stalking meanwhile (T-275)
		SetTacticRep(FTikiTacticRep());
		GetCharacterMovement()->StopMovementImmediately();
		FaceToward(Victim->GetActorLocation());
		if (Now >= ReloadedTime && HasLineOfShot(Victim))
		{
			PendingShotTime = Now + Musket.AimSeconds;
			PlayAction(ETikiAction::Alert);
			return;
		}
		if (Now < ReloadedTime)
		{
			return;
		}
	}
	if (MusketMesh && IsMaimed() && MusketMesh->IsVisible())
	{
		// the arm holding the musket is gone: the musket with it
		MusketMesh->SetVisibility(false);
	}
	// the tactics of its role fight the foe (T-275); a Brawler keeps the classic melee below
	if (Tactics && Tactics->ThinkEngaged(Victim, Distance, Now, DeltaSeconds))
	{
		return;
	}
	if (bSeesTarget && TikiMath::CanStartAttack(Distance, Rules->AttackRange, Now, LastAttackTime, Rules->AttackCooldown))
	{
		BeginStrike(Victim, Now);
		return;
	}
	// out of sight it runs where it saw or heard its foe last, not where the foe is (T-262)
	if (!bSeesTarget)
	{
		MoveToward(TargetSeenAt, Rules->WarriorRunSpeed);
	}
	else if (Distance > Rules->AttackRange * 0.7f)
	{
		MoveToward(Victim->GetActorLocation(), Rules->WarriorRunSpeed);
	}
	else
	{
		FaceToward(Victim->GetActorLocation());
	}
}

void ATikiWarrior::StrikeNow()
{
	// without the arm holding its weapon, it strikes with the other fist; its role may strike harder (T-275)
	const float Scale = (IsMaimed() ? Rules->MaimedDamageScale : 1.0f) * (Tactics ? Tactics->GetStrikeDamageScale() : 1.0f);
	StrikeWith(Rules->AttackReach, Rules->AttackRadius, Rules->AttackDamage * Scale, Rules->AttackKnockback);
}

int32 ATikiWarrior::StrikeWith(float Reach, float Radius, float Damage, float Knockback)
{
	const FVector Forward = GetActorForwardVector();
	const FVector Start = GetActorLocation() + Forward * GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector End = Start + Forward * Reach;

	TArray<FHitResult> Hits;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiStrike), false, this);
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(FMath::Max(Radius, 1.0f)), Params);

	TSet<AActor*> Done;
	for (const FHitResult& Hit : Hits)
	{
		APawn* Victim = Cast<APawn>(Hit.GetActor());
		if (!Hostility::IsLivingFoe(this, Victim) || Done.Contains(Victim))
		{
			continue;
		}
		Done.Add(Victim);
		UGameplayStatics::ApplyDamage(Victim, Damage, GetController(), this, nullptr);
		// the blow throws back what is no heavier than the warrior (the giant crab stands)
		ACharacter* VictimCharacter = Cast<ACharacter>(Victim);
		if (VictimCharacter && VictimCharacter->GetCharacterMovement() && VictimCharacter->GetCharacterMovement()->Mass <= GetCharacterMovement()->Mass)
		{
			VictimCharacter->LaunchCharacter(Forward * Knockback + FVector(0.0f, 0.0f, 150.0f), true, false);
		}
		UE_LOG(Logmonke_island, Log, TEXT("%s strikes %s: %.0f damage"), *GetName(), *Victim->GetName(), Damage);
	}
	return Done.Num();
}

void ATikiWarrior::OnDamaged(AActor* Causer, float Damage)
{
	PendingStrikeTime = 0.0;
	// whoever hurts it is hunted, whatever its side; it knows where the blow came from, hidden or not (T-262)
	if (APawn* Attacker = Cast<APawn>(Causer); Hostility::IsLivingFoe(this, Attacker))
	{
		Target = Attacker;
		LastTargetTime = GetWorld()->GetTimeSeconds();
		MarkSeen(Attacker);
		Lead.Clear();
	}
	// its role answers the blow: a stalker alone and wounded runs away, a cast or a grab may be cut short (T-275)
	if (Tactics)
	{
		Tactics->OnDamaged(Causer, Damage);
	}
}
