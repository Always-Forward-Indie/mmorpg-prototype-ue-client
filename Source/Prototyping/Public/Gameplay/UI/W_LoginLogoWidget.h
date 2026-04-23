// Login screen logo widget.
// Create a Blueprint child (e.g. WBP_LoginLogo) and place any Image /
// RichTextBlock / animation you want to show as the game logo.
// The C++ class intentionally has no bound widgets — all visuals live in BP.
//
// GameInstance creates and shows this widget at Z-order 5 (below the login
// flow panel at 10) so the logo is part of the background layer.
// If you need it on top of the login form, raise the Z-order in GameInstance.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "W_LoginLogoWidget.generated.h"

/**
 * UW_LoginLogoWidget
 *
 * Thin base class for the login-screen game logo panel.
 * All visual content (Image, material, animation) is defined in the
 * Blueprint subclass — no BindWidget requirements from C++.
 *
 * Lifecycle:
 *   Created by UMyGameInstance::AddLoginWidgetToViewport()
 *   Destroyed by UMyGameInstance::RemoveLoginWidgetFromViewport()
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UW_LoginLogoWidget : public UUserWidget
{
    GENERATED_BODY()
};
