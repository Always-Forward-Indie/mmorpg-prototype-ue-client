#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Overlay.h"
#include "UI/SkillBarWidget.h"
#include "Gameplay/UI/PlayerHUD.h"
#include "Gameplay/UI/DamageCanvasWidget.h"
#include "Gameplay/UI/PlayerExperienceWidget.h"
#include "PlayerInterfaceWidget.generated.h"

// Forward declarations
class UMyGameInstance;
class UExperienceManager;

/**
 * Main player interface widget that combines SkillBar, PlayerHUD, DamageCanvas and PlayerExperience
 * This widget manages the layout and positioning of core UI elements
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerInterfaceWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
    /** Initialize the player interface with game instance */
    UFUNCTION(BlueprintCallable, Category = "Player Interface")
    void InterfaceInitialize(UMyGameInstance* InGameInstance);

    /** Get the skill bar widget */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Interface")
    USkillBarWidget* GetSkillBarWidget() const { return SkillBarWidget; }

    /** Get the player HUD widget */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Interface")
    UPlayerHUD* GetPlayerHUD() const { return PlayerHUD; }

    /** Get the damage canvas widget */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Interface")
    UDamageCanvasWidget* GetDamageCanvasWidget() const { return DamageCanvasWidget; }

    /** Get the player experience widget */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Interface")
    UPlayerExperienceWidget* GetPlayerExperienceWidget() const { return PlayerExperienceWidget; }

    /** Setup the positioning of HUD over skill bar */
    UFUNCTION(BlueprintCallable, Category = "Player Interface")
    void SetupHUDPositioning();

    /** Initialize experience widget with character ID and experience manager */
    UFUNCTION(BlueprintCallable, Category = "Player Interface")
    void InitializeExperienceWidget(UExperienceManager* InExperienceManager, int32 CharacterId);

protected:
    // Main interface components
    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Player Interface")
    USkillBarWidget* SkillBarWidget;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Player Interface")
    UPlayerHUD* PlayerHUD;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Player Interface")
    UDamageCanvasWidget* DamageCanvasWidget;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Player Interface")
    UPlayerExperienceWidget* PlayerExperienceWidget;

    // Widget classes for dynamic creation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Interface")
    TSubclassOf<USkillBarWidget> SkillBarWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Interface")
    TSubclassOf<UPlayerHUD> PlayerHUDClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Interface")
    TSubclassOf<UDamageCanvasWidget> DamageCanvasWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Interface")
    TSubclassOf<UPlayerExperienceWidget> PlayerExperienceWidgetClass;

    // Widget overrides
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativePreConstruct() override;

private:
    UPROPERTY()
    UMyGameInstance* GameInstance;

    /** Create widgets dynamically if they're not bound in Blueprint */
    void CreateWidgetsDynamically();

    /** Validate that all required widgets are available */
    bool ValidateWidgets() const;

public:
    /** Check if the interface is fully initialized */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Interface")
    bool IsInterfaceReady() const;

    /** Setup skill bar with specified number of slots */
    UFUNCTION(BlueprintCallable, Category = "Player Interface")
    void SetupSkillBar(int32 NumSlots = 10);

    /** Update HUD with current player stats */
    UFUNCTION(BlueprintCallable, Category = "Player Interface")
    void UpdatePlayerStats(float CurrentHP, float MaxHP, float CurrentMana, float MaxMana);
};