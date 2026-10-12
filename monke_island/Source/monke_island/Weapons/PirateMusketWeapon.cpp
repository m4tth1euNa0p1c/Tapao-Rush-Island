// monke_island

#include "Weapons/PirateMusketWeapon.h"
#include "Weapons/MusketMath.h"
#include "Weapons/PirateMusketRules.h"
#include "monke_island.h"
#include "Net/UnrealNetwork.h"

APirateMusketWeapon::APirateMusketWeapon()
{
}

void APirateMusketWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APirateMusketWeapon, bLoaded);
	DOREPLIFETIME_CONDITION(APirateMusketWeapon, PouchBalls, COND_OwnerOnly);
}

const UPirateMusketRules* APirateMusketWeapon::GetMusketRules() const
{
	return Cast<UPirateMusketRules>(Rules);
}

int32 APirateMusketWeapon::GetMaxRounds() const
{
	const UPirateMusketRules* Musket = GetMusketRules();
	return Musket ? Musket->MaxBalls : 0;
}

void APirateMusketWeapon::BeginPlay()
{
	if (Rules && !GetMusketRules())
	{
		UE_LOG(Logmonke_island, Error, TEXT("Musket %s: %s is not a UPirateMusketRules, the musket is disabled"), *GetName(), *Rules->GetName());
		Rules = nullptr;
	}
	if (HasAuthority())
	{
		if (const UPirateMusketRules* Musket = GetMusketRules())
		{
			const MusketMath::FAmmo Ammo = MusketMath::StartingAmmo(Musket->MaxBalls, Musket->bStartLoaded);
			bLoaded = Ammo.bLoaded;
			PouchBalls = Ammo.PouchBalls;
		}
	}
	Super::BeginPlay();
}

bool APirateMusketWeapon::IsReloading() const
{
	const UPirateMusketRules* Musket = GetMusketRules();
	return Musket && GetServerGesture() == Musket->ReloadGesture;
}

float APirateMusketWeapon::GetReloadProgress() const
{
	return IsReloading() ? GetServerGestureProgress() : 0.0f;
}

bool APirateMusketWeapon::CanAim() const
{
	// the musket is lowered while the reload runs, on the server and on the carrier alike (the replicated gesture), and
	// shouldered again at its end if the right click is still held (network review R-38)
	return !IsReloading();
}

float APirateMusketWeapon::GetGestureRate(const FFirearmClip& Clip) const
{
	const UPirateMusketRules* Musket = GetMusketRules();
	if (Musket && Clip.Gesture == Musket->ReloadGesture)
	{
		// every phase of the reload together, stretched over the tuned reload time
		return MusketMath::StretchRate(Clip.Duration, Musket->ReloadSeconds);
	}
	return Super::GetGestureRate(Clip);
}

float APirateMusketWeapon::FireClipStart(EFirearmGesture Gesture) const
{
	const UPirateMusketRules* Musket = GetMusketRules();
	const FFirearmClip* Clip = Musket ? Musket->FindClip(Gesture) : nullptr;
	const float Trigger = Clip ? Clip->FindEventTime(Musket->TriggerEvent) : -1.0f;
	// the clip starts at the trigger pull: the lock time plays before the ball leaves
	return Trigger >= 0.0f ? Trigger : Super::FireClipStart(Gesture);
}

bool APirateMusketWeapon::PredictFire() const
{
	// the carrier does not know the server's timers: a gesture seen playing here counts as busy
	const MusketMath::FAmmo Ammo{ bLoaded, PouchBalls };
	return GetMusketRules() && MusketMath::CanFire(Ammo, GetServerGesture() != EFirearmGesture::None, false);
}

void APirateMusketWeapon::ServerHandleFire()
{
	const UPirateMusketRules* Musket = GetMusketRules();
	if (!Musket)
	{
		return;
	}
	MusketMath::FAmmo Ammo{ bLoaded, PouchBalls };
	const bool bBusy = IsBusy();
	if (!MusketMath::CanFire(Ammo, bBusy, false))
	{
		if (!Ammo.bLoaded && !bBusy && Musket->bAutoReloadOnEmptyFire)
		{
			ServerHandleReload();
		}
		return;
	}
	const EFirearmGesture Shot = IsAiming() ? EFirearmGesture::AimFire : EFirearmGesture::Fire;
	MusketMath::Fire(Ammo);
	bLoaded = Ammo.bLoaded;
	bShotPending = true;

	// the ball leaves at the Shot event (OnActionEvent); at once when the clip has no lock time
	const FFirearmClip* Clip = Musket->FindClip(Shot);
	const float Start = FireClipStart(Shot);
	const float ShotTime = Clip ? Clip->FindEventTime(Musket->FireEvent) : -1.0f;
	if (!StartAction(Shot, Start) || ShotTime <= Start + UE_KINDA_SMALL_NUMBER)
	{
		ReleaseShot();
	}
}

void APirateMusketWeapon::ServerHandleReload()
{
	const UPirateMusketRules* Musket = GetMusketRules();
	const MusketMath::FAmmo Ammo{ bLoaded, PouchBalls };
	if (!Musket || !MusketMath::CanReload(Ammo, IsBusy(), false))
	{
		return;
	}
	// the reload lowers the aim by the rule (CanAim), the carrier's wish kept (network review R-38)
	if (!StartAction(Musket->ReloadGesture))
	{
		// no clip for the reload in the rules: the ball goes in at once rather than never
		UE_LOG(Logmonke_island, Warning, TEXT("Musket %s: %s has no clip for the reload gesture, reloaded at once"), *GetName(), *Musket->GetName());
		OnActionFinished(Musket->ReloadGesture);
	}
}

void APirateMusketWeapon::ReleaseShot()
{
	if (!bShotPending)
	{
		return;
	}
	bShotPending = false;
	FireProjectiles(GetProjectilesPerShot());
}

void APirateMusketWeapon::OnActionEvent(EFirearmGesture Gesture, FName Event)
{
	if (Rules && (Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire) && Event == Rules->FireEvent)
	{
		ReleaseShot();
	}
}

void APirateMusketWeapon::OnActionFinished(EFirearmGesture Gesture)
{
	const UPirateMusketRules* Musket = GetMusketRules();
	// the trigger was pulled: the ball has left whatever the clip's events
	ReleaseShot();
	if (Musket && Gesture == Musket->ReloadGesture)
	{
		MusketMath::FAmmo Ammo{ bLoaded, PouchBalls };
		MusketMath::CompleteReload(Ammo);
		bLoaded = Ammo.bLoaded;
		PouchBalls = Ammo.PouchBalls;
	}
}

void APirateMusketWeapon::OnActionStopped(EFirearmGesture Gesture)
{
	// a shot cut during the lock time still goes off; a cut reload keeps the ball in the pouch
	ReleaseShot();
}
