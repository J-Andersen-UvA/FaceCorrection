#include "FaceCorrectionConfig.h"

FName UFaceCorrectionConfig::findVisemeId(const FString& phoneme) const
{
    FString normalized = phoneme.TrimStartAndEnd();
    normalized.RemoveFromStart(TEXT("\u02C8"));
    normalized.RemoveFromStart(TEXT("\u02CC"));
    normalized.RemoveFromStart(TEXT("'"));
    normalized.RemoveFromEnd(TEXT("\u02D0"));
    normalized.RemoveFromEnd(TEXT(":"));
    for (const FFaceCorrectionPhonemeMapping& mapping : phonemeMappings)
    {
        if (mapping.phoneme.Equals(phoneme.TrimStartAndEnd(), ESearchCase::CaseSensitive) ||
            mapping.phoneme.Equals(normalized, ESearchCase::CaseSensitive))
        {
            return mapping.visemeId;
        }
    }
    return nullVisemeId;
}

const FFaceCorrectionVisemeDefinition* UFaceCorrectionConfig::findViseme(
    const FName visemeId) const
{
    return visemes.FindByPredicate(
        [visemeId](const FFaceCorrectionVisemeDefinition& definition)
        {
            return definition.visemeId == visemeId;
        });
}

TArray<FName> UFaceCorrectionConfig::getVisemeIds() const
{
    TArray<FName> ids;
    ids.Reserve(visemes.Num() + 1);
    ids.AddUnique(nullVisemeId);
    for (const FFaceCorrectionVisemeDefinition& definition : visemes)
    {
        if (!definition.visemeId.IsNone())
        {
            ids.AddUnique(definition.visemeId);
        }
    }
    return ids;
}

TArray<FString> UFaceCorrectionConfig::getVisemeOptions() const
{
    TArray<FString> options;
    options.Add(TEXT("None"));
    for (const FName visemeId : getVisemeIds())
    {
        options.AddUnique(visemeId.ToString());
    }
    return options;
}
