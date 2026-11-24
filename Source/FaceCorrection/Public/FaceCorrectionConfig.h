#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FaceCorrectionConfig.generated.h"

class UAnimSequence;
class UFaceCorrectionRuleSet;

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionVisemeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction")
    FName visemeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction")
    TSoftObjectPtr<UAnimSequence> animation;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction", meta = (ClampMin = "1"))
    int32 defaultDurationFrames = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction")
    FLinearColor color = FLinearColor(0.12f, 0.42f, 0.57f, 1.0f);
};

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionPhonemeMapping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction")
    FString phoneme;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Face Correction", meta = (GetOptions = "getVisemeOptions"))
    FName visemeId;
};

UCLASS(BlueprintType)
class FACECORRECTION_API UFaceCorrectionConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mouthing Rules")
    TSoftObjectPtr<UFaceCorrectionRuleSet> mouthingRuleSet;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phonemes", meta = (AllowedClasses = "/Script/PiperTTS.PiperVoiceProfile"))
    TSoftObjectPtr<UObject> phonemeProfile;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Visemes")
    FString visemeFolder;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visemes")
    TArray<FFaceCorrectionVisemeDefinition> visemes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visemes")
    TArray<FFaceCorrectionPhonemeMapping> phonemeMappings;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visemes")
    FName nullVisemeId = TEXT("NULL");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Application")
    TArray<FName> applyToBlendshapes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Application")
    FName jawBone;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline", meta = (ClampMin = "1"))
    int32 fallbackVisemeDurationFrames = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline", meta = (ClampMin = "1"))
    int32 minimumWordDurationFrames = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline", meta = (ClampMin = "0"))
    int32 defaultBlendInFrames = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline", meta = (ClampMin = "0"))
    int32 defaultBlendOutFrames = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline", meta = (ClampMin = "0"))
    int32 defaultVisemeBlendFrames = 2;

    FName findVisemeId(const FString& phoneme) const;
    const FFaceCorrectionVisemeDefinition* findViseme(FName visemeId) const;
    TArray<FName> getVisemeIds() const;

    UFUNCTION()
    TArray<FString> getVisemeOptions() const;
};
