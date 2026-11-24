#pragma once

#include "CoreMinimal.h"

class UAnimSequence;
class UFaceCorrectionConfig;
struct FFaceCorrectionWordBlock;

class FFaceCorrectionPreviewBuilder
{
public:
    static UAnimSequence* apply(
        UAnimSequence& source,
        const UFaceCorrectionConfig& config,
        const TArray<FFaceCorrectionWordBlock>& words,
        FString& errorMessage);
};
