#include "UI/InteractionHintWidget.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Players/BasicPlayer.h"

void UInteractionHintWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The widget is created by UIManager without a link to the player, so
	// resolve the binding here (and again lazily in ShowHint in case the
	// pawn was not possessed yet at construct time).
	ResolveKeyBinding();

	if (HintText)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInteractionHintWidget::ResolveKeyBinding()
{
	ResolvedKeyBinding.Empty();

	// Single source of truth: the possessed player's InteractAction.
	// The widget is created by UIManager with no link to the player, so
	// resolve it here (covers both BP-assigned actions and code defaults).
	if (!InteractAction)
	{
		if (APawn* Pawn = GetOwningPlayerPawn())
		{
			if (const ABasicPlayer* Player = Cast<ABasicPlayer>(Pawn))
			{
				InteractAction = Player->InteractAction;
			}
		}
	}

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

	// Re-resolve lazily: at NativeConstruct the pawn may not be possessed yet.
	if (ResolvedKeyBinding.IsEmpty())
	{
		ResolveKeyBinding();
	}

	// The {key} template must never leak to the screen. If the binding is
	// genuinely unknown, fall back to the documented default ("F").
	const FString KeyLabel = !ResolvedKeyBinding.IsEmpty()
		? ResolvedKeyBinding
		: FString(TEXT("F"));
	Text = Text.Replace(TEXT("{key}"), bShowHotkey ? *KeyLabel : TEXT(""));
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
