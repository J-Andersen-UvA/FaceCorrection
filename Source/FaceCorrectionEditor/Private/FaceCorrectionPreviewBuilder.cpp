#include "FaceCorrectionPreviewBuilder.h"

#include "AnimPose.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "FaceCorrectionConfig.h"
#include "FaceCorrectionTypes.h"

namespace
{
using FPoseValues = TMap<FName, float>;

struct FVisemeBlend
{
    FName previousId;
    FName nextId;
    float alpha = 0.0f;

    bool isValid() const
    {
        return !previousId.IsNone() || !nextId.IsNone();
    }
};

FAnimPoseEvaluationOptions makeEvaluationOptions()
{
    FAnimPoseEvaluationOptions options;
    options.EvaluationType = EAnimDataEvalType::Raw;
    options.bShouldRetarget = true;
    options.bEvaluateCurves = false;
    return options;
}

float originalValue(const FFloatCurve* curve, const double time)
{
    return curve != nullptr ? curve->Evaluate(static_cast<float>(time)) : 0.0f;
}

float poseValue(
    const TMap<FName, FPoseValues>& poses,
    const FName visemeId,
    const FName curveName)
{
    const FPoseValues* pose = poses.Find(visemeId);
    const float* value = pose != nullptr ? pose->Find(curveName) : nullptr;
    return value != nullptr ? *value : 0.0f;
}

FName findVisibleViseme(
    const FFaceCorrectionWordBlock& word,
    int32 index,
    const int32 step,
    const FName nullVisemeId)
{
    while (word.visemes.IsValidIndex(index))
    {
        if (!word.visemes[index].isNull(nullVisemeId))
        {
            return word.visemes[index].visemeId;
        }
        index += step;
    }
    return NAME_None;
}

FVisemeBlend evaluateVisemeBlend(
    const FFaceCorrectionWordBlock& word,
    const FName nullVisemeId,
    const double frame)
{
    for (int32 index = 0; index < word.transitions.Num(); ++index)
    {
        const FFaceCorrectionVisemeTransition& transition = word.transitions[index];
        if (frame >= transition.blendStartFrame && frame <= transition.blendEndFrame)
        {
            FVisemeBlend blend;
            blend.previousId = findVisibleViseme(word, index, -1, nullVisemeId);
            blend.nextId = findVisibleViseme(word, index + 1, 1, nullVisemeId);
            blend.alpha = transition.evaluateAlpha(frame);
            if (blend.previousId.IsNone())
            {
                blend.previousId = blend.nextId;
            }
            if (blend.nextId.IsNone())
            {
                blend.nextId = blend.previousId;
            }
            return blend;
        }
    }

    int32 blockIndex = 0;
    for (int32 index = 0; index < word.visemes.Num(); ++index)
    {
        if (frame >= word.visemes[index].startFrame)
        {
            blockIndex = index;
        }
    }
    FVisemeBlend blend;
    blend.previousId = findVisibleViseme(word, blockIndex, -1, nullVisemeId);
    blend.nextId = blend.previousId;
    if (blend.previousId.IsNone())
    {
        blend.previousId = findVisibleViseme(word, blockIndex, 1, nullVisemeId);
        blend.nextId = blend.previousId;
    }
    return blend;
}

float evaluateCurveTarget(
    const FVisemeBlend& blend,
    const TMap<FName, FPoseValues>& poses,
    const FName curveName)
{
    return FMath::Lerp(
        poseValue(poses, blend.previousId, curveName),
        poseValue(poses, blend.nextId, curveName),
        blend.alpha);
}

FTransform blendTransform(
    const FTransform& first,
    const FTransform& second,
    const float alpha)
{
    FTransform result;
    result.SetTranslation(FMath::Lerp(
        first.GetTranslation(),
        second.GetTranslation(),
        alpha));
    result.SetRotation(FQuat::Slerp(
        first.GetRotation(),
        second.GetRotation(),
        alpha).GetNormalized());
    result.SetScale3D(FMath::Lerp(
        first.GetScale3D(),
        second.GetScale3D(),
        alpha));
    return result;
}

FTransform evaluateJawTarget(
    const FVisemeBlend& blend,
    const TMap<FName, FTransform>& poses,
    const FTransform& fallback)
{
    const FTransform* previous = poses.Find(blend.previousId);
    const FTransform* next = poses.Find(blend.nextId);
    return blendTransform(
        previous != nullptr ? *previous : fallback,
        next != nullptr ? *next : fallback,
        blend.alpha);
}
}

UAnimSequence* FFaceCorrectionPreviewBuilder::apply(
    UAnimSequence& source,
    const UFaceCorrectionConfig& config,
    const TArray<FFaceCorrectionWordBlock>& words,
    FString& errorMessage)
{
    if (words.IsEmpty())
    {
        return &source;
    }

    TArray<FName> curveNames;
    for (const FName curveName : config.applyToBlendshapes)
    {
        if (!curveName.IsNone())
        {
            curveNames.AddUnique(curveName);
        }
    }
    const bool applyJaw = !config.jawBone.IsNone();
    if (curveNames.IsEmpty() && !applyJaw)
    {
        errorMessage = TEXT("Add at least one blendshape or a jaw bone to the Face Correction config.");
        return nullptr;
    }

    TMap<FName, FPoseValues> curvePoses;
    TMap<FName, FTransform> jawPoses;
    for (const FFaceCorrectionWordBlock& word : words)
    {
        for (const FFaceCorrectionVisemeBlock& block : word.visemes)
        {
            if (block.isNull(config.nullVisemeId) || curvePoses.Contains(block.visemeId))
            {
                continue;
            }
            const FFaceCorrectionVisemeDefinition* definition =
                config.findViseme(block.visemeId);
            UAnimSequence* animation = definition != nullptr
                ? definition->animation.LoadSynchronous()
                : nullptr;
            if (animation == nullptr || animation->GetDataModel() == nullptr)
            {
                errorMessage = FString::Printf(
                    TEXT("Viseme %s does not resolve to an Animation Sequence."),
                    *block.visemeId.ToString());
                return nullptr;
            }

            FPoseValues& values = curvePoses.Add(block.visemeId);
            for (const FName curveName : curveNames)
            {
                const FAnimationCurveIdentifier curveId(
                    curveName,
                    ERawCurveTrackTypes::RCT_Float);
                const FFloatCurve* curve = animation->GetDataModel()->FindFloatCurve(curveId);
                values.Add(curveName, curve != nullptr ? curve->Evaluate(0.0f) : 0.0f);
            }

            if (applyJaw)
            {
                const USkeleton* skeleton = animation->GetSkeleton();
                if (skeleton == nullptr ||
                    skeleton->GetReferenceSkeleton().FindBoneIndex(config.jawBone) == INDEX_NONE)
                {
                    errorMessage = FString::Printf(
                        TEXT("Viseme %s does not contain jaw bone %s."),
                        *block.visemeId.ToString(),
                        *config.jawBone.ToString());
                    return nullptr;
                }
                FAnimPose pose;
                UAnimPoseExtensions::GetAnimPoseAtTime(
                    animation,
                    0.0,
                    makeEvaluationOptions(),
                    pose);
                if (!UAnimPoseExtensions::IsValid(pose))
                {
                    errorMessage = FString::Printf(
                        TEXT("Could not evaluate jaw pose for viseme %s."),
                        *block.visemeId.ToString());
                    return nullptr;
                }
                jawPoses.Add(
                    block.visemeId,
                    UAnimPoseExtensions::GetBonePose(
                        pose,
                        config.jawBone,
                        EAnimPoseSpaces::Local));
            }
        }
    }

    const IAnimationDataModel* sourceData = source.GetDataModel();
    const FFrameRate samplingRate = source.GetSamplingFrameRate();
    const int32 frameCount = source.GetNumberOfSampledKeys();
    if (sourceData == nullptr || !samplingRate.IsValid() || frameCount < 1)
    {
        errorMessage = TEXT("The source animation has no valid animation data or frame rate.");
        return nullptr;
    }
    const double frameRate = samplingRate.AsDecimal();

    UAnimSequence* output = DuplicateObject<UAnimSequence>(
        &source,
        GetTransientPackage(),
        MakeUniqueObjectName(
            GetTransientPackage(),
            UAnimSequence::StaticClass(),
            TEXT("FaceCorrectionPreview")));
    if (output == nullptr)
    {
        errorMessage = TEXT("Unreal could not create the Face Correction preview animation.");
        return nullptr;
    }
    output->ClearFlags(RF_Public | RF_Standalone);
    output->SetFlags(RF_Transient);

    IAnimationDataController& controller = output->GetController();
    IAnimationDataController::FScopedBracket bracket(
        controller,
        FText::FromString(TEXT("Apply Face Correction preview")),
        false);
    for (const FName curveName : curveNames)
    {
        const FAnimationCurveIdentifier curveId(
            curveName,
            ERawCurveTrackTypes::RCT_Float);
        const FFloatCurve* original = sourceData->FindFloatCurve(curveId);
        TArray<FRichCurveKey> keys = original != nullptr
            ? original->FloatCurve.GetCopyOfKeys()
            : TArray<FRichCurveKey>();

        for (const FFaceCorrectionWordBlock& word : words)
        {
            if (word.visemes.IsEmpty())
            {
                continue;
            }
            const double startTime = word.blendStartFrame / frameRate;
            const double endTime = word.blendEndFrame / frameRate;
            keys.RemoveAll(
                [startTime, endTime](const FRichCurveKey& key)
                {
                    return key.Time >= startTime && key.Time <= endTime;
                });
            for (int32 frame = word.blendStartFrame; frame <= word.blendEndFrame; ++frame)
            {
                const FVisemeBlend blend = evaluateVisemeBlend(
                    word,
                    config.nullVisemeId,
                    frame);
                if (!blend.isValid())
                {
                    continue;
                }
                const float sourceValue = originalValue(original, frame / frameRate);
                const float targetValue = evaluateCurveTarget(
                    blend,
                    curvePoses,
                    curveName);
                FRichCurveKey key(
                    static_cast<float>(frame / frameRate),
                    FMath::Lerp(sourceValue, targetValue, word.evaluateWeight(frame)));
                key.InterpMode = RCIM_Linear;
                keys.Add(key);
            }
        }

        keys.Sort(
            [](const FRichCurveKey& left, const FRichCurveKey& right)
            {
                return left.Time < right.Time;
            });
        if ((original == nullptr && !controller.AddCurve(
                curveId,
                AACF_DefaultCurve,
                false)) ||
            !controller.SetCurveKeys(curveId, keys, false))
        {
            errorMessage = FString::Printf(
                TEXT("Could not write preview curve %s."),
                *curveName.ToString());
            return nullptr;
        }
    }

    if (applyJaw)
    {
        const USkeleton* skeleton = source.GetSkeleton();
        if (skeleton == nullptr ||
            skeleton->GetReferenceSkeleton().FindBoneIndex(config.jawBone) == INDEX_NONE)
        {
            errorMessage = FString::Printf(
                TEXT("The source animation does not contain jaw bone %s."),
                *config.jawBone.ToString());
            return nullptr;
        }

        TArray<double> sampleTimes;
        sampleTimes.Reserve(frameCount);
        for (int32 frame = 0; frame < frameCount; ++frame)
        {
            sampleTimes.Add(samplingRate.AsSeconds(frame));
        }
        TArray<FAnimPose> sourcePoses;
        UAnimPoseExtensions::GetAnimPoseAtTimeIntervals(
            &source,
            sampleTimes,
            makeEvaluationOptions(),
            sourcePoses);
        if (sourcePoses.Num() != frameCount || sourcePoses.ContainsByPredicate(
                [](const FAnimPose& pose)
                {
                    return !UAnimPoseExtensions::IsValid(pose);
                }))
        {
            errorMessage = TEXT("Unreal could not evaluate the source jaw animation.");
            return nullptr;
        }

        TArray<FVector> translations;
        TArray<FQuat> rotations;
        TArray<FVector> scales;
        translations.Reserve(frameCount);
        rotations.Reserve(frameCount);
        scales.Reserve(frameCount);
        for (const FAnimPose& pose : sourcePoses)
        {
            const FTransform transform = UAnimPoseExtensions::GetBonePose(
                pose,
                config.jawBone,
                EAnimPoseSpaces::Local);
            translations.Add(transform.GetTranslation());
            rotations.Add(transform.GetRotation());
            scales.Add(transform.GetScale3D());
        }
        for (const FFaceCorrectionWordBlock& word : words)
        {
            if (word.visemes.IsEmpty())
            {
                continue;
            }
            for (int32 frame = word.blendStartFrame; frame <= word.blendEndFrame; ++frame)
            {
                if (!translations.IsValidIndex(frame))
                {
                    continue;
                }
                const FTransform current(
                    rotations[frame],
                    translations[frame],
                    scales[frame]);
                const FVisemeBlend blend = evaluateVisemeBlend(
                    word,
                    config.nullVisemeId,
                    frame);
                if (!blend.isValid())
                {
                    continue;
                }
                const FTransform blended = blendTransform(
                    current,
                    evaluateJawTarget(blend, jawPoses, current),
                    word.evaluateWeight(frame));
                translations[frame] = blended.GetTranslation();
                rotations[frame] = blended.GetRotation();
                scales[frame] = blended.GetScale3D();
            }
        }

        IAnimationDataModel* outputData = output->GetDataModel();
        if (!outputData->IsValidBoneTrackName(config.jawBone) &&
            !controller.AddBoneCurve(config.jawBone, false))
        {
            errorMessage = FString::Printf(
                TEXT("Could not add jaw track %s."),
                *config.jawBone.ToString());
            return nullptr;
        }
        if (!controller.SetBoneTrackKeys(
                config.jawBone,
                translations,
                rotations,
                scales,
                false))
        {
            errorMessage = FString::Printf(
                TEXT("Could not write jaw track %s."),
                *config.jawBone.ToString());
            return nullptr;
        }
    }

    output->PostEditChange();
    return output;
}
