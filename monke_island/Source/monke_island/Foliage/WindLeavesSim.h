// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Pure motion of the leaves carried by the wind, shared by AWindLeavesActor and the specs
 *  (monke_island.Foliage.WindLeaves). Unreal units: cm, cm/s, s.
 */
namespace WindLeavesSim
{
	enum class EState : uint8
	{
		Dead,
		Flying,
		Landed,
		Floating
	};

	struct FLeaf
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector SpinAxis = FVector::UpVector;
		/** Tumbling speed (rad/s) */
		float Spin = 0.0f;
		float Scale = 1.0f;
		float Age = 0.0f;
		float Life = 10.0f;
		/** Ground height below the leaf (cm) */
		float GroundZ = -1.0e6f;
		/** Random number in [0, 1) of this leaf: own swirl, own pace */
		float Seed = 0.0f;
		/** Leaf mesh (index in the rules' meshes) */
		int32 Kind = 0;
		EState State = EState::Dead;
	};

	struct FAir
	{
		/** Mean wind velocity (cm/s) */
		FVector Wind = FVector::ZeroVector;
		/** Swirl speed (cm/s) */
		float Turbulence = 0.0f;
		/** Swirl size (cm) */
		float TurbulenceSize = 450.0f;
		float FallSpeed = 85.0f;
		/** 1/s */
		float Response = 1.8f;
		/** Water surface height (cm), very low when there is no water */
		float WaterZ = -1.0e6f;
		float WaterDrift = 0.05f;
		/** Clock (s) */
		float Time = 0.0f;
	};

	/** Uniform scale of a leaf at its age: grows in at birth, shrinks out at the end of its life */
	inline float FadeScale(const FLeaf& Leaf)
	{
		const float In = FMath::Clamp(Leaf.Age / 0.4f, 0.0f, 1.0f);
		const float Out = FMath::Clamp((Leaf.Life - Leaf.Age) / 1.5f, 0.0f, 1.0f);
		return Leaf.Scale * In * Out;
	}

	/** Air velocity felt by a leaf: mean wind (each leaf a little faster or slower), swirls, slow fall */
	inline FVector AirVelocity(const FLeaf& Leaf, const FAir& Air)
	{
		const FVector P = Leaf.Location / FMath::Max(Air.TurbulenceSize, 10.0f) + FVector(Leaf.Seed * 37.0f, Leaf.Seed * 11.0f, Air.Time * 0.35f);
		const FVector Swirl(
			FMath::PerlinNoise3D(P),
			FMath::PerlinNoise3D(P + FVector(31.4f, 7.1f, 3.3f)),
			FMath::PerlinNoise3D(P + FVector(-12.7f, 23.9f, 9.1f)) * 0.6f);
		// a tumbling leaf glides and drops in turns
		const float Flutter = 0.55f + 0.9f * FMath::Abs(FMath::Sin(Air.Time * 2.3f + Leaf.Seed * 40.0f));
		return Air.Wind * (0.75f + 0.5f * Leaf.Seed) + Swirl * (2.0f * Air.Turbulence) - FVector(0.0f, 0.0f, Air.FallSpeed * Flutter);
	}

	/** Advances a living leaf by DeltaSeconds; returns false once it is dead */
	inline bool Step(FLeaf& Leaf, const FAir& Air, float DeltaSeconds)
	{
		if (Leaf.State == EState::Dead)
		{
			return false;
		}
		Leaf.Age += DeltaSeconds;
		if (Leaf.Age >= Leaf.Life)
		{
			Leaf.State = EState::Dead;
			return false;
		}

		const float WindSpeed = static_cast<float>(Air.Wind.Size2D());
		switch (Leaf.State)
		{
		case EState::Flying:
		{
			const FVector Target = AirVelocity(Leaf, Air);
			Leaf.Velocity += (Target - Leaf.Velocity) * (1.0f - FMath::Exp(-Air.Response * DeltaSeconds));
			Leaf.Location += Leaf.Velocity * DeltaSeconds;
			const float Spin = Leaf.Spin * (0.4f + FMath::Min(1.5f, WindSpeed / 600.0f));
			Leaf.Rotation = (FQuat(Leaf.SpinAxis, Spin * DeltaSeconds) * Leaf.Rotation).GetNormalized();

			const float Surface = FMath::Max(Leaf.GroundZ, Air.WaterZ);
			if (Leaf.Location.Z <= Surface + 1.0)
			{
				// lands flat, keeping its heading
				Leaf.Location.Z = Surface + 1.0;
				Leaf.Velocity = FVector::ZeroVector;
				Leaf.Rotation = FRotator(0.0, Leaf.Rotation.Rotator().Yaw, 0.0).Quaternion();
				Leaf.State = Air.WaterZ > Leaf.GroundZ ? EState::Floating : EState::Landed;
				// a fallen leaf stays a few seconds, then fades
				Leaf.Life = FMath::Min(Leaf.Life, Leaf.Age + 2.0f + 4.0f * Leaf.Seed);
			}
			break;
		}
		case EState::Floating:
			Leaf.Location += FVector(Air.Wind.X, Air.Wind.Y, 0.0) * (Air.WaterDrift * DeltaSeconds);
			Leaf.Location.Z = Air.WaterZ + 1.0 + 1.5 * FMath::Sin(Air.Time * 1.7f + Leaf.Seed * 20.0f);
			break;
		default:
			break;
		}
		return true;
	}

	/** Birth point of a leaf: in a disc around the centre, pushed upwind so the leaves cross the view */
	inline FVector2D SpawnPoint(const FVector2D& Center, const FVector2D& WindDirection, float Radius, float Angle01, float Distance01)
	{
		const float Angle = Angle01 * UE_TWO_PI;
		const FVector2D Disc = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * (Radius * FMath::Sqrt(Distance01));
		return Center + Disc - WindDirection.GetSafeNormal() * (0.45f * Radius);
	}
}
