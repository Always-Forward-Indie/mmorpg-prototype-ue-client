// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

// ---------------------------------------------------------------------------
// Global button sounds (UI Sound Map coverage for Button Click / Hover)
// ---------------------------------------------------------------------------
//
// Slate SButton plays sounds from its FButtonStyle (Pressed/HoveredSlateSound).
// UMG buttons carry inline styles that ship empty, and no code path played the
// DA-mapped ButtonClick/ButtonHover events — the mapping was silently dead.
// These helpers close the gap without touching any Blueprint:
//  - ApplyDefaultButtonSounds fills the shared CoreStyle "Button" default so
//    every code-created default button sounds right from birth.
//  - ApplyButtonSoundsToSubtree fills empty sounds on live instances (covers
//    every BP-inline style). Called after UI init, on window-open transitions,
//    and in the dynamic row builders (inventory/skill/vendor/trade/dialogue).
// Designer sounds are never overridden (null-resource check).

USoundBase* UAudioManager::FindUISound(EUISoundEvent Event) const
{
	for (const FUISoundEntry& Entry : UISounds)
	{
		if (Entry.Event == Event && Entry.Sound)
		{
			return Entry.Sound;
		}
	}
	return nullptr;
}

void UAudioManager::ApplyDefaultButtonSounds()
{
	USoundBase* Click = FindUISound(EUISoundEvent::ButtonClick);
	USoundBase* Hover = FindUISound(EUISoundEvent::ButtonHover);
	if (!Click && !Hover)
	{
		return;
	}

	FButtonStyle& DefaultBtn =
		const_cast<FButtonStyle&>(FCoreStyle::Get().GetWidgetStyle<FButtonStyle>(TEXT("Button")));
	if (Click && !DefaultBtn.PressedSlateSound.GetResourceObject())
	{
		DefaultBtn.PressedSlateSound.SetResourceObject(Click);
	}
	if (Hover && !DefaultBtn.HoveredSlateSound.GetResourceObject())
	{
		DefaultBtn.HoveredSlateSound.SetResourceObject(Hover);
	}
}

static void InjectButtonSounds(UButton* Button, USoundBase* Click, USoundBase* Hover)
{
	if (!Button)
	{
		return;
	}
	FButtonStyle Style = Button->GetStyle();
	bool bTouched = false;
	if (Click && !Style.PressedSlateSound.GetResourceObject())
	{
		Style.PressedSlateSound.SetResourceObject(Click);
		bTouched = true;
	}
	if (Hover && !Style.HoveredSlateSound.GetResourceObject())
	{
		Style.HoveredSlateSound.SetResourceObject(Hover);
		bTouched = true;
	}
	if (bTouched)
	{
		Button->SetStyle(Style);
	}
}

static void InjectButtonSoundsRecursive(UWidget* Widget, USoundBase* Click, USoundBase* Hover)
{
	if (!Widget)
	{
		return;
	}

	if (UButton* Button = Cast<UButton>(Widget))
	{
		InjectButtonSounds(Button, Click, Hover);
	}

	// Recurse into nested UserWidget trees (rows/slots live in their own trees),
	// then panel children and single-child content — mirrors the hover-blocker walk.
	if (UUserWidget* UserW = Cast<UUserWidget>(Widget))
	{
		if (UserW->WidgetTree && UserW->WidgetTree->RootWidget)
		{
			InjectButtonSoundsRecursive(UserW->WidgetTree->RootWidget, Click, Hover);
		}
		return;
	}
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			InjectButtonSoundsRecursive(Panel->GetChildAt(i), Click, Hover);
		}
		return;
	}
	if (UContentWidget* Content = Cast<UContentWidget>(Widget))
	{
		InjectButtonSoundsRecursive(Content->GetContent(), Click, Hover);
	}
}

void UAudioManager::ApplyButtonSoundsToSubtree(UWidget* Root)
{
	if (!Root)
	{
		return;
	}
	USoundBase* Click = FindUISound(EUISoundEvent::ButtonClick);
	USoundBase* Hover = FindUISound(EUISoundEvent::ButtonHover);
	if (!Click && !Hover)
	{
		return;
	}
	InjectButtonSoundsRecursive(Root, Click, Hover);
}
