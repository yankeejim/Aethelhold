#include "ValleyTerrain.h"

namespace
{
	float Smooth(float Edge0, float Edge1, float X)
	{
		const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
}

FValleyTerrain::FValleyTerrain(int32 InSeed)
	: Seed(InSeed)
{
	FRandomStream Rng(Seed);
	Offset = FVector2D(Rng.FRandRange(-5000.f, 5000.f), Rng.FRandRange(-5000.f, 5000.f));
	Phase1 = Rng.FRandRange(0.f, 2.f * PI);
	Phase2 = Rng.FRandRange(0.f, 2.f * PI);
}

float FValleyTerrain::Noise(float X, float Y, float Scale, int32 Octaves) const
{
	float Sum = 0.f;
	float Amp = 1.f;
	float Norm = 0.f;
	FVector2D P = (FVector2D(X, Y) + Offset) / Scale;
	for (int32 I = 0; I < Octaves; ++I)
	{
		Sum += Amp * FMath::PerlinNoise2D(P);
		Norm += Amp;
		Amp *= 0.5f;
		P = P * 2.03f + FVector2D(17.3f, -9.1f);
	}
	return Sum / Norm;
}

float FValleyTerrain::RiverCenterX(float Y) const
{
	const float V = Y / SizeMeters;
	return SizeMeters * 0.5f
		+ 220.f * FMath::Sin(V * 2.f * PI * 0.75f + Phase1)
		+ 70.f * FMath::Sin(V * 2.f * PI * 2.1f + Phase2);
}

float FValleyTerrain::RiverHalfWidth(float X, float Y) const
{
	return 14.f + 5.f * Noise(X, Y, 180.f, 2);
}

float FValleyTerrain::HeightAt(float X, float Y) const
{
	// Signed distance across the river, corrected for the bend so the channel keeps its width.
	const float Cx = RiverCenterX(Y);
	const float Slope = (RiverCenterX(Y + 1.f) - RiverCenterX(Y - 1.f)) * 0.5f;
	const float Across = (X - Cx) / FMath::Sqrt(1.f + Slope * Slope);
	const float D = FMath::Abs(Across);
	// The water's edge wanders a few meters rather than running dead straight.
	const float HalfWidth = RiverHalfWidth(X, Y) + 3.f * Noise(X, Y, 30.f, 2);

	// How sharply the river bends here, and whether this point is on the inside of the bend.
	// Water cuts into the outside bank (steep, deep) and drops gravel on the inside (a long
	// shallow beach), as real rivers do.
	const float Curve = (RiverCenterX(Y + 25.f) - 2.f * Cx + RiverCenterX(Y - 25.f)) / 625.f;
	const float Bend = FMath::Clamp(FMath::Abs(Curve) / 0.003f, 0.f, 1.f);
	const bool bInside = Across * Curve > 0.f;

	float H;
	if (D < HalfWidth)
	{
		// The deepest water shifts toward the outside of the bend.
		const float U = Across / HalfWidth;
		const float Deep = -FMath::Sign(Curve) * 0.4f * Bend;
		const float Q = (U - Deep) / (U >= Deep ? 1.f - Deep : 1.f + Deep);
		H = -0.4f - 2.6f * (1.f + 0.4f * Bend) * (1.f - Q * Q);
	}
	else
	{
		const float BankWidth = bInside ? FMath::Lerp(10.f, 26.f, Bend) : FMath::Lerp(10.f, 3.5f, Bend);
		const float T = FMath::Clamp((D - HalfWidth) / BankWidth, 0.f, 1.f);
		// Inside: a long, even gravel slope out of the water. Outside: a cut bank that rises at once.
		const float Shape = bInside ? FMath::Pow(T, 0.9f) : FMath::Pow(T, 0.55f);
		const float Bank = FMath::Lerp(Shape, T * T * (3.f - 2.f * T), 1.f - Bend);
		const float Plain = Smooth(HalfWidth + BankWidth, 280.f, D);
		const float Hills = Smooth(220.f, 820.f, D);
		H = FMath::Lerp(-0.4f, 1.4f, Bank)
			+ 9.f * Plain
			+ Hills * (55.f + 45.f * Noise(X, Y, 520.f, 4));
		// Lumps and runnels along the bank where floods have worked it.
		H += 0.3f * Noise(X, Y, 7.f, 2) * FMath::Sin(T * PI);
	}

	// Mountains rise toward the map's edges.
	const float Edge = FMath::Min(FMath::Min(X, SizeMeters - X), FMath::Min(Y, SizeMeters - Y));
	const float Ridge = 1.f - FMath::Abs(Noise(X, Y, 340.f, 4));
	H += Smooth(420.f, 40.f, Edge) * (60.f + 90.f * Ridge * Ridge);

	// Small bumps everywhere except the river bed, rougher on the hills.
	const float Detail = Smooth(HalfWidth + 6.f, HalfWidth + 45.f, D);
	H += Detail * (1.2f * Noise(X, Y, 60.f, 3) + 4.f * Smooth(200.f, 700.f, D) * Noise(X, Y, 110.f, 3));

	return H;
}

FVector2D FValleyTerrain::StartSpot() const
{
	const float Y = SizeMeters * 0.5f;
	return FVector2D(RiverCenterX(Y) + 70.f, Y);
}

float FValleyTerrain::Clumping(float X, float Y) const
{
	return FMath::Clamp(0.5f + 0.9f * Noise(X - 77.f, Y + 413.f, 40.f, 2), 0.f, 1.f);
}

float FValleyTerrain::SlopeAt(float X, float Y) const
{
	const float Dx = HeightAt(X + 1.f, Y) - HeightAt(X - 1.f, Y);
	const float Dy = HeightAt(X, Y + 1.f) - HeightAt(X, Y - 1.f);
	return FMath::Sqrt(Dx * Dx + Dy * Dy) * 0.5f;
}

float FValleyTerrain::DistanceFromWater(float X, float Y) const
{
	const float Slope = (RiverCenterX(Y + 1.f) - RiverCenterX(Y - 1.f)) * 0.5f;
	const float D = FMath::Abs(X - RiverCenterX(Y)) / FMath::Sqrt(1.f + Slope * Slope);
	return D - RiverHalfWidth(X, Y);
}

float FValleyTerrain::ForestDensity(float X, float Y) const
{
	const float H = HeightAt(X, Y);
	if (H < 0.5f)
	{
		return 0.f;
	}
	// Patches: big woods with ragged edges, mostly on rising ground.
	const float Patch = 0.5f + 0.5f * Noise(X + 731.f, Y - 219.f, 300.f, 4);
	const float Rising = Smooth(4.f, 22.f, H);
	float F = Smooth(0.42f - 0.25f * Rising, 0.6f - 0.25f * Rising, Patch);
	// Thinning toward the bare upper mountains, and on slopes too steep for roots.
	F *= Smooth(170.f, 120.f, H) * Smooth(1.1f, 0.6f, SlopeAt(X, Y));
	// The founders' clearing.
	const FVector2D Start = StartSpot();
	F *= Smooth(140.f, 260.f, FVector2D::Distance(FVector2D(X, Y), Start));
	return FMath::Clamp(F, 0.f, 1.f);
}
