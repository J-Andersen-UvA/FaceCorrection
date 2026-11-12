#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FaceCorrectionTypes.generated.h"

UENUM(BlueprintType)
enum class EFaceCorrectionType : uint8
{
    Mouthing,
    MouthGesture,
    FacialProfile
};

UENUM(BlueprintType)
enum class EFaceCorrectionCurveBlendMode : uint8
{
    Replace,
    Additive
};

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionCurveRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    bool enabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    FName sourceCurve;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    FName targetCurve;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    EFaceCorrectionCurveBlendMode blendMode =
        EFaceCorrectionCurveBlendMode::Replace;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    float multiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    float offset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    bool clampOutput = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction", meta = (EditCondition = "clampOutput"))
    float minimumValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction", meta = (EditCondition = "clampOutput"))
    float maximumValue = 1.0f;

    bool isValid() const;
    FName getTargetCurve() const;
    float transformValue(float sourceValue) const;
};

UCLASS(BlueprintType)
class FACECORRECTION_API UFaceCorrectionProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction")
    EFaceCorrectionType correctionType = EFaceCorrectionType::Mouthing;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction")
    TArray<FFaceCorrectionCurveRule> curveRules;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float defaultStrength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction", meta = (ClampMin = "0"))
    int32 defaultBlendInFrames = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction", meta = (ClampMin = "0"))
    int32 defaultBlendOutFrames = 4;

    const FFaceCorrectionCurveRule* findRule(FName sourceCurve) const;
    TArray<FName> getAllowedSourceCurves() const;
};

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionGeneratedCurve
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    FName sourceCurve;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    TArray<float> values;

    bool isValid(int32 expectedSampleCount) const;
};

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionGeneratedData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    float sampleRate = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    int32 sampleCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    TArray<FFaceCorrectionGeneratedCurve> curves;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Correction")
    FString generationKey;

    bool isValid() const;
    float getDurationSeconds() const;
    const FFaceCorrectionGeneratedCurve* findCurve(FName sourceCurve) const;
};

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionBlock
{
    GENERATED_BODY()

    FFaceCorrectionBlock();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Face Correction")
    FGuid correctionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    EFaceCorrectionType correctionType = EFaceCorrectionType::Mouthing;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    FString displayText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    FString spokenText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    TSoftObjectPtr<UFaceCorrectionProfile> profile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    int32 blendInStartFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    int32 contentStartFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    int32 contentEndFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    int32 blendOutEndFrame = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float strength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Face Correction")
    FFaceCorrectionGeneratedData generatedData;

    bool isValid() const;
    double evaluateEnvelope(double frame) const;
    void shiftBy(int32 frameDelta, int32 lastFrame);
    FString getEffectiveSpokenText() const;
};
