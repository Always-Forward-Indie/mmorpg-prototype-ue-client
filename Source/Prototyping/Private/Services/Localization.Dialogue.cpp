#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

FText ULocalizationSubsystem::GetDialogueNodeText(const FString& ClientNodeKey) const
{
    if (ClientNodeKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetDialogueNodeTable();
    if (!Table) return FallbackText(ClientNodeKey);
    const FDialogueNodeDefinition* Row = Table->FindRow<FDialogueNodeDefinition>(FName(*ClientNodeKey), TEXT("GetDialogueNodeText"), false);
    return (Row && !Row->nodeText.IsEmpty()) ? Row->nodeText : FallbackText(ClientNodeKey);
}

bool ULocalizationSubsystem::GetDialogueNodeDefinition(const FString& ClientNodeKey, FDialogueNodeDefinition& OutDefinition) const
{
    if (ClientNodeKey.IsEmpty()) return false;
    UDataTable* Table = GetDialogueNodeTable();
    if (!Table) return false;
    const FDialogueNodeDefinition* Row = Table->FindRow<FDialogueNodeDefinition>(FName(*ClientNodeKey), TEXT("GetDialogueNodeDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Dialogue Choices ??????????????????????????????????????????????????????????

FText ULocalizationSubsystem::GetDialogueChoiceText(const FString& ClientChoiceKey) const
{
    if (ClientChoiceKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetDialogueChoiceTable();
    if (!Table) return FallbackText(ClientChoiceKey);
    const FDialogueChoiceDefinition* Row = Table->FindRow<FDialogueChoiceDefinition>(FName(*ClientChoiceKey), TEXT("GetDialogueChoiceText"), false);
    return (Row && !Row->choiceText.IsEmpty()) ? Row->choiceText : FallbackText(ClientChoiceKey);
}

bool ULocalizationSubsystem::GetDialogueChoiceDefinition(const FString& ClientChoiceKey, FDialogueChoiceDefinition& OutDefinition) const
{
    if (ClientChoiceKey.IsEmpty()) return false;
    UDataTable* Table = GetDialogueChoiceTable();
    if (!Table) return false;
    const FDialogueChoiceDefinition* Row = Table->FindRow<FDialogueChoiceDefinition>(FName(*ClientChoiceKey), TEXT("GetDialogueChoiceDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Items ?????????????????????????????????????????????????????????????????????

