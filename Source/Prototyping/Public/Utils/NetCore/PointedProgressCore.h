#pragma once

#include "CoreMinimal.h"

/**
 * Pure math for the pointed progress bar (<=====>).
 *
 * Single source of truth for the shape used by M_UI_ProgressPointed and
 * UPointedProgressWidget. No UObject, no UWorld, no materials here, only
 * UV math so the Automation spec can pin it. The material implements the
 * same formulas with SmoothStep AA on top.
 */
struct PROTOTYPING_API FPointedProgressCore
{
	/** Clamp any progress-like value to [0..1]. */
	static float Clamp01(float V)
	{
		return FMath::Clamp(V, 0.0f, 1.0f);
	}

	/**
	 * Edge ramp: 0 exactly at a tip, 1 once Tip width inside.
	 * U in [0..1], Tip is one-tip width as fraction of total width (0.1-0.15).
	 * Tip <= 0 means no point (full rectangle).
	 */
	static float EdgeRamp(float U, float Tip)
	{
		if (Tip <= 0.0f)
		{
			return 1.0f;
		}
		const float MinEnd = FMath::Min(U, 1.0f - U);
		return FMath::Clamp(MinEnd / Tip, 0.0f, 1.0f);
	}

	/** Half height of the <=====> hull at U (0 at tips, 0.5 inside). */
	static float ShapeHalfHeight(float U, float Tip)
	{
		return 0.5f * EdgeRamp(U, Tip);
	}

	/** Hard inside test (no AA). True when (U,V) is inside the hull. */
	static bool IsInsideShape(float U, float V, float Tip)
	{
		return FMath::Abs(V - 0.5f) <= ShapeHalfHeight(U, Tip);
	}

	/** Hard fill test (no AA): filled when U is left of Progress. */
	static bool IsFilled(float U, float Progress)
	{
		return U <= Progress;
	}

	/**
	 * Inner ramp for the border ring: same as EdgeRamp but pushed in by
	 * Border on both axes. Returns 0 when the border eats the whole tip.
	 */
	static float InnerRamp(float U, float Tip, float Border)
	{
		const float Denom = Tip - Border;
		if (Denom <= 1e-4f)
		{
			return 0.0f;
		}
		const float MinEnd = FMath::Min(U, 1.0f - U);
		return FMath::Clamp((MinEnd - Border) / Denom, 0.0f, 1.0f);
	}

	/** Half height of the inner (border-inset) hull. */
	static float InnerHalfHeight(float U, float Tip, float Border)
	{
		return 0.5f * InnerRamp(U, Tip, Border) - Border * 0.5f;
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
