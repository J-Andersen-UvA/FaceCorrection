#include "FaceCorrectionTypes.h"

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
