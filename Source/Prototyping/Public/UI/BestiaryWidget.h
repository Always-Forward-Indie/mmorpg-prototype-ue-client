#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Data/DataStructs.h"
#include "UI/BestiaryMobRowWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "BestiaryWidget.generated.h"

class UScrollBox;
class UTextBlock;
class UButton;
class UWidget;
class UBestiaryEntryWidget;
class UBestiaryNetworkHandler;
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBestiaryVisibilityChanged, bool, bIsVisible);

/**
 * BestiaryWidget
 *
 * Main bestiary window. Shows a list of known mobs (icon + name).
 * Clicking a mob row sends getBestiaryEntry and opens BestiaryEntryWidget.
 *
 * Blueprint subclass must bind:
 *   Mob_List_Box       UScrollBox   — populated with mob rows
 *   Entry_Panel        UBestiaryEntryWidget (BindWidget / BindWidgetOptional)
 *                      — can also be a separate widget class set in MobEntryWidgetClass
 *   Close_Button       UButton      (BindWidgetOptional)
 *   Search_Input       UEditableTextBox (BindWidgetOptional) — optional name filter
 *
 * Each mob row in MobRowClass needs:
 *   Row_Mob_Name_Text  UTextBlock
 *   Row_Select_Btn     UButton      — triggers detail view
 *   Row_Kill_Text      UTextBlock   (optional) — kill count badge
 *
 * Drag support: DragHandle widget (BindWidgetOptional) — if absent, whole widget is draggable.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UBestiaryWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    /**
     * Bind to the BestiaryNetworkHandler.
     * Call from UIManager after widget creation.
     */
    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void BindToBestiaryHandler(UBestiaryNetworkHandler* InHandler);

    /** Add or update one entry in the known-mob list. */
    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void AddOrUpdateMobEntry(const FString& MobSlug, int32 KillCount);

    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void OpenBestiary();

    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void CloseBestiary();

    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void ToggleBestiary();

    UPROPERTY(BlueprintAssignable, Category = "Bestiary|Events")
    FOnBestiaryVisibilityChanged OnBestiaryVisibilityChanged;

    /** Request bestiary data for a mob and open the entry panel. */
    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void RequestAndShowEntry(const FString& InMobSlug);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    // ------------------------------------------------------------------
    // Event handlers
    // ------------------------------------------------------------------
    UFUNCTION()
    void HandleBestiaryOverviewReceived(const TArray<FBestiaryOverviewEntryStruct>& Entries);

    UFUNCTION()
    void HandleBestiaryEntryReceived(const FBestiaryEntryStruct& Entry);

    UFUNCTION()
    void HandleBestiaryTierUnlocked(const FString& MobSlug, int32 UnlockedTier, const FString& CategorySlug);

    UFUNCTION()
    void HandleBestiaryKillCountUpdated(const FString& MobSlug, int32 KillCount);

    UFUNCTION()
    void HandleCloseClicked();

    UFUNCTION()
    void HandleMobRowSelected(const FString& MobSlug);

    // ------------------------------------------------------------------
    // Bound widgets
    // ------------------------------------------------------------------
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Bestiary")
    UScrollBox* Mob_List_Box = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary")
    UBestiaryEntryWidget* Entry_Panel = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary")
    UButton* Close_Button = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Bestiary")
    UWidget* DragHandle = nullptr;

    // ------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------

    /** Row widget class for each mob in the list. Must be a subclass of UBestiaryMobRowWidget. Set in Blueprint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary|Config")
    TSubclassOf<UBestiaryMobRowWidget> MobRowClass;

    /**
     * Separate entry detail widget class.
     * If Entry_Panel is not bound, a widget of this class is created and
     * shown as a child of this widget (or standalone).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary|Config")
    TSubclassOf<UBestiaryEntryWidget> MobEntryWidgetClass;

private:

    UFUNCTION()
    void HandleEntryPanelCloseRequested();

    /** Rebuild the whole mob list from CachedMobList. */
    void RebuildMobList();

    struct FBestiaryMobListEntry
    {
        FString MobSlug;
        int32   KillCount = 0;
    };

    TArray<FBestiaryMobListEntry> CachedMobList;

    // Pending request mob slug (waiting for server response)
    FString PendingRequestMobSlug;

    UPROPERTY()
    UBestiaryNetworkHandler* BestiaryHandler = nullptr;

    // Standalone entry widget (used when Entry_Panel is not bound in UMG)
    UPROPERTY()
    UBestiaryEntryWidget* StandaloneEntryWidget = nullptr;

    // Drag support
    bool      bDragging = false;
    FVector2D DragOffset = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;

    int32 CurrentCharacterId = 0;
};
