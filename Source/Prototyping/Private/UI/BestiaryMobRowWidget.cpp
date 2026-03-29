#include "UI/BestiaryMobRowWidget.h"
#include "UI/BestiaryMobRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Data/DataStructs.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UBestiaryMobRowWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (Row_Select_Btn)
        Row_Select_Btn->OnClicked.AddDynamic(this, &UBestiaryMobRowWidget::HandleSelectClicked);
}

void UBestiaryMobRowWidget::Setup(const FString& InMobSlug, int32 InKillCount)
{
    MobSlug = InMobSlug;

    ULocalizationSubsystem* Loc = nullptr;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        Loc = GI->GetSubsystem<ULocalizationSubsystem>();

    if (Row_Mob_Name_Text)
    {
        FText MobName = Loc
            ? Loc->GetMobDisplayName(InMobSlug)
            : FText::FromString(InMobSlug);
        Row_Mob_Name_Text->SetText(MobName);
    }

    if (Row_Kill_Text)
        Row_Kill_Text->SetText(FText::FromString(FString::Printf(TEXT("%d"), InKillCount)));

    // Load mob icon from DT_MobDefinitions
    if (Row_Mob_Icon)
    {
        Row_Mob_Icon->SetVisibility(ESlateVisibility::Collapsed);
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UDataTable* DT = GI->GetMobDefinitionTable())
            {
                if (const FMobDefinition* Row = DT->FindRow<FMobDefinition>(FName(*InMobSlug), TEXT("")))
                {
                    if (!Row->Visual.Icon.IsNull())
                    {
                        TWeakObjectPtr<UBestiaryMobRowWidget> WeakThis = this;
                        TSoftObjectPtr<UTexture2D> IconPtr = Row->Visual.Icon;
                        if (UTexture2D* Already = IconPtr.Get())
                        {
                            Row_Mob_Icon->SetBrushFromTexture(Already);
                            Row_Mob_Icon->SetVisibility(ESlateVisibility::Visible);
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
                                            WeakThis->Row_Mob_Icon->SetBrushFromTexture(Loaded);
                                            WeakThis->Row_Mob_Icon->SetVisibility(ESlateVisibility::Visible);
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

    OnSetupComplete(InMobSlug, InKillCount);
}

void UBestiaryMobRowWidget::HandleSelectClicked()
{
    OnMobRowSelected.Broadcast(MobSlug);
}
