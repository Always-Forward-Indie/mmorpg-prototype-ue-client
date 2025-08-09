// Implementation file for UIManager
#include "Gameplay/UI/UIManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Kismet/GameplayStatics.h"

void UUIManager::Init(APlayerController* InPC, UCanvasPanel* InRootCanvas, 
                     TSubclassOf<UDamageTextWidget> InDamageTextClass)
{
    // Store the references
    PlayerController = InPC;
    RootCanvas = InRootCanvas;
    
    // Create the FCTManager if it doesn't exist
    if (!FCTManager)
    {
        FCTManager = NewObject<UFloatingCombatTextManager>(this);

        if (FCTManager)
        {
            FCTManager->Init(InRootCanvas, InPC, InDamageTextClass);
        }
    }
}
