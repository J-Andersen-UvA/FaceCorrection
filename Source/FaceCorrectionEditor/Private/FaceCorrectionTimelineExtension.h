#pragma once

#if WITH_HANDPOSE_TIMELINE

#include "FaceCorrectionConfig.h"
#include "FaceCorrectionTypes.h"
#include "HandposeTimelineExtension.h"
#include "UObject/StrongObjectPtr.h"

class SFaceCorrectionLane;
class SWindow;

struct FFaceCorrectionReviewItem
{
    FString phoneme;
    FName visemeId;
};

class FFaceCorrectionTimelineExtension final
    : public IHandposeTimelineExtension
    , public TSharedFromThis<FFaceCorrectionTimelineExtension>
{
public:
    FFaceCorrectionTimelineExtension();

    virtual FName getExtensionId() const override;
    virtual FText getDisplayName() const override;
    virtual FText getTooltip() const override;
    virtual bool isEnabled() const override;
    virtual void setEnabled(bool enabled) override;
    virtual void onToolbarClicked() override;
    virtual TSharedRef<SWidget> createLane() override;
    virtual void updateContext(const FHandposeTimelineExtensionContext& context) override;
    virtual bool hasPreviewChanges() const override;
    virtual UAnimSequence* buildPreview(
        UAnimSequence& source,
        FString& errorMessage) override;
    virtual bool deleteSelection() override;

    const FHandposeTimelineExtensionContext& getContext() const;
    const TArray<FFaceCorrectionWordBlock>& getWords() const;
    TArray<FFaceCorrectionWordBlock>& getWords();
    const UFaceCorrectionConfig* getConfig() const;
    int32 getSelectedWordIndex() const;
    void selectWord(int32 wordIndex);
    void insertWordAtFrame(int32 frame);
    void reviewWord(int32 wordIndex);
    void openRulesForBlock(int32 wordIndex, int32 visemeIndex);
    void deleteWord(int32 wordIndex);
    void wordsChanged();

private:
    TArray<FGuid>* findAppliedRuleIds(FGuid wordId, FGuid visemeId);
    void openSettings();
    void restoreConfig();
    void setConfig(UFaceCorrectionConfig* newConfig);
    bool convertText(
        const FString& text,
        FString& phonemeText,
        TArray<FFaceCorrectionReviewItem>& items,
        FString& errorMessage) const;
    void openReview(
        const FString& text,
        const FString& phonemeText,
        TArray<FFaceCorrectionReviewItem> items,
        int32 startFrame,
        int32 wordIndex);
    void commitReview(
        const FString& text,
        const FString& phonemeText,
        const TArray<FFaceCorrectionReviewItem>& items,
        int32 startFrame,
        int32 wordIndex);
    TArray<FString> splitPhonemes(const FString& phonemeText) const;

    bool enabled = false;
    FHandposeTimelineExtensionContext context;
    TStrongObjectPtr<UFaceCorrectionConfig> config;
    TArray<FFaceCorrectionWordBlock> words;
    TWeakPtr<SFaceCorrectionLane> lane;
    TWeakPtr<SWindow> activeWindow;
    TWeakPtr<SWindow> settingsWindow;
    TWeakPtr<SWindow> rulesWindow;
    TWeakObjectPtr<UAnimSequenceBase> activeAnimation;
    int32 selectedWordIndex = INDEX_NONE;
};

#endif
