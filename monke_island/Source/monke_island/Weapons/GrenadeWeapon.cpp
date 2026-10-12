// monke_island

#include "Weapons/GrenadeWeapon.h"
#include "Weapons/GrenadeMath.h"
#include "Weapons/GrenadeProjectile.h"
#include "Weapons/GrenadeRules.h"
#include "monke_island.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"

AGrenadeWeapon::AGrenadeWeapon()
{
}

void AGrenadeWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGrenadeWeapon, bInHand);
	DOREPLIFETIME_CONDITION(AGrenadeWeapon, BagGrenades, COND_OwnerOnly);
}

const UGrenadeRules* AGrenadeWeapon::GetGrenadeRules() const
{
	return Cast<UGrenadeRules>(Rules);
}

int32 AGrenadeWeapon::GetMaxRounds() const
{
	const UGrenadeRules* Grenade = GetGrenadeRules();
	return Grenade ? Grenade->MaxGrenades : 0;
}

void AGrenadeWeapon::BeginPlay()
{
	if (Rules && !GetGrenadeRules())
	{
		UE_LOG(Logmonke_island, Error, TEXT("Grenade %s: %s is not a UGrenadeRules, the grenades are disabled"), *GetName(), *Rules->GetName());
		Rules = nullptr;
	}
	if (HasAuthority())
	{
		if (const UGrenadeRules* Grenade = GetGrenadeRules())
		{
			const GrenadeMath::FGrenadeState State = GrenadeMath::Starting(Grenade->MaxGrenades, Grenade->StartGrenades, Grenade->bStartInHand);
			bInHand = State.bInHand;
			BagGrenades = State.Bag;
			if (!Grenade->ProjectileClass)
			{
				UE_LOG(Logmonke_island, Warning, TEXT("Grenade %s: %s has no ProjectileClass (BP_GrenadeProjectile), nothing will be thrown"), *GetName(), *Grenade->GetName());
			}
		}
	}
	Super::BeginPlay();
}

void AGrenadeWeapon::OnEquippedChanged()
{
	Super::OnEquippedChanged();
	// drawn: the grenade shows in hand only when there is one
	ShowGrenadeInHand(bInHand);
}

bool AGrenadeWeapon::PredictFire() const
{
	// the carrier does not know the server's timers: a gesture seen playing here counts as busy
	const GrenadeMath::FGrenadeState State{ bInHand, BagGrenades };
	return GetGrenadeRules() && GrenadeMath::OnTrigger(State, GetServerGesture() != EFirearmGesture::None, false) == GrenadeMath::ETrigger::Throw;
}

float AGrenadeWeapon::FireClipStart(EFirearmGesture Gesture) const
{
	// the whole wind-up plays: the grenade leaves at the Fire event, not at the start
	return 0.0f;
}

void AGrenadeWeapon::ServerHandleFire()
{
	const UGrenadeRules* Grenade = GetGrenadeRules();
	if (!Grenade)
	{
		return;
	}
	const GrenadeMath::FGrenadeState State{ bInHand, BagGrenades };
	Execute(static_cast<uint8>(GrenadeMath::OnTrigger(State, IsBusy(), Grenade->bAutoTakeNextOnFire)));
}

void AGrenadeWeapon::ServerHandleReload()
{
	const GrenadeMath::FGrenadeState State{ bInHand, BagGrenades };
	Execute(static_cast<uint8>(GrenadeMath::OnReloadKey(State, IsBusy())));
}

void AGrenadeWeapon::Execute(uint8 Trigger)
{
	const UGrenadeRules* Grenade = GetGrenadeRules();
	GrenadeMath::FGrenadeState State{ bInHand, BagGrenades };
	switch (static_cast<GrenadeMath::ETrigger>(Trigger))
	{
	case GrenadeMath::ETrigger::Throw:
	{
		const EFirearmGesture Shot = IsAiming() ? EFirearmGesture::AimFire : EFirearmGesture::Fire;
		GrenadeMath::Throw(State);
		bInHand = State.bInHand;
		bThrowPending = true;
		bThrowLong = IsAiming();
		LitTime = -1.0;
		// lit at Light, thrown at Fire (OnActionEvent); without a clip the grenade leaves at once
		const FFirearmClip* Clip = Grenade ? Grenade->FindClip(Shot) : nullptr;
		if (!StartAction(Shot, 0.0f) || !Clip || Clip->FindEventTime(Grenade->FireEvent) < 0.0f)
		{
			ThrowGrenade(false);
		}
		return;
	}
	case GrenadeMath::ETrigger::TakeNext:
		if (!StartAction(EFirearmGesture::Reload))
		{
			// no Reload clip: the next grenade is in hand at once
			GrenadeMath::TakeNext(State);
		}
		break;
	case GrenadeMath::ETrigger::DryFire:
		StartAction(EFirearmGesture::DryFire);
		break;
	default:
		return;
	}
	bInHand = State.bInHand;
	BagGrenades = State.Bag;
}

void AGrenadeWeapon::ThrowGrenade(bool bDrop)
{
	bThrowPending = false;
	const UGrenadeRules* Grenade = GetGrenadeRules();
	AIslanderCharacter* Carrier = GetCarrier();
	if (!Grenade || !Carrier)
	{
		return;
	}
	if (!Grenade->ProjectileClass)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Grenade %s: no ProjectileClass, the grenade vanishes"), *GetName());
		LitTime = -1.0;
		return;
	}
	// from the hand, in front of the eyes, never through a wall
	const UCameraComponent* Camera = Carrier->GetFirstPersonCamera();
	const FVector Eyes = Camera ? Camera->GetComponentLocation() : Carrier->GetPawnViewLocation();
	const FVector Aim = GetCarrierAim();
	const FRotationMatrix Frame(Aim.Rotation());
	FVector Origin = Eyes + Frame.TransformVector(Grenade->ThrowOffset);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrenadeThrow), false, Carrier);
	Params.AddIgnoredActor(this);
	if (GetWorld()->SweepSingleByChannel(Hit, Eyes, Origin, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(Grenade->ProjectileRadiusCm), Params))
	{
		Origin = Hit.Location;
	}
	const float Speed = bDrop ? Grenade->DropSpeed : (bThrowLong ? Grenade->LongThrowSpeed : Grenade->ShortThrowSpeed);
	const FVector Velocity = GrenadeMath::ThrowVelocity(Aim, Speed, bDrop ? 0.0f : Grenade->ThrowPitchDegrees)
		+ Carrier->GetVelocity() * Grenade->InheritedVelocityScale;
	const double Now = GetWorld()->GetTimeSeconds();
	const float Fuse = LitTime >= 0.0 ? GrenadeMath::FuseLeft(Grenade->FuseSeconds, static_cast<float>(Now - LitTime), Grenade->MinFuseAfterThrow) : Grenade->FuseSeconds;
	LitTime = -1.0;

	const FTransform Transform(Aim.Rotation(), Origin);
	AGrenadeProjectile* Projectile = GetWorld()->SpawnActorDeferred<AGrenadeProjectile>(Grenade->ProjectileClass, Transform, Carrier, Carrier,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Grenade %s: the projectile could not be spawned"), *GetName());
		return;
	}
	Projectile->Init(const_cast<UGrenadeRules*>(Grenade), this, Velocity, Fuse);
	Projectile->FinishSpawning(Transform);
	UE_LOG(Logmonke_island, Log, TEXT("Grenade %s: %s throws one (%s, %.0f cm/s, fuse %.1f s), %d left in the bag"), *GetName(), *Carrier->GetName(),
		bDrop ? TEXT("dropped") : (bThrowLong ? TEXT("long") : TEXT("short")), Speed, Fuse, BagGrenades);
}

void AGrenadeWeapon::OnActionEvent(EFirearmGesture Gesture, FName Event)
{
	const UGrenadeRules* Grenade = GetGrenadeRules();
	if (!Grenade)
	{
		return;
	}
	if (Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire)
	{
		if (Event == Grenade->LightEvent && LitTime < 0.0)
		{
			LitTime = GetWorld()->GetTimeSeconds();
		}
		else if (Event == Grenade->FireEvent && bThrowPending)
		{
			ThrowGrenade(false);
		}
	}
	else if (Gesture == EFirearmGesture::Reload && Event == Grenade->ReloadLoadedEvent)
	{
		GrenadeMath::FGrenadeState State{ bInHand, BagGrenades };
		GrenadeMath::TakeNext(State);
		bInHand = State.bInHand;
		BagGrenades = State.Bag;
	}
}

void AGrenadeWeapon::OnActionFinished(EFirearmGesture Gesture)
{
	const UGrenadeRules* Grenade = GetGrenadeRules();
	if (!Grenade)
	{
		return;
	}
	if (Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire)
	{
		// a clip without its Fire event still throws at its end; then the next grenade comes to hand
		if (bThrowPending)
		{
			ThrowGrenade(false);
		}
		if (Grenade->bAutoTakeNextAfterThrow && !bInHand && BagGrenades > 0)
		{
			StartAction(EFirearmGesture::Reload);
		}
	}
	else if (Gesture == EFirearmGesture::Reload && !bInHand)
	{
		GrenadeMath::FGrenadeState State{ bInHand, BagGrenades };
		GrenadeMath::TakeNext(State);
		bInHand = State.bInHand;
		BagGrenades = State.Bag;
	}
}

void AGrenadeWeapon::OnActionStopped(EFirearmGesture Gesture)
{
	if ((Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire) && bThrowPending)
	{
		if (LitTime >= 0.0)
		{
			// lit and cut (swim, knocked out, put away): it rolls at the feet rather than staying lit in the bag
			ThrowGrenade(true);
		}
		else
		{
			bThrowPending = false;
			bInHand = true;
		}
	}
}

// ---------------------------------------------------------------- cosmetics

void AGrenadeWeapon::ShowGrenadeInHand(bool bShown)
{
	bHandShown = bShown;
	HandCosmeticTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	// the pack keeps the slow match and the bag on the weapon mesh: only the grenade itself goes (owner pack 2026-10-09)
	const UGrenadeRules* Grenade = GetGrenadeRules();
	if (Grenade && !Grenade->HandHiddenBones.IsEmpty())
	{
		SetWeaponMeshVisible(true);
		SetWeaponBonesHidden(Grenade->HandHiddenBones, !bShown);
	}
	else
	{
		SetWeaponMeshVisible(bShown);
	}
}

void AGrenadeWeapon::OnCosmeticEvent(EFirearmGesture Gesture, FName Event)
{
	Super::OnCosmeticEvent(Gesture, Event);
	const UGrenadeRules* Grenade = GetGrenadeRules();
	if (!Grenade)
	{
		return;
	}
	if ((Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire) && Event == Grenade->FireEvent)
	{
		// the hand is empty until the next grenade is taken
		ShowGrenadeInHand(false);
	}
	else if (Gesture == EFirearmGesture::Reload && Event == Grenade->ReloadLoadedEvent)
	{
		ShowGrenadeInHand(true);
	}
}

void AGrenadeWeapon::TickCosmetics(float DeltaSeconds)
{
	// outside the gestures, and once the server's answer had time to arrive, the hand shows the replicated state
	if (GetServerGesture() == EFirearmGesture::None && bHandShown != bInHand && GetWorld()->GetTimeSeconds() - HandCosmeticTime > 1.5)
	{
		ShowGrenadeInHand(bInHand);
	}
}
