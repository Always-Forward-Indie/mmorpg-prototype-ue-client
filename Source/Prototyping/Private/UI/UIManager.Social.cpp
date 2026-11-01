#include "UI/UIManager.h"
#include "Prototyping.h"
#include "UI/PlayerContextMenuWidget.h"
#include "UI/PlayerInspectWidget.h"
#include "UI/ChatWidget.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Social/PlayerInspectManager.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Audio/AudioManager.h"
#include "MyGameInstance.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UUIManager::OpenPlayerContextMenu(ABasicPlayer* TargetPlayer)
{
	if (!TargetPlayer || !GetWorld() || !GetWorld()->GetFirstPlayerController())
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: OpenPlayerContextMenu aborted - null target/world/PC"));
		return;
	}

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld()->GetGameInstance());
	ABasicPlayer* Local = GI ? GI->Player : nullptr;
	const FClientDataStruct TargetData = TargetPlayer->GetPlayerData();
	const int32 TargetId = TargetData.characterData.characterId;
	const bool bIsSelf = Local && Local->GetPlayerCharacterID() == TargetId;
	const bool bLocalDead = Local && Local->GetIsDead();
	const bool bTargetDead = TargetData.characterData.bIsDead;
	const FString TargetName = TargetData.characterData.characterName;
	// Diagnostic (user report: no menu on RMB): log every gate input.
	UE_LOG(LogGameplay, Warning, TEXT("UIManager: OpenPlayerContextMenu target=%d self=%d localDead=%d targetDead=%d name='%s'"),
		TargetId, bIsSelf ? 1 : 0, bLocalDead ? 1 : 0, bTargetDead ? 1 : 0, *TargetName);
	if (!FPlayerSocialRule::CanOpenMenu(bLocalDead, TargetId > 0, bIsSelf, bTargetDead))
	{
		UE_LOG(LogGameplay, Warning, TEXT("UIManager: unit menu blocked by CanOpenMenu gate"));
		return;
	}

	if (!PlayerContextMenu)
	{
		// Blueprint subclass wins when assigned (binds its named rows),
		// otherwise the code-built fallback is used (headless-safe).
		TSubclassOf<UPlayerContextMenuWidget> MenuClass =
			PlayerMenuWidgetClass ? *PlayerMenuWidgetClass : UPlayerContextMenuWidget::StaticClass();
		PlayerContextMenu = CreateWidget<UPlayerContextMenuWidget>(GetWorld(), MenuClass);
		if (PlayerContextMenu)
		{
			PlayerContextMenu->AddToViewport(1100);
			PlayerContextMenu->SetVisibility(ESlateVisibility::Collapsed);
			PlayerContextMenu->OnActionSelected.AddDynamic(this, &UUIManager::HandlePlayerSocialAction);
		}
	}
	if (!PlayerContextMenu) return;

	const FString Name = TargetData.characterData.characterName;
	const int32 Level = TargetData.characterData.characterLevel;
	const FString Display = Level > 0
		? FString::Printf(TEXT("%s  [%d]"), *Name, Level)
		: Name;

	LastSocialTargetId = TargetId;
	LastSocialTargetName = Name;

	FVector2D Mouse = FVector2D::ZeroVector;
	GetWorld()->GetFirstPlayerController()->GetMousePosition(Mouse.X, Mouse.Y);
	PlayerContextMenu->ShowForPlayer(TargetId, Display, Mouse);
	UpdateCursorAndInputMode();
}

void UUIManager::ClosePlayerMenu()
{
	if (PlayerContextMenu)
		PlayerContextMenu->HideMenu();
}

bool UUIManager::IsPlayerMenuOpen() const
{
	return PlayerContextMenu && PlayerContextMenu->IsMenuOpen();
}

void UUIManager::HandlePlayerSocialAction(EPlayerSocialAction Action, int32 TargetCharacterId)
{
	UMyGameInstance* GI = GetWorld() ? Cast<UMyGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!GI) return;

	switch (Action)
	{
	case EPlayerSocialAction::Whisper:
	{
		FString Name = (TargetCharacterId == LastSocialTargetId) ? LastSocialTargetName : FString();
		if (Name.IsEmpty())
		{
			if (ABasicPlayer* P = GI->GetPlayerByCharacterId(TargetCharacterId))
				Name = P->GetPlayerData().characterData.characterName;
		}
		if (!Name.IsEmpty() && ChatWidget)
			ChatWidget->StartWhisperTo(Name);
		break;
	}
	case EPlayerSocialAction::Trade:
	{
		UTradeManager* Trade = GI->GetTradeManager();
		ABasicPlayer* Local = GI->Player;
		ABasicPlayer* Target = GI->GetPlayerByCharacterId(TargetCharacterId);
		if (!Trade || !Local || !Target) break;
		const float Dist = FVector::Dist(Local->GetActorLocation(), Target->GetActorLocation());
		if (!FPlayerSocialRule::CanRequestTrade(Dist))
		{
			UE_LOG(LogGameplay, Warning, TEXT("UIManager: trade target too far (%.0fcm)"), Dist);
			if (GI->AudioManager)
				GI->AudioManager->PlayUISound(EUISoundEvent::ErrorSound);
			break;
		}
		Trade->RequestTrade(Local->GetPlayerCharacterID(), TargetCharacterId, Local->GetActorLocation());
		break;
	}
	case EPlayerSocialAction::Inspect:
	{
		UPlayerInspectManager* Inspect = GI->GetPlayerInspectManager();
		ABasicPlayer* Target = GI->GetPlayerByCharacterId(TargetCharacterId);
		if (!Inspect || !Target) break;
		EnsureInspectWidget();
		Inspect->OnInspectReceivedDelegate.RemoveDynamic(this, &UUIManager::HandlePlayerInspectReceived);
		Inspect->OnInspectErrorDelegate.RemoveDynamic(this, &UUIManager::HandlePlayerInspectError);
		Inspect->OnInspectReceivedDelegate.AddDynamic(this, &UUIManager::HandlePlayerInspectReceived);
		Inspect->OnInspectErrorDelegate.AddDynamic(this, &UUIManager::HandlePlayerInspectError);
		Inspect->RequestInspect(TargetCharacterId);
		break;
	}
	}
}

void UUIManager::EnsureInspectWidget()
{
	if (PlayerInspectWidget || !GetWorld()) return;
	TSubclassOf<UPlayerInspectWidget> InspectClass =
		PlayerInspectWidgetClass ? *PlayerInspectWidgetClass : UPlayerInspectWidget::StaticClass();
	PlayerInspectWidget = CreateWidget<UPlayerInspectWidget>(GetWorld(), InspectClass);
	if (PlayerInspectWidget)
	{
		PlayerInspectWidget->AddToViewport(16);
		PlayerInspectWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UUIManager::HandlePlayerInspectReceived(const FPlayerInspectData& Data)
{
	EnsureInspectWidget();
	if (PlayerInspectWidget)
		PlayerInspectWidget->ShowInspect(Data);
}

void UUIManager::HandlePlayerInspectError(const FString& Reason)
{
	UE_LOG(LogGameplay, Warning, TEXT("UIManager: inspect failed (%s)"), *Reason);
}

void UUIManager::HandleChatSenderClicked(int32 SenderId, const FString& SenderName)
{
	UMyGameInstance* GI = GetWorld() ? Cast<UMyGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
	if (!GI || SenderId <= 0) return;
	// Own lines are not clickable targets.
	if (GI->Player && GI->Player->GetPlayerCharacterID() == SenderId) return;

	// Online player in our world → unit menu; otherwise straight to whisper.
	if (ABasicPlayer* Target = GI->GetPlayerByCharacterId(SenderId))
	{
		OpenPlayerContextMenu(Target);
	}
	else if (!SenderName.IsEmpty() && ChatWidget)
	{
		ChatWidget->StartWhisperTo(SenderName);
	}
}
