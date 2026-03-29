#include "Gameplay/Trade/TradeManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UTradeManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InNetworkManager || !InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("TradeManager: Initialize called with null parameters"));
        return;
    }
    NetworkManager = InNetworkManager;
    GameInstance   = InGameInstance;
}

// ---------------------------------------------------------------------------
// Outgoing requests
// ---------------------------------------------------------------------------

void UTradeManager::RequestTrade(int32 CharacterId, int32 TargetCharacterId, const FVector& PlayerPosition)
{
    if (!NetworkManager || !GameInstance) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("tradeRequest"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Body->SetNumberField(TEXT("targetCharacterId"), TargetCharacterId);
    Body->SetNumberField(TEXT("posX"), PlayerPosition.X);
    Body->SetNumberField(TEXT("posY"), PlayerPosition.Y);
    Body->SetNumberField(TEXT("posZ"), PlayerPosition.Z);
    Body->SetNumberField(TEXT("rotZ"), 0.0);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    SendPacket(Payload);
}

void UTradeManager::RespondToTradeInvite(int32 CharacterId, bool bAccept)
{
    if (!NetworkManager || !GameInstance) return;

    const FString EventType = bAccept ? TEXT("tradeAccept") : TEXT("tradeDecline");

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), EventType);
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    // Protocol sec. 8.3: fromCharacterId is a string value
    Body->SetStringField(TEXT("fromCharacterId"), FString::FromInt(CharacterId));

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    if (!bAccept)
    {
        bHasPendingInvite = false;
    }

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    SendPacket(Payload);
}

void UTradeManager::UpdateTradeOffer(int32 CharacterId, int32 Gold, const TArray<FTradeOfferItem>& Items)
{
    if (!NetworkManager || !GameInstance || CurrentSessionId.IsEmpty()) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("tradeOfferUpdate"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Body->SetNumberField(TEXT("characterId"), CharacterId);
    Body->SetStringField(TEXT("sessionId"),   CurrentSessionId);
    Body->SetNumberField(TEXT("gold"),        Gold);

    TArray<TSharedPtr<FJsonValue>> ItemsArray;
    for (const FTradeOfferItem& Item : Items)
    {
        TSharedPtr<FJsonObject> ItemObj = MakeShared<FJsonObject>();
        ItemObj->SetNumberField(TEXT("inventoryItemId"), Item.inventoryItemId);
        ItemObj->SetNumberField(TEXT("itemId"),          Item.itemId);
        ItemObj->SetNumberField(TEXT("quantity"),        Item.quantity);
        ItemsArray.Add(MakeShared<FJsonValueObject>(ItemObj));
    }
    Body->SetArrayField(TEXT("items"), ItemsArray);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    SendPacket(Payload);
}

void UTradeManager::ConfirmTrade(int32 CharacterId)
{
    if (!NetworkManager || !GameInstance || CurrentSessionId.IsEmpty()) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("tradeConfirm"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Body->SetNumberField(TEXT("characterId"), CharacterId);
    Body->SetStringField(TEXT("sessionId"),   CurrentSessionId);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    SendPacket(Payload);
}

void UTradeManager::CancelTrade(int32 CharacterId)
{
    if (!NetworkManager || !GameInstance || CurrentSessionId.IsEmpty()) return;

    TSharedPtr<FJsonObject> Root   = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Header = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Body   = MakeShared<FJsonObject>();

    Header->SetStringField(TEXT("eventType"), TEXT("tradeCancel"));
    Header->SetNumberField(TEXT("clientId"),  GameInstance->GetCurrentClientID());
    Header->SetStringField(TEXT("hash"),      GameInstance->GetCurrentClientHash());

    Body->SetNumberField(TEXT("characterId"), CharacterId);
    Body->SetStringField(TEXT("sessionId"),   CurrentSessionId);

    Root->SetObjectField(TEXT("header"), Header);
    Root->SetObjectField(TEXT("body"),   Body);

    FString Payload;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Payload);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    SendPacket(Payload);
}

// ---------------------------------------------------------------------------
// Incoming — called by TradeNetworkHandler
// ---------------------------------------------------------------------------

void UTradeManager::OnTradeInviteReceived(const FTradeInviteData& Invite)
{
    PendingInvite     = Invite;
    bHasPendingInvite = true;
    // Store the session ID from the invite so RespondToTradeInvite can echo it back
    CurrentSessionId  = Invite.sessionId;
    OnTradeInviteReceivedDelegate.Broadcast(Invite);
}

void UTradeManager::OnTradeStateReceived(const FTradeStateData& State)
{
    CurrentSessionId = State.sessionId;
    CurrentState     = State;
    OnTradeStateUpdatedDelegate.Broadcast(State);
}

void UTradeManager::OnTradeDeclined(const FTradeDeclinedData& Data)
{
    OnTradeDeclinedDelegate.Broadcast(Data);
}

void UTradeManager::OnTradeCancelled(const FTradeCancelledData& Data)
{
    ClearSession();
    OnTradeCancelledDelegate.Broadcast(Data);
}

void UTradeManager::OnTradeCompleted(const FTradeCompleteData& Data)
{
    ClearSession();
    OnTradeCompletedDelegate.Broadcast(Data);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UTradeManager::SendPacket(const FString& JsonPayload)
{
    if (NetworkManager)
    {
        NetworkManager->SendDataToChunkServer(JsonPayload);
    }
}

void UTradeManager::ClearSession()
{
    CurrentSessionId  = TEXT("");
    CurrentState      = FTradeStateData{};
    bHasPendingInvite = false;
    PendingInvite     = FTradeInviteData{};
}
