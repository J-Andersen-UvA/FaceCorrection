#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FaceCorrectionRuleSet.generated.h"

UENUM(BlueprintType)
enum class EFaceCorrectionRuleStage : uint8
{
    SequenceRewrite,
    VisemeMapping,
    Timing,
    Articulation,
    ProceduralMotion
};

UENUM(BlueprintType)
enum class EFaceCorrectionRuleConditionType : uint8
{
    CurrentPhoneme,
    PreviousPhoneme,
    NextPhoneme,
    PhonemeSequence,
    WordPosition,
    Stress,
    VowelLength,
    CurrentViseme
};

UENUM(BlueprintType)
enum class EFaceCorrectionRuleActionType : uint8
{
    AssignViseme,
    AssignNull,
    ReplaceSequence,
    ScaleDuration,
    ScaleWeight,
    AddArticulation,
    AddProceduralMotion
};

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionRuleCondition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    EFaceCorrectionRuleConditionType conditionType =
        EFaceCorrectionRuleConditionType::CurrentPhoneme;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    TArray<FString> matchValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    bool invert = false;
};

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionRuleAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    EFaceCorrectionRuleActionType actionType =
        EFaceCorrectionRuleActionType::AssignViseme;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    TArray<FString> outputValues;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    float amount = 1.0f;
};

USTRUCT(BlueprintType)
struct FACECORRECTION_API FFaceCorrectionRuleDefinition
{
    GENERATED_BODY()

    FFaceCorrectionRuleDefinition();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rule")
    FGuid ruleId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    bool enabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    EFaceCorrectionRuleStage stage = EFaceCorrectionRuleStage::SequenceRewrite;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    TArray<FFaceCorrectionRuleCondition> conditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
    TArray<FFaceCorrectionRuleAction> actions;
};

UCLASS(BlueprintType)
class FACECORRECTION_API UFaceCorrectionRuleSet : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rules", meta = (TitleProperty = "displayName"))
    TArray<FFaceCorrectionRuleDefinition> rules;
};
