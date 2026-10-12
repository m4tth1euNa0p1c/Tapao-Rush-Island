// monke_island

#include "Stealth/StealthSubsystem.h"
#include "Stealth/StealthRules.h"
#include "DayNight/DayNightCycleActor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogMonkeStealth);

namespace
{
#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarStealthDebug(
		TEXT("monke.Stealth.Debug"), 0,
		TEXT("Stealth (T-262): 1 logs every noise the players make and draws its reach on the server"));

	TAutoConsoleVariable<int32> CVarStealthNight(
		TEXT("monke.Stealth.Night"), -1,
		TEXT("Stealth (T-262): -1 the night of the day / night cycle, 0 always day, 1 always night"));
#endif

	/** Collision radius of an actor's body: its capsule for a character */
	float BodyRadius(const AActor* Actor)
	{
		float Radius = 0.0f;
		float HalfHeight = 0.0f;
		Actor->GetSimpleCollisionCylinder(Radius, HalfHeight);
		return Radius;
	}

	const TCHAR* NoiseName(EStealthNoise Kind)
	{
		switch (Kind)
		{
		case EStealthNoise::Footstep: return TEXT("footstep");
		case EStealthNoise::Voice: return TEXT("voice");
		case EStealthNoise::Gunshot: return TEXT("gunshot");
		case EStealthNoise::Impact: return TEXT("impact");
		default: return TEXT("noise");
		}
	}
}

const TCHAR* UStealthSubsystem::DefaultRulesPath = TEXT("/Game/MonkeIsland/Stealth/Data/DA_StealthRules.DA_StealthRules");

UStealthSubsystem* UStealthSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UStealthSubsystem>() : nullptr;
}

const UStealthRules& UStealthSubsystem::RulesOf(const UObject* WorldContext)
{
	const UStealthSubsystem* Stealth = Get(WorldContext);
	return Stealth ? Stealth->GetRules() : *GetDefault<UStealthRules>();
}

bool UStealthSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UStealthSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// read once while the level loads, as the other rules of the project
	Rules = LoadObject<UStealthRules>(nullptr, DefaultRulesPath, nullptr, LOAD_NoWarn);
	if (!Rules)
	{
		UE_LOG(LogMonkeStealth, Warning, TEXT("Stealth: %s missing, the class defaults are used"), DefaultRulesPath);
		Rules = NewObject<UStealthRules>(this);
	}
	Noises.Reset();
	Noises.Reserve(FMath::Clamp(Rules->Hearing.MaxNoises, 4, 512));
}

void UStealthSubsystem::Deinitialize()
{
	Noises.Empty();
	CoverQuery.Unbind();
	DayNight.Reset();
	Super::Deinitialize();
}

const UStealthRules& UStealthSubsystem::GetRules() const
{
	return Rules ? *Rules : *GetDefault<UStealthRules>();
}

float UStealthSubsystem::GetNoiseRadius(EStealthNoise Kind) const
{
	return GetRules().Hearing.RadiusOf(Kind);
}

bool UStealthSubsystem::IsNight() const
{
#if !UE_BUILD_SHIPPING
	const int32 Forced = CVarStealthNight.GetValueOnGameThread();
	if (Forced >= 0)
	{
		return Forced > 0;
	}
#endif
	const ADayNightCycleActor* Cycle = DayNight.Get();
	if (!Cycle)
	{
		// a level without a cycle is always day; looked up again now and then (a streamed level may bring one)
		const UWorld* World = GetWorld();
		const double Now = World ? World->GetTimeSeconds() : 0.0;
		if (!World || Now < NextDayNightLookup)
		{
			return false;
		}
		NextDayNightLookup = Now + 2.0;
		ADayNightCycleActor* Found = ADayNightCycleActor::Get(World);
		DayNight = Found;
		Cycle = Found;
	}
	return Cycle && Cycle->GetPhase() == EDayPhase::Night;
}

bool UStealthSubsystem::IsPlayerPawn(const AActor* Actor)
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

bool UStealthSubsystem::IsPlayerNoise(const AActor* Instigator)
{
	if (!IsValid(Instigator))
	{
		return false;
	}
	// the player itself, or its weapon or projectile (instigated by it, or owned by it)
	return IsPlayerPawn(Instigator) || IsPlayerPawn(Instigator->GetInstigator()) || IsPlayerPawn(Instigator->GetOwner());
}

bool UStealthSubsystem::IsInBush(const FVector& Location, float RadiusCm) const
{
	return CoverQuery.IsBound() && CoverQuery.Execute(Location, RadiusCm);
}

void UStealthSubsystem::ClearCoverQuery(const UObject* Owner)
{
	if (Owner && CoverQuery.IsBoundToObject(Owner))
	{
		CoverQuery.Unbind();
	}
}

bool UStealthSubsystem::CanSee(const AActor* Observer, const AActor* Target, float SightRangeCm) const
{
	if (!IsValid(Observer) || !IsValid(Target))
	{
		return false;
	}
	return CanSeeFromPlace(Observer->GetActorLocation(), BodyRadius(Observer), Target, SightRangeCm);
}

bool UStealthSubsystem::CanSeeFrom(const FVector& Eyes, float ObserverRadiusCm, const AActor* Target, float SightRangeCm) const
{
	return IsValid(Target) && CanSeeFromPlace(Eyes, ObserverRadiusCm, Target, SightRangeCm);
}

bool UStealthSubsystem::CanSeeFromPlace(const FVector& Eyes, float ObserverRadiusCm, const AActor* Target, float SightRangeCm) const
{
	const FStealthSightRules& Sight = GetRules().Sight;
	const FVector TargetLocation = Target->GetActorLocation();
	FStealthSight Look;
	Look.DistanceCm = static_cast<float>(FVector::Dist(Eyes, TargetLocation));
	Look.RangeCm = SightRangeCm;
	Look.GapCm = StealthMath::BodyGap(Look.DistanceCm, ObserverRadiusCm, BodyRadius(Target));
	Look.bTargetIsPlayer = IsPlayerPawn(Target);
	if (Look.bTargetIsPlayer)
	{
		const ACharacter* Character = Cast<ACharacter>(Target);
		Look.bCrouched = Character && Character->bIsCrouched;
		Look.bNight = IsNight();
		// the bush grid only when the answer depends on it (crouched near the contact)
		if (StealthMath::NeedsBushCheck(Look, Sight))
		{
			Look.bInBush = IsInBush(TargetLocation, Sight.BushRadiusCm);
		}
	}
	return StealthMath::CanSee(Look, Sight);
}

void UStealthSubsystem::ReportNoise(AActor* Instigator, const FVector& Location, float RadiusCm, EStealthNoise Kind)
{
	const UWorld* World = GetWorld();
	// the server decides what the mobs hear: a client's call changes nothing
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}
	if (Location.ContainsNaN())
	{
		UE_LOG(LogMonkeStealth, Warning, TEXT("Stealth: %s noise of %s at an invalid place, ignored"), NoiseName(Kind), *GetNameSafe(Instigator));
		return;
	}
	const FStealthHearingRules& Hearing = GetRules().Hearing;
	const float Radius = StealthMath::ClampNoiseRadius(RadiusCm, Hearing.MaxNoiseRadiusCm);
	if (Radius <= 0.0f)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	// what is no longer audible goes, then the oldest beyond the cap
	Noises.RemoveAll([Now, Window = Hearing.NoiseWindowSeconds](const FStealthNoiseRecord& Record)
	{
		return !StealthMath::IsFresh(Record.Noise.Time, Now, Window);
	});
	const int32 Cap = FMath::Clamp(Hearing.MaxNoises, 4, 512);
	if (Noises.Num() >= Cap)
	{
		Noises.RemoveAt(0, Noises.Num() - Cap + 1, EAllowShrinking::No);
	}
	FStealthNoiseRecord& Record = Noises.AddDefaulted_GetRef();
	Record.Noise.Instigator = Instigator;
	Record.Noise.Location = Location;
	Record.Noise.RadiusCm = Radius;
	Record.Noise.Kind = Kind;
	Record.Noise.Time = Now;
	Record.bFromPlayer = IsPlayerNoise(Instigator);
	UE_LOG(LogMonkeStealth, Verbose, TEXT("Stealth: %s of %s, %.0f m"), NoiseName(Kind), *GetNameSafe(Instigator), Radius / 100.0f);
#if !UE_BUILD_SHIPPING
	if (CVarStealthDebug.GetValueOnGameThread() > 0)
	{
		UE_LOG(LogMonkeStealth, Log, TEXT("Stealth: %s of %s at %s, heard up to %.0f m%s"), NoiseName(Kind), *GetNameSafe(Instigator),
			*Location.ToCompactString(), Radius / 100.0f, Record.bFromPlayer ? TEXT("") : TEXT(" (not a player: nobody listens)"));
#if ENABLE_DRAW_DEBUG
		const FColor Color = Kind == EStealthNoise::Footstep ? FColor::Yellow : Kind == EStealthNoise::Voice ? FColor::Cyan : FColor::Red;
		DrawDebugCircle(World, Location + FVector(0.0f, 0.0f, 5.0f), Radius, 48, Color, false, Hearing.NoiseWindowSeconds, 0, 3.0f,
			FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), false);
#endif
	}
#endif
}

void UStealthSubsystem::ReportNoiseOfKind(AActor* Instigator, const FVector& Location, EStealthNoise Kind)
{
	ReportNoise(Instigator, Location, GetNoiseRadius(Kind), Kind);
}

void UStealthSubsystem::Report(const UObject* WorldContext, AActor* Instigator, const FVector& Location, EStealthNoise Kind)
{
	if (UStealthSubsystem* Stealth = Get(WorldContext))
	{
		Stealth->ReportNoiseOfKind(Instigator, Location, Kind);
	}
}

bool UStealthSubsystem::FindHeardNoise(const AActor* Listener, float HearingScale, FStealthNoise& OutNoise) const
{
	return IsValid(Listener) && FindHeardNoiseAt(Listener->GetActorLocation(), Listener, HearingScale, OutNoise);
}

bool UStealthSubsystem::FindHeardNoiseAt(const FVector& ListenerLocation, const AActor* ListenerActor, float HearingScale, FStealthNoise& OutNoise) const
{
	const UWorld* World = GetWorld();
	if (!World || Noises.IsEmpty())
	{
		return false;
	}
	const int32 Index = StealthMath::PickHeardNoise(Noises, ListenerLocation, ListenerActor, HearingScale, World->GetTimeSeconds(),
		GetRules().Hearing.NoiseWindowSeconds);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	OutNoise = Noises[Index].Noise;
	return true;
}

EStealthExposure UStealthSubsystem::GetExposure(const ACharacter* Player, bool bRunning) const
{
	if (!IsValid(Player))
	{
		return EStealthExposure::Visible;
	}
	return StealthMath::Exposure(Player->bIsCrouched, IsNight(), bRunning, GetRules().Sight);
}
