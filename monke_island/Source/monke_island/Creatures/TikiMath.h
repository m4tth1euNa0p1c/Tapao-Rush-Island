// monke_island

#pragma once

#include "CoreMinimal.h"

/** Locomotion clip a creature plays for its speed */
enum class ETikiLocomotion : uint8
{
	Idle,
	Walk,
	Run
};

/** What a villager does at a point of interest of its village (marker tag "Activity.<name>") */
enum class ETikiActivity : uint8
{
	None,
	/** Well, storehouse door, yard: the basket clip, facing the decor */
	Work,
	/** Notice board: stands still in front of it */
	Read,
	/** Shrine: kneels */
	Pray,
	/** In front of the engawa: sits on its heels */
	Rest,
	/** Fire watchtower: stands and watches */
	Watch,
	/** Around the square: talks, the basket offered */
	Chat
};

/** Pure rules of the tiki creatures, tested without a world */
namespace TikiMath
{
	/** Activity of a point of interest from its tag value ("work", "pray"...), None when unknown */
	inline ETikiActivity ActivityFromName(const FName& Name)
	{
		if (Name == FName(TEXT("work")))
		{
			return ETikiActivity::Work;
		}
		if (Name == FName(TEXT("read")))
		{
			return ETikiActivity::Read;
		}
		if (Name == FName(TEXT("pray")))
		{
			return ETikiActivity::Pray;
		}
		if (Name == FName(TEXT("rest")))
		{
			return ETikiActivity::Rest;
		}
		if (Name == FName(TEXT("watch")))
		{
			return ETikiActivity::Watch;
		}
		if (Name == FName(TEXT("chat")))
		{
			return ETikiActivity::Chat;
		}
		return ETikiActivity::None;
	}

	/** True when a player stands within Radius (2D) of a village centre: its villagers follow their routine */
	inline bool IsAnyWithin(const FVector& Center, const TArray<FVector>& Players, float Radius)
	{
		const double RadiusSq = FMath::Square(static_cast<double>(Radius));
		for (const FVector& Player : Players)
		{
			if (FVector::DistSquared2D(Player, Center) <= RadiusSq)
			{
				return true;
			}
		}
		return false;
	}

	/** Where the Index-th of Count alarm warriors comes from: evenly spread on the ring of radius Radius around Center
	 *  (the village centre for an alarm, the killer for a kill), the whole ring turned by Phase in [0, 1) (Z kept) */
	inline FVector AlarmRingPoint(const FVector& Center, float Radius, int32 Index, int32 Count, float Phase)
	{
		const float Angle = 2.0f * PI * (static_cast<float>(Index) + Phase) / static_cast<float>(FMath::Max(Count, 1));
		return Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0f);
	}

	/** Monsters a bush zone hides: none under MinBushes, then one, plus one per BushesPerMonster, up to MaxPerZone */
	inline int32 AmbushSlots(int32 Bushes, int32 MinBushes, int32 BushesPerMonster, int32 MaxPerZone)
	{
		if (Bushes < FMath::Max(MinBushes, 1))
		{
			return 0;
		}
		return FMath::Clamp(1 + Bushes / FMath::Max(BushesPerMonster, 1), 1, FMath::Max(MaxPerZone, 1));
	}

	/** True when Point is at least MinSpacing (2D) from every point already taken */
	inline bool IsFarFromAll(const FVector& Point, const TArray<FVector>& Taken, float MinSpacing)
	{
		const double SpacingSq = FMath::Square(static_cast<double>(MinSpacing));
		for (const FVector& Other : Taken)
		{
			if (FVector::DistSquared2D(Point, Other) < SpacingSq)
			{
				return false;
			}
		}
		return true;
	}

	/** Indices 0..Count-1 in a random order drawn from Stream, each once */
	inline TArray<int32> ShuffledIndices(int32 Count, FRandomStream& Stream)
	{
		TArray<int32> Order;
		Order.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Order.Add(Index);
		}
		for (int32 Index = Count - 1; Index > 0; --Index)
		{
			Order.Swap(Index, Stream.RandRange(0, Index));
		}
		return Order;
	}

	/** Warriors to bring out against a player: Wanted, but never more than MaxAlive hunting it with the Alive ones */
	inline int32 MissingWarriors(int32 Wanted, int32 MaxAlive, int32 Alive)
	{
		return FMath::Max(0, FMath::Min(Wanted, MaxAlive - Alive));
	}

	/** Position in a clip after Delta seconds: wraps when it loops, holds the last frame otherwise */
	inline float AdvanceClip(float Time, float Delta, float Length, bool bLoop)
	{
		if (Length <= 0.0f)
		{
			return 0.0f;
		}
		const float Next = Time + Delta;
		return bLoop ? FMath::Fmod(FMath::Fmod(Next, Length) + Length, Length) : FMath::Clamp(Next, 0.0f, Length);
	}

	/** Speed of a locomotion clip so the feet keep up with the body: the body speed over the speed the clip was
	 *  authored for, within [MinRate, MaxRate] */
	inline float ClipRate(float Speed, float ClipSpeed, float MinRate, float MaxRate)
	{
		return ClipSpeed > 1.0f ? FMath::Clamp(Speed / ClipSpeed, MinRate, MaxRate) : 1.0f;
	}

	/** True when a villager running home goes through the gate of its plot first: it is farther from the door
	 *  than the gate is (outside the fence) */
	inline bool ShouldPassGate(const FVector& Villager, const FVector& Door, const FVector& Gate)
	{
		return FVector::DistSquared2D(Villager, Door) > FVector::DistSquared2D(Gate, Door);
	}

	/** True when a blow travelling along BlowDirection (from the attacker to the creature) came from behind it */
	inline bool IsBlowFromBehind(const FVector& BlowDirection, const FVector& Forward)
	{
		return FVector::DotProduct(BlowDirection.GetSafeNormal2D(), Forward.GetSafeNormal2D()) > 0.3f;
	}

	/** Idle under 10 cm/s, walk up to 1.3 x the walking speed, run above */
	inline ETikiLocomotion ChooseLocomotion(float Speed, float WalkSpeed)
	{
		if (Speed < 10.0f)
		{
			return ETikiLocomotion::Idle;
		}
		return Speed <= WalkSpeed * 1.3f ? ETikiLocomotion::Walk : ETikiLocomotion::Run;
	}

	/** True when the blow can start: in range and the cooldown is over */
	inline bool CanStartAttack(float Distance, float Range, double Now, double LastAttackTime, float Cooldown)
	{
		return Distance <= Range && Now - LastAttackTime >= Cooldown;
	}

	/** Index of the point nearest to Origin within MaxDistance, or INDEX_NONE */
	inline int32 NearestIndex(const TArray<FVector>& Points, const FVector& Origin, float MaxDistance)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = FMath::Square(static_cast<double>(MaxDistance));
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const double DistSq = FVector::DistSquared(Points[Index], Origin);
			if (DistSq <= BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Index;
			}
		}
		return Best;
	}

	/** True when a name contains one of the keywords (case insensitive) */
	inline bool HasKeyword(const FString& Name, const TArray<FString>& Keywords)
	{
		for (const FString& Keyword : Keywords)
		{
			if (!Keyword.IsEmpty() && Name.Contains(Keyword, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** True when a mesh name contains one of the bush keywords (case insensitive) */
	inline bool IsBushMeshName(const FString& MeshName, const TArray<FString>& Keywords)
	{
		return HasKeyword(MeshName, Keywords);
	}

	/** Point on the ring [MinRadius, MaxRadius] around Center for an angle and a fraction in [0, 1] (Z kept) */
	inline FVector PointOnRing(const FVector& Center, float AngleRad, float Fraction, float MinRadius, float MaxRadius)
	{
		const float Radius = FMath::Lerp(MinRadius, MaxRadius, FMath::Clamp(Fraction, 0.0f, 1.0f));
		return Center + FVector(FMath::Cos(AngleRad) * Radius, FMath::Sin(AngleRad) * Radius, 0.0f);
	}

	/** Value of a "Prefix.Value" actor tag ("Village.V01" -> "V01", case insensitive prefix), NAME_None when absent */
	inline FName TagValue(const TArray<FName>& Tags, const FString& Prefix)
	{
		const FString Head = Prefix + TEXT(".");
		for (const FName& Tag : Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.Len() > Head.Len() && Text.StartsWith(Head, ESearchCase::IgnoreCase))
			{
				return FName(*Text.Mid(Head.Len()));
			}
		}
		return NAME_None;
	}

	/** Index of a spawn point at least MinGap (2D) away from every occupied position, drawn among the free ones with
	 *  Random01 in [0, 1); INDEX_NONE when they are all taken */
	inline int32 PickFreeSpawn(const TArray<FVector>& Points, const TArray<FVector>& Occupied, float MinGap, float Random01)
	{
		TArray<int32, TInlineAllocator<64>> Free;
		const double GapSq = FMath::Square(static_cast<double>(MinGap));
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			bool bTaken = false;
			for (const FVector& Other : Occupied)
			{
				if (FVector::DistSquared2D(Points[Index], Other) < GapSq)
				{
					bTaken = true;
					break;
				}
			}
			if (!bTaken)
			{
				Free.Add(Index);
			}
		}
		if (Free.IsEmpty())
		{
			return INDEX_NONE;
		}
		return Free[FMath::Clamp(FMath::FloorToInt32(Random01 * Free.Num()), 0, Free.Num() - 1)];
	}

	/** True when a guardian strayed farther than Leash x PostRadius from its post and must walk back */
	inline bool IsOffPost(const FVector& Location, const FVector& Post, float PostRadius, float Leash = 1.5f)
	{
		return FVector::Dist2D(Location, Post) > PostRadius * Leash;
	}

	/** A land creature may step where the water is at most MaxDepth deep, or anywhere shallower than where it
	 *  stands (a tiki pushed into the sea walks back out) */
	inline bool CanWade(float DepthAhead, float DepthHere, float MaxDepth)
	{
		return DepthAhead <= MaxDepth || DepthAhead < DepthHere - 1.0f;
	}

	/** Where a dropped weapon pickup appears (its mesh in grip space, long axis +Z): at the grip of the bone in the world,
	 *  +Z along the held weapon, from the grip toward MiddleInBone (the middle of the weapon at rest, in the bone frame) */
	inline FTransform WeaponDropTransform(const FTransform& GripWorld, const FVector& MiddleInBone, const FVector& Scale)
	{
		const FQuat Bone = GripWorld.GetRotation();
		const FVector Axis = MiddleInBone.IsNearlyZero() ? Bone.GetUpVector() : GripWorld.TransformVectorNoScale(MiddleInBone.GetSafeNormal());
		// the roll around the weapon follows the bone axis the least aligned with it
		const FVector Side = FMath::Abs(FVector::DotProduct(Axis, Bone.GetForwardVector())) < 0.9f ? Bone.GetForwardVector() : Bone.GetRightVector();
		return FTransform(FRotationMatrix::MakeFromZX(Axis, Side).ToQuat(), GripWorld.GetLocation(), Scale);
	}
}
