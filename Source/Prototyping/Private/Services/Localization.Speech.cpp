#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

UDataTable* ULocalizationSubsystem::GetAmbientSpeechTable() const
{
    if (!CachedAmbientSpeechTable && LocalizationData)
        CachedAmbientSpeechTable = LocalizationData->AmbientSpeechLines.Get();
    return CachedAmbientSpeechTable;
}

FText ULocalizationSubsystem::GetNPCSpeechText(const FString& LineKey) const
{
    if (LineKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetAmbientSpeechTable();
    if (!Table) return FallbackText(LineKey);
    const FAmbientSpeechLineDefinition* Row = Table->FindRow<FAmbientSpeechLineDefinition>(
        FName(*LineKey), TEXT("GetNPCSpeechText"), false);
    return (Row && !Row->speechText.IsEmpty()) ? Row->speechText : FallbackText(LineKey);
}

bool ULocalizationSubsystem::GetNPCSpeechLineDefinition(const FString& LineKey, FAmbientSpeechLineDefinition& OutDefinition) const
{
    if (LineKey.IsEmpty()) return false;
    UDataTable* Table = GetAmbientSpeechTable();
    if (!Table) return false;
    const FAmbientSpeechLineDefinition* Row = Table->FindRow<FAmbientSpeechLineDefinition>(
        FName(*LineKey), TEXT("GetNPCSpeechLineDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// -- World Interactive Objects -----------------------------------------------

