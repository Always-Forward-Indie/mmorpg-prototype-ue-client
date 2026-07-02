// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/NameplateCanvasWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

// -----------------------------------------------------------------------
// UUserWidget overrides
// -----------------------------------------------------------------------

void UNameplateCanvasWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UNameplateCanvasWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!NameplateCanvas)
    {
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    APawn* Pawn = PC->GetPawn();
    // On the login/character-select screen there is no pawn — the player is
    // viewing through a CameraActor.  Fall back to the camera position so
    // distance-based visibility culling works correctly for preview characters.
    FVector PawnLocation;
    if (Pawn)
    {
        PawnLocation = Pawn->GetActorLocation();
    }
    else if (PC->PlayerCameraManager)
    {
        PawnLocation = PC->PlayerCameraManager->GetCameraLocation();
    }
    else
    {
        PawnLocation = FVector::ZeroVector;
    }

    for (FNameplateEntry& Entry : Entries)
    {
        if (!Entry.Actor.IsValid())
        {
            // Actor was destroyed - hide widget, will be cleaned up next unregister call
            if (Entry.Widget)
            {
                Entry.Widget->SetVisibility(ESlateVisibility::Collapsed);
            }
            continue;
        }

        TickEntry(Entry, PC, PawnLocation, InDeltaTime);
    }

    // ---- Chat-bubble tick --------------------------------------------------
    // Same projection pipeline as nameplates but at an extra Z offset so the
    // bubble appears above the nameplate.  Opacity is driven by distance so the
    // bubble fades in/out smoothly as the local player approaches / retreats,
    // and also fades in on message arrival and out on timer expiry.
    for (auto It = ChatBubbles.CreateIterator(); It; ++It)
    {
        TWeakObjectPtr<AActor> WeakActor = It.Key();
        AActor* BubbleActor = WeakActor.Get();
        UChatBubbleWidget* Bubble = It.Value();

        if (!IsValid(BubbleActor) || !Bubble)
        {
            if (Bubble) Bubble->RemoveFromParent();
            ChatBubbleOpacity.Remove(WeakActor);
            It.RemoveCurrent();
            continue;
        }

        float& CurrentOpacity = ChatBubbleOpacity.FindOrAdd(WeakActor);

        // Distance-based target opacity: fade to 0 beyond ChatBubbleMaxVisibleDistance.
        const float DistToBubble = FVector::Dist(PawnLocation, BubbleActor->GetActorLocation());
        float TargetOpacity = 0.0f;
        if (Bubble->IsShowing() && DistToBubble <= ChatBubbleMaxVisibleDistance)
        {
            const float FadeStart = ChatBubbleMaxVisibleDistance - FMath::Max(ChatBubbleFadeDistance, 1.f);
            if (DistToBubble <= FadeStart)
            {
                TargetOpacity = 1.0f;
            }
            else
            {
                TargetOpacity = FMath::Clamp(
                    1.0f - (DistToBubble - FadeStart) / FMath::Max(ChatBubbleFadeDistance, 1.f),
                    0.0f, 1.0f);
            }
        }

        // World-space anchor: capsule top + extra offset above the nameplate.
        float CapsuleHalf = 0.f;
        if (const ACharacter* Char = Cast<ACharacter>(BubbleActor))
            if (const UCapsuleComponent* Cap = Char->GetCapsuleComponent())
                CapsuleHalf = Cap->GetScaledCapsuleHalfHeight();

        const FVector BubbleWorld = BubbleActor->GetActorLocation()
                                  + FVector(0.f, 0.f, CapsuleHalf + ChatBubbleHeadOffsetZ);

        // Camera dot-product guard: snap opacity to 0 instantly when behind camera.
        if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
        {
            const FVector CamFwd = CamMgr->GetActorForwardVector();
            const FVector ToPoint = BubbleWorld - CamMgr->GetCameraLocation();
            if (FVector::DotProduct(ToPoint, CamFwd) <= 0.f)
            {
                CurrentOpacity = 0.f;
                Bubble->SetRenderOpacity(0.f);
                Bubble->SetVisibility(ESlateVisibility::Collapsed);
                if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                    S->SetPosition(FVector2D(-9999.f, -9999.f));
                continue;
            }
        }

        // Smooth fade toward target (skip interp when already at 0 and target is 0).
        if (CurrentOpacity < 0.01f && TargetOpacity <= 0.f)
            CurrentOpacity = 0.f;
        else
            CurrentOpacity = ChatBubbleFadeSpeed > 0.f
                ? FMath::FInterpConstantTo(CurrentOpacity, TargetOpacity, InDeltaTime, ChatBubbleFadeSpeed)
                : TargetOpacity;

        const bool bShouldBeVisible = CurrentOpacity > 0.01f;

        if (!bShouldBeVisible)
        {
            Bubble->SetVisibility(ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                S->SetPosition(FVector2D(-9999.f, -9999.f));
            continue;
        }

        FVector2D ScreenPosPx;
        if (!PC->ProjectWorldLocationToScreen(BubbleWorld, ScreenPosPx, true))
        {
            // Off-screen: snap so the bubble doesn’t linger on a stale position.
            CurrentOpacity = 0.f;
            Bubble->SetVisibility(ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                S->SetPosition(FVector2D(-9999.f, -9999.f));
            continue;
        }

        const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(this);
        const FVector2D ScreenPos = ScreenPosPx / FMath::Max(DPIScale, 0.01f);

        Bubble->SetRenderOpacity(CurrentOpacity);
        if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
            S->SetPosition(ScreenPos);
        if (Bubble->GetVisibility() == ESlateVisibility::Collapsed)
            Bubble->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    // ---- NPC Speech-bubble tick -------------------------------------------
    // Identical distance-fade logic as player chat bubbles.
    for (auto It = NPCSpeechBubbles.CreateIterator(); It; ++It)
    {
        TWeakObjectPtr<AActor> WeakActor = It.Key();
        AActor* BubbleActor = WeakActor.Get();
        UChatBubbleWidget* Bubble = It.Value();

        if (!IsValid(BubbleActor) || !Bubble)
        {
            if (Bubble) Bubble->RemoveFromParent();
            NPCSpeechBubbleOpacity.Remove(WeakActor);
            NPCSpeechBubbleExpiry.Remove(WeakActor);
            It.RemoveCurrent();
            continue;
        }

        float& CurrentOpacity = NPCSpeechBubbleOpacity.FindOrAdd(WeakActor);

        // Distance-based target opacity.
        const float DistToBubble = FVector::Dist(PawnLocation, BubbleActor->GetActorLocation());
        float TargetOpacity = 0.0f;
        if (Bubble->IsShowing() && DistToBubble <= ChatBubbleMaxVisibleDistance)
        {
            const float FadeStart = ChatBubbleMaxVisibleDistance - FMath::Max(ChatBubbleFadeDistance, 1.f);
            if (DistToBubble <= FadeStart)
            {
                TargetOpacity = 1.0f;
            }
            else
            {
                TargetOpacity = FMath::Clamp(
                    1.0f - (DistToBubble - FadeStart) / FMath::Max(ChatBubbleFadeDistance, 1.f),
                    0.0f, 1.0f);
            }
        }

        float CapsuleHalf = 0.f;
        if (const ACharacter* Char = Cast<ACharacter>(BubbleActor))
            if (const UCapsuleComponent* Cap = Char->GetCapsuleComponent())
                CapsuleHalf = Cap->GetScaledCapsuleHalfHeight();

        const FVector BubbleWorld = BubbleActor->GetActorLocation()
                                  + FVector(0.f, 0.f, CapsuleHalf + NPCSpeechBubbleHeadOffsetZ);

        // Camera dot-product guard: snap opacity to 0 instantly when behind camera.
        if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
        {
            const FVector CamFwd = CamMgr->GetActorForwardVector();
            const FVector ToPoint = BubbleWorld - CamMgr->GetCameraLocation();
            if (FVector::DotProduct(ToPoint, CamFwd) <= 0.f)
            {
                CurrentOpacity = 0.f;
                Bubble->SetRenderOpacity(0.f);
                Bubble->SetVisibility(ESlateVisibility::Collapsed);
                if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                    S->SetPosition(FVector2D(-9999.f, -9999.f));
                continue;
            }
        }

        // Smooth fade.
        if (CurrentOpacity < 0.01f && TargetOpacity <= 0.f)
            CurrentOpacity = 0.f;
        else
            CurrentOpacity = ChatBubbleFadeSpeed > 0.f
                ? FMath::FInterpConstantTo(CurrentOpacity, TargetOpacity, InDeltaTime, ChatBubbleFadeSpeed)
                : TargetOpacity;

        const bool bShouldBeVisible = CurrentOpacity > 0.01f;

        if (!bShouldBeVisible)
        {
            Bubble->SetVisibility(ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                S->SetPosition(FVector2D(-9999.f, -9999.f));
            continue;
        }

        FVector2D ScreenPosPx;
        if (!PC->ProjectWorldLocationToScreen(BubbleWorld, ScreenPosPx, true))
        {
            CurrentOpacity = 0.f;
            Bubble->SetVisibility(ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                S->SetPosition(FVector2D(-9999.f, -9999.f));
            continue;
        }

        const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(this);
        const FVector2D ScreenPos = ScreenPosPx / FMath::Max(DPIScale, 0.01f);

        Bubble->SetRenderOpacity(CurrentOpacity);
        if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
            S->SetPosition(ScreenPos);
        if (Bubble->GetVisibility() == ESlateVisibility::Collapsed)
            Bubble->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

// -----------------------------------------------------------------------
// Registration API
// -----------------------------------------------------------------------

void UNameplateCanvasWidget::RegisterPlayer(AActor*        Actor,
                                             const FString& Name,
                                             const FString& Class,
                                             int32          Level,
                                             bool           bIsDead,
                                             float          HeadOffsetZ)
{
    UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::RegisterPlayer called for actor '%s' (name='%s')"), 
        Actor ? *Actor->GetName() : TEXT("NULL"), *Name);

    if (!Actor || FindEntry(Actor))
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::RegisterPlayer - actor is null or already registered"));
        return;
    }

    if (!PlayerNameplateWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget: PlayerNameplateWidgetClass is not set"));
        return;
    }

    UUserWidget* RawWidget = AddWidgetToCanvas(PlayerNameplateWidgetClass);
    if (!RawWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("NameplateCanvasWidget::RegisterPlayer - AddWidgetToCanvas returned null"));
        return;
    }

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(RawWidget);
    if (Nameplate)
    {
        Nameplate->SetPlayerInfo(Name, Class, Level, bIsDead);
        // Explicitly hide the title row — title arrives later via SetPlayerTitle.
        // Prevents an empty text slot from showing when the player has no title.
        Nameplate->SetTitle(FText::GetEmpty());
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::RegisterPlayer - successfully created and initialized nameplate for '%s'"), *Name);
    }

    FNameplateEntry& Entry  = Entries.AddDefaulted_GetRef();
    Entry.Actor             = Actor;
    Entry.Widget            = RawWidget;
    Entry.HeadOffsetZ       = (HeadOffsetZ > 0.f) ? HeadOffsetZ : DefaultPlayerHeadOffsetZ;
    Entry.bIsNPC            = false;
    Entry.bIsLocalPlayer    = false;
}

void UNameplateCanvasWidget::RegisterNPC(AActor*               Actor,
                                          const FString&        Name,
                                          const FString&        NPCType,
                                          int32                 Level,
                                          ENPCInteractionState  InteractionState,
                                          float                 InteractRadius,
                                          float                 HeadOffsetZ)
{
    UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::RegisterNPC called for actor '%s' (name='%s', type='%s')"), 
        Actor ? *Actor->GetName() : TEXT("NULL"), *Name, *NPCType);

    if (!Actor || FindEntry(Actor))
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::RegisterNPC - actor is null or already registered"));
        return;
    }

    if (!NPCNameplateWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget: NPCNameplateWidgetClass is not set"));
        return;
    }

    UUserWidget* RawWidget = AddWidgetToCanvas(NPCNameplateWidgetClass);
    if (!RawWidget)
    {
        UE_LOG(LogTemp, Error, TEXT("NameplateCanvasWidget::RegisterNPC - AddWidgetToCanvas returned null"));
        return;
    }

    UW_NPCNameplateWidget* Nameplate = Cast<UW_NPCNameplateWidget>(RawWidget);
    if (Nameplate)
    {
        Nameplate->SetNPCInfo(Name, NPCType, Level, InteractionState);
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::RegisterNPC - successfully created and initialized nameplate for '%s'"), *Name);
    }

    FNameplateEntry& Entry  = Entries.AddDefaulted_GetRef();
    Entry.Actor             = Actor;
    Entry.Widget            = RawWidget;
    Entry.HeadOffsetZ       = (HeadOffsetZ > 0.f) ? HeadOffsetZ : DefaultNPCHeadOffsetZ;
    Entry.InteractRadius    = InteractRadius;
    Entry.bIsNPC            = true;
    Entry.bIsLocalPlayer    = false;
}

void UNameplateCanvasWidget::Unregister(AActor* Actor)
{
    // Remove chat bubble for this actor
    TWeakObjectPtr<AActor> WeakActor(Actor);
    if (UChatBubbleWidget** BubblePtr = ChatBubbles.Find(WeakActor))
    {
        if (*BubblePtr) (*BubblePtr)->RemoveFromParent();
        ChatBubbles.Remove(WeakActor);
        ChatBubbleOpacity.Remove(WeakActor);
    }

    // Remove NPC speech bubble for this actor
    if (UChatBubbleWidget** NPCBubblePtr = NPCSpeechBubbles.Find(WeakActor))
    {
        if (*NPCBubblePtr) (*NPCBubblePtr)->RemoveFromParent();
        NPCSpeechBubbles.Remove(WeakActor);
        NPCSpeechBubbleOpacity.Remove(WeakActor);
    }
    NPCSpeechBubbleExpiry.Remove(WeakActor);

    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        if (Entries[i].Actor == Actor)
        {
            if (Entries[i].Widget && NameplateCanvas)
            {
                Entries[i].Widget->RemoveFromParent();
            }
            Entries.RemoveAtSwap(i);
            return;
        }
    }
}

void UNameplateCanvasWidget::UnregisterAll()
{
    for (FNameplateEntry& Entry : Entries)
    {
        if (Entry.Widget)
        {
            Entry.Widget->RemoveFromParent();
        }
    }
    Entries.Reset();

    for (auto& Pair : ChatBubbles)
    {
        if (Pair.Value) Pair.Value->RemoveFromParent();
    }
    ChatBubbles.Empty();
    ChatBubbleOpacity.Empty();

    for (auto& Pair : NPCSpeechBubbles)
    {
        if (Pair.Value) Pair.Value->RemoveFromParent();
    }
    NPCSpeechBubbles.Empty();
    NPCSpeechBubbleOpacity.Empty();
    NPCSpeechBubbleExpiry.Empty();
}

// -----------------------------------------------------------------------
// Live update API
// -----------------------------------------------------------------------

void UNameplateCanvasWidget::UpdatePlayerHealth(AActor* Actor, int32 CurrentHP, int32 MaxHP)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC)
    {
        return;
    }

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        Nameplate->UpdateHealthBar(CurrentHP, MaxHP);
    }
}

void UNameplateCanvasWidget::SetPlayerDeadState(AActor* Actor, bool bDead)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC)
    {
        return;
    }

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        Nameplate->SetDeadState(bDead);
    }
}

void UNameplateCanvasWidget::SetPlayerLevel(AActor* Actor, int32 NewLevel)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC) return;

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        Nameplate->SetPlayerLevel(NewLevel);
    }
}

void UNameplateCanvasWidget::SetPlayerTitle(AActor* Actor, const FText& InTitle)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC) return;

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        Nameplate->SetTitle(InTitle);
    }
}

void UNameplateCanvasWidget::ShowPlayerChatBubble(AActor* Actor, const FString& Text, float Duration)
{
    if (!Actor) return;

    // Only for registered remote player nameplates (not NPCs).
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC) return;

    if (!ChatBubbleWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::ShowPlayerChatBubble - ChatBubbleWidgetClass is not set"));
        return;
    }

    // Look up or lazy-create the bubble for this actor.
    TWeakObjectPtr<AActor> WeakActor(Actor);
    UChatBubbleWidget** Found = ChatBubbles.Find(WeakActor);
    UChatBubbleWidget*  Bubble = Found ? *Found : nullptr;

    if (!Bubble)
    {
        APlayerController* PC = GetOwningPlayer();
        if (!PC) return;

        UUserWidget* RawWidget = AddWidgetToCanvas(ChatBubbleWidgetClass);
        Bubble = Cast<UChatBubbleWidget>(RawWidget);
        if (!Bubble)
        {
            UE_LOG(LogTemp, Error, TEXT("NameplateCanvasWidget::ShowPlayerChatBubble - failed to create ChatBubbleWidget"));
            return;
        }
        ChatBubbles.Add(WeakActor, Bubble);
    }

    Bubble->Show(Text, Duration);
}

void UNameplateCanvasWidget::ShowNPCSpeechBubble(AActor* Actor, const FText& Text, float Duration)
{
    if (!Actor) return;

    // Determine which widget class to use (NPC variant or shared fallback)
    TSubclassOf<UChatBubbleWidget> BubbleClass = NPCSpeechBubbleWidgetClass
        ? NPCSpeechBubbleWidgetClass : ChatBubbleWidgetClass;

    if (!BubbleClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::ShowNPCSpeechBubble - no bubble widget class set"));
        return;
    }

    TWeakObjectPtr<AActor> WeakActor(Actor);
    UChatBubbleWidget** Found = NPCSpeechBubbles.Find(WeakActor);
    UChatBubbleWidget*  Bubble = Found ? *Found : nullptr;

    if (!Bubble)
    {
        UUserWidget* RawWidget = AddWidgetToCanvas(BubbleClass);
        Bubble = Cast<UChatBubbleWidget>(RawWidget);
        if (!Bubble)
        {
            UE_LOG(LogTemp, Error, TEXT("NameplateCanvasWidget::ShowNPCSpeechBubble - failed to create bubble widget"));
            return;
        }
        NPCSpeechBubbles.Add(WeakActor, Bubble);
    }

    Bubble->Show(Text.ToString(), Duration);

    // Record expiry time so NativeTick can position the bubble even while it is visible
    if (UWorld* World = GetWorld())
    {
        NPCSpeechBubbleExpiry.Add(WeakActor, World->GetTimeSeconds() + Duration);
    }
}

void UNameplateCanvasWidget::SetNPCInteractionState(AActor* Actor, ENPCInteractionState NewState)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || !Entry->bIsNPC)
    {
        return;
    }

    UW_NPCNameplateWidget* Nameplate = Cast<UW_NPCNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        // Re-use SetNPCInfo with cached name/type is verbose; a dedicated method is cleaner.
        // For now drive only the indicator icons via a targeted call if the widget exposes it,
        // otherwise accept that a full refresh requires re-registering with bForceRefresh.
        (void)NewState; // placeholder until W_NPCNameplateWidget exposes SetInteractionState
    }
}

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

FNameplateEntry* UNameplateCanvasWidget::FindEntry(AActor* Actor)
{
    for (FNameplateEntry& Entry : Entries)
    {
        if (Entry.Actor == Actor)
        {
            return &Entry;
        }
    }
    return nullptr;
}

UUserWidget* UNameplateCanvasWidget::AddWidgetToCanvas(TSubclassOf<UUserWidget> WidgetClass)
{
    if (!NameplateCanvas || !WidgetClass)
    {
        return nullptr;
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::AddWidgetToCanvas � no owning PlayerController"));
        return nullptr;
    }

    UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
    if (!Widget)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::AddWidgetToCanvas � CreateWidget returned null for class %s"),
            *WidgetClass->GetName());
        return nullptr;
    }

    UCanvasPanelSlot* NewWidgetSlot = NameplateCanvas->AddChildToCanvas(Widget);
    if (NewWidgetSlot)
    {
        NewWidgetSlot->SetAutoSize(true);
        NewWidgetSlot->SetAlignment(FVector2D(0.5f, 1.0f));
        NewWidgetSlot->SetPosition(FVector2D(-9999.f, -9999.f));
    }

    Widget->SetVisibility(ESlateVisibility::Collapsed);
    return Widget;
}

void UNameplateCanvasWidget::TickEntry(FNameplateEntry&  Entry,
                                        APlayerController* PC,
                                        const FVector&     PawnLocation,
                                        float              DeltaTime)
{
    if (Entry.bIsLocalPlayer || !Entry.Widget)
    {
        return;
    }

    // For ACharacter the actor origin is at the *centre* of the capsule, not the feet.
    // We need to step up by CapsuleHalfHeight to reach the top of the capsule, then
    // add HeadOffsetZ as an extra margin above the head.  For non-Character actors
    // (e.g. StaticMeshActor props) we fall back to a pure Z offset from the origin.
    float CapsuleHalf = 0.f;
    if (const ACharacter* Char = Cast<ACharacter>(Entry.Actor.Get()))
    {
        if (const UCapsuleComponent* Cap = Char->GetCapsuleComponent())
        {
            CapsuleHalf = Cap->GetScaledCapsuleHalfHeight();
        }
    }
    const FVector HeadWorld = Entry.Actor->GetActorLocation()
                            + FVector(0.f, 0.f, CapsuleHalf + Entry.HeadOffsetZ);

    // --- Camera dot-product guard ---
    // ProjectWorldLocationToScreen alone is not sufficient: it can return true for
    // a point that is technically in front of the near plane but whose projected
    // screen coordinates land far outside the viewport (actor near the frustum edge,
    // high FOV, etc.).  We add an explicit dot-product test first so that any actor
    // behind or exactly at the camera plane is rejected before projection, avoiding
    // the "ghost nameplate appears on the wrong NPC for one frame" artefact that
    // occurred when rotating/zooming the camera while the source actor was off-screen
    // to the left or behind the camera.
    if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
    {
        const FVector CamFwd = CamMgr->GetActorForwardVector();
        const FVector ToHead = HeadWorld - CamMgr->GetCameraLocation();
        if (FVector::DotProduct(ToHead, CamFwd) <= 0.f)
        {
            // Snap to invisible instantly � no fade tail that could linger over
            // another visible actor while this one is behind the camera.
            Entry.CurrentOpacity = 0.f;
            Entry.Widget->SetRenderOpacity(0.f);
            if (UCanvasPanelSlot* EntrySlot = Cast<UCanvasPanelSlot>(Entry.Widget->Slot))
            {
                EntrySlot->SetPosition(FVector2D(-9999.f, -9999.f));
            }
            if (Entry.Widget->GetVisibility() != ESlateVisibility::Collapsed)
            {
                Entry.Widget->SetVisibility(ESlateVisibility::Collapsed);
            }
            return;
        }
    }

    // --- Project to screen ---
    // ProjectWorldLocationToScreen returns pixel coordinates but does NOT guarantee
    // the result is within the visible viewport � points behind or beside the camera
    // produce mirrored / out-of-bounds coordinates that must be rejected explicitly.
    FVector2D ScreenPosPx;
    const bool bOnScreen = PC->ProjectWorldLocationToScreen(HeadWorld, ScreenPosPx, true);

    const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(this);
    const FVector2D ScreenPos = ScreenPosPx / FMath::Max(DPIScale, 0.01f);

    // --- Distance ---
    Entry.DistanceCm = FVector::Dist(PawnLocation, Entry.Actor->GetActorLocation());
    const float Dist = Entry.DistanceCm;

    // --- Opacity ---
    float TargetOpacity = 0.0f;
    if (bOnScreen && Dist >= MinVisibleDistance && Dist <= MaxVisibleDistance)
    {
        TargetOpacity = 1.0f;
    }
    else if (bOnScreen && Dist < MinVisibleDistance)
    {
        const float FadeZone = MinVisibleDistance * 0.5f;
        TargetOpacity = FMath::Clamp(
            (Dist - FadeZone) / FMath::Max(FadeZone, 1.f), 0.f, 1.f);
    }

    // Smooth fade in both directions.
    // If was already invisible and target is also 0 � snap to stay at 0
    // so we never accumulate a tiny residual opacity from the interp.
    const bool bWasInvisible = Entry.CurrentOpacity < 0.01f;
    if (bWasInvisible && TargetOpacity <= 0.f)
    {
        Entry.CurrentOpacity = 0.f;
    }
    else
    {
        Entry.CurrentOpacity = FadeSpeed > 0.f
            ? FMath::FInterpConstantTo(Entry.CurrentOpacity, TargetOpacity, DeltaTime, FadeSpeed)
            : TargetOpacity;
    }

    const bool bShouldBeVisible = Entry.CurrentOpacity > 0.01f;

    // --- Scale ---
    float DesiredScale;
    if (Dist >= ReferenceDistance)
    {
        DesiredScale = 1.0f;
    }
    else
    {
        const float T = Dist / FMath::Max(ReferenceDistance, 1.f);
        DesiredScale = FMath::Lerp(MinScale, 1.0f, T);
    }
    DesiredScale = FMath::Clamp(DesiredScale, MinScale, MaxScale);

    // --- Position and opacity must be committed BEFORE making the widget visible.
    // If we set Visible first and then SetPosition, Slate can render the widget
    // for one frame at its previous (stale) position � which is how a nameplate
    // appears to "teleport" or show above the wrong actor for a single frame.
    if (UCanvasPanelSlot* EntrySlot = Cast<UCanvasPanelSlot>(Entry.Widget->Slot))
    {
        if (bShouldBeVisible)
        {
            EntrySlot->SetPosition(ScreenPos);
        }
        else
        {
            // Park off-screen while hidden so it can never accidentally overlap
            // a visible nameplate during the frame it transitions to Collapsed.
            EntrySlot->SetPosition(FVector2D(-9999.f, -9999.f));
        }
    }

    Entry.Widget->SetRenderOpacity(Entry.CurrentOpacity);
    Entry.Widget->SetRenderScale(FVector2D(DesiredScale, DesiredScale));

    const ESlateVisibility DesiredVis = bShouldBeVisible
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed;

    if (Entry.Widget->GetVisibility() != DesiredVis)
    {
        Entry.Widget->SetVisibility(DesiredVis);
    }

    // --- NPC: interact hint ---
    if (bShouldBeVisible && Entry.bIsNPC)
    {
        if (UW_NPCNameplateWidget* NPCWidget = Cast<UW_NPCNameplateWidget>(Entry.Widget))
        {
            NPCWidget->SetPlayerInRange(Dist <= Entry.InteractRadius);
        }
    }
}
