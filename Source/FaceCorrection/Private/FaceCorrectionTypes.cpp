#include "FaceCorrectionTypes.h"

#include "Algo/AllOf.h"

FFaceCorrectionVisemeBlock::FFaceCorrectionVisemeBlock()
    : blockId(FGuid::NewGuid())
{
}

bool FFaceCorrectionVisemeBlock::isValid() const
{
    return blockId.IsValid() &&
        !sourcePhoneme.IsEmpty() &&
        !visemeId.IsNone() &&
        startFrame >= 0 &&
        endFrame >= startFrame;
}

bool FFaceCorrectionVisemeBlock::isNull(const FName nullVisemeId) const
{
    return visemeId == nullVisemeId;
}

bool FFaceCorrectionVisemeTransition::isValid(
    const FFaceCorrectionVisemeBlock& previous,
    const FFaceCorrectionVisemeBlock& next) const
{
    return previous.endFrame + 1 == next.startFrame &&
        boundaryFrame == next.startFrame &&
        blendStartFrame >= previous.startFrame &&
        blendStartFrame <= boundaryFrame &&
        blendEndFrame >= boundaryFrame &&
        blendEndFrame <= next.endFrame;
}

float FFaceCorrectionVisemeTransition::evaluateAlpha(const double frame) const
{
    if (frame <= blendStartFrame)
    {
        return 0.0f;
    }
    if (frame >= blendEndFrame)
    {
        return 1.0f;
    }
    const double duration = blendEndFrame - blendStartFrame;
    const float alpha = duration > 0.0
        ? FMath::Clamp(static_cast<float>((frame - blendStartFrame) / duration), 0.0f, 1.0f)
        : (frame >= boundaryFrame ? 1.0f : 0.0f);
    switch (transitionType)
    {
    case EFaceCorrectionTransitionType::Linear:
        return alpha;
    case EFaceCorrectionTransitionType::Sharp:
        return alpha < 0.5f
            ? 0.5f * FMath::Pow(alpha * 2.0f, 3.0f)
            : 1.0f - 0.5f * FMath::Pow((1.0f - alpha) * 2.0f, 3.0f);
    case EFaceCorrectionTransitionType::EaseIntoNext:
        return 1.0f - FMath::Pow(1.0f - alpha, 3.0f);
    case EFaceCorrectionTransitionType::HoldThenRelease:
        return alpha < 0.65f
            ? 0.0f
            : FMath::SmoothStep(0.65f, 1.0f, alpha);
    case EFaceCorrectionTransitionType::Smooth:
    default:
        return FMath::SmoothStep(0.0f, 1.0f, alpha);
    }
}

FFaceCorrectionWordBlock::FFaceCorrectionWordBlock()
    : blockId(FGuid::NewGuid())
{
}

namespace
{
float evaluateCubicInOut(const float alpha)
{
    const float clampedAlpha = FMath::Clamp(alpha, 0.0f, 1.0f);
    return clampedAlpha < 0.5f
        ? 4.0f * clampedAlpha * clampedAlpha * clampedAlpha
        : 1.0f - FMath::Pow(-2.0f * clampedAlpha + 2.0f, 3.0f) * 0.5f;
}
}

bool FFaceCorrectionWordBlock::isValid() const
{
    if (!blockId.IsValid() || text.IsEmpty() ||
        blendStartFrame < 0 || startFrame < blendStartFrame ||
        endFrame < startFrame || blendEndFrame < endFrame)
    {
        return false;
    }
    if (!Algo::AllOf(
        visemes,
        [this](const FFaceCorrectionVisemeBlock& viseme)
        {
            return viseme.isValid() &&
                viseme.startFrame >= startFrame &&
                viseme.endFrame <= endFrame;
        }))
    {
        return false;
    }
    if (transitions.Num() != FMath::Max(0, visemes.Num() - 1))
    {
        return false;
    }
    for (int32 index = 0; index < transitions.Num(); ++index)
    {
        if (!transitions[index].isValid(visemes[index], visemes[index + 1]))
        {
            return false;
        }
    }
    return true;
}

float FFaceCorrectionWordBlock::evaluateWeight(const double frame) const
{
    if (frame < blendStartFrame || frame > blendEndFrame)
    {
        return 0.0f;
    }
    if (frame < startFrame)
    {
        const double duration = startFrame - blendStartFrame;
        return duration > 0.0
            ? evaluateCubicInOut(static_cast<float>((frame - blendStartFrame) / duration))
            : 1.0f;
    }
    if (frame <= endFrame)
    {
        return 1.0f;
    }
    const double duration = blendEndFrame - endFrame;
    return duration > 0.0
        ? 1.0f - evaluateCubicInOut(static_cast<float>((frame - endFrame) / duration))
        : 1.0f;
}

bool FFaceCorrectionCurveRule::isValid() const
{
    return enabled && !sourceCurve.IsNone() && minimumValue <= maximumValue;
}

FName FFaceCorrectionCurveRule::getTargetCurve() const
{
    return targetCurve.IsNone() ? sourceCurve : targetCurve;
}

float FFaceCorrectionCurveRule::transformValue(const float sourceValue) const
{
    const float transformed = sourceValue * multiplier + offset;
    return clampOutput
        ? FMath::Clamp(transformed, minimumValue, maximumValue)
        : transformed;
}

const FFaceCorrectionCurveRule* UFaceCorrectionProfile::findRule(
    const FName sourceCurve) const
{
    return curveRules.FindByPredicate(
        [sourceCurve](const FFaceCorrectionCurveRule& rule)
        {
            return rule.isValid() && rule.sourceCurve == sourceCurve;
        });
}

TArray<FName> UFaceCorrectionProfile::getAllowedSourceCurves() const
{
    TArray<FName> allowedCurves;
    allowedCurves.Reserve(curveRules.Num());
    for (const FFaceCorrectionCurveRule& rule : curveRules)
    {
        if (rule.isValid())
        {
            allowedCurves.AddUnique(rule.sourceCurve);
        }
    }
    return allowedCurves;
}

bool FFaceCorrectionGeneratedCurve::isValid(const int32 expectedSampleCount) const
{
    return !sourceCurve.IsNone() &&
        expectedSampleCount > 0 &&
        values.Num() == expectedSampleCount;
}

bool FFaceCorrectionGeneratedData::isValid() const
{
    if (sampleRate <= 0.0f || sampleCount <= 0 || curves.IsEmpty())
    {
        return false;
    }
    return !curves.ContainsByPredicate(
        [this](const FFaceCorrectionGeneratedCurve& curve)
        {
            return !curve.isValid(sampleCount);
        });
}

float FFaceCorrectionGeneratedData::getDurationSeconds() const
{
    return sampleRate > 0.0f && sampleCount > 1
        ? static_cast<float>(sampleCount - 1) / sampleRate
        : 0.0f;
}

const FFaceCorrectionGeneratedCurve* FFaceCorrectionGeneratedData::findCurve(
    const FName sourceCurve) const
{
    return curves.FindByPredicate(
        [sourceCurve](const FFaceCorrectionGeneratedCurve& curve)
        {
            return curve.sourceCurve == sourceCurve;
        });
}

FFaceCorrectionBlock::FFaceCorrectionBlock()
    : correctionId(FGuid::NewGuid())
{
}

bool FFaceCorrectionBlock::isValid() const
{
    return correctionId.IsValid() &&
        blendInStartFrame >= 0 &&
        blendInStartFrame <= contentStartFrame &&
        contentStartFrame <= contentEndFrame &&
        contentEndFrame <= blendOutEndFrame &&
        strength >= 0.0f &&
        strength <= 1.0f;
}

double FFaceCorrectionBlock::evaluateEnvelope(const double frame) const
{
    if (!isValid() || frame < blendInStartFrame || frame > blendOutEndFrame)
    {
        return 0.0;
    }
    if (frame < contentStartFrame)
    {
        const double duration = contentStartFrame - blendInStartFrame;
        return duration > 0.0
            ? FMath::Clamp((frame - blendInStartFrame) / duration, 0.0, 1.0)
            : 1.0;
    }
    if (frame <= contentEndFrame)
    {
        return 1.0;
    }
    const double duration = blendOutEndFrame - contentEndFrame;
    return duration > 0.0
        ? FMath::Clamp((blendOutEndFrame - frame) / duration, 0.0, 1.0)
        : 0.0;
}

void FFaceCorrectionBlock::shiftBy(const int32 frameDelta, const int32 lastFrame)
{
    if (!isValid() || lastFrame < 0)
    {
        return;
    }
    if (blendOutEndFrame - blendInStartFrame > lastFrame)
    {
        return;
    }
    const int32 minimumDelta = -blendInStartFrame;
    const int32 maximumDelta = lastFrame - blendOutEndFrame;
    const int32 clampedDelta = FMath::Clamp(
        frameDelta,
        minimumDelta,
        maximumDelta);
    blendInStartFrame += clampedDelta;
    contentStartFrame += clampedDelta;
    contentEndFrame += clampedDelta;
    blendOutEndFrame += clampedDelta;
}

FString FFaceCorrectionBlock::getEffectiveSpokenText() const
{
    return spokenText.IsEmpty() ? displayText : spokenText;
}
