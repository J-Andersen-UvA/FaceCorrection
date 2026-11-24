#include "FaceCorrectionConfigCustomization.h"

#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Dialogs/DlgPickPath.h"
#include "FaceCorrectionConfig.h"
#include "IPropertyUtilities.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const TArray<FString>& getDutchPhonemes()
{
    static const TArray<FString> phonemes = {
        TEXT("p"), TEXT("b"), TEXT("t"), TEXT("d"), TEXT("k"), TEXT("ɡ"),
        TEXT("f"), TEXT("v"), TEXT("s"), TEXT("z"), TEXT("x"), TEXT("ɣ"),
        TEXT("h"), TEXT("m"), TEXT("n"), TEXT("ŋ"), TEXT("l"), TEXT("r"),
        TEXT("j"), TEXT("w"), TEXT("ʋ"), TEXT("ʃ"), TEXT("ʒ"), TEXT("ç"),
        TEXT("ɲ"), TEXT("i"), TEXT("ɪ"), TEXT("e"), TEXT("ɛ"), TEXT("a"),
        TEXT("ɑ"), TEXT("o"), TEXT("ɔ"), TEXT("u"), TEXT("ʊ"), TEXT("y"),
        TEXT("ʏ"), TEXT("ø"), TEXT("œ"), TEXT("ə")};
    return phonemes;
}

FName suggestViseme(
    const FString& phoneme,
    const UFaceCorrectionConfig& config)
{
    const FName exactId(*phoneme);
    if (config.findViseme(exactId) != nullptr)
    {
        return exactId;
    }

    static const TMap<FString, FName> suggestions = {
        {TEXT("b"), TEXT("bp")},
        {TEXT("p"), TEXT("bp")},
        {TEXT("t"), TEXT("tdn")},
        {TEXT("d"), TEXT("tdn")},
        {TEXT("n"), TEXT("tdn")},
        {TEXT("f"), TEXT("fv")},
        {TEXT("v"), TEXT("fv")},
        {TEXT("s"), TEXT("sz")},
        {TEXT("z"), TEXT("sz")},
        {TEXT("ʃ"), TEXT("shCh")},
        {TEXT("ʒ"), TEXT("shCh")},
        {TEXT("a"), TEXT("aa")},
        {TEXT("ɑ"), TEXT("aa")},
        {TEXT("e"), TEXT("eh")},
        {TEXT("ɛ"), TEXT("eh")},
        {TEXT("i"), TEXT("ih")},
        {TEXT("ɪ"), TEXT("ih")},
        {TEXT("o"), TEXT("oh")},
        {TEXT("ɔ"), TEXT("oh")},
        {TEXT("u"), TEXT("oe")},
        {TEXT("ʊ"), TEXT("oe")},
        {TEXT("y"), TEXT("uu")},
        {TEXT("ʏ"), TEXT("uu")},
        {TEXT("ø"), TEXT("eu")},
        {TEXT("œ"), TEXT("eu")},
        {TEXT("ə"), TEXT("schwa")}};
    const FName* suggestedId = suggestions.Find(phoneme);
    return suggestedId != nullptr && config.findViseme(*suggestedId) != nullptr
        ? *suggestedId
        : NAME_None;
}
}

TSharedRef<IDetailCustomization> FFaceCorrectionConfigCustomization::makeInstance()
{
    return MakeShared<FFaceCorrectionConfigCustomization>();
}

void FFaceCorrectionConfigCustomization::CustomizeDetails(
    IDetailLayoutBuilder& detailBuilder)
{
    TArray<TWeakObjectPtr<UObject>> objects;
    detailBuilder.GetObjectsBeingCustomized(objects);
    config.Reset();
    for (const TWeakObjectPtr<UObject>& object : objects)
    {
        if (UFaceCorrectionConfig* candidate = Cast<UFaceCorrectionConfig>(object.Get()))
        {
            config = candidate;
            break;
        }
    }
    propertyUtilities = detailBuilder.GetPropertyUtilities();

    IDetailCategoryBuilder& category = detailBuilder.EditCategory(TEXT("Visemes"));
    category.AddCustomRow(FText::FromString(TEXT("Populate Visemes Folder")))
    .WholeRowContent()
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("Populate Visemes from Folder")))
        .ToolTipText(FText::FromString(
            TEXT("Choose a Content Browser folder and populate the Visemes array with every Animation Sequence in that folder and its subfolders.")))
        .IsEnabled_Lambda([this]() { return config.IsValid(); })
        .OnClicked(this, &FFaceCorrectionConfigCustomization::populateFromFolder)
    ];
    category.AddCustomRow(FText::FromString(TEXT("Populate Phoneme Mappings")))
    .WholeRowContent()
    [
        SNew(SButton)
        .Text(FText::FromString(TEXT("Populate Dutch Phoneme Mappings")))
        .ToolTipText(FText::FromString(
            TEXT("Add the Dutch phoneme inventory and map confident matches to the current viseme array. Unmatched phonemes keep an empty viseme selection.")))
        .IsEnabled_Lambda([this]() { return config.IsValid(); })
        .OnClicked(this, &FFaceCorrectionConfigCustomization::populatePhonemeMappings)
    ];
}

FReply FFaceCorrectionConfigCustomization::populateFromFolder()
{
    UFaceCorrectionConfig* targetConfig = config.Get();
    if (targetConfig == nullptr)
    {
        return FReply::Handled();
    }

    const TSharedRef<SDlgPickPath> dialog = SNew(SDlgPickPath)
        .Title(FText::FromString(TEXT("Choose Viseme Animation Folder")))
        .DefaultPath(FText::FromString(
            targetConfig->visemeFolder.IsEmpty()
                ? TEXT("/Game")
                : targetConfig->visemeFolder))
        .AllowReadOnlyFolders(false)
        .CanShowDevelopersFolder(false)
        .ShowViewOptions(true);
    if (dialog->ShowModal() != EAppReturnType::Ok)
    {
        return FReply::Handled();
    }

    const FString folder = dialog->GetPath().ToString();
    FARFilter filter;
    filter.PackagePaths.Add(FName(*folder));
    filter.ClassPaths.Add(UAnimSequence::StaticClass()->GetClassPathName());
    filter.bRecursivePaths = true;
    filter.bRecursiveClasses = true;

    FAssetRegistryModule& assetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> assets;
    assetRegistry.Get().GetAssets(filter, assets);
    assets.Sort(
        [](const FAssetData& left, const FAssetData& right)
        {
            const int32 nameComparison = left.AssetName.Compare(right.AssetName);
            return nameComparison == 0
                ? left.PackageName.LexicalLess(right.PackageName)
                : nameComparison < 0;
        });

    TMap<FName, FFaceCorrectionVisemeDefinition> existingById;
    for (const FFaceCorrectionVisemeDefinition& definition : targetConfig->visemes)
    {
        existingById.Add(definition.visemeId, definition);
    }

    TArray<FFaceCorrectionVisemeDefinition> populated;
    populated.Reserve(assets.Num());
    TSet<FName> usedIds;
    for (const FAssetData& asset : assets)
    {
        const FName visemeId = asset.AssetName;
        if (usedIds.Contains(visemeId))
        {
            continue;
        }
        usedIds.Add(visemeId);

        FFaceCorrectionVisemeDefinition definition;
        if (const FFaceCorrectionVisemeDefinition* existing = existingById.Find(visemeId))
        {
            definition = *existing;
        }
        const uint32 hash = GetTypeHash(asset.ToSoftObjectPath());
        definition.color = FLinearColor::MakeFromHSV8(
            static_cast<uint8>(hash & 0xFF),
            170,
            210);
        definition.visemeId = visemeId;
        definition.animation = TSoftObjectPtr<UAnimSequence>(asset.ToSoftObjectPath());
        populated.Add(MoveTemp(definition));
    }

    const FScopedTransaction transaction(
        FText::FromString(TEXT("Populate Face Correction Visemes")));
    targetConfig->Modify();
    targetConfig->visemeFolder = folder;
    targetConfig->visemes = MoveTemp(populated);
    targetConfig->PostEditChange();
    targetConfig->MarkPackageDirty();

    if (const TSharedPtr<IPropertyUtilities> utilities = propertyUtilities.Pin())
    {
        utilities->ForceRefresh();
    }
    FMessageDialog::Open(
        EAppMsgType::Ok,
        FText::FromString(FString::Printf(
            TEXT("Added %d viseme animation%s from %s."),
            targetConfig->visemes.Num(),
            targetConfig->visemes.Num() == 1 ? TEXT("") : TEXT("s"),
            *folder)),
        FText::FromString(TEXT("Face Correction Visemes")));
    return FReply::Handled();
}

FReply FFaceCorrectionConfigCustomization::populatePhonemeMappings()
{
    UFaceCorrectionConfig* targetConfig = config.Get();
    if (targetConfig == nullptr)
    {
        return FReply::Handled();
    }

    TMap<FString, FName> existingMappings;
    for (const FFaceCorrectionPhonemeMapping& mapping : targetConfig->phonemeMappings)
    {
        existingMappings.Add(mapping.phoneme, mapping.visemeId);
    }

    TArray<FFaceCorrectionPhonemeMapping> mappings;
    mappings.Reserve(getDutchPhonemes().Num());
    int32 mappedCount = 0;
    for (const FString& phoneme : getDutchPhonemes())
    {
        FFaceCorrectionPhonemeMapping mapping;
        mapping.phoneme = phoneme;
        if (const FName* existing = existingMappings.Find(phoneme);
            existing != nullptr && !existing->IsNone())
        {
            mapping.visemeId = *existing;
        }
        else
        {
            mapping.visemeId = suggestViseme(phoneme, *targetConfig);
        }
        if (!mapping.visemeId.IsNone())
        {
            ++mappedCount;
        }
        mappings.Add(MoveTemp(mapping));
    }

    const FScopedTransaction transaction(
        FText::FromString(TEXT("Populate Face Correction Phoneme Mappings")));
    targetConfig->Modify();
    targetConfig->phonemeMappings = MoveTemp(mappings);
    targetConfig->PostEditChange();
    targetConfig->MarkPackageDirty();

    if (const TSharedPtr<IPropertyUtilities> utilities = propertyUtilities.Pin())
    {
        utilities->ForceRefresh();
    }
    FMessageDialog::Open(
        EAppMsgType::Ok,
        FText::FromString(FString::Printf(
            TEXT("Added %d Dutch phonemes. Automatically mapped %d; the remaining %d are ready for a viseme or NULL selection."),
            targetConfig->phonemeMappings.Num(),
            mappedCount,
            targetConfig->phonemeMappings.Num() - mappedCount)),
        FText::FromString(TEXT("Face Correction Phoneme Mappings")));
    return FReply::Handled();
}
