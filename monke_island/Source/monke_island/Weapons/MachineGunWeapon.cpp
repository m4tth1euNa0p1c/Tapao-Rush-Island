// monke_island

#include "Weapons/MachineGunWeapon.h"
#include "Weapons/MachineGunMath.h"
#include "Weapons/MachineGunRules.h"
#include "monke_island.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderMovementComponent.h"
#include "Sound/SoundBase.h"

namespace
{
	/** Rounds one frame fires at most: a hitch drops the rest instead of emptying half a belt at once */
	constexpr int32 MaxRoundsPerFrame = 3;

	/** The carrier's client stops showing a burst the server has not confirmed after this long (s; a press the server
	 *  saw during another gesture): a network tolerance, well above a round trip */
	constexpr double UnconfirmedBurstSeconds = 0.75;

	/** The systems of the steam blow along their local +X: up */
	const FRotator SteamUp(90.0f, 0.0f, 0.0f);

	/** The reload of the belt (the drum): Reload, or ReloadEmpty for an empty drum when the pack has it (the submachine
	 *  gun, T-335) */
	bool IsReloadGesture(EFirearmGesture Gesture)
	{
		return Gesture == EFirearmGesture::Reload || Gesture == EFirearmGesture::ReloadEmpty;
	}

	/** A looping sound on the gun, played and stopped by hand (its attenuation is the sound's own) */
	UAudioComponent* MakeGunAudio(AActor* Owner, USoundBase* Sound)
	{
		UAudioComponent* Audio = NewObject<UAudioComponent>(Owner);
		Audio->SetSound(Sound);
		Audio->bAutoActivate = false;
		Audio->bAutoDestroy = false;
		Audio->SetupAttachment(Owner->GetRootComponent());
		Audio->RegisterComponent();
		return Audio;
	}
}

AMachineGunWeapon::AMachineGunWeapon()
{
}

void AMachineGunWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMachineGunWeapon, GunState);
	DOREPLIFETIME_CONDITION(AMachineGunWeapon, ReserveRounds, COND_OwnerOnly);
}

void AMachineGunWeapon::BeginPlay()
{
	GunRules = Cast<UMachineGunRules>(Rules);
	if (Rules && !GunRules)
	{
		UE_LOG(Logmonke_island, Error, TEXT("Machine gun %s: %s is not a UMachineGunRules, the gun is disabled"), *GetName(), *Rules->GetName());
		Rules = nullptr;
	}
	if (HasAuthority() && GunRules)
	{
		const MachineGunMath::FBelt Start = MachineGunMath::StartingBelt(GunRules->MaxRounds, GunRules->BeltCapacity, GunRules->StartRounds);
		GunState.BeltRounds = static_cast<uint16>(Start.Belt);
		ReserveRounds = Start.Reserve;
		HeatTime = GetWorld()->GetTimeSeconds();
	}
	Super::BeginPlay();
}

void AMachineGunWeapon::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ApplyCarrySpeed(false);
	StopCosmetics();
	Super::EndPlay(EndPlayReason);
}

int32 AMachineGunWeapon::GetMaxRounds() const
{
	const UMachineGunRules* Gun = Cast<UMachineGunRules>(Rules);
	return Gun ? Gun->MaxRounds : 0;
}

void AMachineGunWeapon::SetRounds(int32 Loaded, int32 Reserve)
{
	const UMachineGunRules* Gun = Cast<UMachineGunRules>(Rules);
	const int32 Capacity = Gun ? Gun->BeltCapacity : 0;
	const int32 OnBelt = FMath::Clamp(Loaded, 0, Capacity);
	GunState.BeltRounds = static_cast<uint16>(OnBelt);
	// rounds beyond the belt go back into the boxes
	ReserveRounds = FMath::Max(0, Reserve) + FMath::Max(0, Loaded - OnBelt);
}

float AMachineGunWeapon::GetHeat() const
{
	return HasAuthority() ? ServerHeat : MachineGunMath::UnpackHeat(GunState.Heat);
}

// ---------------------------------------------------------------- equip, carry

void AMachineGunWeapon::OnEquippedChanged()
{
	if (!IsEquipped())
	{
		if (HasAuthority())
		{
			// put away with the trigger held: the crank stops (the base cuts the gesture)
			bTriggerHeld = false;
			bReloadAfterFireEnd = false;
			EndBurst(false);
		}
		bLocalTriggerHeld = false;
		StopCosmetics();
	}
	ApplyCarrySpeed(IsEquipped());
	Super::OnEquippedChanged();
}

void AMachineGunWeapon::OnAttachedToCarrier()
{
	Super::OnAttachedToCarrier();
	// a new carrier (picked up from the ground): its speed, the former one gets its own back
	ApplyCarrySpeed(IsEquipped());
}

void AMachineGunWeapon::ApplyCarrySpeed(bool bInHand)
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UIslanderMovementComponent* Move = Carrier ? Carrier->GetIslanderMovement() : nullptr;
	if (SlowedMovement.IsValid() && (!bInHand || SlowedMovement.Get() != Move))
	{
		SlowedMovement->SetHeldItemSpeedFactors(1.0f, 1.0f);
		SlowedMovement.Reset();
	}
	// set on every machine, so both sides of the movement prediction agree
	if (bInHand && Move && GunRules)
	{
		Move->SetHeldItemSpeedFactors(GunRules->WalkSpeedFactor, GunRules->SprintSpeedFactor);
		SlowedMovement = Move;
	}
}

bool AMachineGunWeapon::IsCarrierRunning() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	const UIslanderMovementComponent* Move = Carrier ? Carrier->GetIslanderMovement() : nullptr;
	return Move && Move->IsSprinting();
}

bool AMachineGunWeapon::IsCarrierMoving() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	return Carrier && Rules && Carrier->GetVelocity().Size2D() > Rules->SteadySpeedCmps;
}

// ---------------------------------------------------------------- trigger (carrier)

void AMachineGunWeapon::BindTrigger()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	UEnhancedInputComponent* Input = Carrier ? Cast<UEnhancedInputComponent>(Carrier->InputComponent) : nullptr;
	if (!Input || !FireAction || TriggerInput.Get() == Input)
	{
		return;
	}
	// the press also reaches the base's RequestFire (server side of the trigger); these show it here at once and let it go
	KeepInputBinding(Input, Input->BindAction(FireAction, ETriggerEvent::Started, this, &AMachineGunWeapon::HandleTriggerPressed).GetHandle());
	KeepInputBinding(Input, Input->BindAction(FireAction, ETriggerEvent::Completed, this, &AMachineGunWeapon::HandleTriggerReleased).GetHandle());
	KeepInputBinding(Input, Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &AMachineGunWeapon::HandleTriggerReleased).GetHandle());
	TriggerInput = Input;
}

void AMachineGunWeapon::OnInputBindingsRemoved()
{
	Super::OnInputBindingsRemoved();
	// the trigger bindings went with the others: bound again for the next carrier, nothing held any more
	TriggerInput.Reset();
	bLocalTriggerHeld = false;
}

void AMachineGunWeapon::HandleTriggerPressed()
{
	// keys of a former carrier never drive the item someone else now holds (T-288, network review R-37)
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	// the fire key is shared by the firearms (IMC_Musket): only the gun in hand listens
	if (!IsUsable() || !GunRules)
	{
		return;
	}
	bLocalTriggerHeld = true;
	// a client shows the crank starting before the server answers (the listen server starts it for real)
	if (!HasAuthority() && PredictsFire() && GunRules->FindClip(EFirearmGesture::FireStart))
	{
		PredictGesture(EFirearmGesture::FireStart, 0.0f);
	}
}

void AMachineGunWeapon::HandleTriggerReleased()
{
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	if (!bLocalTriggerHeld)
	{
		return;
	}
	const bool bWasFiring = IsFiringHere();
	bLocalTriggerHeld = false;
	if (!HasAuthority() && bWasFiring && GunRules && GunRules->FindClip(EFirearmGesture::FireEnd))
	{
		PredictGesture(EFirearmGesture::FireEnd, 0.0f);
	}
	RequestCeaseFire();
}

void AMachineGunWeapon::RequestCeaseFire()
{
	if (HasAuthority())
	{
		ServerCeaseFire_Implementation();
	}
	else if (IsCarrierLocallyControlled())
	{
		ServerCeaseFire();
	}
}

void AMachineGunWeapon::ServerCeaseFire_Implementation()
{
	// see AFirearmWeapon::ServerFire_Implementation
	if (!HasAuthority())
	{
		return;
	}
	bTriggerHeld = false;
	ServerTickGun(0.0f);
}

bool AMachineGunWeapon::PredictsFire() const
{
	if (!GunRules || !IsUsable() || GunState.bOverheated || GunState.BeltRounds == 0 || !MachineGunMath::AllowsFire(GetServerGesture()))
	{
		return false;
	}
	return GunRules->bCanFireWhileSprinting || !IsCarrierRunning();
}

bool AMachineGunWeapon::IsFiringHere() const
{
	// the carrier's client shows its own trigger at once; the server and the other machines the server's crank
	if (!HasAuthority() && IsCarrierLocallyControlled())
	{
		return bLocalTriggerHeld && PredictsFire();
	}
	return GunState.bFiring;
}

// ---------------------------------------------------------------- server

void AMachineGunWeapon::ServerHandleFire()
{
	if (!GunRules || !MachineGunMath::AllowsFire(GetServerGesture()))
	{
		// drawing, reloading, overheated (the clip), thrown away: the press does nothing
		return;
	}
	if (GunState.BeltRounds == 0)
	{
		// nothing in reserve either: the base already threw the gun away; rounds left: a new belt
		if (GunRules->bAutoReloadOnEmptyFire)
		{
			BeginReload();
		}
		return;
	}
	if (GunState.bOverheated)
	{
		return;
	}
	bTriggerHeld = true;
	// the first round leaves now, not at the next frame
	ServerTickGun(0.0f);
}

void AMachineGunWeapon::ServerHandleReload()
{
	BeginReload();
}

void AMachineGunWeapon::ServerTickGun(float DeltaSeconds)
{
	const UMachineGunRules* Gun = GunRules;
	const UWorld* World = GetWorld();
	if (!Gun || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();

	// the barrel cools once the fire stopped, put away too (the whole time since the last update)
	const float Step = static_cast<float>(FMath::Max(Now - HeatTime, 0.0));
	HeatTime = Now;
	SetHeat(MachineGunMath::Cool(ServerHeat, static_cast<float>(Now - LastRoundTime), Step, Gun->CoolingDelaySeconds, Gun->CoolingPerSecond));
	if (GunState.bOverheated && MachineGunMath::HasRecovered(ServerHeat, Gun->OverheatRecoverHeat) && GetServerGesture() != EFirearmGesture::Overheat)
	{
		GunState.bOverheated = false;
		UE_LOG(Logmonke_island, Log, TEXT("Machine gun %s: cooled down to %.2f, fires again"), *GetName(), ServerHeat);
	}

	// any other gesture (draw, reload, overheat, inspect, throw), a swim or a held object lets go of the trigger
	const bool bAllowed = IsUsable() && MachineGunMath::AllowsFire(GetServerGesture());
	if (!bAllowed)
	{
		bTriggerHeld = false;
	}
	const bool bCanFire = bTriggerHeld && !GunState.bOverheated && GunState.BeltRounds > 0 && (Gun->bCanFireWhileSprinting || !IsCarrierRunning());
	if (bCanFire && !GunState.bFiring)
	{
		BeginBurst(Now);
	}
	else if (!bCanFire && GunState.bFiring)
	{
		// FireEnd, unless another gesture already took over
		EndBurst(bAllowed);
	}

	ServerCrank = MachineGunMath::StepCrank(ServerCrank, GunState.bFiring, DeltaSeconds, Gun->SpinUpSeconds, Gun->SpinDownSeconds);
	if (!GunState.bFiring)
	{
		Bloom = MachineGunMath::RecoverBloom(Bloom, DeltaSeconds, Gun->BloomRecoveryPerSecond);
		return;
	}
	const float Rate = MachineGunMath::RoundsPerSecond(ServerCrank, Gun->MinCadenceRpm, Gun->MaxCadenceRpm);
	const int32 Rounds = MachineGunMath::AdvanceClock(ServerClock, DeltaSeconds, Rate, MaxRoundsPerFrame);
	for (int32 Index = 0; Index < Rounds && GunState.bFiring; ++Index)
	{
		FireRound(Now);
	}
}

void AMachineGunWeapon::BeginBurst(double Now)
{
	check(HasAuthority());
	GunState.bFiring = true;
	bReloadAfterFireEnd = false;
	// a round at once, unless the last one is younger than a round interval: tapping never beats the crank
	ServerClock = MachineGunMath::BurstStartClock(static_cast<float>(Now - LastRoundTime),
		MachineGunMath::RoundsPerSecond(1.0f, GunRules->MinCadenceRpm, GunRules->MaxCadenceRpm));
	if (!StartAction(EFirearmGesture::FireStart) && GetServerGesture() == EFirearmGesture::FireEnd)
	{
		// no FireStart clip: the crank loop comes back at once
		StopAction();
	}
	UE_LOG(Logmonke_island, Log, TEXT("Machine gun %s: %s fires (%d on the belt, heat %.2f)"), *GetName(), *GetNameSafe(GetCarrier()),
		GunState.BeltRounds, ServerHeat);
}

void AMachineGunWeapon::EndBurst(bool bPlayEnd)
{
	if (!GunState.bFiring)
	{
		return;
	}
	GunState.bFiring = false;
	UE_LOG(Logmonke_island, Log, TEXT("Machine gun %s: ceases fire (%d on the belt, %d in reserve, heat %.2f)"), *GetName(), GunState.BeltRounds,
		ReserveRounds, ServerHeat);
	if (bPlayEnd && !StartAction(EFirearmGesture::FireEnd) && GetServerGesture() == EFirearmGesture::FireStart)
	{
		// no FireEnd clip: a FireStart still playing is cut, the carry loops come back
		StopAction();
	}
	if (bReloadAfterFireEnd && GetServerGesture() != EFirearmGesture::FireEnd)
	{
		// no FireEnd to wait for: the new belt now
		bReloadAfterFireEnd = false;
		BeginReload();
	}
}

void AMachineGunWeapon::FireRound(double Now)
{
	const UMachineGunRules* Gun = GunRules;
	// this round spreads by the bloom so far: the first round of a burst is a steady one
	FireProjectiles(1);
	GunState.BeltRounds = static_cast<uint16>(FMath::Max(static_cast<int32>(GunState.BeltRounds) - 1, 0));
	LastRoundTime = Now;
	SetHeat(MachineGunMath::HeatAfterRound(ServerHeat, Gun->HeatPerRound));
	Bloom = MachineGunMath::BloomAfterRound(Bloom, Gun->BloomPerRound, Gun->MaxBloomDegrees);
	// the listen server draws its real rounds as they leave
	if (GetNetMode() != NM_DedicatedServer)
	{
		PlayRoundCosmetics();
	}
	if (MachineGunMath::IsOverheating(ServerHeat, Gun->OverheatHeat))
	{
		Overheat();
		return;
	}
	if (GunState.BeltRounds == 0)
	{
		// the belt ran dry: the crank stops, then the new belt when there is one
		bTriggerHeld = false;
		bReloadAfterFireEnd = Gun->bAutoReloadWhenBeltRunsOut && ReserveRounds > 0;
		EndBurst(true);
	}
}

void AMachineGunWeapon::Overheat()
{
	check(HasAuthority());
	GunState.bOverheated = true;
	bTriggerHeld = false;
	bReloadAfterFireEnd = false;
	EndBurst(false);
	StartAction(EFirearmGesture::Overheat);
	UE_LOG(Logmonke_island, Log, TEXT("Machine gun %s: overheated (%d on the belt), no fire until %.2f"), *GetName(), GunState.BeltRounds,
		GunRules->OverheatRecoverHeat);
}

bool AMachineGunWeapon::BeginReload()
{
	const UMachineGunRules* Gun = GunRules;
	const MachineGunMath::FBelt Belt{ GunState.BeltRounds, ReserveRounds };
	if (!Gun || !MachineGunMath::CanReload(Belt, Gun->BeltCapacity) || !MachineGunMath::AllowsReload(GetServerGesture()))
	{
		return false;
	}
	bTriggerHeld = false;
	bReloadAfterFireEnd = false;
	EndBurst(false);
	bBeltLoadedThisReload = false;
	UE_LOG(Logmonke_island, Log, TEXT("Machine gun %s: reload (%d on the belt, %d in reserve)"), *GetName(), GunState.BeltRounds, ReserveRounds);
	// an empty drum is let fall (ReloadEmpty, when the pack has it: the submachine gun), else it is swapped by hand
	const EFirearmGesture Gesture = GunState.BeltRounds == 0 && Gun->FindClip(EFirearmGesture::ReloadEmpty) ? EFirearmGesture::ReloadEmpty
		: EFirearmGesture::Reload;
	if (!StartAction(Gesture))
	{
		// no Reload clip: the belt is in at once
		LoadBelt();
	}
	return true;
}

void AMachineGunWeapon::LoadBelt()
{
	MachineGunMath::FBelt Belt{ GunState.BeltRounds, ReserveRounds };
	if (GunRules && MachineGunMath::LoadBelt(Belt, GunRules->BeltCapacity))
	{
		GunState.BeltRounds = static_cast<uint16>(Belt.Belt);
		ReserveRounds = Belt.Reserve;
		UE_LOG(Logmonke_island, Log, TEXT("Machine gun %s: new belt, %d rounds, %d in reserve"), *GetName(), Belt.Belt, Belt.Reserve);
	}
}

void AMachineGunWeapon::SetHeat(float NewHeat)
{
	ServerHeat = FMath::Clamp(NewHeat, 0.0f, 1.0f);
	const uint8 Packed = MachineGunMath::PackHeat(ServerHeat);
	if (GunState.Heat != Packed)
	{
		GunState.Heat = Packed;
	}
}

void AMachineGunWeapon::OnActionEvent(EFirearmGesture Gesture, FName Event)
{
	if (GunRules && IsReloadGesture(Gesture) && Event == GunRules->ReloadLoadedEvent && !bBeltLoadedThisReload)
	{
		LoadBelt();
		bBeltLoadedThisReload = true;
	}
}

void AMachineGunWeapon::OnActionFinished(EFirearmGesture Gesture)
{
	if (IsReloadGesture(Gesture))
	{
		// a Reload clip without its BeltIn event in the DA still loads, at its end
		if (!bBeltLoadedThisReload)
		{
			LoadBelt();
		}
		bBeltLoadedThisReload = false;
	}
	else if (Gesture == EFirearmGesture::FireEnd && bReloadAfterFireEnd)
	{
		bReloadAfterFireEnd = false;
		BeginReload();
	}
}

void AMachineGunWeapon::OnActionStopped(EFirearmGesture Gesture)
{
	if (IsReloadGesture(Gesture))
	{
		// cut before BeltIn: the old belt stays, nothing was taken from the boxes
		bBeltLoadedThisReload = false;
	}
	else if (Gesture == EFirearmGesture::FireEnd)
	{
		bReloadAfterFireEnd = false;
	}
}

float AMachineGunWeapon::GetGestureRate(const FFirearmClip& Clip) const
{
	if (const UMachineGunRules* Gun = GunRules)
	{
		if (Clip.Gesture == EFirearmGesture::FireLoop)
		{
			// one clip is one turn of the crank: RoundsPerCrankTurn rounds at full speed
			return MachineGunMath::CrankClipRate(Clip.Duration, MachineGunMath::RoundsPerSecond(1.0f, Gun->MinCadenceRpm, Gun->MaxCadenceRpm),
				Gun->RoundsPerCrankTurn);
		}
		if (IsReloadGesture(Clip.Gesture) && Gun->ReloadSeconds > 0.0f)
		{
			return MachineGunMath::StretchRate(Clip.Duration, Gun->ReloadSeconds);
		}
	}
	return Super::GetGestureRate(Clip);
}

EFirearmGesture AMachineGunWeapon::DesiredLoop() const
{
	// the crank turns while the gun fires here; the carry loops otherwise (Idle, Walk, Sprint)
	if (GunRules && IsFiringHere() && GunRules->FindClip(EFirearmGesture::FireLoop))
	{
		return EFirearmGesture::FireLoop;
	}
	return Super::DesiredLoop();
}

float AMachineGunWeapon::GetShotSpreadDegrees(bool bMoving) const
{
	const float Stance = Super::GetShotSpreadDegrees(bMoving);
	return GunRules ? MachineGunMath::Spread(Stance, Bloom, bMoving, GunRules->MovingBloomScale) : Stance;
}

// ---------------------------------------------------------------- per frame

void AMachineGunWeapon::Tick(float DeltaSeconds)
{
	// the server's gun first, so the gestures and the effects of this frame see its state
	if (HasAuthority() && GunRules && GetCarrier())
	{
		ServerTickGun(DeltaSeconds);
	}
	if (IsEquipped())
	{
		// every frame in hand: another heavy gun put away after this one was drawn (the inventory goes slot by slot)
		// gave the carrier its speed back
		ApplyCarrySpeed(true);
	}
	if (IsCarrierLocallyControlled())
	{
		BindTrigger();
		if (bLocalTriggerHeld && (!IsUsable() || !MachineGunMath::AllowsFire(GetServerGesture())))
		{
			// a reload, an overheat or a held object let go of the trigger (the server does the same): press again
			bLocalTriggerHeld = false;
		}
	}
	Super::Tick(DeltaSeconds);
}

void AMachineGunWeapon::TickCosmetics(float DeltaSeconds)
{
	const UMachineGunRules* Gun = GunRules;
	if (!Gun || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bFiring = IsEquipped() && IsFiringHere();
	if (bFiring != bBurstShownHere)
	{
		if (bFiring && !HasAuthority())
		{
			CosmeticClock = MachineGunMath::BurstStartClock(static_cast<float>(Now - LastCosmeticRoundTime),
				MachineGunMath::RoundsPerSecond(1.0f, Gun->MinCadenceRpm, Gun->MaxCadenceRpm));
			CosmeticBurstStart = Now;
		}
		OnBurstChangedHere(bFiring);
	}
	// the carrier's client: a burst the server never started (it saw the press during another gesture) stops here too
	if (bFiring && !HasAuthority() && IsCarrierLocallyControlled() && !GunState.bFiring && Now - CosmeticBurstStart > UnconfirmedBurstSeconds)
	{
		bLocalTriggerHeld = false;
	}

	// the rounds of a gun this machine does not own, and of the carrier's client: drawn at the cadence of the crank (the
	// listen server draws its real rounds in FireRound; the hits are the server's anyway)
	if (!HasAuthority())
	{
		CosmeticCrank = MachineGunMath::StepCrank(CosmeticCrank, bFiring, DeltaSeconds, Gun->SpinUpSeconds, Gun->SpinDownSeconds);
		if (bFiring)
		{
			const float Rate = MachineGunMath::RoundsPerSecond(CosmeticCrank, Gun->MinCadenceRpm, Gun->MaxCadenceRpm);
			const int32 Rounds = MachineGunMath::AdvanceClock(CosmeticClock, DeltaSeconds, Rate, MaxRoundsPerFrame);
			for (int32 Index = 0; Index < Rounds; ++Index)
			{
				PlayRoundCosmetics();
			}
		}
	}

	// the overheat seen here: its sound, and the vent when the Overheat clip has no event for it
	if (GunState.bOverheated != bOverheatShownHere)
	{
		bOverheatShownHere = GunState.bOverheated;
		if (bOverheatShownHere && IsEquipped())
		{
			if (Gun->OverheatSound)
			{
				UGameplayStatics::SpawnSoundAttached(Gun->OverheatSound, RootComponent);
			}
			const FFirearmClip* Clip = Gun->FindClip(EFirearmGesture::Overheat);
			if (!Clip || Clip->CountEvents(Gun->SteamVentEvent) == 0)
			{
				VentSteam();
			}
		}
	}

	UpdateBrass(DeltaSeconds, CasingComponents, Casings, 1.0f, Gun->CasingLifeSeconds, Gun->CasingBounce);
	UpdateBrass(DeltaSeconds, LinkComponents, Links, 0.55f, Gun->CasingLifeSeconds, Gun->CasingBounce);
	UpdateBrass(DeltaSeconds, BoxComponents, Boxes, 0.0f, Gun->DroppedBoxSeconds, 0.0f);
	UpdateHeatLook(DeltaSeconds);
	UpdateAudio(DeltaSeconds);
	UpdateRecoil(DeltaSeconds);
	UpdateBelt();
}

bool AMachineGunWeapon::HasCosmeticsRunning() const
{
	auto Flying = [](const FBrass& Item) { return Item.bActive; };
	return Casings.ContainsByPredicate(Flying) || Links.ContainsByPredicate(Flying) || Boxes.ContainsByPredicate(Flying)
		|| (SteamComponent && SteamComponent->IsActive());
}

// ---------------------------------------------------------------- cosmetics

void AMachineGunWeapon::OnCosmeticEvent(EFirearmGesture Gesture, FName Event)
{
	// the rounds are drawn at the cadence of the crank, never at the Fire events of its clips
	if (GunRules && Event == GunRules->FireEvent)
	{
		return;
	}
	// the sounds of the clips (cover, belt, box, charge, steam, draw)
	Super::OnCosmeticEvent(Gesture, Event);
	if (GunRules && Gesture == EFirearmGesture::Overheat && Event == GunRules->SteamVentEvent)
	{
		VentSteam();
	}
	if (GunRules && IsReloadGesture(Gesture) && Event == GunRules->DroppedBoxEvent)
	{
		DropBox();
	}
}

void AMachineGunWeapon::PlayRoundCosmetics()
{
	const AIslanderCharacter* Carrier = GetCarrier();
	if (!GunRules || !Carrier || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	LastCosmeticRoundTime = GetWorld()->GetTimeSeconds();
	// the shot of the base: the round's report, the flash and its light (flickering at the cadence), the muzzle blast
	Super::OnCosmeticEvent(EFirearmGesture::Fire, GunRules->FireEvent);
	EjectBrass(false);
	EjectBrass(true);
	if (!IsCarrierLocallyControlled())
	{
		return;
	}
	// the carrier: the muzzle climbs and wanders sideways (a random walk to fight), the camera jolts; more on the move
	const bool bMoving = IsCarrierMoving();
	RecoilDrift = FMath::Clamp(RecoilDrift * 0.8f + FMath::FRandRange(-1.0f, 1.0f), -1.5f, 1.5f);
	PendingRecoil += FVector2D(GunRules->RecoilPitchPerRound, GunRules->RecoilYawPerRound * RecoilDrift) * (bMoving ? GunRules->MovingRecoilScale : 1.0f);
	const APlayerController* Controller = Cast<APlayerController>(Carrier->GetController());
	if (GunRules->FireCameraShake && Controller && Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager->StartCameraShake(GunRules->FireCameraShake,
			GunRules->FireShakeScale * (bMoving ? GunRules->MovingShakeScale : 1.0f));
	}
}

void AMachineGunWeapon::EjectBrass(bool bLink)
{
	const UMachineGunRules* Gun = GunRules;
	UStaticMesh* Mesh = bLink ? Gun->LinkMesh.Get() : Gun->CasingMesh.Get();
	const AIslanderCharacter* Carrier = GetCarrier();
	const USkeletalMeshComponent* Weapon = GetVisibleWeaponMesh();
	if (!Mesh || !Carrier || !Weapon)
	{
		return;
	}
	const bool bPort = !Gun->EjectSocket.IsNone() && Weapon->DoesSocketExist(Gun->EjectSocket);
	const FTransform Port = bPort ? Weapon->GetSocketTransform(Gun->EjectSocket) : Weapon->GetComponentTransform();
	// nobody sees brass far away
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (Camera && FVector::DistSquared(Camera->GetCameraLocation(), Port.GetLocation()) > FMath::Square(Gun->EffectsCullDistance))
	{
		return;
	}
	// out of the port toward the second socket of the pack (along the port's X without it, the carrier's right
	// without either), in a cone, carried along with the carrier
	FVector Axis = FRotationMatrix(FRotator(0.0f, GetCarrierAim().Rotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y);
	if (bPort && !Gun->EjectDirectionSocket.IsNone() && Weapon->DoesSocketExist(Gun->EjectDirectionSocket))
	{
		Axis = (Weapon->GetSocketLocation(Gun->EjectDirectionSocket) - Port.GetLocation()).GetSafeNormal(UE_SMALL_NUMBER, Axis);
	}
	else if (bPort)
	{
		Axis = Port.GetUnitAxis(EAxis::X);
	}
	const FVector Direction = FMath::VRandCone(Axis, FMath::DegreesToRadians(Gun->CasingSpreadDegrees));
	const FVector Velocity = Direction * (bLink ? Gun->LinkSpeedCmps : Gun->CasingSpeedCmps) * FMath::FRandRange(0.85f, 1.15f) + Carrier->GetVelocity();
	const FRotator Spin(FMath::FRandRange(-900.0f, 900.0f), FMath::FRandRange(-600.0f, 600.0f), FMath::FRandRange(-900.0f, 900.0f));
	LaunchBrass(Mesh, bLink ? Gun->MaxLinks : Gun->MaxCasings, bLink ? LinkComponents : CasingComponents, bLink ? Links : Casings,
		Port.GetLocation(), Direction.Rotation() + FRotator(0.0f, 90.0f, 0.0f), Velocity, Spin);
}

void AMachineGunWeapon::DropBox()
{
	const UMachineGunRules* Gun = GunRules;
	const AIslanderCharacter* Carrier = GetCarrier();
	const USkeletalMeshComponent* Weapon = GetVisibleWeaponMesh();
	if (!Gun || !Gun->DroppedBoxMesh || !Carrier || !Weapon || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	const FTransform From = !Gun->DroppedBoxSocket.IsNone() && Weapon->DoesSocketExist(Gun->DroppedBoxSocket)
		? Weapon->GetSocketTransform(Gun->DroppedBoxSocket) : Weapon->GetComponentTransform();
	// let go: it drops, a little sideways, with the carrier's own speed
	const FVector Velocity = Carrier->GetVelocity() + FVector(0.0f, 0.0f, -60.0f)
		+ FRotationMatrix(FRotator(0.0f, GetCarrierAim().Rotation().Yaw, 0.0f)).GetUnitAxis(EAxis::Y) * -40.0f;
	const FRotator Spin(FMath::FRandRange(-60.0f, 60.0f), FMath::FRandRange(-30.0f, 30.0f), FMath::FRandRange(-90.0f, 90.0f));
	LaunchBrass(Gun->DroppedBoxMesh, 2, BoxComponents, Boxes, From.GetLocation(), From.Rotator(), Velocity, Spin);
}

void AMachineGunWeapon::LaunchBrass(UStaticMesh* Mesh, int32 Max, TArray<TObjectPtr<UStaticMeshComponent>>& Components, TArray<FBrass>& Items,
	const FVector& Location, const FRotator& Rotation, const FVector& Velocity, const FRotator& Spin)
{
	if (!Mesh || Max <= 0)
	{
		return;
	}
	int32 Index = Items.IndexOfByPredicate([](const FBrass& Item) { return !Item.bActive; });
	if (Index == INDEX_NONE && Components.Num() < Max)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCastShadow(false);
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->RegisterComponent();
		Index = Components.Add(Component);
		Items.AddDefaulted();
	}
	if (Index == INDEX_NONE)
	{
		// the pool is full: the oldest one goes
		Index = 0;
		for (int32 Other = 1; Other < Items.Num(); ++Other)
		{
			Index = Items[Other].Age > Items[Index].Age ? Other : Index;
		}
	}
	FBrass& Item = Items[Index];
	Item.Velocity = Velocity;
	Item.Spin = Spin;
	Item.Age = 0.0f;
	Item.bActive = true;
	Item.bResting = false;
	Item.bBounced = false;
	UStaticMeshComponent* Component = Components[Index];
	Component->SetWorldLocationAndRotation(Location, Rotation);
	Component->SetVisibility(true);
}

void AMachineGunWeapon::UpdateBrass(float DeltaSeconds, TArray<TObjectPtr<UStaticMeshComponent>>& Components, TArray<FBrass>& Items, float ClinkVolume,
	float LifeSeconds, float Bounce)
{
	const UMachineGunRules* Gun = GunRules;
	UWorld* World = GetWorld();
	if (Items.IsEmpty() || !Gun || !World)
	{
		return;
	}
	const float Gravity = World->GetGravityZ();
	const double Now = World->GetTimeSeconds();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MachineGunBrass), false, this);
	Params.AddIgnoredActor(GetCarrier());
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		FBrass& Item = Items[Index];
		UStaticMeshComponent* Component = Components.IsValidIndex(Index) ? Components[Index].Get() : nullptr;
		if (!Item.bActive || !Component)
		{
			continue;
		}
		Item.Age += DeltaSeconds;
		if (Item.Age >= LifeSeconds)
		{
			Item.bActive = false;
			Component->SetVisibility(false);
			continue;
		}
		if (Item.bResting)
		{
			continue;
		}
		// falls and tumbles; one short trace per frame finds the ground
		Item.Velocity.Z += Gravity * DeltaSeconds;
		const FVector From = Component->GetComponentLocation();
		const FVector To = From + Item.Velocity * DeltaSeconds;
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
		{
			Component->SetWorldLocation(To);
			Component->AddWorldRotation(Item.Spin * DeltaSeconds);
			continue;
		}
		if (!Item.bBounced && ClinkVolume > 0.0f && Gun->CasingLandSound && Now - LastCasingSoundTime >= Gun->CasingSoundMinInterval)
		{
			LastCasingSoundTime = Now;
			UGameplayStatics::PlaySoundAtLocation(this, Gun->CasingLandSound, Hit.ImpactPoint, ClinkVolume, FMath::FRandRange(0.92f, 1.08f));
		}
		if (!Item.bBounced && Bounce > 0.0f)
		{
			// one bounce off the ground, then it settles where it lands next
			Item.Velocity = FMath::GetReflectionVector(Item.Velocity, Hit.ImpactNormal) * Bounce;
			Item.Spin *= 0.5f;
			Item.bBounced = true;
			Component->SetWorldLocation(Hit.ImpactPoint + Hit.ImpactNormal * 0.5f);
			continue;
		}
		// at rest on the ground, upright about the normal (a case lies on its side)
		const FRotator Current = Component->GetComponentRotation();
		Component->SetWorldLocationAndRotation(Hit.ImpactPoint + Hit.ImpactNormal * 0.4f, FRotator(0.0f, Current.Yaw, Bounce > 0.0f ? 90.0f : 0.0f));
		Item.bResting = true;
	}
}

void AMachineGunWeapon::SetGlow(USkeletalMeshComponent* Mesh, float Value) const
{
	// only the slots with a glow mask (the jacket): the other parts share the master material without one
	for (const FName& Slot : Mesh->GetMaterialSlotNames())
	{
		const FString SlotName = Slot.ToString();
		if (!GunRules->HeatMaterialSlots.IsEmpty()
			&& !GunRules->HeatMaterialSlots.ContainsByPredicate([&SlotName](const FString& Part) { return SlotName.Contains(Part); }))
		{
			continue;
		}
		const int32 Index = Mesh->GetMaterialIndex(Slot);
		if (UMaterialInstanceDynamic* Material = Index != INDEX_NONE ? Mesh->CreateDynamicMaterialInstance(Index) : nullptr)
		{
			Material->SetScalarParameterValue(GunRules->HeatParameterName, Value);
		}
	}
}

float& AMachineGunWeapon::AppliedGlowFor(const USkeletalMeshComponent* Mesh)
{
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (GlowMeshes[Index].Get() == Mesh)
		{
			return GlowValues[Index];
		}
	}
	const int32 Free = GlowMeshes[0].IsValid() ? 1 : 0;
	GlowMeshes[Free] = Mesh;
	GlowValues[Free] = 0.0f;
	return GlowValues[Free];
}

void AMachineGunWeapon::UpdateHeatLook(float DeltaSeconds)
{
	const UMachineGunRules* Gun = GunRules;
	USkeletalMeshComponent* Mesh = GetVisibleWeaponMesh();
	if (!Gun || !Mesh)
	{
		return;
	}
	DisplayedHeat = FMath::FInterpTo(DisplayedHeat, GetHeat(), DeltaSeconds, 4.0f);
	const bool bShown = IsEquipped() && !Mesh->bHiddenInGame;
	const float Glow = bShown ? MachineGunMath::GlowAlpha(DisplayedHeat, Gun->GlowStartHeat) : 0.0f;

	// the barrel and the jacket glow (set only when it changes: no material instance while the gun is cold)
	if (!Gun->HeatParameterName.IsNone())
	{
		float& Applied = AppliedGlowFor(Mesh);
		if (FMath::Abs(Glow - Applied) > 0.004f || (Glow <= 0.0f && Applied > 0.0f))
		{
			SetGlow(Mesh, Glow * Gun->HeatParameterMax);
			Applied = Glow;
		}
	}

	// the glowing jacket lights the hands and the ground a little
	const bool bLight = Gun->HeatLightIntensity > 0.0f && Glow > 0.02f;
	if (bLight && !HeatLight)
	{
		HeatLight = NewObject<UPointLightComponent>(this, TEXT("HeatLight"));
		HeatLight->SetCastShadows(false);
		HeatLight->SetLightColor(Gun->HeatLightColor);
		HeatLight->SetAttenuationRadius(Gun->HeatLightRadius);
		HeatLight->RegisterComponent();
	}
	if (HeatLight)
	{
		if (bLight)
		{
			const FName Socket = Mesh->DoesSocketExist(Gun->JacketSocket) ? Gun->JacketSocket : NAME_None;
			if (HeatLight->GetAttachParent() != Mesh || HeatLight->GetAttachSocketName() != Socket)
			{
				HeatLight->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
			}
			HeatLight->SetIntensity(Gun->HeatLightIntensity * Glow);
		}
		if (HeatLight->IsVisible() != bLight)
		{
			HeatLight->SetVisibility(bLight);
		}
	}

	// wisps out of the steam cap of a hot gun: a one-shot system restarts while hot, a looping one keeps going
	const bool bSteam = Gun->SteamSystem && bShown && (DisplayedHeat >= Gun->SteamStartHeat || GunState.bOverheated);
	if (bSteam && !SteamComponent)
	{
		SteamComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(Gun->SteamSystem, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, false, false, ENCPoolMethod::None, false);
		if (SteamComponent)
		{
			SteamComponent->SetUsingAbsoluteRotation(true);
		}
	}
	if (!SteamComponent)
	{
		return;
	}
	if (!bSteam)
	{
		if (SteamComponent->IsActive())
		{
			SteamComponent->Deactivate();
		}
		return;
	}
	const FName Cap = Mesh->DoesSocketExist(Gun->SteamCapSocket) ? Gun->SteamCapSocket : NAME_None;
	if (SteamComponent->GetAttachParent() != Mesh || SteamComponent->GetAttachSocketName() != Cap)
	{
		SteamComponent->AttachToComponent(Mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Cap);
	}
	SteamComponent->SetWorldRotation(SteamUp);
	if (!SteamComponent->IsActive())
	{
		SteamComponent->Activate(true);
	}
}

void AMachineGunWeapon::VentSteam()
{
	const UMachineGunRules* Gun = GunRules;
	USkeletalMeshComponent* Mesh = GetVisibleWeaponMesh();
	UNiagaraSystem* System = Gun ? (Gun->SteamVentSystem ? Gun->SteamVentSystem.Get() : Gun->SteamSystem.Get()) : nullptr;
	if (!System || !Mesh || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// out of the cap and along the jacket; one burst at the gun when it has neither
	TArray<FName, TInlineAllocator<2>> Sockets;
	for (const FName Socket : { Gun->SteamCapSocket, Gun->JacketSocket })
	{
		if (!Socket.IsNone() && Mesh->DoesSocketExist(Socket))
		{
			Sockets.AddUnique(Socket);
		}
	}
	if (Sockets.IsEmpty())
	{
		Sockets.Add(NAME_None);
	}
	for (const FName Socket : Sockets)
	{
		if (UNiagaraComponent* Burst = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Mesh, Socket, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true))
		{
			Burst->SetUsingAbsoluteRotation(true);
			Burst->SetWorldRotation(SteamUp);
		}
	}
}

void AMachineGunWeapon::OnBurstChangedHere(bool bFiring)
{
	bBurstShownHere = bFiring;
	const UMachineGunRules* Gun = GunRules;
	if (!Gun)
	{
		return;
	}
	if (bFiring)
	{
		if (Gun->BurstStartSound)
		{
			UGameplayStatics::SpawnSoundAttached(Gun->BurstStartSound, RootComponent);
		}
		if (Gun->MechanismLoopSound)
		{
			if (!MechanismAudio)
			{
				MechanismAudio = MakeGunAudio(this, Gun->MechanismLoopSound);
			}
			MechanismAudio->SetPitchMultiplier(Gun->MechanismPitchAtSpinUp);
			MechanismAudio->FadeIn(0.04f, 1.0f);
		}
		return;
	}
	if (MechanismAudio && MechanismAudio->IsPlaying())
	{
		MechanismAudio->FadeOut(0.12f, 0.0f);
	}
	if (Gun->BurstTailSound)
	{
		UGameplayStatics::SpawnSoundAttached(Gun->BurstTailSound, RootComponent);
	}
}

void AMachineGunWeapon::UpdateAudio(float DeltaSeconds)
{
	const UMachineGunRules* Gun = GunRules;
	if (!Gun)
	{
		return;
	}
	// the clatter of the crank speeds up with it
	if (bBurstShownHere && MechanismAudio && MechanismAudio->IsPlaying())
	{
		const float Crank = HasAuthority() ? ServerCrank : CosmeticCrank;
		MechanismAudio->SetPitchMultiplier(FMath::Lerp(Gun->MechanismPitchAtSpinUp, 1.0f, FMath::Clamp(Crank, 0.0f, 1.0f)));
	}
	// the jacket simmers while hot, loudest overheated
	const float Simmer = !Gun->SteamLoopSound || !IsEquipped() ? 0.0f
		: GunState.bOverheated ? 1.0f : FMath::Clamp((DisplayedHeat - Gun->SteamStartHeat) / FMath::Max(1.0f - Gun->SteamStartHeat, 0.01f), 0.0f, 1.0f);
	if (Simmer > 0.02f)
	{
		if (!SteamAudio)
		{
			SteamAudio = MakeGunAudio(this, Gun->SteamLoopSound);
		}
		SteamAudio->SetVolumeMultiplier(Simmer * Gun->SteamLoopVolume);
		if (!SteamAudio->IsPlaying())
		{
			SteamAudio->FadeIn(0.3f, Simmer * Gun->SteamLoopVolume);
		}
	}
	else if (SteamAudio && SteamAudio->IsPlaying())
	{
		SteamAudio->FadeOut(0.4f, 0.0f);
	}
}

void AMachineGunWeapon::UpdateRecoil(float DeltaSeconds)
{
	if (!GunRules || PendingRecoil.IsNearlyZero(0.001f))
	{
		return;
	}
	const AIslanderCharacter* Carrier = GetCarrier();
	AController* Controller = Carrier ? Carrier->GetController() : nullptr;
	const FVector2D Applied = PendingRecoil * FMath::Min(1.0f, DeltaSeconds * GunRules->RecoilApplySpeed);
	PendingRecoil -= Applied;
	if (Controller && IsCarrierLocallyControlled())
	{
		FRotator View = Controller->GetControlRotation();
		View.Pitch += Applied.X;
		View.Yaw += Applied.Y;
		Controller->SetControlRotation(View);
	}
}

void AMachineGunWeapon::UpdateBelt()
{
	if (!GunRules || GunRules->LoadedBeltBones.IsEmpty())
	{
		return;
	}
	// an empty gun shows no cartridges on its belt (the Reload clip brings the new belt)
	const bool bHide = GunState.BeltRounds == 0 && !IsReloadGesture(GetServerGesture());
	if (bHide != bBeltHidden)
	{
		bBeltHidden = bHide;
		SetWeaponBonesHidden(GunRules->LoadedBeltBones, bHide);
	}
}

void AMachineGunWeapon::StopCosmetics()
{
	bBurstShownHere = false;
	CosmeticCrank = 0.0f;
	PendingRecoil = FVector2D::ZeroVector;
	if (MechanismAudio)
	{
		MechanismAudio->Stop();
	}
	if (SteamAudio)
	{
		SteamAudio->Stop();
	}
	if (SteamComponent)
	{
		SteamComponent->Deactivate();
	}
	if (HeatLight)
	{
		HeatLight->SetVisibility(false);
	}
}
