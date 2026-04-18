#include "UI/WIOInteractionPromptWidget.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Services/LocalizationSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Kismet/GameplayStatics.h"

void UWIOInteractionPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	bIsVisible = false;
}

void UWIOInteractionPromptWidget::ShowForObject(AWorldInteractiveObjectActor* InActor)
{
	if (!InActor || !InActor->IsInteractable())
	{
		HidePrompt();
		return;
	}

	CurrentActor = InActor;
	bIsVisible   = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// Resolve localised texts
	FText DisplayName;
	FText PromptText;

	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(this))
	{
		if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
		{
			DisplayName = LocSys->GetWIODisplayName(InActor->GetNameKey());
			PromptText  = LocSys->GetWIOInteractionPrompt(InActor->GetNameKey());
		}
	}

	// Fallback display name
	if (DisplayName.IsEmpty())
	{
		DisplayName = FText::FromString(InActor->GetNameKey());
	}

	// Fallback prompt text built from object type
	if (PromptText.IsEmpty())
	{
		FString TypeLabel;
		switch (InActor->GetObjectType())
		{
		case EWIOObjectType::Examine:    TypeLabel = TEXT("Examine");  break;
		case EWIOObjectType::Search:     TypeLabel = TEXT("Search");   break;
		case EWIOObjectType::Activate:   TypeLabel = TEXT("Activate"); break;
		case EWIOObjectType::UseWithItem:TypeLabel = TEXT("Use");      break;
		case EWIOObjectType::Channeled:  TypeLabel = TEXT("Channel");  break;
		default:                         TypeLabel = TEXT("Interact"); break;
		}
		PromptText = FText::FromString(FString::Printf(TEXT("[%s] %s"), *InteractionKeyName, *TypeLabel));
	}

	// ── Auto-populate bound widget members ──────────────────────────────
	if (ObjectNameText)
	{
		ObjectNameText->SetText(DisplayName);
	}
	if (InteractionPromptText)
	{
		InteractionPromptText->SetText(PromptText);
	}

	// Optional icon from DataTable definition (UTexture2D stored in FWIODefinitionRow)
	// InteractionIcon is left as-is if not set by DataTable — designers set it in the WBP default.

	// Notify BP (optional hook for animations, sound, etc.)
	OnPromptUpdated(InActor->GetObjectData(), DisplayName, PromptText);
}

void UWIOInteractionPromptWidget::HidePrompt()
{
	CurrentActor = nullptr;
	bIsVisible   = false;
	SetVisibility(ESlateVisibility::Collapsed);
}
