// monke_island

#include "Weapons/BoltRifleWeapon.h"
#include "Weapons/BoltRifleMath.h"
#include "Weapons/BoltRifleRules.h"
#include "monke_island.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace
{
	/** Spent cases kept in flight at once per rifle */
	constexpr int32 MaxCasesInFlight = 4;
}

ABoltRifleWeapon::ABoltRifleWeapon()
{
}

void ABoltRifleWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ABoltRifleWeapon, Rounds);
	DOREPLIFETIME(ABoltRifleWeapon, bChambered);
	DOREPLIFETIME_CONDITION(ABoltRifleWeapon, ReserveRounds, COND_OwnerOnly);
}

const UBoltRifleRules* ABoltRifleWeapon::GetBoltRules() const
{
	return Cast<UBoltRifleRules>(Rules);
}

void ABoltRifleWeapon::BeginPlay()
{
	if (Rules && !GetBoltRules())
	{
		UE_LOG(Logmonke_island, Error, TEXT("Bolt rifle %s: %s is not a UBoltRifleRules, the rifle is disabled"), *GetName(), *Rules->GetName());
		Rules = nullptr;
	}
	if (HasAuthority())
	{
		if (const UBoltRifleRules* Bolt = GetBoltRules())
		{
			const BoltRifleMath::FState Start = BoltRifleMath::StartingRifle(Bolt->MaxRounds, Bolt->Capacity, Bolt->StartLoadedRounds);
			Rounds = Start.Loaded;
			bChambered = Start.bChambered;
			ReserveRounds = Start.Reserve;
		}
	}
	Super::BeginPlay();
}

int32 ABoltRifleWeapon::GetMaxRounds() const
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	return Bolt ? Bolt->MaxRounds : 0;
}

void ABoltRifleWeapon::SetRounds(int32 Loaded, int32 Reserve)
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	const int32 Capacity = Bolt ? Bolt->Capacity : FMath::Max(Loaded, 0);
	Rounds = FMath::Clamp(Loaded, 0, Capacity);
	// rounds beyond the rifle go back into the pouch; the chamber keeps its state (a fired case stays a fired case)
	ReserveRounds = FMath::Max(0, Reserve) + FMath::Max(0, Loaded - Rounds);
	bChambered = bChambered && Rounds > 0;
}

bool ABoltRifleWeapon::LowersAim(EFirearmGesture Gesture) const
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	if (Gesture == EFirearmGesture::Reload || (Gesture == EFirearmGesture::BoltCycle && (!Bolt || Bolt->bBoltCycleLowersScope)))
	{
		return true;
	}
	return Super::LowersAim(Gesture);
}

float ABoltRifleWeapon::GetShotSpreadDegrees(bool bMoving) const
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	return Bolt ? BoltRifleMath::Spread(IsAiming(), bMoving, Bolt->AimSpreadDegrees, Bolt->NoScopeSpreadDegrees, Bolt->HipSpreadDegrees)
		: Super::GetShotSpreadDegrees(bMoving);
}

float ABoltRifleWeapon::GetGestureRate(const FFirearmClip& Clip) const
{
	if (const UBoltRifleRules* Bolt = GetBoltRules())
	{
		if (Clip.Gesture == EFirearmGesture::Reload && Bolt->ReloadSeconds > 0.0f)
		{
			return BoltRifleMath::StretchRate(Clip.Duration, Bolt->ReloadSeconds);
		}
		if (Clip.Gesture == EFirearmGesture::BoltCycle && Bolt->BoltCycleSeconds > 0.0f)
		{
			return BoltRifleMath::StretchRate(Clip.Duration, Bolt->BoltCycleSeconds);
		}
	}
	return Super::GetGestureRate(Clip);
}

EFirearmGesture ABoltRifleWeapon::ShotGesture() const
{
	// through the scope the pack's aimed shot, when it has one
	return IsAiming() && Rules && Rules->FindClip(EFirearmGesture::AimFire) ? EFirearmGesture::AimFire : EFirearmGesture::Fire;
}

// ---------------------------------------------------------------- server

bool ABoltRifleWeapon::PredictFire() const
{
	// the carrier does not know the server's timers: a gesture seen playing here is busy, unless the bolt closed already
	const BoltRifleMath::FState State{ Rounds, bChambered, ReserveRounds };
	const EFirearmGesture Playing = GetServerGesture();
	const bool bBusy = Playing != EFirearmGesture::None && !BoltRifleMath::CanCutForShot(Playing, State);
	return GetBoltRules() && BoltRifleMath::OnTrigger(State, bBusy, false) == BoltRifleMath::ETrigger::Fire;
}

void ABoltRifleWeapon::ServerHandleFire()
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	if (!Bolt)
	{
		return;
	}
	const BoltRifleMath::FState State{ Rounds, bChambered, ReserveRounds };
	// the end of the bolt cycle (or of the reload) after BoltClose gives way to the shot; nothing before BoltClose
	const bool bCut = IsBusy() && BoltRifleMath::CanCutForShot(GetServerGesture(), State);
	switch (BoltRifleMath::OnTrigger(State, IsBusy() && !bCut, Bolt->bAutoReloadOnEmptyFire))
	{
	case BoltRifleMath::ETrigger::Fire:
		if (bCut)
		{
			StopAction();
		}
		Shoot();
		break;
	case BoltRifleMath::ETrigger::CycleBolt:
		CycleBolt();
		break;
	case BoltRifleMath::ETrigger::Reload:
		BeginReload();
		break;
	case BoltRifleMath::ETrigger::DryFire:
		StartAction(EFirearmGesture::DryFire);
		break;
	default:
		break;
	}
}

void ABoltRifleWeapon::Shoot()
{
	BoltRifleMath::FState State{ Rounds, bChambered, ReserveRounds };
	BoltRifleMath::Fire(State);
	Rounds = State.Loaded;
	bChambered = State.bChambered;
	FireProjectiles(1);
	const EFirearmGesture Shot = ShotGesture();
	if (!StartAction(Shot, FireClipStart(Shot)))
	{
		// no shot clip: what follows the shot comes at once
		FollowShot();
	}
}

void ABoltRifleWeapon::FollowShot()
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	if (!Bolt)
	{
		return;
	}
	const BoltRifleMath::FState State{ Rounds, bChambered, ReserveRounds };
	switch (BoltRifleMath::AfterShot(State, Bolt->bAutoCycleBolt, Bolt->bAutoReloadAfterLastShot))
	{
	case EFirearmGesture::BoltCycle:
		CycleBolt();
		break;
	case EFirearmGesture::Reload:
		BeginReload();
		break;
	default:
		break;
	}
}

void ABoltRifleWeapon::CycleBolt()
{
	bBoltClosedThisGesture = false;
	// the cycle lowers the scope by the rule, on the server and on the carrier alike (LowersAim, network review R-38)
	if (!StartAction(EFirearmGesture::BoltCycle))
	{
		// no BoltCycle clip in the rules: the bolt is worked at once rather than never
		CloseBolt();
	}
}

void ABoltRifleWeapon::ServerHandleReload()
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	if (!Bolt || IsBusy())
	{
		return;
	}
	if (!BeginReload() && Rounds > 0 && !bChambered)
	{
		// full (or nothing in the pouch) with a case in the chamber: R works the bolt
		CycleBolt();
	}
}

bool ABoltRifleWeapon::BeginReload()
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	const BoltRifleMath::FState State{ Rounds, bChambered, ReserveRounds };
	if (!Bolt || IsBusy() || !BoltRifleMath::CanReload(State, Bolt->Capacity))
	{
		return false;
	}
	bClipLoadedThisReload = false;
	bBoltClosedThisGesture = false;
	if (!StartAction(EFirearmGesture::Reload))
	{
		// no Reload clip in the rules: the clip goes in at once rather than never
		UE_LOG(Logmonke_island, Warning, TEXT("Bolt rifle %s: %s has no Reload clip, reloaded at once"), *GetName(), *Bolt->GetName());
		LoadClip();
		CloseBolt();
	}
	return true;
}

void ABoltRifleWeapon::CloseBolt()
{
	BoltRifleMath::FState State{ Rounds, bChambered, ReserveRounds };
	BoltRifleMath::CloseBolt(State);
	bChambered = State.bChambered;
	bBoltClosedThisGesture = true;
}

void ABoltRifleWeapon::LoadClip()
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	BoltRifleMath::FState State{ Rounds, bChambered, ReserveRounds };
	if (Bolt && BoltRifleMath::LoadClip(State, Bolt->Capacity))
	{
		Rounds = State.Loaded;
		ReserveRounds = State.Reserve;
		UE_LOG(Logmonke_island, Log, TEXT("Bolt rifle %s: clip in, %d in the rifle, %d in the pouch"), *GetName(), Rounds, ReserveRounds);
	}
	bClipLoadedThisReload = true;
}

void ABoltRifleWeapon::OnActionEvent(EFirearmGesture Gesture, FName Event)
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	if (!Bolt)
	{
		return;
	}
	if (Gesture == EFirearmGesture::Reload && Event == Bolt->ReloadLoadedEvent && !bClipLoadedThisReload)
	{
		LoadClip();
	}
	if ((Gesture == EFirearmGesture::BoltCycle || Gesture == EFirearmGesture::Reload) && Event == Bolt->BoltCloseEvent)
	{
		CloseBolt();
	}
}

void ABoltRifleWeapon::OnActionFinished(EFirearmGesture Gesture)
{
	switch (Gesture)
	{
	case EFirearmGesture::Fire:
	case EFirearmGesture::AimFire:
		FollowShot();
		break;
	case EFirearmGesture::BoltCycle:
		// a clip without its BoltClose event in the DA still closes the bolt, at its end
		if (!bBoltClosedThisGesture)
		{
			CloseBolt();
		}
		break;
	case EFirearmGesture::Reload:
		// a clip without its MagIn or BoltClose event still loads and chambers, at its end
		if (!bClipLoadedThisReload)
		{
			LoadClip();
		}
		if (!bChambered)
		{
			CloseBolt();
		}
		bClipLoadedThisReload = false;
		break;
	default:
		break;
	}
}

void ABoltRifleWeapon::OnActionStopped(EFirearmGesture Gesture)
{
	// cut (put away, swim, a shot after BoltClose): what was done stays done; a bolt cut before BoltClose is worked again
	// at the next press, a clip cut before MagIn took nothing from the pouch
	if (Gesture == EFirearmGesture::Reload)
	{
		bClipLoadedThisReload = false;
	}
}

// ---------------------------------------------------------------- spent cases (every machine)

void ABoltRifleWeapon::OnCosmeticEvent(EFirearmGesture Gesture, FName Event)
{
	Super::OnCosmeticEvent(Gesture, Event);
	const UBoltRifleRules* Bolt = GetBoltRules();
	if (!Bolt || (Gesture != EFirearmGesture::BoltCycle && Gesture != EFirearmGesture::Reload))
	{
		return;
	}
	if (Event == Bolt->CaseEjectEvent)
	{
		ThrowPiece(Bolt->SpentCaseMesh, Bolt->EjectSocket, NAME_None, Bolt->SpentCaseVelocity);
	}
	else if (Event == Bolt->ClipEjectEvent)
	{
		ThrowPiece(Bolt->EmptyClipMesh, Bolt->ClipSocket, Bolt->EjectSocket, Bolt->EmptyClipVelocity);
	}
}

void ABoltRifleWeapon::ThrowPiece(UStaticMesh* Mesh, FName Socket, FName FallbackSocket, const FVector& Velocity)
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	const USkeletalMeshComponent* Gun = GetVisibleWeaponMesh();
	if (!Bolt || !Mesh || !GetCarrier() || !Gun || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// reuse the oldest case when the pool is full
	int32 Index = Cases.IndexOfByPredicate([](const FCase& Case) { return !Case.bActive; });
	if (Index == INDEX_NONE && CaseComponents.Num() < MaxCasesInFlight)
	{
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->SetCastShadow(false);
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->RegisterComponent();
		Index = CaseComponents.Add(Component);
		Cases.AddDefaulted();
	}
	if (Index == INDEX_NONE)
	{
		Index = 0;
		for (int32 Other = 1; Other < Cases.Num(); ++Other)
		{
			Index = Cases[Other].Age > Cases[Index].Age ? Other : Index;
		}
	}
	const FName From = !Socket.IsNone() && Gun->DoesSocketExist(Socket) ? Socket
		: (!FallbackSocket.IsNone() && Gun->DoesSocketExist(FallbackSocket) ? FallbackSocket : NAME_None);
	const FVector Start = From.IsNone() ? Gun->GetComponentLocation() : Gun->GetSocketLocation(From);
	// out to the right of the carrier's view, a little up and back (the clip up and away)
	const FRotator View = GetCarrierAim().Rotation();
	const FRotationMatrix Frame(FRotator(0.0f, View.Yaw, 0.0f));
	FCase& Case = Cases[Index];
	Case.Velocity = (Frame.GetUnitAxis(EAxis::X) * Velocity.X + Frame.GetUnitAxis(EAxis::Y) * Velocity.Y + FVector::UpVector * Velocity.Z)
		* FMath::FRandRange(0.85f, 1.15f);
	Case.Spin = FVector(FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-720.0f, 720.0f), FMath::FRandRange(-360.0f, 360.0f));
	Case.Age = 0.0f;
	Case.bActive = true;
	Case.bResting = false;
	UStaticMeshComponent* Component = CaseComponents[Index];
	// one pool for the cases and the clips: the piece's mesh each time
	if (Component->GetStaticMesh() != Mesh)
	{
		Component->SetStaticMesh(Mesh);
	}
	Component->SetWorldLocationAndRotation(Start, FRotator(0.0f, View.Yaw + 90.0f, 0.0f));
	Component->SetWorldScale3D(FVector::OneVector);
	Component->SetVisibility(true);
}

bool ABoltRifleWeapon::HasCosmeticsRunning() const
{
	return Cases.ContainsByPredicate([](const FCase& Case) { return Case.bActive; });
}

void ABoltRifleWeapon::TickCosmetics(float DeltaSeconds)
{
	const UBoltRifleRules* Bolt = GetBoltRules();
	UWorld* World = GetWorld();
	if (!Bolt || !World || Cases.IsEmpty())
	{
		return;
	}
	const float Gravity = World->GetGravityZ();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BoltRifleCase), false, this);
	Params.AddIgnoredActor(GetOwner());
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		FCase& Case = Cases[Index];
		UStaticMeshComponent* Component = CaseComponents.IsValidIndex(Index) ? CaseComponents[Index].Get() : nullptr;
		if (!Case.bActive || !Component)
		{
			continue;
		}
		Case.Age += DeltaSeconds;
		if (Case.Age >= Bolt->SpentCaseSeconds)
		{
			Case.bActive = false;
			Component->SetVisibility(false);
			continue;
		}
		if (Case.bResting)
		{
			continue;
		}
		// falls and tumbles until it lands (one short trace per frame)
		Case.Velocity.Z += Gravity * DeltaSeconds;
		const FVector From = Component->GetComponentLocation();
		const FVector To = From + Case.Velocity * DeltaSeconds;
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
		{
			Component->SetWorldLocation(Hit.ImpactPoint + Hit.ImpactNormal * 0.5f);
			Component->SetWorldRotation(FRotator(0.0f, Component->GetComponentRotation().Yaw, 90.0f));
			Case.bResting = true;
			continue;
		}
		Component->SetWorldLocation(To);
		Component->AddWorldRotation(FRotator(Case.Spin.Y, Case.Spin.Z, Case.Spin.X) * DeltaSeconds);
	}
}
