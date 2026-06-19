// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Utils/JSONParser.h"
#include "Networking/NetworkManager.h"
#include "Networking/PingManager.h"
#include "Authentication/LoginFlowTypes.h"
#include <Kismet/GameplayStatics.h>

#include "AuthenticationManager.generated.h"

class UMyGameInstance;

/**
 * Authentication & login-flow manager.
 *
 * Handles login, registration, character list, character creation / deletion,
 * and character-creation-options network requests.
 * Broadcasts typed delegates so the UI (LoginFlowWidget) can react.
 */
UCLASS()
class PROTOTYPING_API UAuthenticationManager : public UObject
{
	GENERATED_BODY()
private:
	UPROPERTY()
	UWorld* worldContext = nullptr;

	UPROPERTY()
	UPingManager* pingManager = nullptr;

	UPROPERTY()
	UNetworkManager* networkManager = nullptr;

	UPROPERTY()
	UMyGameInstance* gameInstance = nullptr;

	// ── Internal helpers ─────────────────────────────────────────────────────

	/** Build header JSON entries with clientId + hash from GameInstance. */
	void FillAuthHeader(TMap<FString, TSharedPtr<FJsonValue>>& HeaderData) const;

	/** Check for "Unauthorized" and broadcast OnSessionExpired if matched. Returns true if unauthorized. */
	bool HandleUnauthorized(const FString& Status, const FString& Message);

	// ── Request timeout timers ────────────────────────────────────────────────

	static constexpr float RequestTimeoutSeconds = 10.0f;

	FTimerHandle LoginRequestTimeoutHandle;
	FTimerHandle RegisterRequestTimeoutHandle;

	UFUNCTION()
	void OnLoginRequestTimeout();

	UFUNCTION()
	void OnRegisterRequestTimeout();

	/** Start/clear a timeout timer using worldContext's TimerManager. */
	void StartRequestTimeout(FTimerHandle& Handle, void (UAuthenticationManager::* Callback)());
	void ClearRequestTimeout(FTimerHandle& Handle);

public:
	UAuthenticationManager(const FObjectInitializer& ObjectInitializer);
	void Initialize(UNetworkManager* NetworkManager, UPingManager* PingManager);
	void SubscribeToNetworkManager();
	void SetWorldContext(UWorld* World);

	// ── Outgoing requests ────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
	void SendLoginRequest(const FString& Username, const FString& Password);

	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
	void SendRegisterRequest(const FString& Username, const FString& Password, const FString& Email);

	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
	void SendCharacterCreationOptionsRequest();

	void SendCharacterListRequest(FClientDataStruct& ClientData);

	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
	void SendCreateCharacterRequest(const FString& CharacterName, const FString& CharacterClass,
	                                 const FString& CharacterRace, const FString& CharacterGender);

	UFUNCTION(BlueprintCallable, Category = "Network|Auth")
	void SendDeleteCharacterRequest(int32 CharacterId);

	void SendLeaveGameRequest(FClientDataStruct& ClientData);

	// ── Incoming response processing ─────────────────────────────────────────

	UFUNCTION()
	void ProcessLoginResponse(const FString& ReceivedData);

	// ── Validation (legacy — kept for API compat, but LoginFlowWidget uses LoginFlowTypes) ──
	bool IsLoginValueValid(const FString& Login);
	bool IsPasswordValueValid(const FString& Password);

	// ── Ping ─────────────────────────────────────────────────────────────────
	void StartPing();

	// ── Delegates (AuthManager → UI) ─────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "Network|Auth")
	FOnLoginResponse OnLoginResponse;

	UPROPERTY(BlueprintAssignable, Category = "Network|Auth")
	FOnRegisterResponse OnRegisterResponse;

	UPROPERTY(BlueprintAssignable, Category = "Network|Auth")
	FOnCharacterCreationOptionsReceived OnCharacterCreationOptionsReceived;

	UPROPERTY(BlueprintAssignable, Category = "Network|Auth")
	FOnCharacterListReceived OnCharacterListReceived;

	UPROPERTY(BlueprintAssignable, Category = "Network|Auth")
	FOnCreateCharacterResponse OnCreateCharacterResponse;

	UPROPERTY(BlueprintAssignable, Category = "Network|Auth")
	FOnDeleteCharacterResponse OnDeleteCharacterResponse;

	UPROPERTY(BlueprintAssignable, Category = "Network|Auth")
	FOnSessionExpired OnSessionExpired;

	UPROPERTY(BlueprintAssignable, Category = "Network|Auth")
	FOnVersionMismatch OnVersionMismatch;
};
