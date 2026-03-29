#include "UI/BestiaryEntryWidget.h"
#include "UI/BestiaryEntryWidget.h"
#include "UI/BestiaryTierRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UBestiaryEntryWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Close_Button)
        Close_Button->OnClicked.AddDynamic(this, &UBestiaryEntryWidget::HandleCloseClicked);
}

void UBestiaryEntryWidget::HandleCloseClicked()
{
    OnCloseRequested.Broadcast();
}

void UBestiaryEntryWidget::DisplayEntry(const FBestiaryEntryStruct& Entry)
{
    CurrentEntry = Entry;

    ULocalizationSubsystem* Loc = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        Loc = GI->GetSubsystem<ULocalizationSubsystem>();

    // Mob name
    if (Mob_Name_Text)
    {
        FText MobName = Loc ? Loc->GetMobDisplayName(Entry.mobSlug) : FText::FromString(Entry.mobSlug);
        Mob_Name_Text->SetText(MobName);
    }

    // Mob description
    if (Mob_Description_Text)
    {
        FText MobDesc = Loc ? Loc->GetMobDescription(Entry.mobSlug) : FText::GetEmpty();
        Mob_Description_Text->SetText(MobDesc);
    }

    // Mob icon from DT_MobDefinitions
    if (Mob_Icon)
    {
        Mob_Icon->SetVisibility(ESlateVisibility::Collapsed);
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UDataTable* DT = GI->GetMobDefinitionTable())
            {
                if (const FMobDefinition* Row = DT->FindRow<FMobDefinition>(FName(*Entry.mobSlug), TEXT("")))
                {
                    if (!Row->Visual.Icon.IsNull())
                    {
                        TWeakObjectPtr<UBestiaryEntryWidget> WeakThis = this;
                        TSoftObjectPtr<UTexture2D> IconPtr = Row->Visual.Icon;
                        if (UTexture2D* Already = IconPtr.Get())
                        {
                            Mob_Icon->SetBrushFromTexture(Already);
                            Mob_Icon->SetVisibility(ESlateVisibility::Visible);
                        }
                        else
                        {
                            UAssetManager::GetStreamableManager().RequestAsyncLoad(
                                IconPtr.ToSoftObjectPath(),
                                FStreamableDelegate::CreateLambda([WeakThis, IconPtr]()
                                {
                                    if (WeakThis.IsValid())
                                    {
                                        if (UTexture2D* Loaded = IconPtr.Get())
                                        {
                                            WeakThis->Mob_Icon->SetBrushFromTexture(Loaded);
                                            WeakThis->Mob_Icon->SetVisibility(ESlateVisibility::Visible);
                                        }
                                    }
                                })
                            );
                        }
                    }
                }
            }
        }
    }

    // Kill count
    if (Kill_Count_Text)
    {
        Kill_Count_Text->SetText(FText::FromString(
            FString::Printf(TEXT("Kills: %d"), Entry.killCount)));
    }

    // Rebuild tiers
    if (Tiers_Box)
    {
        Tiers_Box->ClearChildren();
        for (const FBestiaryTierStruct& Tier : Entry.tiers)
        {
            BuildTierRow(Tier);
        }
    }
}

void UBestiaryEntryWidget::ClearEntry()
{
    CurrentEntry = FBestiaryEntryStruct();

    if (Mob_Name_Text)          Mob_Name_Text->SetText(FText::GetEmpty());
    if (Mob_Description_Text)   Mob_Description_Text->SetText(FText::GetEmpty());
    if (Mob_Icon)               Mob_Icon->SetVisibility(ESlateVisibility::Collapsed);
    if (Kill_Count_Text)        Kill_Count_Text->SetText(FText::GetEmpty());
    if (Tiers_Box)              Tiers_Box->ClearChildren();
}

void UBestiaryEntryWidget::BuildTierRow(const FBestiaryTierStruct& Tier)
{
    if (!Tiers_Box) return;

    if (!TierRowClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("BestiaryEntryWidget: BuildTierRow skipped - TierRowClass is not set"));
        return;
    }

    UBestiaryTierRowWidget* Row = CreateWidget<UBestiaryTierRowWidget>(GetOwningPlayer(), TierRowClass);
    if (!Row) return;

    Row->Setup(Tier);
    Tiers_Box->AddChild(Row);
}

