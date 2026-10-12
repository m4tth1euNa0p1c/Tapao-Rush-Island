// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Capture/CaptureTypes.h"
#include "CaptureSubsystem.generated.h"

class ACaptureCage;
class ACapturePrisoner;
class APawn;

/**
 *  Capture of prisoners (T-336) in a world: the cages set down and the prisoners, registered by themselves (no actor
 *  search: ARC-05), the targets a pair of handcuffs is closing on (server: one at a time, §12 n°2), and what the HUD of
 *  the local player shows (UI/Tapao/STapaoHUD reads GetHudLine): a short message of the capture (sale, capture, escape,
 *  refusal) for a few seconds, else the prompt of the capture item in hand, else a reminder of the prisoner led.
 */
UCLASS()
class MONKE_ISLAND_API UCaptureSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** The subsystem of the world of an object, nullptr without a game world */
	static UCaptureSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Deinitialize() override;

	/** A cage begins or ends (every machine) */
	void RegisterCage(ACaptureCage* Cage);
	void UnregisterCage(ACaptureCage* Cage);

	/** A prisoner begins or ends (every machine) */
	void RegisterPrisoner(ACapturePrisoner* Prisoner);
	void UnregisterPrisoner(ACapturePrisoner* Prisoner);

	/** Nearest cage to Location within Radius (cm, horizontal), Ignore left out; nullptr when none */
	ACaptureCage* FindCageNear(const FVector& Location, float Radius, const ACaptureCage* Ignore = nullptr) const;

	/** Prisoner on the chain of this pawn, nullptr when none */
	ACapturePrisoner* FindPrisonerLedBy(const APawn* Jailer) const;

	/** A led prisoner within Radius of Location (cm, horizontal), nullptr when none */
	ACapturePrisoner* FindLedPrisonerNear(const FVector& Location, float Radius) const;

	/** Server: Claimer starts closing handcuffs on Target; false when someone else is at it already */
	bool ClaimTarget(const APawn* Target, const AActor* Claimer);

	/** Server: the claim of Claimer on Target ends (done or cut) */
	void ReleaseClaim(const APawn* Target, const AActor* Claimer);

	/** Server: someone other than Claimer is closing handcuffs on Target */
	bool IsClaimedByOther(const APawn* Target, const AActor* Claimer) const;

	/** This machine: a short message for the local player, shown Seconds by the HUD (the last one wins) */
	void PostNotice(const FText& Text, float Seconds, bool bWarning);

	/** This machine: the line the HUD shows for the local pawn; false when there is nothing to show */
	bool GetHudLine(const APawn* LocalPawn, FCaptureHudLine& Out) const;

	/** Seconds a message of the capture stays on the HUD */
	static constexpr float NoticeSeconds = 4.0f;

private:

	/** Cages and prisoners of the world */
	TArray<TWeakObjectPtr<ACaptureCage>> Cages;
	TArray<TWeakObjectPtr<ACapturePrisoner>> Prisoners;

	/** Server: target being cuffed -> the handcuffs at it */
	TMap<TWeakObjectPtr<const APawn>, TWeakObjectPtr<const AActor>> Claims;

	/** The line of the HUD, made once per frame (the widgets read it several times) */
	bool MakeHudLine(const APawn* LocalPawn, FCaptureHudLine& Out) const;

	/** The message on screen and the world time it goes */
	FText NoticeText;
	double NoticeEndTime = -1.0;
	bool bNoticeWarning = false;

	/** The line of the HUD of this frame, for that pawn */
	mutable FCaptureHudLine CachedLine;
	mutable bool bCachedLine = false;
	mutable uint64 CachedFrame = MAX_uint64;
	mutable TWeakObjectPtr<const APawn> CachedPawn;
};
