#include "Services/LocalizationSubsystem.h"
#include "Data/LocalizationDataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/ConfigCacheIni.h"
#include "Internationalization/Internationalization.h"

FText ULocalizationSubsystem::GetQuestDisplayName(const FString& ClientQuestKey) const
{
    if (ClientQuestKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetQuestTable();
    if (!Table) return FallbackText(ClientQuestKey);
    const FQuestDefinition* Row = Table->FindRow<FQuestDefinition>(FName(*ClientQuestKey), TEXT("GetQuestDisplayName"), false);
    return (Row && !Row->displayName.IsEmpty()) ? Row->displayName : FallbackText(ClientQuestKey);
}

FText ULocalizationSubsystem::GetQuestDescription(const FString& ClientQuestKey) const
{
    if (ClientQuestKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetQuestTable();
    if (!Table) return FText::GetEmpty();
    const FQuestDefinition* Row = Table->FindRow<FQuestDefinition>(FName(*ClientQuestKey), TEXT("GetQuestDescription"), false);
    return Row ? Row->description : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetQuestDefinition(const FString& ClientQuestKey, FQuestDefinition& OutDefinition) const
{
    if (ClientQuestKey.IsEmpty()) return false;
    UDataTable* Table = GetQuestTable();
    if (!Table) return false;
    const FQuestDefinition* Row = Table->FindRow<FQuestDefinition>(FName(*ClientQuestKey), TEXT("GetQuestDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Quest Steps ???????????????????????????????????????????????????????????????

FText ULocalizationSubsystem::GetQuestStepDescription(const FString& ClientStepKey) const
{
    if (ClientStepKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetQuestStepTable();
    if (!Table) return FallbackText(ClientStepKey);
    const FQuestStepDefinition* Row = Table->FindRow<FQuestStepDefinition>(FName(*ClientStepKey), TEXT("GetQuestStepDescription"), false);
    return (Row && !Row->description.IsEmpty()) ? Row->description : FallbackText(ClientStepKey);
}

FText ULocalizationSubsystem::GetQuestStepHint(const FString& ClientStepKey) const
{
    if (ClientStepKey.IsEmpty()) return FText::GetEmpty();
    UDataTable* Table = GetQuestStepTable();
    if (!Table) return FText::GetEmpty();
    const FQuestStepDefinition* Row = Table->FindRow<FQuestStepDefinition>(FName(*ClientStepKey), TEXT("GetQuestStepHint"), false);
    return Row ? Row->hint : FText::GetEmpty();
}

bool ULocalizationSubsystem::GetQuestStepDefinition(const FString& ClientStepKey, FQuestStepDefinition& OutDefinition) const
{
    if (ClientStepKey.IsEmpty()) return false;
    UDataTable* Table = GetQuestStepTable();
    if (!Table) return false;
    const FQuestStepDefinition* Row = Table->FindRow<FQuestStepDefinition>(FName(*ClientStepKey), TEXT("GetQuestStepDefinition"), false);
    if (Row) { OutDefinition = *Row; return true; }
    return false;
}

// ?? Dialogue Nodes ????????????????????????????????????????????????????????????

