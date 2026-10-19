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

float FValleyTerrain::HeightAt(float X, float Y) const
{
	// Distance across the river, corrected for the bend so the channel keeps its width.
	const float Slope = (RiverCenterX(Y + 1.f) - RiverCenterX(Y - 1.f)) * 0.5f;
	const float D = FMath::Abs(X - RiverCenterX(Y)) / FMath::Sqrt(1.f + Slope * Slope);
	const float HalfWidth = 14.f + 5.f * Noise(X, Y, 180.f, 2);

	float H;
	if (D < HalfWidth)
	{
		const float T = D / HalfWidth;
		H = -0.4f - 2.6f * (1.f - T * T);
	}
	else
	{
		const float Bank = Smooth(HalfWidth, HalfWidth + 14.f, D);
		const float Plain = Smooth(HalfWidth + 14.f, 280.f, D);
		const float Hills = Smooth(220.f, 820.f, D);
		H = FMath::Lerp(-0.4f, 1.4f, Bank)
			+ 9.f * Plain
			+ Hills * (55.f + 45.f * Noise(X, Y, 520.f, 4));
	}

	// Mountains rise toward the map's edges.
	const float Edge = FMath::Min(FMath::Min(X, SizeMeters - X), FMath::Min(Y, SizeMeters - Y));
	const float Ridge = 1.f - FMath::Abs(Noise(X, Y, 340.f, 4));
	H += Smooth(420.f, 40.f, Edge) * (60.f + 90.f * Ridge * Ridge);

	// Small bumps everywhere except the river bed, rougher on the hills.
	const float Detail = Smooth(HalfWidth, HalfWidth + 40.f, D);
	H += Detail * (1.2f * Noise(X, Y, 60.f, 3) + 4.f * Smooth(200.f, 700.f, D) * Noise(X, Y, 110.f, 3));

	return H;
}

FVector2D FValleyTerrain::StartSpot() const
{
	const float Y = SizeMeters * 0.5f;
	return FVector2D(RiverCenterX(Y) + 70.f, Y);
}
