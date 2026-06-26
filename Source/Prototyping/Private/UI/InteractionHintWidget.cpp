#include "UI/InteractionHintWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"

void UInteractionHintWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveKeyBinding();

	if (HintText)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInteractionHintWidget::ResolveKeyBinding()
{
	ResolvedKeyBinding.Empty();

	if (!InteractAction) return;

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP) return;

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP);
	if (!Subsystem) return;

	TArray<FKey> Keys = Subsystem->QueryKeysMappedToAction(InteractAction);
	if (Keys.Num() > 0)
	{
		ResolvedKeyBinding = Keys[0].GetDisplayName().ToString();
	}
}

void UInteractionHintWidget::ShowHint(EInteractableType Type, bool bShowHotkey)
{
	if (!HintText) return;

	const FText& Template = GetTemplate(Type);
	if (Template.IsEmpty())
	{
		HideHint();
		return;
	}

	FString Text = Template.ToString();
	if (!ResolvedKeyBinding.IsEmpty())
	{
		Text = Text.Replace(TEXT("{key}"), bShowHotkey ? *ResolvedKeyBinding : TEXT(""));
	}
	Text = Text.TrimStartAndEnd().Replace(TEXT("  "), TEXT(" "));

	HintText->SetText(FText::FromString(Text));
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UInteractionHintWidget::HideHint()
{
	SetVisibility(ESlateVisibility::Collapsed);
}

const FText& UInteractionHintWidget::GetTemplate(EInteractableType Type) const
{
	switch (Type)
	{
	case EInteractableType::DroppedItem:    return Template_DroppedItem;
	case EInteractableType::MOB_Harvestable: return Template_MobHarvestable;
	case EInteractableType::MOB_Harvested:  return Template_MobHarvested;
	case EInteractableType::NPC:            return Template_NPC;
	case EInteractableType::RemotePlayer:   return Template_RemotePlayer;
	default:                                break;
	}

	static const FText Empty;
	return Empty;
}
