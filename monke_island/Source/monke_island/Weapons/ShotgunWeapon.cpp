// monke_island

#include "Weapons/ShotgunWeapon.h"
#include "Weapons/FirearmDamage.h"
#include "Weapons/FirearmMath.h"
#include "Weapons/ShotgunRules.h"
#include "monke_island.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Spent shells kept in flight at once per gun */
	constexpr int32 MaxShellsInFlight = 4;

	FirearmMath::EReloadPhase PhaseOf(EFirearmGesture Gesture)
	{
		switch (Gesture)
		{
		case EFirearmGesture::ReloadStart: return FirearmMath::EReloadPhase::Start;
		case EFirearmGesture::ReloadInsert: return FirearmMath::EReloadPhase::Insert;
		case EFirearmGesture::ReloadEnd: return FirearmMath::EReloadPhase::End;
		case EFirearmGesture::ReloadEmpty: return FirearmMath::EReloadPhase::Empty;
		default: return FirearmMath::EReloadPhase::None;
		}
	}

	EFirearmGesture GestureOf(FirearmMath::EReloadPhase Phase)
	{
		switch (Phase)
		{
		case FirearmMath::EReloadPhase::Start: return EFirearmGesture::ReloadStart;
		case FirearmMath::EReloadPhase::Insert: return EFirearmGesture::ReloadInsert;
		case FirearmMath::EReloadPhase::End: return EFirearmGesture::ReloadEnd;
		case FirearmMath::EReloadPhase::Empty: return EFirearmGesture::ReloadEmpty;
		default: return EFirearmGesture::None;
		}
	}
}

AShotgunWeapon::AShotgunWeapon()
{
}

void AShotgunWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AShotgunWeapon, Rounds);
	DOREPLIFETIME_CONDITION(AShotgunWeapon, ReserveShells, COND_OwnerOnly);
}

const UShotgunRules* AShotgunWeapon::GetShotgunRules() const
{
	return Cast<UShotgunRules>(Rules);
}

bool AShotgunWeapon::LowersAim(EFirearmGesture Gesture) const
{
	// every step of the reload, shells in one by one or the empty gun
	return PhaseOf(Gesture) != FirearmMath::EReloadPhase::None || Super::LowersAim(Gesture);
}

float AShotgunWeapon::GetCloseBurstDistance() const
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	return Shotgun ? Shotgun->CloseBurstDistanceCm : 0.0f;
}

int32 AShotgunWeapon::GetMaxRounds() const
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	return Shotgun ? Shotgun->MaxShells : 0;
}

void AShotgunWeapon::DescribeShot(FFirearmDamageEvent& Shot, float Distance)
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	if (!Shotgun)
	{
		return;
	}
	// point blank: the pellets shred the body if they kill (T-167)
	Shot.CloseBurstDistance = Shotgun->CloseBurstDistanceCm;
	if (Shotgun->CloseBurstDistanceCm > 0.0f && Distance <= Shotgun->CloseBurstDistanceCm)
	{
		Shot.ShredLimbs = SpreadStream.RandRange(Shotgun->CloseShredLimbsMin, FMath::Max(Shotgun->CloseShredLimbsMin, Shotgun->CloseShredLimbsMax));
	}
}

void AShotgunWeapon::BeginPlay()
{
	if (Rules && !GetShotgunRules())
	{
		UE_LOG(Logmonke_island, Error, TEXT("Shotgun %s: %s is not a UShotgunRules, the shotgun is disabled"), *GetName(), *Rules->GetName());
		Rules = nullptr;
	}
	if (HasAuthority())
	{
		if (const UShotgunRules* Shotgun = GetShotgunRules())
		{
			const FirearmMath::FShotgunState State = FirearmMath::StartingShotgun(Shotgun->MaxShells, Shotgun->Capacity, Shotgun->StartLoadedShells);
			Rounds = State.Rounds;
			ReserveShells = State.Reserve;
		}
	}
	Super::BeginPlay();
}

bool AShotgunWeapon::PredictFire() const
{
	const FirearmMath::FShotgunState State{ Rounds, ReserveShells };
	const EFirearmGesture Playing = GetServerGesture();
	const bool bReloadCut = FirearmMath::CanInterruptReloadForFire(PhaseOf(Playing), State);
	return FirearmMath::CanFire(State, Playing != EFirearmGesture::None && !bReloadCut);
}

void AShotgunWeapon::ServerHandleFire()
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	if (!Shotgun)
	{
		return;
	}
	const FirearmMath::FShotgunState State{ Rounds, ReserveShells };
	const FirearmMath::EReloadPhase Phase = static_cast<FirearmMath::EReloadPhase>(ReloadPhase);
	if (Phase != FirearmMath::EReloadPhase::None)
	{
		// a press on fire cuts the shell by shell reload and shoots at once (not ReloadEmpty: the gun is open)
		if (FirearmMath::CanInterruptReloadForFire(Phase, State))
		{
			StopAction();
			Shoot();
		}
		return;
	}
	if (FirearmMath::CanFire(State, IsBusy()))
	{
		Shoot();
	}
	else if (!IsBusy() && Rounds <= 0)
	{
		if (ReserveShells > 0 && Shotgun->bAutoReloadOnEmptyFire)
		{
			BeginReload();
		}
		else
		{
			StartAction(EFirearmGesture::DryFire);
		}
	}
}

void AShotgunWeapon::Shoot()
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	FirearmMath::FShotgunState State{ Rounds, ReserveShells };
	FirearmMath::Fire(State);
	Rounds = State.Rounds;
	const EFirearmGesture Shot = IsAiming() ? EFirearmGesture::AimFire : EFirearmGesture::Fire;
	StartAction(Shot, FireClipStart(Shot));
	FireProjectiles(Shotgun ? Shotgun->PelletCount : 1);
}

void AShotgunWeapon::ServerHandleReload()
{
	if (!IsBusy())
	{
		BeginReload();
		return;
	}
	const EFirearmGesture Playing = GetServerGesture();
	if (Playing == EFirearmGesture::Fire || Playing == EFirearmGesture::AimFire)
	{
		// R during the pump of the shot: the reload follows the pump (T-335)
		bReloadQueued = true;
	}
}

int32 AShotgunWeapon::GetEmptyReloadShells() const
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	const FFirearmClip* Clip = Shotgun ? Shotgun->FindClip(EFirearmGesture::ReloadEmpty) : nullptr;
	if (!Clip)
	{
		// no ReloadEmpty in the pack: an empty gun reloads shell by shell
		return MAX_int32;
	}
	return Shotgun->bShellByShellWhenShort ? Clip->CountEvents(Shotgun->InsertEvent) : 0;
}

void AShotgunWeapon::BeginReload()
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	if (!Shotgun)
	{
		return;
	}
	bReloadQueued = false;
	const FirearmMath::FShotgunState State{ Rounds, ReserveShells };
	// ReloadEmpty only when the pouch can fill every shell it shows (T-335), else shell by shell
	const FirearmMath::EReloadPhase Phase = FirearmMath::FirstReloadPhase(State, Shotgun->Capacity, GetEmptyReloadShells());
	if (Phase == FirearmMath::EReloadPhase::None)
	{
		return;
	}
	// the reload lowers the aim by the rule, on the server and on the carrier alike (LowersAim, network review R-38)
	if (StartAction(GestureOf(Phase)))
	{
		ReloadPhase = static_cast<uint8>(Phase);
		bInsertedThisStep = false;
	}
}

void AShotgunWeapon::OnActionEvent(EFirearmGesture Gesture, FName Event)
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	if (!Shotgun || Event != Shotgun->InsertEvent || (Gesture != EFirearmGesture::ReloadInsert && Gesture != EFirearmGesture::ReloadEmpty))
	{
		return;
	}
	// one shell per InsertSlug: one in ReloadInsert, three in ReloadEmpty (fewer when the pouch runs out)
	FirearmMath::FShotgunState State{ Rounds, ReserveShells };
	if (FirearmMath::InsertShell(State, Shotgun->Capacity))
	{
		bInsertedThisStep = true;
	}
	Rounds = State.Rounds;
	ReserveShells = State.Reserve;
}

void AShotgunWeapon::OnActionFinished(EFirearmGesture Gesture)
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	if (Shotgun && bReloadQueued && (Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire))
	{
		// the reload asked for during the pump
		BeginReload();
		return;
	}
	const FirearmMath::EReloadPhase Phase = PhaseOf(Gesture);
	if (!Shotgun || Phase == FirearmMath::EReloadPhase::None)
	{
		return;
	}
	FirearmMath::FShotgunState State{ Rounds, ReserveShells };
	if (Phase == FirearmMath::EReloadPhase::Insert && !bInsertedThisStep && !Shotgun->InsertEvent.IsNone())
	{
		// a ReloadInsert clip without its event in the DA still loads its shell
		FirearmMath::InsertShell(State, Shotgun->Capacity);
		Rounds = State.Rounds;
		ReserveShells = State.Reserve;
	}
	// after ReloadEmpty, a gun that holds more goes on shell by shell until full (T-335)
	const FirearmMath::EReloadPhase Next = FirearmMath::NextReloadPhase(Phase, State, Shotgun->Capacity, false, Shotgun->bTopUpAfterEmptyReload);
	ReloadPhase = static_cast<uint8>(FirearmMath::EReloadPhase::None);
	if (Next != FirearmMath::EReloadPhase::None && StartAction(GestureOf(Next)))
	{
		ReloadPhase = static_cast<uint8>(Next);
		bInsertedThisStep = false;
	}
}

void AShotgunWeapon::OnActionStopped(EFirearmGesture Gesture)
{
	if (Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire)
	{
		// a pump cut (put away, swim) forgets the reload asked for meanwhile
		bReloadQueued = false;
	}
	if (PhaseOf(Gesture) != FirearmMath::EReloadPhase::None)
	{
		// cut (shot, put away, swim): the shells already in stay in
		ReloadPhase = static_cast<uint8>(FirearmMath::EReloadPhase::None);
		bInsertedThisStep = false;
	}
}

// ---------------------------------------------------------------- spent shells

void AShotgunWeapon::OnCosmeticEvent(EFirearmGesture Gesture, FName Event)
{
	Super::OnCosmeticEvent(Gesture, Event);
	const UShotgunRules* Shotgun = GetShotgunRules();
	if (Shotgun && Event == Shotgun->EjectEvent && (Gesture == EFirearmGesture::Fire || Gesture == EFirearmGesture::AimFire))
	{
		EjectShell();
	}
}

void AShotgunWeapon::EjectShell()
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	if (!Shotgun || !Shotgun->SpentShellMesh || !GetCarrier())
	{
		return;
	}
	// reuse the oldest shell when the pool is full
	int32 Index = Shells.IndexOfByPredicate([](const FShell& Shell) { return !Shell.bActive; });
	if (Index == INDEX_NONE && ShellComponents.Num() < MaxShellsInFlight)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetStaticMesh(Shotgun->SpentShellMesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCastShadow(false);
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->RegisterComponent();
		Index = ShellComponents.Add(Component);
		Shells.AddDefaulted();
	}
	if (Index == INDEX_NONE)
	{
		Index = 0;
		for (int32 Other = 1; Other < Shells.Num(); ++Other)
		{
			Index = Shells[Other].Age > Shells[Index].Age ? Other : Index;
		}
	}
	static const FName EjectPort(TEXT("socket_eject_port"));
	const USkeletalMeshComponent* Gun = GetVisibleWeaponMesh();
	const FVector Start = Gun->DoesSocketExist(EjectPort) ? Gun->GetSocketLocation(EjectPort) : Gun->GetComponentLocation();
	// out to the right of the carrier's view, a little up and back
	const FRotator View = GetCarrierAim().Rotation();
	const FRotationMatrix Frame(FRotator(0.0f, View.Yaw, 0.0f));
	FShell& Shell = Shells[Index];
	Shell.Velocity = Frame.GetUnitAxis(EAxis::X) * Shotgun->SpentShellVelocity.X + Frame.GetUnitAxis(EAxis::Y) * Shotgun->SpentShellVelocity.Y
		+ FVector::UpVector * Shotgun->SpentShellVelocity.Z;
	Shell.Velocity *= FMath::FRandRange(0.85f, 1.15f);
	Shell.Spin = FVector(FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-360.0f, 360.0f));
	Shell.Age = 0.0f;
	Shell.bActive = true;
	Shell.bResting = false;
	UStaticMeshComponent* Component = ShellComponents[Index];
	Component->SetWorldLocationAndRotation(Start, FRotator(0.0f, View.Yaw + 90.0f, 0.0f));
	Component->SetWorldScale3D(FVector::OneVector);
	Component->SetVisibility(true);
}

bool AShotgunWeapon::HasCosmeticsRunning() const
{
	return Shells.ContainsByPredicate([](const FShell& Shell) { return Shell.bActive; });
}

void AShotgunWeapon::TickCosmetics(float DeltaSeconds)
{
	const UShotgunRules* Shotgun = GetShotgunRules();
	if (!Shotgun)
	{
		return;
	}
	const float Gravity = GetWorld()->GetGravityZ();
	for (int32 Index = 0; Index < Shells.Num(); ++Index)
	{
		FShell& Shell = Shells[Index];
		UStaticMeshComponent* Component = ShellComponents.IsValidIndex(Index) ? ShellComponents[Index].Get() : nullptr;
		if (!Shell.bActive || !Component)
		{
			continue;
		}
		Shell.Age += DeltaSeconds;
		if (Shell.Age >= Shotgun->SpentShellSeconds)
		{
			Shell.bActive = false;
			Component->SetVisibility(false);
			continue;
		}
		if (Shell.bResting)
		{
			continue;
		}
		// falls and tumbles until it lands (one short trace per frame)
		Shell.Velocity.Z += Gravity * DeltaSeconds;
		const FVector From = Component->GetComponentLocation();
		const FVector To = From + Shell.Velocity * DeltaSeconds;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(SpentShell), false, this);
		Params.AddIgnoredActor(GetOwner());
		if (GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
		{
			Component->SetWorldLocation(Hit.ImpactPoint + Hit.ImpactNormal * 1.0f);
			Component->SetWorldRotation(FRotator(0.0f, Component->GetComponentRotation().Yaw, 90.0f));
			Shell.bResting = true;
			continue;
		}
		Component->SetWorldLocation(To);
		Component->AddWorldRotation(FRotator(Shell.Spin.Y, Shell.Spin.Z, Shell.Spin.X) * DeltaSeconds);
	}
}
