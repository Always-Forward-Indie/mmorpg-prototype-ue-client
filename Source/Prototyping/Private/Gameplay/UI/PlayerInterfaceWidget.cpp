#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "MyGameInstance.h"
#include "Components/OverlaySlot.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Kismet/GameplayStatics.h"

void UPlayerInterfaceWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
}

void UPlayerInterfaceWidget::NativeConstruct()
{
    Super::NativeConstruct();
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Starting construction"));
    
    // Validate widgets first
    if (!ValidateWidgets())
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: Widget validation failed"));
        CreateWidgetsDynamically();
    }
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Constructed successfully"));
}

void UPlayerInterfaceWidget::NativeDestruct()
{
    Super::NativeDestruct();

    bReadySignalSent = false;
    UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Destructed"));
}

void UPlayerInterfaceWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bReadySignalSent) { return; }
    if (!IsInterfaceReady()) { return; }

    bReadySignalSent = true;

    UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] PlayerInterfaceWidget: all child widgets valid � broadcasting OnPlayerInterfaceReady"));
    OnPlayerInterfaceReady.Broadcast();
}

FReply UPlayerInterfaceWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
        if (ABasicPlayer* Player = Cast<ABasicPlayer>(LocalPawn))
        {
            Player->ClearLockedTarget();
        }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UPlayerInterfaceWidget::InterfaceInitialize(UMyGameInstance* InGameInstance)
{
    if (!InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: Cannot initialize with null GameInstance"));
        return;
    }

    GameInstance = InGameInstance;
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Starting initialization"));

    // Initialize skill bar widget
    if (SkillBarWidget)
    {
        SkillBarWidget->BarInitialize(GameInstance);
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: SkillBar initialized"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: SkillBarWidget is null"));
    }

    // Initialize damage canvas
    if (DamageCanvasWidget)
    {
        DamageCanvasWidget->InitializeDamageCanvas();
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: DamageCanvas initialized"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: DamageCanvasWidget is null"));
    }

    if (PlayerHUD) {

    }

    // PlayerExperienceWidget will be initialized separately when character data is available

    // Setup positioning
    //SetupHUDPositioning();
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Initialization completed"));
}

void UPlayerInterfaceWidget::InitializeExperienceWidget(UExperienceManager* InExperienceManager, int32 CharacterId)
{
    if (!PlayerExperienceWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Experience widget not created yet"));
        return;
    }

    if (!InExperienceManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: ExperienceManager not available for experience widget initialization"));
        return;
    }

    if (CharacterId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Invalid character ID for experience widget initialization: %d"), CharacterId);
        return;
    }

    // Initialize the widget with the character ID
    PlayerExperienceWidget->InitializeWidget(InExperienceManager, CharacterId);
    
    UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Experience widget initialized for character %d"), CharacterId);
}

void UPlayerInterfaceWidget::SetupHUDPositioning()
{
    if (!SkillBarWidget || !PlayerHUD)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: Cannot setup positioning - missing widgets"));
        return;
    }

    // Get the overlay container from skill bar
    UOverlay* SkillBarOverlay = SkillBarWidget->GetSkillBarContainerOverlay();
    if (!SkillBarOverlay)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: SkillBarContainerOverlay not found"));
        return;
    }

    // Remove PlayerHUD from its current parent if it has one
    if (PlayerHUD->GetParent())
    {
        PlayerHUD->RemoveFromParent();
    }

    // Add PlayerHUD to the skill bar overlay
    UOverlaySlot* HUDSlot = SkillBarOverlay->AddChildToOverlay(PlayerHUD);
    if (HUDSlot)
    {
        // Position the HUD above the skill bar
        HUDSlot->SetHorizontalAlignment(HAlign_Center);
        HUDSlot->SetVerticalAlignment(VAlign_Top);
        
        // Add some padding to position it nicely above the skill bar
        HUDSlot->SetPadding(FMargin(0, -80, 0, 0)); // Adjust these values as needed
        
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: PlayerHUD positioned over SkillBar"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: Failed to add PlayerHUD to overlay"));
    }
}

void UPlayerInterfaceWidget::CreateWidgetsDynamically()
{
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: No world context for dynamic widget creation"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Creating widgets dynamically"));

    // Create SkillBarWidget if needed
    if (!SkillBarWidget && SkillBarWidgetClass)
    {
        SkillBarWidget = CreateWidget<USkillBarWidget>(GetWorld(), SkillBarWidgetClass);
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: SkillBarWidget created dynamically"));
    }

    // Create PlayerHUD if needed
    if (!PlayerHUD && PlayerHUDClass)
    {
        PlayerHUD = CreateWidget<UPlayerHUD>(GetWorld(), PlayerHUDClass);
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: PlayerHUD created dynamically"));
    }

    // Create DamageCanvasWidget if needed
    if (!DamageCanvasWidget && DamageCanvasWidgetClass)
    {
        DamageCanvasWidget = CreateWidget<UDamageCanvasWidget>(GetWorld(), DamageCanvasWidgetClass);
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: DamageCanvasWidget created dynamically"));
    }

    // Create PlayerExperienceWidget if needed
    if (!PlayerExperienceWidget && PlayerExperienceWidgetClass)
    {
        PlayerExperienceWidget = CreateWidget<UPlayerExperienceWidget>(GetWorld(), PlayerExperienceWidgetClass);
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: PlayerExperienceWidget created dynamically"));
    }

    // Create ActiveEffectsWidget if needed
    if (!ActiveEffectsWidget && ActiveEffectsWidgetClass)
    {
        ActiveEffectsWidget = CreateWidget<UActiveEffectsWidget>(GetWorld(), ActiveEffectsWidgetClass);
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: ActiveEffectsWidget created dynamically"));
    }
}

bool UPlayerInterfaceWidget::ValidateWidgets() const
{
    bool bIsValid = true;

    if (!SkillBarWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: SkillBarWidget is null"));
        bIsValid = false;
    }

    if (!PlayerHUD)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: PlayerHUD is null"));
        bIsValid = false;
    }

    if (!DamageCanvasWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: DamageCanvasWidget is null"));
        bIsValid = false;
    }

    if (!PlayerExperienceWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: PlayerExperienceWidget is null"));
        bIsValid = false;
    }

    return bIsValid;
}

bool UPlayerInterfaceWidget::IsInterfaceReady() const
{
    return SkillBarWidget != nullptr && 
           PlayerHUD != nullptr && 
           DamageCanvasWidget != nullptr &&
           PlayerExperienceWidget != nullptr &&
           GameInstance != nullptr;
}

void UPlayerInterfaceWidget::SetupSkillBar(int32 NumSlots)
{
    if (!SkillBarWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerInterfaceWidget: Cannot setup skill bar - SkillBarWidget is null"));
        return;
    }

    SkillBarWidget->CreateSkillSlots(NumSlots);
    UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Skill bar set up with %d slots"), NumSlots);
}

void UPlayerInterfaceWidget::UpdatePlayerStats(float CurrentHP, float MaxHP, float CurrentMana, float MaxMana)
{
    if (!PlayerHUD)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerInterfaceWidget: Cannot update stats - PlayerHUD is null"));
        return;
    }

    PlayerHUD->SetHP(CurrentHP, MaxHP);
    PlayerHUD->SetMana(CurrentMana, MaxMana);
}