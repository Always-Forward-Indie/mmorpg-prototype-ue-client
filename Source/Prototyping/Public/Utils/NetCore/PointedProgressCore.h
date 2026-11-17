#pragma once

#include "CoreMinimal.h"

/**
 * Pure math for the pointed progress bar (<=====>).
 *
 * Single source of truth for the shape used by M_UI_ProgressPointed and
 * UPointedProgressWidget. No UObject, no UWorld, no materials here, only
 * bar-space math so the Automation spec can pin it. The material implements
 * the same formulas with SmoothStep AA on top.
 *
 * Bar space is height-normalized: V in 0..1, U scaled by Aspect = W/H so one
 * unit is square. Tips use h = 0.5*t*(1.4-0.4t): a defined (crisp but
 * kink-free) corner at the tip junction plus a smooth nose, never needle-sharp.
 */
struct PROTOTYPING_API FPointedProgressCore
{
	/** Clamp any progress-like value to [0..1]. */
	static float Clamp01(float V)
	{
		return FMath::Clamp(V, 0.0f, 1.0f);
	}

	/** Tip parameter: saturate(edgeH / TipLenH), 0 at the nose, 1 inside. */
	static float TipT(float U, float TipLenH, float Aspect)
	{
		if (TipLenH <= 0.0f)
		{
			return 1.0f;
		}
		const float EdgeH = FMath::Min(U, 1.0f - U) * Aspect;
		return FMath::Clamp(EdgeH / TipLenH, 0.0f, 1.0f);
	}

	/** Half height of the <=====> hull at U (0 at the nose, 0.5 inside). */
	static float ShapeHalfHeight(float U, float TipLenH, float Aspect)
	{
		const float T = TipT(U, TipLenH, Aspect);
		return 0.5f * T * (1.4f - 0.4f * T);
	}

	/** Hard inside test (no AA). True when (U,V) is inside the hull. */
	static bool IsInsideShape(float U, float V, float TipLenH, float Aspect)
	{
		return FMath::Abs(V - 0.5f) <= ShapeHalfHeight(U, TipLenH, Aspect);
	}

	/** Hard fill test (no AA): filled when U is left of Progress. */
	static bool IsFilled(float U, float Progress)
	{
		return U <= Progress;
	}

	/** Inner fill factor: 1 deep inside, ramping to 0 within BorderH of the hull
	 * edge (hard, no-AA analog of the shader SDF band). Single-hull ring, so the
	 * width is uniform by construction - the old two-curve ring pinched at the
	 * tip junction. */
	static float InnerFactor(float U, float V, float TipLenH, float Aspect, float BorderH)
	{
		const float H = ShapeHalfHeight(U, TipLenH, Aspect);
		const float Dy = FMath::Abs(V - 0.5f);
		return FMath::Clamp((H - BorderH - Dy) / FMath::Max(BorderH, 1e-4f), 0.0f, 1.0f);
	}

	/**
	 * Ghost-trail step: critically-damped move of Current toward Target.
	 * Mirrors FMath::FInterpTo semantics used by the widget tick so the
	 * spec pins the exact runtime behaviour.
	 */
	static float StepGhost(float Current, float Target, float DeltaTime, float Speed)
	{
		Current = Clamp01(Current);
		Target = Clamp01(Target);
		if (FMath::IsNearlyEqual(Current, Target, 1e-4f) || DeltaTime <= 0.0f || Speed <= 0.0f)
		{
			return Target;
		}
		return FMath::FInterpTo(Current, Target, DeltaTime, Speed);
	}
};
