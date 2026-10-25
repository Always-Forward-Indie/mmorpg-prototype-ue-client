// Fill out your copyright notice in the Description page of Project Settings.

#include "Authentication/AuthenticationManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Utils/NetCore/AuthValidation.h"

UAuthenticationManager::UAuthenticationManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// initialize the authentication manager
void UAuthenticationManager::Initialize(UNetworkManager* NetworkManager, UPingManager* PingManager)
{
	// Initialize the network manager
	networkManager = NetworkManager;

	// Initialize the ping manager
	pingManager = PingManager;

	// Get the game instance
	gameInstance = Cast<UMyGameInstance>(worldContext->GetGameInstance());

	if (gameInstance)
	{
		UE_LOG(LogConnection, Verbose, TEXT("GameInstance found"));
		
		// Set up PingManager with TimeSyncService integration
		if (pingManager)
		{
			pingManager->SetTimeSyncService(gameInstance->GetTimeSyncService());
			UE_LOG(LogConnection, Verbose, TEXT("AuthenticationManager: PingManager configured with TimeSyncService"));
		}
	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("GameInstance not found"));
	}

	// Subscribe to the network manager's event
	//SubscribeToNetworkManager();
}

//subscribe to network manager
void UAuthenticationManager::SubscribeToNetworkManager()
{
	// Subscribe to the network manager's event
	if (networkManager != nullptr)
	{
		UE_LOG(LogConnection, Verbose, TEXT("Network Manager found and subscribed to LoginServerResponse delegate"));

		if (IsValid(networkManager))
		{
			UE_LOG(LogConnection, Verbose, TEXT("Network Manager is valid"));
			networkManager->OnLoginServerDataReceived.RemoveDynamic(this, &UAuthenticationManager::ProcessLoginResponse);
			networkManager->OnLoginServerDataReceived.AddDynamic(this, &UAuthenticationManager::ProcessLoginResponse);
		}
		else
		{
			UE_LOG(LogConnection, Error, TEXT("Network Manager is not valid"));
		}

	}
	else
	{
		UE_LOG(LogConnection, Error, TEXT("Network Manager not found"));
	}
}

// set world context 
void UAuthenticationManager::SetWorldContext(UWorld* WorldContext)
{
	// Set the world context
	worldContext = WorldContext;
}

// ─────────────────────────────────────────────────────────────────────────────
// Request timeout helpers
// ─────────────────────────────────────────────────────────────────────────────

void UAuthenticationManager::StartRequestTimeout(FTimerHandle& Handle, void (UAuthenticationManager::* Callback)())
{
	UWorld* World = worldContext ? worldContext : (gameInstance ? gameInstance->GetWorld() : nullptr);
	if (!World) return;

	World->GetTimerManager().ClearTimer(Handle);
	World->GetTimerManager().SetTimer(Handle, this, Callback, RequestTimeoutSeconds, false);
}

void UAuthenticationManager::ClearRequestTimeout(FTimerHandle& Handle)
{
	UWorld* World = worldContext ? worldContext : (gameInstance ? gameInstance->GetWorld() : nullptr);
	if (!World) return;

	World->GetTimerManager().ClearTimer(Handle);
}

void UAuthenticationManager::OnLoginRequestTimeout()
{
	UE_LOG(LogConnection, Error, TEXT("AuthManager: Login request timed out after %.0fs — broadcasting error"), RequestTimeoutSeconds);
	OnLoginResponse.Broadcast(false, TEXT("ERR_TIMEOUT"));
}

void UAuthenticationManager::OnRegisterRequestTimeout()
{
	UE_LOG(LogConnection, Error, TEXT("AuthManager: Register request timed out after %.0fs — broadcasting error"), RequestTimeoutSeconds);
	OnRegisterResponse.Broadcast(false, TEXT("ERR_TIMEOUT"));
}



void UAuthenticationManager::FillAuthHeader(TMap<FString, TSharedPtr<FJsonValue>>& HeaderData) const
{
	if (!gameInstance) return;

	HeaderData.Add("clientId", MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID())));
	HeaderData.Add("hash",     MakeShareable(new FJsonValueString(gameInstance->GetCurrentClientHash())));
}

bool UAuthenticationManager::HandleUnauthorized(const FString& Status, const FString& Message, const FString& ErrorCode)
{
	if (Status == "error" && (Message == "Unauthorized" || ErrorCode == "Unauthorized"))
	{
		UE_LOG(LogConnection, Warning, TEXT("AuthManager: Session expired (Unauthorized)"));
		OnSessionExpired.Broadcast();
		return true;
	}
	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Validation (legacy — kept for backward compat)
// ─────────────────────────────────────────────────────────────────────────────

bool UAuthenticationManager::IsLoginValueValid(const FString& Login)
{
	// Predicate lives in FAuthValidation (pinned by MMO.Auth.Validation);
	// this wrapper keeps the legacy user-facing side effects 1:1.
	const EAuthValueError Error = FAuthValidation::ValidateLogin(Login);
	if (Error == EAuthValueError::Empty)
	{
		UE_LOG(LogConnection, Warning, TEXT("Login could not be empty!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login could not be empty!");
		}
		return false;
	}
	if (Error == EAuthValueError::TooShort)
	{
		UE_LOG(LogConnection, Warning, TEXT("Login is too short!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login is too short!");
		}
		return false;
	}
	if (Error == EAuthValueError::TooLong)
	{
		UE_LOG(LogConnection, Warning, TEXT("Login is too long!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Login is too long!");
		}
		return false;
	}
	return true;
}

bool UAuthenticationManager::IsPasswordValueValid(const FString& Password)
{
	// Predicate lives in FAuthValidation (pinned by MMO.Auth.Validation);
	// this wrapper keeps the legacy user-facing side effects 1:1.
	const EAuthValueError Error = FAuthValidation::ValidatePassword(Password);
	if (Error == EAuthValueError::Empty)
	{
		UE_LOG(LogConnection, Warning, TEXT("Password could not be empty!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password could not be empty!");
		}
		return false;
	}
	// FIXED: server expects 8–100 chars, not 3–20
	if (Error == EAuthValueError::TooShort)
	{
		UE_LOG(LogConnection, Warning, TEXT("Password is too short!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password must be at least 8 characters!");
		}
		return false;
	}
	if (Error == EAuthValueError::TooLong)
	{
		UE_LOG(LogConnection, Warning, TEXT("Password is too long!"));
		if (gameInstance)
		{
			gameInstance->OnLoginResponseReceived.Broadcast(gameInstance->GetCurrentClientID(), "Password must be 100 characters or fewer!");
		}
		return false;
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Outgoing requests
// ─────────────────────────────────────────────────────────────────────────────

