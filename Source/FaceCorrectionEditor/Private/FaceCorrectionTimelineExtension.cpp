#if WITH_HANDPOSE_TIMELINE

#include "FaceCorrectionTimelineExtension.h"
#include "FaceCorrectionPreviewBuilder.h"
#include "FaceCorrectionRuleSet.h"

#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "IContentBrowserSingleton.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "PropertyCustomizationHelpers.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_PIPER_TTS
#include "PiperBlueprintLibrary.h"
#include "PiperTTSTypes.h"
#endif

namespace
{
const TCHAR* configSection = TEXT("FaceCorrectionTimeline");
const TCHAR* configKey = TEXT("LastConfigAsset");
constexpr float faceLaneHeight = 152.0f;
constexpr float faceLanePaintHeight = 148.0f;
constexpr float wordTop = 6.0f;
constexpr float wordBlendHandleTop = 18.0f;
constexpr float visemeTop = 38.0f;
constexpr float visemeHeight = 32.0f;
constexpr float boundaryHandleTop = 72.0f;
constexpr float influenceGraphTop = 86.0f;
constexpr float influenceGraphMiddle = 108.0f;
constexpr float influenceGraphBottom = 130.0f;
constexpr float transitionHandleCenterY = 141.0f;
constexpr int32 influenceCurveSamples = 48;

bool isModifier(const TCHAR character)
{
    return character == TEXT(':') ||
        character == TEXT('\u02D0') ||
        character == TEXT('\u02D1') ||
        (character >= TEXT('\u0300') && character <= TEXT('\u036F'));
}

bool isStress(const TCHAR character)
{
    return character == TEXT('\'') ||
        character == TEXT('\u02C8') ||
        character == TEXT('\u02CC');
}

FText transitionTypeText(const EFaceCorrectionTransitionType transitionType)
{
    switch (transitionType)
    {
    case EFaceCorrectionTransitionType::Linear:
        return FText::FromString(TEXT("Linear"));
    case EFaceCorrectionTransitionType::Sharp:
        return FText::FromString(TEXT("Sharp"));
    case EFaceCorrectionTransitionType::EaseIntoNext:
        return FText::FromString(TEXT("Ease Into Next"));
    case EFaceCorrectionTransitionType::HoldThenRelease:
        return FText::FromString(TEXT("Hold Then Release"));
    case EFaceCorrectionTransitionType::Smooth:
    default:
        return FText::FromString(TEXT("Smooth"));
    }
}

void paintTriangle(
    const FVector2D& first,
    const FVector2D& second,
    const FVector2D& third,
    const FGeometry& geometry,
    const FLinearColor& color,
    const FSlateBrush& brush,
    FSlateWindowElementList& elements,
    const int32 layer)
{
    TArray<FSlateVertex> vertices;
    vertices.Reserve(3);
    for (const FVector2D& point : {first, second, third})
    {
        FSlateVertex& vertex = vertices.AddDefaulted_GetRef();
        vertex.Position = FVector2f(geometry.LocalToAbsolute(point));
        vertex.Color = color.ToFColor(false);
    }
    const TArray<SlateIndex> indices = {0, 1, 2};
    FSlateDrawElement::MakeCustomVerts(
        elements,
        layer,
        brush.GetRenderingResource(),
        vertices,
        indices,
        nullptr,
        0,
        0,
        ESlateDrawEffect::None);
}

float evaluateVisemeInfluence(
    const FFaceCorrectionWordBlock& word,
    const int32 visemeIndex,
    const double frame)
{
    if (!word.visemes.IsValidIndex(visemeIndex))
    {
        return 0.0f;
    }

    float localWeight = 0.0f;
    bool transitionFound = false;
    for (int32 transitionIndex = 0;
        transitionIndex < word.transitions.Num();
        ++transitionIndex)
    {
        const FFaceCorrectionVisemeTransition& transition =
            word.transitions[transitionIndex];
        if (frame < transition.blendStartFrame || frame > transition.blendEndFrame)
        {
            continue;
        }
        const float alpha = transition.evaluateAlpha(frame);
        if (visemeIndex == transitionIndex)
        {
            localWeight = 1.0f - alpha;
        }
        else if (visemeIndex == transitionIndex + 1)
        {
            localWeight = alpha;
        }
        transitionFound = true;
        break;
    }

    if (!transitionFound)
    {
        if (frame < word.startFrame)
        {
            localWeight = visemeIndex == 0 ? 1.0f : 0.0f;
        }
        else if (frame > word.endFrame)
        {
            localWeight = visemeIndex == word.visemes.Num() - 1 ? 1.0f : 0.0f;
        }
        else
        {
            const FFaceCorrectionVisemeBlock& viseme = word.visemes[visemeIndex];
            localWeight = frame >= viseme.startFrame && frame <= viseme.endFrame
                ? 1.0f
                : 0.0f;
        }
    }
    return localWeight * word.evaluateWeight(frame);
}

class FFaceCorrectionReviewDragOperation final : public FDecoratedDragDropOp
{
public:
    DRAG_DROP_OPERATOR_TYPE(
        FFaceCorrectionReviewDragOperation,
        FDecoratedDragDropOp)

    static TSharedRef<FFaceCorrectionReviewDragOperation> create(
        const int32 sourceIndex)
    {
        TSharedRef<FFaceCorrectionReviewDragOperation> operation =
            MakeShared<FFaceCorrectionReviewDragOperation>();
        operation->sourceIndex = sourceIndex;
        operation->DefaultHoverText = FText::FromString(TEXT("Move phoneme and viseme"));
        operation->Construct();
        return operation;
    }

    int32 sourceIndex = INDEX_NONE;
};

class SFaceCorrectionReviewDragHandle final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SFaceCorrectionReviewDragHandle) {}
        SLATE_ARGUMENT(int32, ItemIndex)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments)
    {
        itemIndex = arguments._ItemIndex;
        ChildSlot
        [
            SNew(SBorder)
            .Padding(5.0f, 2.0f)
            .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("≡")))
                .ToolTipText(FText::FromString(TEXT("Drag to rearrange")))
            ]
        ];
    }

    virtual FReply OnMouseButtonDown(
        const FGeometry&,
        const FPointerEvent& event) override
    {
        return event.GetEffectingButton() == EKeys::LeftMouseButton
            ? FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton)
            : FReply::Unhandled();
    }

    virtual FReply OnDragDetected(
        const FGeometry&,
        const FPointerEvent&) override
    {
        return FReply::Handled().BeginDragDrop(
            FFaceCorrectionReviewDragOperation::create(itemIndex));
    }

private:
    int32 itemIndex = INDEX_NONE;
};

class SFaceCorrectionReviewDropRow final : public SCompoundWidget
{
public:
    using FMove = TFunction<void(int32, int32)>;

    SLATE_BEGIN_ARGS(SFaceCorrectionReviewDropRow) {}
        SLATE_DEFAULT_SLOT(FArguments, Content)
        SLATE_ARGUMENT(int32, ItemIndex)
        SLATE_ARGUMENT(FMove, OnMove)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments)
    {
        itemIndex = arguments._ItemIndex;
        onMove = arguments._OnMove;
        ChildSlot[arguments._Content.Widget];
    }

    virtual FReply OnDrop(
        const FGeometry&,
        const FDragDropEvent& event) override
    {
        const TSharedPtr<FFaceCorrectionReviewDragOperation> operation =
            event.GetOperationAs<FFaceCorrectionReviewDragOperation>();
        if (!operation.IsValid())
        {
            return FReply::Unhandled();
        }
        onMove(operation->sourceIndex, itemIndex);
        return FReply::Handled();
    }

private:
    int32 itemIndex = INDEX_NONE;
    FMove onMove;
};

class SFaceCorrectionReviewWidget final : public SCompoundWidget
{
public:
    using FContinue = TFunction<void(const TArray<FFaceCorrectionReviewItem>&)>;

    SLATE_BEGIN_ARGS(SFaceCorrectionReviewWidget) {}
        SLATE_ARGUMENT(TArray<FFaceCorrectionReviewItem>, Items)
        SLATE_ARGUMENT(TArray<FName>, VisemeIds)
        SLATE_ARGUMENT(FString, PhonemeText)
        SLATE_ARGUMENT(TWeakPtr<SWindow>, OwnerWindow)
        SLATE_ARGUMENT(FContinue, OnContinue)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments)
    {
        items = arguments._Items;
        visemeIds = arguments._VisemeIds;
        phonemeText = arguments._PhonemeText;
        ownerWindow = arguments._OwnerWindow;
        onContinue = arguments._OnContinue;

        ChildSlot
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(10.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(
                    TEXT("Piper phonemes: %s"),
                    *phonemeText)))
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(10.0f, 0.0f)
            [
                SNew(SScrollBox)
                + SScrollBox::Slot()
                [
                    SAssignNew(rows, SVerticalBox)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(10.0f, 8.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Add Viseme")))
                    .OnClicked_Lambda([this]()
                    {
                        FFaceCorrectionReviewItem item;
                        item.phoneme = TEXT("manual");
                        item.visemeId = visemeIds.IsEmpty() ? NAME_None : visemeIds[0];
                        items.Add(MoveTemp(item));
                        rebuildRows();
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Cancel")))
                    .OnClicked_Lambda([this]()
                    {
                        if (const TSharedPtr<SWindow> window = ownerWindow.Pin())
                        {
                            window->RequestDestroyWindow();
                        }
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Continue")))
                    .IsEnabled_Lambda([this]() { return !items.IsEmpty(); })
                    .OnClicked_Lambda([this]()
                    {
                        for (FFaceCorrectionReviewItem& item : items)
                        {
                            item.phoneme.TrimStartAndEndInline();
                            if (item.phoneme.IsEmpty())
                            {
                                item.phoneme = TEXT("manual");
                            }
                        }
                        onContinue(items);
                        if (const TSharedPtr<SWindow> window = ownerWindow.Pin())
                        {
                            window->RequestDestroyWindow();
                        }
                        return FReply::Handled();
                    })
                ]
            ]
        ];
        rebuildRows();
    }

private:
    TSharedRef<SWidget> makeVisemeMenu(const int32 itemIndex)
    {
        FMenuBuilder menu(true, nullptr);
        for (const FName visemeId : visemeIds)
        {
            menu.AddMenuEntry(
                FText::FromName(visemeId),
                FText::GetEmpty(),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda(
                    [this, itemIndex, visemeId]()
                    {
                        if (items.IsValidIndex(itemIndex))
                        {
                            items[itemIndex].visemeId = visemeId;
                            rebuildRows();
                        }
                    })));
        }
        return menu.MakeWidget();
    }

    void moveItem(const int32 sourceIndex, const int32 targetIndex)
    {
        if (!items.IsValidIndex(sourceIndex) || !items.IsValidIndex(targetIndex) ||
            sourceIndex == targetIndex)
        {
            return;
        }
        FFaceCorrectionReviewItem item = MoveTemp(items[sourceIndex]);
        items.RemoveAt(sourceIndex);
        items.Insert(MoveTemp(item), FMath::Clamp(targetIndex, 0, items.Num()));
        rebuildRows();
    }

    void rebuildRows()
    {
        if (!rows.IsValid())
        {
            return;
        }
        rows->ClearChildren();
        for (int32 index = 0; index < items.Num(); ++index)
        {
            rows->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                SNew(SFaceCorrectionReviewDropRow)
                .ItemIndex(index)
                .OnMove([this](const int32 source, const int32 target)
                {
                    moveItem(source, target);
                })
                [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(SFaceCorrectionReviewDragHandle)
                    .ItemIndex(index)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::AsNumber(index + 1))
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(SEditableTextBox)
                    .Text(FText::FromString(items[index].phoneme))
                    .HintText(FText::FromString(TEXT("Source phoneme")))
                    .OnTextCommitted_Lambda(
                        [this, index](const FText& text, ETextCommit::Type)
                        {
                            if (items.IsValidIndex(index))
                            {
                                items[index].phoneme = text.ToString();
                            }
                        })
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(6.0f, 0.0f)
                [
                    SNew(SComboButton)
                    .ButtonContent()
                    [
                        SNew(STextBlock)
                        .Text_Lambda(
                            [this, index]()
                            {
                                return items.IsValidIndex(index)
                                    ? FText::FromName(items[index].visemeId)
                                    : FText::GetEmpty();
                            })
                    ]
                    .OnGetMenuContent_Lambda(
                        [this, index]() { return makeVisemeMenu(index); })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Remove")))
                    .OnClicked_Lambda(
                        [this, index]()
                        {
                            if (items.IsValidIndex(index))
                            {
                                items.RemoveAt(index);
                                rebuildRows();
                            }
                            return FReply::Handled();
                        })
                ]
                ]
            ];
        }
    }

    TArray<FFaceCorrectionReviewItem> items;
    TArray<FName> visemeIds;
    FString phonemeText;
    TWeakPtr<SWindow> ownerWindow;
    FContinue onContinue;
    TSharedPtr<SVerticalBox> rows;
};
}

class SFaceCorrectionLane final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SFaceCorrectionLane) {}
        SLATE_ARGUMENT(TWeakPtr<FFaceCorrectionTimelineExtension>, Owner)
    SLATE_END_ARGS()

    void Construct(const FArguments& arguments)
    {
        owner = arguments._Owner;
    }

    virtual FVector2D ComputeDesiredSize(float) const override
    {
        const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin();
        return FVector2D(
            extension.IsValid() ? extension->getContext().getTotalWidth() : 1.0f,
            extension.IsValid() && extension->getSelectedWordIndex() != INDEX_NONE
                ? faceLaneHeight
                : 78.0f);
    }

    virtual int32 OnPaint(
        const FPaintArgs&,
        const FGeometry& geometry,
        const FSlateRect&,
        FSlateWindowElementList& elements,
        const int32 layer,
        const FWidgetStyle&,
        bool) const override
    {
        const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin();
        if (!extension.IsValid())
        {
            return layer;
        }

        const FHandposeTimelineExtensionContext& timeline = extension->getContext();
        const FSlateBrush* brush = FAppStyle::GetBrush(TEXT("WhiteBrush"));
        FSlateDrawElement::MakeBox(
            elements,
            layer,
            geometry.ToPaintGeometry(),
            brush,
            ESlateDrawEffect::None,
            FLinearColor(0.055f, 0.055f, 0.055f, 1.0f));

        const auto drawBox = [&elements, &geometry, brush, layer](
            const int32 depth,
            const float x,
            const float y,
            const float width,
            const float height,
            const FLinearColor& color)
        {
            if (width > 0.0f)
            {
                FSlateDrawElement::MakeBox(
                    elements,
                    layer + depth,
                    geometry.ToPaintGeometry(
                        FVector2f(width, height),
                        FSlateLayoutTransform(FVector2f(x, y))),
                    brush,
                    ESlateDrawEffect::None,
                    color);
            }
        };

        const float trimStartX = timeline.getOffsetAtFrame(timeline.trimStartFrame);
        const float trimEndX = timeline.getOffsetAtFrame(timeline.trimEndFrame + 1);
        drawBox(
            1,
            0.0f,
            0.0f,
            trimStartX,
            faceLanePaintHeight,
            FLinearColor(0.025f, 0.025f, 0.025f, 0.82f));
        drawBox(
            1,
            trimEndX,
            0.0f,
            timeline.getTotalWidth() - trimEndX,
            faceLanePaintHeight,
            FLinearColor(0.025f, 0.025f, 0.025f, 0.82f));
        const auto getVisemeColor = [extension](const FFaceCorrectionVisemeBlock& viseme)
        {
            if (const UFaceCorrectionConfig* config = extension->getConfig())
            {
                if (viseme.visemeId == config->nullVisemeId)
                {
                    return FLinearColor(0.20f, 0.20f, 0.20f, 1.0f);
                }
                if (const FFaceCorrectionVisemeDefinition* definition =
                    config->findViseme(viseme.visemeId))
                {
                    return definition->color;
                }
            }
            return FLinearColor(0.32f, 0.26f, 0.45f, 1.0f);
        };
        const TArray<FFaceCorrectionWordBlock>& words = extension->getWords();
        for (int32 wordIndex = 0; wordIndex < words.Num(); ++wordIndex)
        {
            const FFaceCorrectionWordBlock& word =
                drag.wordIndex == wordIndex ? draftWord : words[wordIndex];
            const float blendLeft = timeline.getOffsetAtFrame(word.blendStartFrame);
            const float wordLeft = timeline.getOffsetAtFrame(word.startFrame);
            const float wordRight = timeline.getOffsetAtFrame(word.endFrame + 1);
            const float blendRight = timeline.getOffsetAtFrame(word.blendEndFrame + 1);
            drawBox(
                1,
                blendLeft,
                wordTop,
                wordLeft - blendLeft,
                24.0f,
                FLinearColor(0.08f, 0.22f, 0.30f, 1.0f));
            drawBox(
                1,
                wordLeft,
                wordTop,
                wordRight - wordLeft,
                24.0f,
                drag.wordIndex == wordIndex ||
                    extension->getSelectedWordIndex() == wordIndex
                    ? FLinearColor(0.16f, 0.50f, 0.68f, 1.0f)
                    : FLinearColor(0.10f, 0.34f, 0.48f, 1.0f));
            drawBox(
                1,
                wordRight,
                wordTop,
                blendRight - wordRight,
                24.0f,
                FLinearColor(0.08f, 0.22f, 0.30f, 1.0f));
            drawBox(3, wordLeft - 2.0f, wordTop, 4.0f, 24.0f, FLinearColor::White);
            drawBox(3, wordRight - 2.0f, wordTop, 4.0f, 24.0f, FLinearColor::White);
            drawBox(3, blendLeft - 1.0f, wordBlendHandleTop, 2.0f, 12.0f, FLinearColor(0.65f, 0.80f, 0.88f));
            drawBox(3, blendRight - 1.0f, wordBlendHandleTop, 2.0f, 12.0f, FLinearColor(0.65f, 0.80f, 0.88f));

            if (wordRight - wordLeft > 10.0f)
            {
                const FPaintGeometry labelGeometry = geometry.ToPaintGeometry(
                    FVector2f(wordRight - wordLeft - 8.0f, 18.0f),
                    FSlateLayoutTransform(FVector2f(wordLeft + 4.0f, 9.0f)));
                elements.PushClip(FSlateClippingZone(labelGeometry));
                FSlateDrawElement::MakeText(
                    elements,
                    layer + 2,
                    labelGeometry,
                    word.text,
                    FAppStyle::GetFontStyle(TEXT("SmallFontBold")),
                    ESlateDrawEffect::None,
                    FLinearColor::White);
                elements.PopClip();
            }

            for (int32 visemeIndex = 0; visemeIndex < word.visemes.Num(); ++visemeIndex)
            {
                const FFaceCorrectionVisemeBlock& viseme = word.visemes[visemeIndex];
                const float left = timeline.getOffsetAtFrame(viseme.startFrame);
                const float right = timeline.getOffsetAtFrame(viseme.endFrame + 1);
                const FLinearColor color = getVisemeColor(viseme);
                drawBox(1, left, visemeTop, right - left, visemeHeight, color);
                drawBox(3, left - 1.0f, visemeTop, 2.0f, visemeHeight, FLinearColor(0.75f, 0.75f, 0.75f));
                drawBox(3, right - 1.0f, visemeTop, 2.0f, visemeHeight, FLinearColor(0.75f, 0.75f, 0.75f));
                if (right - left > 8.0f)
                {
                    const FString label = FString::Printf(
                        TEXT("%s → %s"),
                        *viseme.sourcePhoneme,
                        *viseme.visemeId.ToString());
                    const FPaintGeometry labelGeometry = geometry.ToPaintGeometry(
                        FVector2f(right - left - 6.0f, 20.0f),
                        FSlateLayoutTransform(FVector2f(left + 3.0f, 44.0f)));
                    elements.PushClip(FSlateClippingZone(labelGeometry));
                    FSlateDrawElement::MakeText(
                        elements,
                        layer + 2,
                        labelGeometry,
                        label,
                        FAppStyle::GetFontStyle(TEXT("SmallFont")),
                        ESlateDrawEffect::None,
                        FLinearColor::White);
                    elements.PopClip();
                }
            }

            const bool showInfluenceGraph =
                extension->getSelectedWordIndex() == wordIndex ||
                drag.wordIndex == wordIndex;
            if (!showInfluenceGraph || word.visemes.IsEmpty())
            {
                continue;
            }

            drawBox(
                1,
                blendLeft,
                influenceGraphTop,
                blendRight - blendLeft,
                influenceGraphBottom - influenceGraphTop,
                FLinearColor(0.035f, 0.035f, 0.04f, 0.92f));
            for (const float guideY : {
                    influenceGraphTop,
                    influenceGraphMiddle,
                    influenceGraphBottom})
            {
                drawBox(
                    2,
                    blendLeft,
                    guideY,
                    blendRight - blendLeft,
                    1.0f,
                    FLinearColor(0.55f, 0.55f, 0.58f, 0.35f));
            }

            for (int32 visemeIndex = 0;
                visemeIndex < word.visemes.Num();
                ++visemeIndex)
            {
                const double curveStart = visemeIndex == 0
                    ? word.blendStartFrame
                    : word.transitions[visemeIndex - 1].blendStartFrame;
                const double curveEnd = visemeIndex == word.visemes.Num() - 1
                    ? word.blendEndFrame
                    : word.transitions[visemeIndex].blendEndFrame;
                TArray<FVector2D> curvePoints;
                curvePoints.Reserve(influenceCurveSamples + 1);
                for (int32 sampleIndex = 0;
                    sampleIndex <= influenceCurveSamples;
                    ++sampleIndex)
                {
                    const double alpha =
                        static_cast<double>(sampleIndex) / influenceCurveSamples;
                    const double frame = FMath::Lerp(curveStart, curveEnd, alpha);
                    const float influence = evaluateVisemeInfluence(
                        word,
                        visemeIndex,
                        frame);
                    curvePoints.Add(FVector2D(
                        timeline.getOffsetAtFrame(frame),
                        FMath::Lerp(
                            influenceGraphBottom,
                            influenceGraphTop,
                            influence)));
                }
                FLinearColor curveColor = getVisemeColor(word.visemes[visemeIndex]);
                curveColor.A = 1.0f;
                FSlateDrawElement::MakeLines(
                    elements,
                    layer + 4,
                    geometry.ToPaintGeometry(),
                    curvePoints,
                    ESlateDrawEffect::None,
                    curveColor,
                    true,
                    2.5f);
            }

            const bool wordBlendStartHovered =
                (hover.wordIndex == wordIndex && hover.handle == EHandle::BlendStart) ||
                (drag.wordIndex == wordIndex && drag.handle == EHandle::BlendStart);
            const bool wordBlendEndHovered =
                (hover.wordIndex == wordIndex && hover.handle == EHandle::BlendEnd) ||
                (drag.wordIndex == wordIndex && drag.handle == EHandle::BlendEnd);
            const FLinearColor wordHandleColor(0.2f, 0.65f, 0.88f, 1.0f);
            const float wordStartHandleSize = wordBlendStartHovered ? 17.0f : 14.0f;
            const float wordEndHandleSize = wordBlendEndHovered ? 17.0f : 14.0f;
            paintTriangle(
                FVector2D(blendLeft + wordStartHandleSize * 0.6f, transitionHandleCenterY),
                FVector2D(blendLeft - wordStartHandleSize * 0.4f, transitionHandleCenterY - wordStartHandleSize * 0.5f),
                FVector2D(blendLeft - wordStartHandleSize * 0.4f, transitionHandleCenterY + wordStartHandleSize * 0.5f),
                geometry,
                wordBlendStartHovered ? FLinearColor::White : wordHandleColor,
                *brush,
                elements,
                layer + 5);
            paintTriangle(
                FVector2D(blendRight - wordEndHandleSize * 0.6f, transitionHandleCenterY),
                FVector2D(blendRight + wordEndHandleSize * 0.4f, transitionHandleCenterY - wordEndHandleSize * 0.5f),
                FVector2D(blendRight + wordEndHandleSize * 0.4f, transitionHandleCenterY + wordEndHandleSize * 0.5f),
                geometry,
                wordBlendEndHovered ? FLinearColor::White : wordHandleColor,
                *brush,
                elements,
                layer + 5);

            for (const TPair<float, EHandle>& wordEdge : {
                    TPair<float, EHandle>(wordLeft, EHandle::WordStart),
                    TPair<float, EHandle>(wordRight, EHandle::WordEnd)})
            {
                const bool edgeHovered =
                    (hover.wordIndex == wordIndex && hover.handle == wordEdge.Value) ||
                    (drag.wordIndex == wordIndex && drag.handle == wordEdge.Value);
                const float edgeSize = edgeHovered ? 15.0f : 12.0f;
                paintTriangle(
                    FVector2D(wordEdge.Key, boundaryHandleTop + edgeSize),
                    FVector2D(wordEdge.Key - edgeSize * 0.55f, boundaryHandleTop),
                    FVector2D(wordEdge.Key + edgeSize * 0.55f, boundaryHandleTop),
                    geometry,
                    edgeHovered
                        ? FLinearColor::White
                        : FLinearColor(0.72f, 0.82f, 0.9f, 1.0f),
                    *brush,
                    elements,
                    layer + 5);
            }

            for (int32 transitionIndex = 0;
                transitionIndex < word.transitions.Num();
                ++transitionIndex)
            {
                const FFaceCorrectionVisemeTransition& transition =
                    word.transitions[transitionIndex];
                const float transitionLeft = timeline.getOffsetAtFrame(
                    transition.blendStartFrame);
                const float boundary = timeline.getOffsetAtFrame(
                    transition.boundaryFrame);
                const float transitionRight = timeline.getOffsetAtFrame(
                    transition.blendEndFrame + 1);
                const FLinearColor previousColor = getVisemeColor(
                    word.visemes[transitionIndex]);
                const FLinearColor nextColor = getVisemeColor(
                    word.visemes[transitionIndex + 1]);
                const bool boundaryHovered =
                    (hover.wordIndex == wordIndex &&
                        hover.visemeIndex == transitionIndex &&
                        hover.handle == EHandle::VisemeBoundary) ||
                    (drag.wordIndex == wordIndex &&
                        drag.visemeIndex == transitionIndex &&
                        drag.handle == EHandle::VisemeBoundary);
                const bool startHovered =
                    (hover.wordIndex == wordIndex &&
                        hover.visemeIndex == transitionIndex &&
                        hover.handle == EHandle::TransitionStart) ||
                    (drag.wordIndex == wordIndex &&
                        drag.visemeIndex == transitionIndex &&
                        drag.handle == EHandle::TransitionStart);
                const bool endHovered =
                    (hover.wordIndex == wordIndex &&
                        hover.visemeIndex == transitionIndex &&
                        hover.handle == EHandle::TransitionEnd) ||
                    (drag.wordIndex == wordIndex &&
                        drag.visemeIndex == transitionIndex &&
                        drag.handle == EHandle::TransitionEnd);

                const float boundarySize = boundaryHovered ? 16.0f : 13.0f;
                paintTriangle(
                    FVector2D(boundary, boundaryHandleTop + boundarySize),
                    FVector2D(boundary - boundarySize * 0.55f, boundaryHandleTop),
                    FVector2D(boundary + boundarySize * 0.55f, boundaryHandleTop),
                    geometry,
                    boundaryHovered
                        ? FLinearColor::White
                        : FLinearColor(0.82f, 0.9f, 1.0f, 1.0f),
                    *brush,
                    elements,
                    layer + 5);
                drawBox(
                    3,
                    boundary - 0.5f,
                    influenceGraphTop,
                    1.0f,
                    influenceGraphBottom - influenceGraphTop,
                    FLinearColor(0.9f, 0.9f, 0.95f, 0.55f));

                const float startSize = startHovered ? 16.0f : 13.0f;
                const float endSize = endHovered ? 16.0f : 13.0f;
                paintTriangle(
                    FVector2D(transitionLeft + startSize * 0.6f, transitionHandleCenterY),
                    FVector2D(transitionLeft - startSize * 0.4f, transitionHandleCenterY - startSize * 0.5f),
                    FVector2D(transitionLeft - startSize * 0.4f, transitionHandleCenterY + startSize * 0.5f),
                    geometry,
                    startHovered ? FLinearColor::White : previousColor,
                    *brush,
                    elements,
                    layer + 5);
                paintTriangle(
                    FVector2D(transitionRight - endSize * 0.6f, transitionHandleCenterY),
                    FVector2D(transitionRight + endSize * 0.4f, transitionHandleCenterY - endSize * 0.5f),
                    FVector2D(transitionRight + endSize * 0.4f, transitionHandleCenterY + endSize * 0.5f),
                    geometry,
                    endHovered ? FLinearColor::White : nextColor,
                    *brush,
                    elements,
                    layer + 5);
            }
        }

        drawBox(6, trimStartX, 0.0f, 2.0f, faceLanePaintHeight, FLinearColor(0.15f, 0.65f, 0.95f, 0.95f));
        drawBox(6, trimEndX - 2.0f, 0.0f, 2.0f, faceLanePaintHeight, FLinearColor(0.15f, 0.65f, 0.95f, 0.95f));
        const float playheadX = timeline.getOffsetAtFrame(timeline.currentFrame);
        drawBox(7, playheadX, 0.0f, 1.0f, faceLanePaintHeight, FLinearColor(0.8f, 0.8f, 0.8f, 0.75f));
        return layer + 7;
    }

    virtual FReply OnMouseButtonDown(
        const FGeometry& geometry,
        const FPointerEvent& event) override
    {
        const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin();
        if (!extension.IsValid() || !extension->getContext().animation.IsValid())
        {
            return FReply::Unhandled();
        }

        const FVector2D local = geometry.AbsoluteToLocal(event.GetScreenSpacePosition());
        const FHit hit = hitTest(local);
        if (event.GetEffectingButton() == EKeys::RightMouseButton)
        {
            if (hit.wordIndex == INDEX_NONE)
            {
                return FReply::Unhandled();
            }
            extension->selectWord(hit.wordIndex);
            const TWeakPtr<FFaceCorrectionTimelineExtension> weakOwner = owner;
            FMenuBuilder menu(true, nullptr);
            if (hit.visemeIndex != INDEX_NONE &&
                (hit.handle == EHandle::VisemeBoundary ||
                    hit.handle == EHandle::TransitionStart ||
                    hit.handle == EHandle::TransitionEnd))
            {
                menu.AddSubMenu(
                    FText::FromString(TEXT("Transition Type")),
                    FText::FromString(TEXT("Choose how the two viseme poses blend.")),
                    FNewMenuDelegate::CreateLambda(
                        [weakOwner, hit](FMenuBuilder& transitionMenu)
                        {
                            const EFaceCorrectionTransitionType types[] = {
                                EFaceCorrectionTransitionType::Smooth,
                                EFaceCorrectionTransitionType::Linear,
                                EFaceCorrectionTransitionType::Sharp,
                                EFaceCorrectionTransitionType::EaseIntoNext,
                                EFaceCorrectionTransitionType::HoldThenRelease};
                            for (const EFaceCorrectionTransitionType type : types)
                            {
                                transitionMenu.AddMenuEntry(
                                    transitionTypeText(type),
                                    FText::GetEmpty(),
                                    FSlateIcon(),
                                    FUIAction(FExecuteAction::CreateLambda(
                                        [weakOwner, hit, type]()
                                        {
                                            const TSharedPtr<FFaceCorrectionTimelineExtension> pinned =
                                                weakOwner.Pin();
                                            if (pinned.IsValid() &&
                                                pinned->getWords().IsValidIndex(hit.wordIndex) &&
                                                pinned->getWords()[hit.wordIndex].transitions.IsValidIndex(
                                                    hit.visemeIndex))
                                            {
                                                pinned->getWords()[hit.wordIndex]
                                                    .transitions[hit.visemeIndex]
                                                    .transitionType = type;
                                                pinned->wordsChanged();
                                            }
                                        })));
                            }
                        }));
            }
            menu.AddMenuEntry(
                FText::FromString(TEXT("Block Mouthing Rules")),
                FText::FromString(TEXT("View and change the ordered rules assigned to this block.")),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda(
                    [weakOwner, hit]()
                    {
                        if (const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weakOwner.Pin())
                        {
                            pinned->openRulesForBlock(hit.wordIndex, hit.visemeIndex);
                        }
                    })));
            menu.AddMenuEntry(
                FText::FromString(TEXT("Review Phonemes and Visemes")),
                FText::GetEmpty(),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda(
                    [weakOwner, hit]()
                    {
                        if (const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weakOwner.Pin())
                        {
                            pinned->reviewWord(hit.wordIndex);
                        }
                    })));
            menu.AddMenuEntry(
                FText::FromString(TEXT("Delete Word")),
                FText::GetEmpty(),
                FSlateIcon(),
                FUIAction(FExecuteAction::CreateLambda(
                    [weakOwner, hit]()
                    {
                        if (const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weakOwner.Pin())
                        {
                            pinned->deleteWord(hit.wordIndex);
                        }
                    })));
            FSlateApplication::Get().PushMenu(
                SharedThis(this),
                event.GetEventPath() ? *event.GetEventPath() : FWidgetPath{},
                menu.MakeWidget(),
                event.GetScreenSpacePosition(),
                FPopupTransitionEffect::ContextMenu);
            return FReply::Handled();
        }
        if (event.GetEffectingButton() != EKeys::LeftMouseButton)
        {
            return FReply::Unhandled();
        }
        if (hit.wordIndex == INDEX_NONE)
        {
            extension->selectWord(INDEX_NONE);
            extension->insertWordAtFrame(extension->getContext().getFrameAtOffset(local.X));
            return FReply::Handled();
        }

        extension->selectWord(hit.wordIndex);
        if (hit.handle == EHandle::SelectOnly)
        {
            return FReply::Handled();
        }
        drag = hit;
        originalWord = extension->getWords()[hit.wordIndex];
        draftWord = originalWord;
        dragFrame = extension->getContext().getFrameAtOffset(local.X);
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }

    virtual FReply OnMouseMove(
        const FGeometry& geometry,
        const FPointerEvent& event) override
    {
        const FVector2D local = geometry.AbsoluteToLocal(event.GetScreenSpacePosition());
        if (!HasMouseCapture())
        {
            const FHit next = hitTest(local);
            if (hover.wordIndex != next.wordIndex ||
                hover.visemeIndex != next.visemeIndex ||
                hover.handle != next.handle)
            {
                hover = next;
                Invalidate(EInvalidateWidgetReason::Paint);
            }
            return FReply::Unhandled();
        }
        const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin();
        if (!extension.IsValid())
        {
            return FReply::Unhandled();
        }
        const float x = local.X;
        if (drag.wordIndex == INDEX_NONE)
        {
            return FReply::Unhandled();
        }
        updateDraft(extension->getContext().getFrameAtOffset(x));
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    virtual FReply OnMouseButtonUp(
        const FGeometry& geometry,
        const FPointerEvent& event) override
    {
        if (event.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture())
        {
            return FReply::Unhandled();
        }
        const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin();
        if (extension.IsValid() && extension->getWords().IsValidIndex(drag.wordIndex))
        {
            const float x = geometry.AbsoluteToLocal(event.GetScreenSpacePosition()).X;
            updateDraft(extension->getContext().getFrameAtOffset(x));
            extension->getWords()[drag.wordIndex] = draftWord;
            extension->wordsChanged();
        }
        drag = {};
        return FReply::Handled().ReleaseMouseCapture();
    }

    virtual void OnMouseCaptureLost(const FCaptureLostEvent& event) override
    {
        drag = {};
        SLeafWidget::OnMouseCaptureLost(event);
    }

    virtual void OnMouseLeave(const FPointerEvent& event) override
    {
        hover = {};
        Invalidate(EInvalidateWidgetReason::Paint);
        SLeafWidget::OnMouseLeave(event);
    }

    virtual FReply OnMouseButtonDoubleClick(
        const FGeometry& geometry,
        const FPointerEvent& event) override
    {
        if (event.GetEffectingButton() == EKeys::LeftMouseButton)
        {
            const FHit hit = hitTest(geometry.AbsoluteToLocal(event.GetScreenSpacePosition()));
            if (hit.wordIndex != INDEX_NONE)
            {
                if (const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin())
                {
                    extension->reviewWord(hit.wordIndex);
                }
                return FReply::Handled();
            }
        }
        return FReply::Unhandled();
    }

    virtual FCursorReply OnCursorQuery(
        const FGeometry& geometry,
        const FPointerEvent& event) const override
    {
        const FHit hit = HasMouseCapture()
            ? drag
            : hitTest(geometry.AbsoluteToLocal(event.GetScreenSpacePosition()));
        if (hit.wordIndex == INDEX_NONE)
        {
            return FCursorReply::Unhandled();
        }
        return FCursorReply::Cursor(
            hit.handle == EHandle::WordMove
                ? EMouseCursor::CardinalCross
                : hit.handle == EHandle::SelectOnly
                    ? EMouseCursor::Default
                    : EMouseCursor::ResizeLeftRight);
    }

private:
    enum class EHandle
    {
        WordMove,
        SelectOnly,
        WordStart,
        WordEnd,
        BlendStart,
        BlendEnd,
        VisemeBoundary,
        TransitionStart,
        TransitionEnd
    };

    struct FHit
    {
        int32 wordIndex = INDEX_NONE;
        int32 visemeIndex = INDEX_NONE;
        EHandle handle = EHandle::WordMove;
    };

    FHit hitTest(const FVector2D local) const
    {
        const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin();
        if (!extension.IsValid())
        {
            return {};
        }
        const FHandposeTimelineExtensionContext& timeline = extension->getContext();
        const TArray<FFaceCorrectionWordBlock>& words = extension->getWords();
        for (int32 wordIndex = words.Num() - 1; wordIndex >= 0; --wordIndex)
        {
            const FFaceCorrectionWordBlock& word = words[wordIndex];
            if (local.Y >= visemeTop)
            {
                const bool selected =
                    extension->getSelectedWordIndex() == wordIndex;
                const float blendLeft = timeline.getOffsetAtFrame(word.blendStartFrame);
                const float wordLeft = timeline.getOffsetAtFrame(word.startFrame);
                const float wordRight = timeline.getOffsetAtFrame(word.endFrame + 1);
                const float blendRight = timeline.getOffsetAtFrame(word.blendEndFrame + 1);
                const bool boundaryRow = local.Y >= boundaryHandleTop &&
                    local.Y < influenceGraphTop;
                const bool lowerHandleRow = local.Y >= influenceGraphBottom - 4.0f;
                if (selected && (boundaryRow || lowerHandleRow))
                {
                    float bestDistance = 11.0f;
                    FHit transitionHit;
                    if (boundaryRow)
                    {
                        const float startDistance = FMath::Abs(
                            static_cast<float>(local.X) - wordLeft);
                        if (startDistance <= bestDistance)
                        {
                            bestDistance = startDistance;
                            transitionHit = {
                                wordIndex,
                                INDEX_NONE,
                                EHandle::WordStart};
                        }
                        const float endDistance = FMath::Abs(
                            static_cast<float>(local.X) - wordRight);
                        if (endDistance <= bestDistance)
                        {
                            bestDistance = endDistance;
                            transitionHit = {
                                wordIndex,
                                INDEX_NONE,
                                EHandle::WordEnd};
                        }
                    }
                    else
                    {
                        const float startDistance = FMath::Abs(
                            static_cast<float>(local.X) - blendLeft);
                        if (startDistance <= bestDistance)
                        {
                            bestDistance = startDistance;
                            transitionHit = {
                                wordIndex,
                                INDEX_NONE,
                                EHandle::BlendStart};
                        }
                        const float endDistance = FMath::Abs(
                            static_cast<float>(local.X) - blendRight);
                        if (endDistance <= bestDistance)
                        {
                            bestDistance = endDistance;
                            transitionHit = {
                                wordIndex,
                                INDEX_NONE,
                                EHandle::BlendEnd};
                        }
                    }
                    for (int32 transitionIndex = 0;
                        transitionIndex < word.transitions.Num();
                        ++transitionIndex)
                    {
                        const FFaceCorrectionVisemeTransition& transition =
                            word.transitions[transitionIndex];
                        const EHandle firstHandle = boundaryRow
                            ? EHandle::VisemeBoundary
                            : EHandle::TransitionStart;
                        const float firstX = timeline.getOffsetAtFrame(
                            boundaryRow
                                ? transition.boundaryFrame
                                : transition.blendStartFrame);
                        const float firstDistance = FMath::Abs(
                            static_cast<float>(local.X) - firstX);
                        if (firstDistance <= bestDistance)
                        {
                            bestDistance = firstDistance;
                            transitionHit = {wordIndex, transitionIndex, firstHandle};
                        }
                        if (!boundaryRow)
                        {
                            const float endX = timeline.getOffsetAtFrame(
                                transition.blendEndFrame + 1);
                            const float endDistance = FMath::Abs(
                                static_cast<float>(local.X) - endX);
                            if (endDistance <= bestDistance)
                            {
                                bestDistance = endDistance;
                                transitionHit = {
                                    wordIndex,
                                    transitionIndex,
                                    EHandle::TransitionEnd};
                            }
                        }
                    }
                    if (transitionHit.wordIndex != INDEX_NONE)
                    {
                        return transitionHit;
                    }
                }
                if (local.Y < boundaryHandleTop)
                {
                    for (int32 visemeIndex = 0;
                        visemeIndex < word.visemes.Num();
                        ++visemeIndex)
                    {
                        const FFaceCorrectionVisemeBlock& viseme =
                            word.visemes[visemeIndex];
                        const float left = timeline.getOffsetAtFrame(viseme.startFrame);
                        const float right = timeline.getOffsetAtFrame(viseme.endFrame + 1);
                        if (local.X >= left && local.X <= right)
                        {
                            return {wordIndex, visemeIndex, EHandle::WordMove};
                        }
                    }
                }
                if (selected && local.X >= blendLeft && local.X <= blendRight)
                {
                    return {wordIndex, INDEX_NONE, EHandle::SelectOnly};
                }
            }
            else
            {
                const float blendLeft = timeline.getOffsetAtFrame(word.blendStartFrame);
                const float left = timeline.getOffsetAtFrame(word.startFrame);
                const float right = timeline.getOffsetAtFrame(word.endFrame + 1);
                const float blendRight = timeline.getOffsetAtFrame(word.blendEndFrame + 1);
                if (local.Y >= wordBlendHandleTop &&
                    FMath::Abs(static_cast<float>(local.X) - blendLeft) <= 5.0f)
                {
                    return {wordIndex, INDEX_NONE, EHandle::BlendStart};
                }
                if (local.Y >= wordBlendHandleTop &&
                    FMath::Abs(static_cast<float>(local.X) - blendRight) <= 5.0f)
                {
                    return {wordIndex, INDEX_NONE, EHandle::BlendEnd};
                }
                if (local.Y < wordBlendHandleTop &&
                    FMath::Abs(static_cast<float>(local.X) - left) <= 5.0f)
                {
                    return {wordIndex, INDEX_NONE, EHandle::WordStart};
                }
                if (local.Y < wordBlendHandleTop &&
                    FMath::Abs(static_cast<float>(local.X) - right) <= 5.0f)
                {
                    return {wordIndex, INDEX_NONE, EHandle::WordEnd};
                }
                if (local.X >= blendLeft && local.X <= blendRight)
                {
                    return {wordIndex, INDEX_NONE, EHandle::WordMove};
                }
            }
        }
        return {};
    }

    void scaleWord(const int32 newStart, const int32 newEnd)
    {
        const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin();
        if (!extension.IsValid())
        {
            return;
        }
        draftWord = originalWord;
        const double oldDuration = originalWord.endFrame - originalWord.startFrame + 1.0;
        const double newDuration = newEnd - newStart + 1.0;
        const int32 blendInFrames = originalWord.startFrame - originalWord.blendStartFrame;
        const int32 blendOutFrames = originalWord.blendEndFrame - originalWord.endFrame;
        draftWord.startFrame = newStart;
        draftWord.endFrame = newEnd;
        draftWord.blendStartFrame = FMath::Max(0, newStart - blendInFrames);
        draftWord.blendEndFrame = FMath::Min(
            extension->getContext().lastFrame,
            newEnd + blendOutFrames);
        for (int32 index = 0; index < draftWord.visemes.Num(); ++index)
        {
            const FFaceCorrectionVisemeBlock& original = originalWord.visemes[index];
            FFaceCorrectionVisemeBlock& scaled = draftWord.visemes[index];
            const double startAlpha = (original.startFrame - originalWord.startFrame) / oldDuration;
            const double endAlpha = (original.endFrame + 1.0 - originalWord.startFrame) / oldDuration;
            scaled.startFrame = FMath::Clamp(
                FMath::RoundToInt(newStart + startAlpha * newDuration),
                newStart,
                newEnd);
            const int32 endExclusive = FMath::Clamp(
                FMath::RoundToInt(newStart + endAlpha * newDuration),
                scaled.startFrame + 1,
                newEnd + 1);
            scaled.endFrame = endExclusive - 1;
        }
        for (int32 index = 0; index < draftWord.transitions.Num(); ++index)
        {
            const FFaceCorrectionVisemeTransition& original = originalWord.transitions[index];
            FFaceCorrectionVisemeTransition& scaled = draftWord.transitions[index];
            const int32 boundary = draftWord.visemes[index + 1].startFrame;
            const auto scaleFrame = [oldDuration, newDuration, newStart, this](const int32 frame)
            {
                const double alpha =
                    (frame - originalWord.startFrame) / oldDuration;
                return FMath::RoundToInt(newStart + alpha * newDuration);
            };
            scaled.boundaryFrame = boundary;
            scaled.blendStartFrame = FMath::Clamp(
                scaleFrame(original.blendStartFrame),
                draftWord.visemes[index].startFrame,
                boundary);
            scaled.blendEndFrame = FMath::Clamp(
                scaleFrame(original.blendEndFrame),
                boundary,
                draftWord.visemes[index + 1].endFrame);
        }
    }

    void updateDraft(const int32 frame)
    {
        const TSharedPtr<FFaceCorrectionTimelineExtension> extension = owner.Pin();
        if (!extension.IsValid())
        {
            return;
        }
        const int32 delta = frame - dragFrame;
        draftWord = originalWord;
        switch (drag.handle)
        {
        case EHandle::WordMove:
        {
            const int32 clampedDelta = FMath::Clamp(
                delta,
                -originalWord.blendStartFrame,
                extension->getContext().lastFrame - originalWord.blendEndFrame);
            draftWord.blendStartFrame += clampedDelta;
            draftWord.startFrame += clampedDelta;
            draftWord.endFrame += clampedDelta;
            draftWord.blendEndFrame += clampedDelta;
            for (FFaceCorrectionVisemeBlock& viseme : draftWord.visemes)
            {
                viseme.startFrame += clampedDelta;
                viseme.endFrame += clampedDelta;
            }
            for (FFaceCorrectionVisemeTransition& transition : draftWord.transitions)
            {
                transition.boundaryFrame += clampedDelta;
                transition.blendStartFrame += clampedDelta;
                transition.blendEndFrame += clampedDelta;
            }
            break;
        }
        case EHandle::WordStart:
            scaleWord(
                FMath::Clamp(originalWord.startFrame + delta, 0, originalWord.endFrame),
                originalWord.endFrame);
            break;
        case EHandle::WordEnd:
            scaleWord(
                originalWord.startFrame,
                FMath::Clamp(
                    originalWord.endFrame + delta,
                    originalWord.startFrame,
                    extension->getContext().lastFrame));
            break;
        case EHandle::BlendStart:
            draftWord.blendStartFrame = FMath::Clamp(
                originalWord.blendStartFrame + delta,
                0,
                originalWord.startFrame);
            break;
        case EHandle::BlendEnd:
            draftWord.blendEndFrame = FMath::Clamp(
                originalWord.blendEndFrame + delta,
                originalWord.endFrame,
                extension->getContext().lastFrame);
            break;
        case EHandle::VisemeBoundary:
        case EHandle::TransitionStart:
        case EHandle::TransitionEnd:
            if (draftWord.transitions.IsValidIndex(drag.visemeIndex) &&
                draftWord.visemes.IsValidIndex(drag.visemeIndex + 1))
            {
                const FFaceCorrectionVisemeTransition& original =
                    originalWord.transitions[drag.visemeIndex];
                FFaceCorrectionVisemeTransition& transition =
                    draftWord.transitions[drag.visemeIndex];
                FFaceCorrectionVisemeBlock& previous =
                    draftWord.visemes[drag.visemeIndex];
                FFaceCorrectionVisemeBlock& next =
                    draftWord.visemes[drag.visemeIndex + 1];
                if (drag.handle == EHandle::VisemeBoundary)
                {
                    const int32 minimumBoundary = previous.startFrame + 1;
                    const int32 maximumBoundary = next.endFrame;
                    const int32 newBoundary = FMath::Clamp(
                        original.boundaryFrame + delta,
                        minimumBoundary,
                        maximumBoundary);
                    const int32 appliedDelta = newBoundary - original.boundaryFrame;
                    previous.endFrame = newBoundary - 1;
                    next.startFrame = newBoundary;
                    transition.boundaryFrame = newBoundary;
                    transition.blendStartFrame = FMath::Clamp(
                        original.blendStartFrame + appliedDelta,
                        previous.startFrame,
                        newBoundary);
                    transition.blendEndFrame = FMath::Clamp(
                        original.blendEndFrame + appliedDelta,
                        newBoundary,
                        next.endFrame);
                    if (draftWord.transitions.IsValidIndex(drag.visemeIndex - 1))
                    {
                        FFaceCorrectionVisemeTransition& previousTransition =
                            draftWord.transitions[drag.visemeIndex - 1];
                        previousTransition.blendEndFrame = FMath::Clamp(
                            previousTransition.blendEndFrame,
                            previousTransition.boundaryFrame,
                            previous.endFrame);
                    }
                    if (draftWord.transitions.IsValidIndex(drag.visemeIndex + 1))
                    {
                        FFaceCorrectionVisemeTransition& nextTransition =
                            draftWord.transitions[drag.visemeIndex + 1];
                        nextTransition.blendStartFrame = FMath::Clamp(
                            nextTransition.blendStartFrame,
                            next.startFrame,
                            nextTransition.boundaryFrame);
                    }
                }
                else if (drag.handle == EHandle::TransitionStart)
                {
                    transition.blendStartFrame = FMath::Clamp(
                        original.blendStartFrame + delta,
                        previous.startFrame,
                        transition.boundaryFrame);
                }
                else
                {
                    transition.blendEndFrame = FMath::Clamp(
                        original.blendEndFrame + delta,
                        transition.boundaryFrame,
                        next.endFrame);
                }
            }
            break;
        }
    }

    TWeakPtr<FFaceCorrectionTimelineExtension> owner;
    FHit hover;
    FHit drag;
    int32 dragFrame = 0;
    FFaceCorrectionWordBlock originalWord;
    FFaceCorrectionWordBlock draftWord;
};

FFaceCorrectionTimelineExtension::FFaceCorrectionTimelineExtension()
{
    restoreConfig();
}

FName FFaceCorrectionTimelineExtension::getExtensionId() const
{
    return TEXT("FaceCorrection");
}

FText FFaceCorrectionTimelineExtension::getDisplayName() const
{
    return FText::FromString(TEXT("Face Correction"));
}

FText FFaceCorrectionTimelineExtension::getTooltip() const
{
    return FText::FromString(TEXT("Show the word-mouthing correction timeline."));
}

bool FFaceCorrectionTimelineExtension::isEnabled() const
{
    return enabled;
}

void FFaceCorrectionTimelineExtension::setEnabled(const bool newEnabled)
{
    enabled = newEnabled;
    if (context.requestPreviewRebuild)
    {
        context.requestPreviewRebuild(true);
    }
    if (context.setStatus)
    {
        context.setStatus(FText::FromString(
            enabled
                ? TEXT("Face correction mode is active. Click the lane to insert a word.")
                : TEXT("Face correction mode is inactive.")));
    }
}

void FFaceCorrectionTimelineExtension::onToolbarClicked()
{
    openSettings();
}

TSharedRef<SWidget> FFaceCorrectionTimelineExtension::createLane()
{
    const TWeakPtr<FFaceCorrectionTimelineExtension> weak = SharedThis(this);
    TSharedRef<SFaceCorrectionLane> createdLane =
        SNew(SFaceCorrectionLane).Owner(weak);
    lane = createdLane;

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
        .Padding(12.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 5.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Face Correction")))
                    .Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .Text_Lambda(
                        [weak]()
                        {
                            const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin();
                            return FText::FromString(
                                pinned.IsValid() && pinned->getConfig() != nullptr
                                    ? pinned->getConfig()->GetName()
                                    : TEXT("No config selected"));
                        })
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Left)
            [
                createdLane
            ]
        ];
}

void FFaceCorrectionTimelineExtension::updateContext(
    const FHandposeTimelineExtensionContext& newContext)
{
    if (activeAnimation.Get() != newContext.animation.Get())
    {
        words.Reset();
        selectedWordIndex = INDEX_NONE;
        activeAnimation = newContext.animation;
    }
    context = newContext;
    if (const TSharedPtr<SFaceCorrectionLane> pinned = lane.Pin())
    {
        pinned->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
}

bool FFaceCorrectionTimelineExtension::hasPreviewChanges() const
{
    return config.IsValid() && !words.IsEmpty();
}

UAnimSequence* FFaceCorrectionTimelineExtension::buildPreview(
    UAnimSequence& source,
    FString& errorMessage)
{
    return config.IsValid()
        ? FFaceCorrectionPreviewBuilder::apply(source, *config, words, errorMessage)
        : &source;
}

bool FFaceCorrectionTimelineExtension::deleteSelection()
{
    if (!words.IsValidIndex(selectedWordIndex))
    {
        return false;
    }
    deleteWord(selectedWordIndex);
    return true;
}

const FHandposeTimelineExtensionContext&
FFaceCorrectionTimelineExtension::getContext() const
{
    return context;
}

const TArray<FFaceCorrectionWordBlock>&
FFaceCorrectionTimelineExtension::getWords() const
{
    return words;
}

TArray<FFaceCorrectionWordBlock>& FFaceCorrectionTimelineExtension::getWords()
{
    return words;
}

const UFaceCorrectionConfig* FFaceCorrectionTimelineExtension::getConfig() const
{
    return config.Get();
}

int32 FFaceCorrectionTimelineExtension::getSelectedWordIndex() const
{
    return selectedWordIndex;
}

void FFaceCorrectionTimelineExtension::selectWord(const int32 wordIndex)
{
    selectedWordIndex = words.IsValidIndex(wordIndex) ? wordIndex : INDEX_NONE;
    if (const TSharedPtr<SFaceCorrectionLane> pinned = lane.Pin())
    {
        pinned->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
}

void FFaceCorrectionTimelineExtension::openSettings()
{
    if (const TSharedPtr<SWindow> existing = settingsWindow.Pin())
    {
        existing->BringToFront(true);
        return;
    }

    const TSharedRef<SWindow> window = SNew(SWindow)
        .Title(FText::FromString(TEXT("Face Correction Settings")))
        .ClientSize(FVector2D(540.0f, 190.0f))
        .SupportsMaximize(false)
        .SupportsMinimize(false);
    settingsWindow = window;
    const TWeakPtr<FFaceCorrectionTimelineExtension> weak = SharedThis(this);
    const TWeakPtr<SWindow> weakWindow = window;
    const TSharedRef<TWeakObjectPtr<UFaceCorrectionConfig>> selectedConfig =
        MakeShared<TWeakObjectPtr<UFaceCorrectionConfig>>(config.Get());
    const TSharedRef<bool> selectedEnabled = MakeShared<bool>(enabled);
    const TSharedRef<STextBlock> errorText = SNew(STextBlock).AutoWrapText(true);

    window->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.0f, 10.0f, 12.0f, 5.0f)
        [
            SNew(SCheckBox)
            .IsChecked_Lambda(
                [selectedEnabled]()
                {
                    return *selectedEnabled
                        ? ECheckBoxState::Checked
                        : ECheckBoxState::Unchecked;
                })
            .OnCheckStateChanged_Lambda(
                [selectedEnabled](const ECheckBoxState state)
                {
                    *selectedEnabled = state == ECheckBoxState::Checked;
                })
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Enable Face Correction timeline")))
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.0f, 5.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Face Correction Config")))
            ]
            + SHorizontalBox::Slot()
            .FillWidth(2.0f)
            [
                SNew(SObjectPropertyEntryBox)
                .AllowedClass(UFaceCorrectionConfig::StaticClass())
                .ObjectPath_Lambda(
                    [selectedConfig]()
                    {
                        return selectedConfig->IsValid()
                            ? selectedConfig->Get()->GetPathName()
                            : FString();
                    })
                .OnObjectChanged_Lambda(
                    [selectedConfig](const FAssetData& assetData)
                    {
                        *selectedConfig = Cast<UFaceCorrectionConfig>(assetData.GetAsset());
                    })
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.0f, 3.0f)
        [
            errorText
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(12.0f, 8.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Cancel")))
                .OnClicked_Lambda(
                    [weakWindow]()
                    {
                        if (const TSharedPtr<SWindow> pinned = weakWindow.Pin())
                        {
                            pinned->RequestDestroyWindow();
                        }
                        return FReply::Handled();
                    })
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Apply")))
                .OnClicked_Lambda(
                    [weak, weakWindow, selectedConfig, selectedEnabled, errorText]()
                    {
                        const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin();
                        if (!pinned.IsValid())
                        {
                            return FReply::Handled();
                        }
                        if (*selectedEnabled && !selectedConfig->IsValid())
                        {
                            errorText->SetText(FText::FromString(
                                TEXT("Choose a Face Correction config before enabling the timeline.")));
                            return FReply::Handled();
                        }
                        pinned->setConfig(selectedConfig->Get());
                        pinned->setEnabled(*selectedEnabled);
                        if (const TSharedPtr<SWindow> settings = weakWindow.Pin())
                        {
                            settings->RequestDestroyWindow();
                        }
                        return FReply::Handled();
                    })
            ]
        ]);
    window->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
        [weak, weakWindow](const TSharedRef<SWindow>&)
        {
            if (const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin();
                pinned.IsValid() && pinned->settingsWindow.Pin() == weakWindow.Pin())
            {
                pinned->settingsWindow.Reset();
            }
        }));
    FSlateApplication::Get().AddWindow(window);
}

void FFaceCorrectionTimelineExtension::restoreConfig()
{
    FString assetPath;
    if (GConfig->GetString(configSection, configKey, assetPath, GEditorPerProjectIni) &&
        !assetPath.IsEmpty())
    {
        setConfig(LoadObject<UFaceCorrectionConfig>(nullptr, *assetPath));
    }
}

void FFaceCorrectionTimelineExtension::setConfig(UFaceCorrectionConfig* newConfig)
{
    config.Reset(newConfig);
    if (newConfig != nullptr)
    {
        GConfig->SetString(
            configSection,
            configKey,
            *newConfig->GetPathName(),
            GEditorPerProjectIni);
        GConfig->Flush(false, GEditorPerProjectIni);
    }
    wordsChanged();
}

void FFaceCorrectionTimelineExtension::insertWordAtFrame(const int32 frame)
{
    if (!context.animation.IsValid())
    {
        return;
    }
    if (config.Get() == nullptr)
    {
        if (context.setStatus)
        {
            context.setStatus(FText::FromString(
                TEXT("Choose a Face Correction config before inserting a word.")));
        }
        return;
    }
    if (const TSharedPtr<SWindow> window = activeWindow.Pin())
    {
        window->BringToFront(true);
        return;
    }

    const TSharedRef<SWindow> window = SNew(SWindow)
        .Title(FText::FromString(TEXT("Insert Word Mouthing")))
        .ClientSize(FVector2D(460.0f, 160.0f))
        .SupportsMaximize(false)
        .SupportsMinimize(false);
    activeWindow = window;
    const TWeakPtr<FFaceCorrectionTimelineExtension> weak = SharedThis(this);
    const TWeakPtr<SWindow> weakWindow = window;
    const TSharedRef<SEditableTextBox> textBox = SNew(SEditableTextBox)
        .HintText(FText::FromString(TEXT("Dutch word or phrase")));
    const TSharedRef<STextBlock> errorText = SNew(STextBlock)
        .AutoWrapText(true);
    window->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(10.0f, 10.0f, 10.0f, 6.0f)
        [
            textBox
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(10.0f, 0.0f)
        [
            errorText
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Right)
        .Padding(10.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Convert to Phonemes")))
            .OnClicked_Lambda(
                [weak, weakWindow, textBox, errorText, frame]()
                {
                    const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin();
                    if (!pinned.IsValid())
                    {
                        return FReply::Handled();
                    }
                    const FString text = textBox->GetText().ToString().TrimStartAndEnd();
                    FString phonemeText;
                    FString error;
                    TArray<FFaceCorrectionReviewItem> items;
                    if (text.IsEmpty() ||
                        !pinned->convertText(text, phonemeText, items, error))
                    {
                        errorText->SetText(FText::FromString(
                            text.IsEmpty() ? TEXT("Enter a Dutch word or phrase.") : error));
                        return FReply::Handled();
                    }
                    if (const TSharedPtr<SWindow> currentWindow = weakWindow.Pin())
                    {
                        currentWindow->RequestDestroyWindow();
                    }
                    pinned->activeWindow.Reset();
                    pinned->openReview(
                        text,
                        phonemeText,
                        MoveTemp(items),
                        frame,
                        INDEX_NONE);
                    return FReply::Handled();
                })
        ]);
    window->SetWidgetToFocusOnActivate(textBox);
    window->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
        [weak, weakWindow](const TSharedRef<SWindow>&)
        {
            if (const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin();
                pinned.IsValid() && pinned->activeWindow.Pin() == weakWindow.Pin())
            {
                pinned->activeWindow.Reset();
            }
        }));
    FSlateApplication::Get().AddWindow(window);
}

void FFaceCorrectionTimelineExtension::reviewWord(const int32 wordIndex)
{
    if (!words.IsValidIndex(wordIndex) || config.Get() == nullptr)
    {
        return;
    }
    const FFaceCorrectionWordBlock& word = words[wordIndex];
    TArray<FFaceCorrectionReviewItem> items;
    items.Reserve(word.visemes.Num());
    for (const FFaceCorrectionVisemeBlock& viseme : word.visemes)
    {
        items.Add({viseme.sourcePhoneme, viseme.visemeId});
    }
    openReview(word.text, word.phonemeText, MoveTemp(items), word.startFrame, wordIndex);
}

void FFaceCorrectionTimelineExtension::openRulesForBlock(
    const int32 wordIndex,
    const int32 visemeIndex)
{
    if (!words.IsValidIndex(wordIndex) || config.Get() == nullptr)
    {
        return;
    }
    UFaceCorrectionRuleSet* ruleSet = config->mouthingRuleSet.LoadSynchronous();
    if (ruleSet == nullptr)
    {
        FMessageDialog::Open(
            EAppMsgType::Ok,
            FText::FromString(
                TEXT("Assign a Mouthing Rule Set in the selected Face Correction config first.")));
        return;
    }
    if (const TSharedPtr<SWindow> existing = rulesWindow.Pin())
    {
        existing->RequestDestroyWindow();
    }
    const FFaceCorrectionWordBlock& word = words[wordIndex];
    const FGuid wordId = word.blockId;
    FGuid visemeId;
    FString selectionText = FString::Printf(TEXT("Word: %s"), *word.text);
    if (word.visemes.IsValidIndex(visemeIndex))
    {
        const FFaceCorrectionVisemeBlock& viseme = word.visemes[visemeIndex];
        visemeId = viseme.blockId;
        selectionText += FString::Printf(
            TEXT("    Viseme: %s → %s"),
            *viseme.sourcePhoneme,
            *viseme.visemeId.ToString());
    }

    const TSharedRef<SVerticalBox> appliedRuleRows = SNew(SVerticalBox);
    const TSharedRef<SWindow> window = SNew(SWindow)
        .Title(FText::FromString(TEXT("Block Mouthing Rules")))
        .ClientSize(FVector2D(620.0f, 420.0f))
        .SupportsMaximize(false)
        .SupportsMinimize(false);
    rulesWindow = window;
    const TWeakPtr<FFaceCorrectionTimelineExtension> weak = SharedThis(this);
    const TWeakPtr<SWindow> weakWindow = window;
    const TSharedRef<TFunction<void()>> rebuildRows =
        MakeShared<TFunction<void()>>();
    const TWeakPtr<TFunction<void()>> weakRebuildRows = rebuildRows;
    *rebuildRows = [weak, ruleSet, appliedRuleRows, weakRebuildRows, wordId, visemeId]()
    {
        appliedRuleRows->ClearChildren();
        const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin();
        TArray<FGuid>* appliedRuleIds = pinned.IsValid()
            ? pinned->findAppliedRuleIds(wordId, visemeId)
            : nullptr;
        if (appliedRuleIds == nullptr || appliedRuleIds->IsEmpty())
        {
            appliedRuleRows->AddSlot()
            .AutoHeight()
            .Padding(4.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("No rules are applied to this block.")))
                .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            ];
            return;
        }

        for (int32 index = 0; index < appliedRuleIds->Num(); ++index)
        {
            const FGuid ruleId = (*appliedRuleIds)[index];
            const FFaceCorrectionRuleDefinition* definition =
                ruleSet->rules.FindByPredicate(
                    [ruleId](const FFaceCorrectionRuleDefinition& rule)
                    {
                        return rule.ruleId == ruleId;
                    });
            const FText name = definition != nullptr
                ? definition->displayName
                : FText::FromString(TEXT("Missing rule"));
            const FText stage = definition != nullptr
                ? StaticEnum<EFaceCorrectionRuleStage>()->GetDisplayNameTextByValue(
                    static_cast<int64>(definition->stage))
                : FText::GetEmpty();
            appliedRuleRows->AddSlot()
            .AutoHeight()
            .Padding(0.0f, 2.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
                .Padding(6.0f, 4.0f)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::AsNumber(index + 1))
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(name)
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(8.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(stage)
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("↑")))
                        .IsEnabled(index > 0)
                        .OnClicked_Lambda(
                            [weak, weakRebuildRows, wordId, visemeId, index]()
                            {
                                if (const TSharedPtr<FFaceCorrectionTimelineExtension> owner = weak.Pin())
                                {
                                    if (TArray<FGuid>* ids = owner->findAppliedRuleIds(wordId, visemeId))
                                    {
                                        ids->Swap(index, index - 1);
                                        if (const TSharedPtr<TFunction<void()>> rebuild =
                                            weakRebuildRows.Pin())
                                        {
                                            (*rebuild)();
                                        }
                                    }
                                }
                                return FReply::Handled();
                            })
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("↓")))
                        .IsEnabled(index + 1 < appliedRuleIds->Num())
                        .OnClicked_Lambda(
                            [weak, weakRebuildRows, wordId, visemeId, index]()
                            {
                                if (const TSharedPtr<FFaceCorrectionTimelineExtension> owner = weak.Pin())
                                {
                                    if (TArray<FGuid>* ids = owner->findAppliedRuleIds(wordId, visemeId))
                                    {
                                        ids->Swap(index, index + 1);
                                        if (const TSharedPtr<TFunction<void()>> rebuild =
                                            weakRebuildRows.Pin())
                                        {
                                            (*rebuild)();
                                        }
                                    }
                                }
                                return FReply::Handled();
                            })
                    ]
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Remove")))
                        .OnClicked_Lambda(
                            [weak, weakRebuildRows, wordId, visemeId, ruleId]()
                            {
                                if (const TSharedPtr<FFaceCorrectionTimelineExtension> owner = weak.Pin())
                                {
                                    if (TArray<FGuid>* ids = owner->findAppliedRuleIds(wordId, visemeId))
                                    {
                                        ids->RemoveSingle(ruleId);
                                        if (const TSharedPtr<TFunction<void()>> rebuild =
                                            weakRebuildRows.Pin())
                                        {
                                            (*rebuild)();
                                        }
                                    }
                                }
                                return FReply::Handled();
                            })
                    ]
                ]
            ];
        }
    };

    window->SetContent(
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.0f, 10.0f, 12.0f, 2.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(selectionText))
                .Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Add Rule")))
                .ToolTipText(FText::FromString(
                    TEXT("Choose a rule from the configured ruleset for this block.")))
                .OnClicked_Lambda(
                    [weak, ruleSet, rebuildRows, wordId, visemeId]()
                    {
                        const TSharedRef<SVerticalBox> pickerRows = SNew(SVerticalBox);
                        const TSharedRef<SWindow> picker = SNew(SWindow)
                            .Title(FText::FromString(TEXT("Add Rule to Block")))
                            .ClientSize(FVector2D(440.0f, 360.0f))
                            .SupportsMaximize(false)
                            .SupportsMinimize(false);
                        const TWeakPtr<SWindow> weakPicker = picker;
                        for (const FFaceCorrectionRuleDefinition& rule : ruleSet->rules)
                        {
                            const FGuid ruleId = rule.ruleId;
                            const bool alreadyApplied = [&weak, wordId, visemeId, ruleId]()
                            {
                                const TSharedPtr<FFaceCorrectionTimelineExtension> owner = weak.Pin();
                                const TArray<FGuid>* ids = owner.IsValid()
                                    ? owner->findAppliedRuleIds(wordId, visemeId)
                                    : nullptr;
                                return ids != nullptr && ids->Contains(ruleId);
                            }();
                            const FText stage =
                                StaticEnum<EFaceCorrectionRuleStage>()->GetDisplayNameTextByValue(
                                    static_cast<int64>(rule.stage));
                            pickerRows->AddSlot()
                            .AutoHeight()
                            .Padding(4.0f, 2.0f)
                            [
                                SNew(SButton)
                                .IsEnabled(rule.enabled && !alreadyApplied)
                                .ToolTipText(rule.enabled
                                    ? FText::FromString(TEXT("Apply this rule to the block."))
                                    : FText::FromString(TEXT("This rule is disabled in the ruleset.")))
                                .OnClicked_Lambda(
                                    [weak, weakPicker, rebuildRows, wordId, visemeId, ruleId]()
                                    {
                                        if (const TSharedPtr<FFaceCorrectionTimelineExtension> owner = weak.Pin())
                                        {
                                            if (TArray<FGuid>* ids = owner->findAppliedRuleIds(wordId, visemeId))
                                            {
                                                ids->AddUnique(ruleId);
                                                (*rebuildRows)();
                                            }
                                        }
                                        if (const TSharedPtr<SWindow> currentPicker = weakPicker.Pin())
                                        {
                                            currentPicker->RequestDestroyWindow();
                                        }
                                        return FReply::Handled();
                                    })
                                [
                                    SNew(SHorizontalBox)
                                    + SHorizontalBox::Slot()
                                    .FillWidth(1.0f)
                                    [
                                        SNew(STextBlock)
                                        .Text(rule.displayName)
                                    ]
                                    + SHorizontalBox::Slot()
                                    .AutoWidth()
                                    [
                                        SNew(STextBlock)
                                        .Text(stage)
                                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                                    ]
                                ]
                            ];
                        }
                        picker->SetContent(
                            SNew(SScrollBox)
                            + SScrollBox::Slot()
                            [
                                pickerRows
                            ]);
                        FSlateApplication::Get().AddWindow(picker);
                        return FReply::Handled();
                    })
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(12.0f, 2.0f, 12.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(
                TEXT("These are the ordered rules assigned to this block. Rule execution is not connected yet, so assigning a rule does not change the animation.")))
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
            .AutoWrapText(true)
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(12.0f, 4.0f, 12.0f, 12.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                appliedRuleRows
            ]
        ]);
    window->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
        [weak, weakWindow](const TSharedRef<SWindow>&)
        {
            if (const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin();
                pinned.IsValid() && pinned->rulesWindow.Pin() == weakWindow.Pin())
            {
                pinned->rulesWindow.Reset();
            }
        }));
    (*rebuildRows)();
    FSlateApplication::Get().AddWindow(window);
}

TArray<FGuid>* FFaceCorrectionTimelineExtension::findAppliedRuleIds(
    const FGuid wordId,
    const FGuid visemeId)
{
    FFaceCorrectionWordBlock* word = words.FindByPredicate(
        [wordId](const FFaceCorrectionWordBlock& candidate)
        {
            return candidate.blockId == wordId;
        });
    if (word == nullptr)
    {
        return nullptr;
    }
    if (!visemeId.IsValid())
    {
        return &word->appliedRuleIds;
    }
    FFaceCorrectionVisemeBlock* viseme = word->visemes.FindByPredicate(
        [visemeId](const FFaceCorrectionVisemeBlock& candidate)
        {
            return candidate.blockId == visemeId;
        });
    return viseme != nullptr ? &viseme->appliedRuleIds : nullptr;
}

void FFaceCorrectionTimelineExtension::deleteWord(const int32 wordIndex)
{
    if (words.IsValidIndex(wordIndex))
    {
        words.RemoveAt(wordIndex);
        selectedWordIndex = INDEX_NONE;
        wordsChanged();
    }
}

void FFaceCorrectionTimelineExtension::wordsChanged()
{
    const FGuid selectedId = words.IsValidIndex(selectedWordIndex)
        ? words[selectedWordIndex].blockId
        : FGuid();
    words.Sort(
        [](const FFaceCorrectionWordBlock& left, const FFaceCorrectionWordBlock& right)
        {
            return left.startFrame < right.startFrame;
        });
    selectedWordIndex = selectedId.IsValid()
        ? words.IndexOfByPredicate(
            [selectedId](const FFaceCorrectionWordBlock& word)
            {
                return word.blockId == selectedId;
            })
        : INDEX_NONE;
    if (const TSharedPtr<SFaceCorrectionLane> pinned = lane.Pin())
    {
        pinned->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
    if (context.requestPreviewRebuild)
    {
        context.requestPreviewRebuild(true);
    }
}

bool FFaceCorrectionTimelineExtension::convertText(
    const FString& text,
    FString& phonemeText,
    TArray<FFaceCorrectionReviewItem>& items,
    FString& errorMessage) const
{
    if (config.Get() == nullptr)
    {
        errorMessage = TEXT("Choose a Face Correction config.");
        return false;
    }
#if WITH_PIPER_TTS
    const UPiperVoiceProfile* profile =
        Cast<UPiperVoiceProfile>(config->phonemeProfile.LoadSynchronous());
    if (profile == nullptr)
    {
        errorMessage = TEXT("Choose a Piper voice profile in the Face Correction config.");
        return false;
    }
    phonemeText = UPiperBlueprintLibrary::textToPhonemes(text, profile, errorMessage);
    if (phonemeText.IsEmpty())
    {
        return false;
    }
#else
    errorMessage = TEXT("PiperTTS is not available in this project.");
    return false;
#endif

    for (const FString& phoneme : splitPhonemes(phonemeText))
    {
        items.Add({phoneme, config->findVisemeId(phoneme)});
    }
    if (items.IsEmpty())
    {
        errorMessage = TEXT("No phonemes were found in the Piper result.");
        return false;
    }
    return true;
}

TArray<FString> FFaceCorrectionTimelineExtension::splitPhonemes(
    const FString& phonemeText) const
{
    TArray<FString> result;
    FString prefix;
    bool joinNext = false;
    for (int32 index = 0; index < phonemeText.Len(); ++index)
    {
        const TCHAR character = phonemeText[index];
        if (FChar::IsWhitespace(character) ||
            character == TEXT('|') ||
            character == TEXT('/') ||
            character == TEXT(',') ||
            character == TEXT('.') ||
            character == TEXT(';'))
        {
            continue;
        }
        if (isStress(character))
        {
            prefix.AppendChar(character);
            continue;
        }
        if (isModifier(character))
        {
            if (!result.IsEmpty())
            {
                result.Last().AppendChar(character);
            }
            continue;
        }
        if (character == TEXT('\u0361') || character == TEXT('\u035C'))
        {
            if (!result.IsEmpty())
            {
                result.Last().AppendChar(character);
                joinNext = true;
            }
            continue;
        }
        if (joinNext && !result.IsEmpty())
        {
            result.Last().AppendChar(character);
            joinNext = false;
            continue;
        }
        FString phoneme = MoveTemp(prefix);
        prefix.Reset();
        phoneme.AppendChar(character);
        result.Add(MoveTemp(phoneme));
    }
    if (!prefix.IsEmpty() && !result.IsEmpty())
    {
        result.Last() += prefix;
    }
    return result;
}

void FFaceCorrectionTimelineExtension::openReview(
    const FString& text,
    const FString& phonemeText,
    TArray<FFaceCorrectionReviewItem> items,
    const int32 startFrame,
    const int32 wordIndex)
{
    if (const TSharedPtr<SWindow> window = activeWindow.Pin())
    {
        window->BringToFront(true);
        return;
    }
    if (config.Get() == nullptr)
    {
        return;
    }

    const TSharedRef<SWindow> window = SNew(SWindow)
        .Title(FText::FromString(TEXT("Review Phonemes and Visemes")))
        .ClientSize(FVector2D(760.0f, 520.0f))
        .SupportsMaximize(false)
        .SupportsMinimize(false);
    activeWindow = window;
    const TWeakPtr<FFaceCorrectionTimelineExtension> weak = SharedThis(this);
    const TWeakPtr<SWindow> weakWindow = window;
    window->SetContent(
        SNew(SFaceCorrectionReviewWidget)
        .Items(MoveTemp(items))
        .VisemeIds(config->getVisemeIds())
        .PhonemeText(phonemeText)
        .OwnerWindow(weakWindow)
        .OnContinue(
            [weak, text, phonemeText, startFrame, wordIndex](
                const TArray<FFaceCorrectionReviewItem>& reviewedItems)
            {
                if (const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin())
                {
                    pinned->commitReview(
                        text,
                        phonemeText,
                        reviewedItems,
                        startFrame,
                        wordIndex);
                }
            }));
    window->SetOnWindowClosed(FOnWindowClosed::CreateLambda(
        [weak, weakWindow](const TSharedRef<SWindow>&)
        {
            if (const TSharedPtr<FFaceCorrectionTimelineExtension> pinned = weak.Pin();
                pinned.IsValid() && pinned->activeWindow.Pin() == weakWindow.Pin())
            {
                pinned->activeWindow.Reset();
            }
        }));
    FSlateApplication::Get().AddWindow(window);
}

void FFaceCorrectionTimelineExtension::commitReview(
    const FString& text,
    const FString& phonemeText,
    const TArray<FFaceCorrectionReviewItem>& items,
    const int32 requestedStartFrame,
    const int32 wordIndex)
{
    if (config.Get() == nullptr || items.IsEmpty() || !context.animation.IsValid())
    {
        return;
    }

    TArray<int32> durations;
    durations.Reserve(items.Num());
    int32 desiredDuration = 0;
    for (const FFaceCorrectionReviewItem& item : items)
    {
        const FFaceCorrectionVisemeDefinition* definition =
            config->findViseme(item.visemeId);
        const int32 duration = FMath::Max(
            1,
            definition != nullptr
                ? definition->defaultDurationFrames
                : config->fallbackVisemeDurationFrames);
        durations.Add(duration);
        desiredDuration += duration;
    }

    FFaceCorrectionWordBlock word;
    int32 totalDuration = FMath::Max(desiredDuration, config->minimumWordDurationFrames);
    int32 startFrame = FMath::Clamp(requestedStartFrame, 0, context.lastFrame);
    int32 blendInFrames = config->defaultBlendInFrames;
    int32 blendOutFrames = config->defaultBlendOutFrames;
    if (words.IsValidIndex(wordIndex))
    {
        word = words[wordIndex];
        blendInFrames = word.startFrame - word.blendStartFrame;
        blendOutFrames = word.blendEndFrame - word.endFrame;
        startFrame = word.startFrame;
        totalDuration = word.endFrame - word.startFrame + 1;
    }
    totalDuration = FMath::Clamp(totalDuration, 1, context.lastFrame - startFrame + 1);
    word.text = text;
    word.phonemeText = phonemeText;
    word.startFrame = startFrame;
    word.endFrame = startFrame + totalDuration - 1;
    word.blendStartFrame = FMath::Max(
        0,
        word.startFrame - blendInFrames);
    word.blendEndFrame = FMath::Min(
        context.lastFrame,
        word.endFrame + blendOutFrames);
    word.visemes.Reset();

    int32 accumulatedDuration = 0;
    for (int32 index = 0; index < items.Num(); ++index)
    {
        const int32 childStart = startFrame + FMath::RoundToInt(
            static_cast<double>(accumulatedDuration) /
            FMath::Max(1, desiredDuration) * totalDuration);
        accumulatedDuration += durations[index];
        const int32 childEndExclusive = index == items.Num() - 1
            ? word.endFrame + 1
            : startFrame + FMath::RoundToInt(
                static_cast<double>(accumulatedDuration) /
                FMath::Max(1, desiredDuration) * totalDuration);

        FFaceCorrectionVisemeBlock viseme;
        viseme.sourcePhoneme = items[index].phoneme;
        viseme.visemeId = items[index].visemeId;
        viseme.startFrame = FMath::Clamp(childStart, word.startFrame, word.endFrame);
        viseme.endFrame = FMath::Clamp(
            FMath::Max(viseme.startFrame, childEndExclusive - 1),
            word.startFrame,
            word.endFrame);
        word.visemes.Add(MoveTemp(viseme));
    }

    word.transitions.Reset();
    for (int32 index = 0; index + 1 < word.visemes.Num(); ++index)
    {
        const FFaceCorrectionVisemeBlock& previous = word.visemes[index];
        const FFaceCorrectionVisemeBlock& next = word.visemes[index + 1];
        FFaceCorrectionVisemeTransition transition;
        transition.boundaryFrame = next.startFrame;
        transition.blendStartFrame = FMath::Max(
            previous.startFrame,
            transition.boundaryFrame - config->defaultVisemeBlendFrames);
        transition.blendEndFrame = FMath::Min(
            next.endFrame,
            transition.boundaryFrame + config->defaultVisemeBlendFrames);
        word.transitions.Add(transition);
    }

    if (words.IsValidIndex(wordIndex))
    {
        words[wordIndex] = MoveTemp(word);
    }
    else
    {
        words.Add(MoveTemp(word));
    }
    wordsChanged();
    if (context.setStatus)
    {
        context.setStatus(FText::FromString(TEXT("Added word mouthing to the Face Correction timeline.")));
    }
}

#endif
