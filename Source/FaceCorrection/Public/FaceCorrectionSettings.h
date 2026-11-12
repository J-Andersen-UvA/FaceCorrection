#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FaceCorrectionSettings.generated.h"

UCLASS(Config = EditorPerProjectUserSettings, DefaultConfig, meta = (DisplayName = "Face Correction"))
class FACECORRECTION_API UFaceCorrectionSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    virtual FName GetCategoryName() const override;

    UPROPERTY(Config, EditAnywhere, Category = "Audio2Face")
    FDirectoryPath audio2FaceSdkRoot;

    UPROPERTY(Config, EditAnywhere, Category = "Audio2Face")
    FFilePath audio2FaceModel;

    UPROPERTY(Config, EditAnywhere, Category = "Audio2Face", meta = (ClampMin = "0"))
    int32 cudaDevice = 0;

    UPROPERTY(Config, EditAnywhere, Category = "Audio2Face", meta = (ClampMin = "1.0"))
    float generationFrameRate = 60.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Text To Speech")
    FName textToSpeechProvider;

    UPROPERTY(Config, EditAnywhere, Category = "Text To Speech")
    FString dutchVoice;
};
