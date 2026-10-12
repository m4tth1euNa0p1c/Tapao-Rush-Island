// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Village/VillageMath.h"

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
	/** First reason a candidate cannot be placed (also the keys of the placement summary) */
	enum class ETotemReject : uint8
	{
		None, Invalid, Cap, Spacing, Quiet, Building, NoGround, Water, Slope, Bush, Spawn, Count
	};

	/** Finite initial-placement request, with no UObject ownership */
	struct FTotemRequest
	{
		FVector Location = FVector::ZeroVector;
		float Yaw = 0.0f;
		bool bHouse = false;
	};

	/** Pure jittered darts for a Poisson disk sample. A dart reserves no space until EvaluateTotemCandidate accepts
	 *  its traced ground: rejected water cannot starve narrow/disconnected strips of land. Bounded CPU and memory. */
	inline TArray<FVector> GroundTotemCandidates(const FBox2D& Bounds, float Spacing, int32 Seed, float Jitter, int32 Budget)
	{
		TArray<FVector> Points;
		if (!Bounds.bIsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN() || !FMath::IsFinite(Spacing)
			|| Spacing <= 0.0f || !FMath::IsFinite(Jitter) || Budget <= 0)
		{
			return Points;
		}
		const FVector2D Size = Bounds.GetSize();
		if (Size.X <= 0.0 || Size.Y <= 0.0)
		{
			return Points;
		}
		const double CellSize = Spacing / FMath::Sqrt(2.0);
		const int32 NX = FMath::CeilToInt(FMath::Clamp(Size.X / CellSize, 1.0, 1000000.0));
		const int32 NY = FMath::CeilToInt(FMath::Clamp(Size.Y / CellSize, 1.0, 1000000.0));
		const float Amount = FMath::Clamp(Jitter, 0.0f, 1.0f);
		FRandomStream Stream(Seed);
		Budget = FMath::Clamp(Budget, 0, 20000);
		Points.Reserve(Budget);
		for (int32 Index = 0; Index < Budget; ++Index)
		{
			const int32 X = Stream.RandHelper(NX);
			const int32 Y = Stream.RandHelper(NY);
			const double U = (X + 0.5 + (Stream.FRand() - 0.5) * Amount) / NX;
			const double V = (Y + 0.5 + (Stream.FRand() - 0.5) * Amount) / NY;
			Points.Emplace(Bounds.Min.X + Size.X * U, Bounds.Min.Y + Size.Y * V, 0.0);
		}
		return Points;
	}

	/** Pure clearance test shared by ground and indoor placement. Houses bypass their own footprint, never quiet zones */
	inline ETotemReject TotemExclusion(const FVector& Point, const TArray<FSphere>& Quiet,
		const TArray<FVillageClearArea>& Areas, float Margin, bool bHouse)
	{
		if (Point.ContainsNaN())
		{
			return ETotemReject::Invalid;
		}
		for (const FSphere& Zone : Quiet)
		{
			if (FVector::DistSquared2D(Point, Zone.Center) <= FMath::Square(Zone.W))
			{
				return ETotemReject::Quiet;
			}
		}
		if (!bHouse)
		{
			for (const FVillageClearArea& Area : Areas)
			{
				if ((Area.Kind == EVillageClearKind::Building || Area.Kind == EVillageClearKind::Plaza)
					&& VillageMath::IsInArea(Area, FVector2D(Point), FMath::Max(0.0f, Margin)))
				{
					return ETotemReject::Building;
				}
			}
		}
		return ETotemReject::None;
	}

	/** Validate the measured surface without a world. Accepted ground points form a capped Poisson disk sample in XY */
	inline ETotemReject EvaluateTotemCandidate(const FVector& Point, bool bHasGround, float WaterZ, float Clearance,
		float NormalZ, float MinNormalZ, const TArray<FVector>& Taken, float Spacing, int32 MaxCount,
		const TArray<FSphere>& Quiet, const TArray<FVillageClearArea>& Areas, float Margin, bool bHouse = false)
	{
		if (Point.ContainsNaN() || !FMath::IsFinite(WaterZ) || !FMath::IsFinite(NormalZ)
			|| !FMath::IsFinite(Spacing) || !FMath::IsFinite(Clearance))
		{
			return ETotemReject::Invalid;
		}
		const ETotemReject Excluded = TotemExclusion(Point, Quiet, Areas, Margin, bHouse);
		if (Excluded != ETotemReject::None)
		{
			return Excluded;
		}
		if (!bHasGround) { return ETotemReject::NoGround; }
		if (Point.Z < WaterZ + FMath::Max(0.0f, Clearance)) { return ETotemReject::Water; }
		if (NormalZ < MinNormalZ) { return ETotemReject::Slope; }
		if (!bHouse)
		{
			if (MaxCount <= Taken.Num()) { return ETotemReject::Cap; }
			for (const FVector& Other : Taken)
			{
				if (FVector::DistSquared2D(Point, Other) < FMath::Square(static_cast<double>(Spacing)))
				{
					return ETotemReject::Spacing;
				}
			}
		}
		return ETotemReject::None;
	}

	/** House base Z is kept for a short interior floor trace. Offsets rotate with the house, away from the mesh pivot */
	inline FVector HouseTotemPoint(const FTransform& House, const FBox& MeshBounds, const FVector& Offset, float Inset)
	{
		const FVector Center = MeshBounds.GetCenter();
		const FVector Extent = MeshBounds.GetExtent();
		const double X = FMath::Max(0.0, Extent.X - FMath::Max(0.0f, Inset));
		const double Y = FMath::Max(0.0, Extent.Y - FMath::Max(0.0f, Inset));
		return House.TransformPosition(FVector(Center.X + FMath::Clamp(Offset.X, -X, X),
			Center.Y + FMath::Clamp(Offset.Y, -Y, Y), 0.0));
	}

	/** Pure seeded orientation; reversed bounds are normalized */
	inline float HouseTotemYaw(float HouseYaw, const FVector2D& Range, float Random01)
	{
		return HouseYaw + FMath::Lerp(FMath::Min(Range.X, Range.Y), FMath::Max(Range.X, Range.Y),
			static_cast<double>(FMath::Clamp(Random01, 0.0f, 1.0f)));
	}

	/** Move a spawned mesh so its lowest bound rests on the measured floor, even with a centred pivot */
	inline FVector RestTotemOnFloor(const FVector& ActorLocation, double BoundsBottom, double FloorZ, float Clearance)
	{
		return ActorLocation + FVector(0.0, 0.0, FloorZ + FMath::Max(0.0f, Clearance) - BoundsBottom);
	}

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

	/** A one-shot action received Late seconds after the server started it (T-306, review R-27): it plays Duration
	 *  seconds in all (0: the clip once at Rate), so it starts OutStart seconds into its clip (wrapped when it loops) and
	 *  plays OutLeft more seconds, ending when the server's does; false once it is over (nothing plays) */
	inline bool LateActionStart(float Late, float Duration, float Rate, float ClipLength, bool bLoop, float& OutStart, float& OutLeft)
	{
		const float SafeRate = FMath::Max(Rate, 0.05f);
		const float SafeClip = FMath::Max(ClipLength, KINDA_SMALL_NUMBER);
		const float Total = Duration > 0.0f ? Duration : SafeClip / SafeRate;
		const float Behind = FMath::Max(Late, 0.0f);
		OutStart = 0.0f;
		OutLeft = 0.0f;
		if (Behind >= Total)
		{
			return false;
		}
		OutLeft = Total - Behind;
		OutStart = bLoop ? FMath::Fmod(Behind * SafeRate, SafeClip) : FMath::Min(Behind * SafeRate, SafeClip);
		return true;
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

	/** A hostile count scaled by the time of day: rounded, never below one when the base is not zero */
	inline int32 ScaleSpawnCount(int32 Base, float Scale)
	{
		return Base <= 0 ? 0 : FMath::Max(1, FMath::RoundToInt(Base * FMath::Max(0.0f, Scale)));
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

	/** Speed of a dead body bounded by MaxSpeed (cm/s), direction kept; none when MaxSpeed is 0 (T-151) */
	inline FVector BoundedVelocity(const FVector& Velocity, float MaxSpeed)
	{
		return MaxSpeed <= 0.0f ? FVector::ZeroVector : Velocity.GetClampedToMaxSize(MaxSpeed);
	}

	/** Vertical move that lays a corpse on the ground: its lowest point onto GroundZ, nothing within Tolerance
	 *  (positive lifts a body sunk in the ground, negative lowers a body floating above it) */
	inline float GroundSnapOffset(float LowestZ, float GroundZ, float Tolerance)
	{
		const float Gap = GroundZ - LowestZ;
		return FMath::Abs(Gap) > FMath::Max(0.0f, Tolerance) ? Gap : 0.0f;
	}

	/** Vertical move that lays a frozen corpse down before it simulates (T-151): its lowest collision point Clearance
	 *  above GroundZ. bLower false only lifts a part out of the ground (a flung piece falls on its own) */
	inline float CorpseRestOffset(float LowestZ, float GroundZ, float Clearance, bool bLower)
	{
		const float Offset = GroundZ + FMath::Max(0.0f, Clearance) - LowestZ;
		return bLower ? Offset : FMath::Max(0.0f, Offset);
	}

	/** Move of a frozen corpse toward the ground (T-151): never up (a mesh touching the ground stays), nothing within
	 *  DeadZone, else down to the ground exactly (never under it) */
	inline float FrozenSettleOffset(float LowestZ, float GroundZ, float DeadZone)
	{
		const float Gap = GroundZ - LowestZ;
		return Gap < -FMath::Max(0.0f, DeadZone) ? Gap : 0.0f;
	}

	/** Part of a remaining descent (negative, cm) done in Delta at Speed (cm/s): slow, never past the end */
	inline float SettleStep(float Remaining, float Speed, float Delta)
	{
		return Remaining >= 0.0f ? 0.0f : FMath::Max(Remaining, -FMath::Max(0.0f, Speed) * FMath::Max(0.0f, Delta));
	}

	/** Speed a corpse starts with: none when frozen (no jump), else bounded */
	inline FVector CorpseSpawnVelocity(const FVector& Velocity, float MaxSpeed, bool bFrozen)
	{
		return bFrozen ? FVector::ZeroVector : BoundedVelocity(Velocity, MaxSpeed);
	}

	/** Speed a cut piece (a limb, a half) starts with (T-155): flat, along the blow, at most MaxSpeed (cm/s); it only
	 *  drifts a little and falls by its weight at the foot of the body, never thrown up */
	inline FVector CutPieceVelocity(const FVector& Velocity, float MaxSpeed)
	{
		return BoundedVelocity(FVector(Velocity.X, Velocity.Y, 0.0), MaxSpeed);
	}

	/** Speed of a corpse or a piece after a blow (an impulse giving it Change, cm/s) (T-155): the blow pushes it to
	 *  MaxSpeed at most, and never speeds up a body already faster than that (a fall keeps its speed) */
	inline FVector BoundedBlowVelocity(const FVector& Current, const FVector& Change, float MaxSpeed)
	{
		if (MaxSpeed <= 0.0f)
		{
			return Current;
		}
		const FVector After = Current + Change;
		return Current.SizeSquared() >= FMath::Square(MaxSpeed) ? Current : After.GetClampedToMaxSize(MaxSpeed);
	}

	/** True when a fresh piece may go to sleep (T-155): slower than RestSpeed, MinAge after it appeared (time to fall) */
	inline bool IsPieceAtRest(float Speed, float Age, float RestSpeed, float MinAge)
	{
		return Age >= MinAge && Speed <= RestSpeed;
	}

	/** Water above a corpse (cm): from its lowest point, never from below the ground it lies on (bHasGround), so a
	 *  sea lower than the dry ground never covers it, whatever the bounds say (T-151) */
	inline float CorpseWaterDepth(float SurfaceZ, float LowestZ, bool bHasGround, float GroundZ)
	{
		return SurfaceZ - (bHasGround ? FMath::Max(LowestZ, GroundZ) : LowestZ);
	}

	/** True when the water covers a corpse deeper than FloodDepth: it is washed away */
	inline bool IsCorpseWashedAway(float WaterDepth, float FloodDepth)
	{
		return WaterDepth > FloodDepth;
	}
}
