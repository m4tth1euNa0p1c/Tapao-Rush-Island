// monke_island

#include "Weapons/PistolWeapon.h"
#include "Weapons/FirearmMath.h"
#include "Weapons/PistolRules.h"
#include "monke_island.h"
#include "Net/UnrealNetwork.h"

APistolWeapon::APistolWeapon()
{
}

void APistolWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(APistolWeapon, LoadedCharges);
	DOREPLIFETIME(APistolWeapon, bCocked);
	DOREPLIFETIME(APistolWeapon, LastShotBarrel);
	DOREPLIFETIME_CONDITION(APistolWeapon, ReserveCharges, COND_OwnerOnly);
}

const UPistolRules* APistolWeapon::GetPistolRules() const
{
	return Cast<UPistolRules>(Rules);
}

int32 APistolWeapon::GetBarrels() const
{
	const UPistolRules* Pistol = GetPistolRules();
	return Pistol ? FMath::Max(Pistol->Barrels, 1) : 1;
}

int32 APistolWeapon::GetMaxRounds() const
{
	const UPistolRules* Pistol = GetPistolRules();
	return Pistol ? Pistol->MaxCharges : 0;
}

void APistolWeapon::SetRounds(int32 Loaded, int32 Reserve)
{
	LoadedCharges = FMath::Clamp(Loaded, 0, GetBarrels());
	ReserveCharges = FMath::Max(0, Reserve);
}

void APistolWeapon::BeginPlay()
{
	if (Rules && !GetPistolRules())
	{
		UE_LOG(Logmonke_island, Error, TEXT("Pistol %s: %s is not a UPistolRules, the pistol is disabled"), *GetName(), *Rules->GetName());
		Rules = nullptr;
	}
	if (HasAuthority())
	{
		if (const UPistolRules* Pistol = GetPistolRules())
		{
			const FirearmMath::FPistolState State = FirearmMath::StartingPistol(Pistol->MaxCharges, Pistol->bStartLoaded, Pistol->bStartCocked, Pistol->Barrels);
			LoadedCharges = State.Loaded;
			bCocked = State.bCocked;
			ReserveCharges = State.Reserve;
		}
	}
	Super::BeginPlay();
}

bool APistolWeapon::PredictFire() const
{
	const UPistolRules* Pistol = GetPistolRules();
	const FirearmMath::FPistolState State{ LoadedCharges, bCocked, ReserveCharges, GetBarrels() };
	// the carrier does not know the server's timers: a gesture seen playing here counts as busy
	const bool bBusy = GetServerGesture() != EFirearmGesture::None;
	return Pistol && FirearmMath::OnTrigger(State, bBusy, Pistol->bAutoReloadOnEmptyFire, Pistol->bAutoCockOnFire) == FirearmMath::EPistolTrigger::Fire;
}

bool APistolWeapon::LowersAim(EFirearmGesture Gesture) const
{
	return Gesture == EFirearmGesture::Reload || Super::LowersAim(Gesture);
}

void APistolWeapon::OnShotPredicted(EFirearmGesture Shot)
{
	// the barrel the server will fire: the first empty one (the server's value replaces this guess)
	LastShotBarrel = static_cast<uint8>(FMath::Clamp(GetBarrels() - LoadedCharges, 0, GetBarrels() - 1));
}

FName APistolWeapon::GetShotMuzzleSocket() const
{
	const UPistolRules* Pistol = GetPistolRules();
	if (Pistol && LastShotBarrel > 0 && !Pistol->SecondMuzzleSocket.IsNone())
	{
		return Pistol->SecondMuzzleSocket;
	}
	return Super::GetShotMuzzleSocket();
}

void APistolWeapon::ServerHandleFire()
{
	const UPistolRules* Pistol = GetPistolRules();
	if (!Pistol)
	{
		return;
	}
	const FirearmMath::FPistolState State{ LoadedCharges, bCocked, ReserveCharges, GetBarrels() };
	Execute(static_cast<uint8>(FirearmMath::OnTrigger(State, IsBusy(), Pistol->bAutoReloadOnEmptyFire, Pistol->bAutoCockOnFire)));
}

void APistolWeapon::ServerHandleReload()
{
	const FirearmMath::FPistolState State{ LoadedCharges, bCocked, ReserveCharges, GetBarrels() };
	Execute(static_cast<uint8>(FirearmMath::OnReloadKey(State, IsBusy())));
}

void APistolWeapon::Execute(uint8 Trigger)
{
	FirearmMath::FPistolState State{ LoadedCharges, bCocked, ReserveCharges, GetBarrels() };
	switch (static_cast<FirearmMath::EPistolTrigger>(Trigger))
	{
	case FirearmMath::EPistolTrigger::Fire:
	{
		const EFirearmGesture Shot = IsAiming() ? EFirearmGesture::AimFire : EFirearmGesture::Fire;
		// the barrel is known before the gesture starts: the flame of this machine comes out of its muzzle
		LastShotBarrel = static_cast<uint8>(FirearmMath::Fire(State));
		StartAction(Shot, FireClipStart(Shot));
		FireProjectiles(1);
		break;
	}
	case FirearmMath::EPistolTrigger::DryFire:
		FirearmMath::DryFire(State);
		StartAction(EFirearmGesture::DryFire);
		break;
	case FirearmMath::EPistolTrigger::Cock:
		// the hammer is up at CockHammer (OnActionEvent)
		StartAction(EFirearmGesture::Cock);
		break;
	case FirearmMath::EPistolTrigger::Reload:
		// the reload lowers the aim by the rule, on the server and on the carrier alike (LowersAim, network review R-38)
		StartAction(EFirearmGesture::Reload);
		break;
	default:
		return;
	}
	LoadedCharges = State.Loaded;
	bCocked = State.bCocked;
	ReserveCharges = State.Reserve;
}

void APistolWeapon::OnActionEvent(EFirearmGesture Gesture, FName Event)
{
	const UPistolRules* Pistol = GetPistolRules();
	if (!Pistol)
	{
		return;
	}
	FirearmMath::FPistolState State{ LoadedCharges, bCocked, ReserveCharges, GetBarrels() };
	if (Gesture == EFirearmGesture::Cock && Event == Pistol->CockEvent)
	{
		FirearmMath::Cock(State);
	}
	else if (Gesture == EFirearmGesture::Reload && Event == Pistol->ReloadLoadedEvent)
	{
		FirearmMath::CompleteReload(State);
	}
	LoadedCharges = State.Loaded;
	bCocked = State.bCocked;
	ReserveCharges = State.Reserve;
}

void APistolWeapon::OnActionFinished(EFirearmGesture Gesture)
{
	// a clip whose event is missing from the DA still does its job at its end
	FirearmMath::FPistolState State{ LoadedCharges, bCocked, ReserveCharges, GetBarrels() };
	if (Gesture == EFirearmGesture::Cock)
	{
		FirearmMath::Cock(State);
	}
	else if (Gesture == EFirearmGesture::Reload && State.Loaded < State.Barrels)
	{
		FirearmMath::CompleteReload(State);
	}
	LoadedCharges = State.Loaded;
	bCocked = State.bCocked;
	ReserveCharges = State.Reserve;
}
