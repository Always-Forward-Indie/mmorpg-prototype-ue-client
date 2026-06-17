// Login Flow — Data types for registration, character creation/selection/deletion.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "LoginFlowTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Character creation option entries (received from server)
// ─────────────────────────────────────────────────────────────────────────────

/** Single class option from getCharacterCreationOptions. */
USTRUCT(BlueprintType)
struct FCharacterClassOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString Slug;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString Description;
};

/** Single race option from getCharacterCreationOptions. */
USTRUCT(BlueprintType)
struct FCharacterRaceOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString Slug;
};

/** Single gender option from getCharacterCreationOptions. */
USTRUCT(BlueprintType)
struct FCharacterGenderOption
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	int32 Id = 0;

	/** Sent to server in createCharacter (e.g. "male", "female"). */
	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString Name;

	/** Human-readable label (e.g. "Male", "Female"). */
	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString Label;
};

/** Aggregated creation options. */
USTRUCT(BlueprintType)
struct FCharacterCreationOptions
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	TArray<FCharacterClassOption> Classes;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	TArray<FCharacterRaceOption> Races;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	TArray<FCharacterGenderOption> Genders;

	bool IsValid() const { return Classes.Num() > 0 && Races.Num() > 0 && Genders.Num() > 0; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Character list entry (lightweight, for login screen)
// ─────────────────────────────────────────────────────────────────────────────

/** Single equipped item returned with the character list (for preview). */
USTRUCT(BlueprintType)
struct FLoginEquipmentEntry
{
	GENERATED_BODY()

	/** Equipment slot id (matches server equip_slot_id). */
	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	int32 SlotId = 0;

	/** Item slug (e.g. "wooden_staff"). */
	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString ItemSlug;
};

USTRUCT(BlueprintType)
struct FLoginCharacterEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	int32 CharacterId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString CharacterName;

	/** Class slug (e.g. "mage"). */
	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString CharacterClass;

	/** Race slug (e.g. "human"). */
	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString CharacterRace;

	/** Gender slug (e.g. "male"). */
	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	FString CharacterGender;

	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	int32 CharacterLevel = 0;

	/** Equipment currently worn — used to build the character-select preview. */
	UPROPERTY(BlueprintReadOnly, Category = "Login Flow")
	TArray<FLoginEquipmentEntry> Equipment;
};

// ─────────────────────────────────────────────────────────────────────────────
// Delegates — AuthenticationManager → UI
// ─────────────────────────────────────────────────────────────────────────────

/** Fired after registerAccount response. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRegisterResponse, bool, bSuccess, const FString&, Message);

/** Fired after authentificationClient response. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginResponse, bool, bSuccess, const FString&, Message);

/** Fired after getCharacterCreationOptions response. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterCreationOptionsReceived, const FCharacterCreationOptions&, Options);

/** Fired after getCharactersList response. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterListReceived, const TArray<FLoginCharacterEntry>&, Characters);

/** Fired after createCharacter response. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnCreateCharacterResponse, bool, bSuccess, const FString&, Message, int32, CharacterId);

/** Fired after deleteCharacter response. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnDeleteCharacterResponse, bool, bSuccess, const FString&, Message, int32, CharacterId);

/** Fired when server rejects client version (ERR_VERSION_OUTDATED / ERR_VERSION_TOO_NEW). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVersionMismatch, const FString&, ErrorCode);

/** Fired when server returns Unauthorized — UI must return to login. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSessionExpired);

// ─────────────────────────────────────────────────────────────────────────────
// Enum — panels inside LoginFlowWidget
// ─────────────────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ELoginFlowPanel : uint8
{
	Login          UMETA(DisplayName = "Login"),
	Registration   UMETA(DisplayName = "Registration"),
	CharacterSelect UMETA(DisplayName = "Character Select"),
	CharacterCreate UMETA(DisplayName = "Character Create")
};

// ─────────────────────────────────────────────────────────────────────────────
// Data Table row — server error code → localised UI message
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Each row maps one server ERR_* code to a localised FText.
 * Create a DataTable with this row struct and fill it in the Editor.
 * Row Name = exact server error code (e.g. "ERR_LOGIN_TAKEN").
 */
USTRUCT(BlueprintType)
struct FLoginErrorTableRow : public FTableRowBase
{
	GENERATED_BODY()

	/** Localised message shown to the player. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Login Flow")
	FText DisplayMessage;
};

// ─────────────────────────────────────────────────────────────────────────────
// Validation helpers (client-side, for UX only — server is authoritative)
// ─────────────────────────────────────────────────────────────────────────────

namespace LoginFlowValidation
{
	/** Login: 3–20 chars, [A-Za-z0-9_]. */
	inline bool IsLoginValid(const FString& Login, FText& OutError)
	{
		if (Login.IsEmpty())
		{
			OutError = NSLOCTEXT("LoginFlow", "LoginEmpty", "Login cannot be empty.");
			return false;
		}
		if (Login.Len() < 3)
		{
			OutError = NSLOCTEXT("LoginFlow", "LoginTooShort", "Login must be at least 3 characters.");
			return false;
		}
		if (Login.Len() > 20)
		{
			OutError = NSLOCTEXT("LoginFlow", "LoginTooLong", "Login must be 20 characters or fewer.");
			return false;
		}
		static const FRegexPattern LoginPattern(TEXT("^[A-Za-z0-9_]+$"));
		FRegexMatcher Matcher(LoginPattern, Login);
		if (!Matcher.FindNext())
		{
			OutError = NSLOCTEXT("LoginFlow", "LoginInvalidChars", "Login may only contain letters, numbers and underscore.");
			return false;
		}
		return true;
	}

	/** Password: 8–100 chars. */
	inline bool IsPasswordValid(const FString& Password, FText& OutError)
	{
		if (Password.IsEmpty())
		{
			OutError = NSLOCTEXT("LoginFlow", "PasswordEmpty", "Password cannot be empty.");
			return false;
		}
		if (Password.Len() < 8)
		{
			OutError = NSLOCTEXT("LoginFlow", "PasswordTooShort", "Password must be at least 8 characters.");
			return false;
		}
		if (Password.Len() > 100)
		{
			OutError = NSLOCTEXT("LoginFlow", "PasswordTooLong", "Password must be 100 characters or fewer.");
			return false;
		}
		return true;
	}

	/** Email: either empty (optional) or contains '@'. */
	inline bool IsEmailValid(const FString& Email, FText& OutError)
	{
		if (Email.IsEmpty()) return true;
		if (!Email.Contains(TEXT("@")))
		{
			OutError = NSLOCTEXT("LoginFlow", "EmailInvalid", "Invalid email address.");
			return false;
		}
		return true;
	}

	/** Character name: 2–20 chars, letters and spaces only. */
	inline bool IsCharacterNameValid(const FString& Name, FText& OutError)
	{
		if (Name.IsEmpty())
		{
			OutError = NSLOCTEXT("LoginFlow", "CharNameEmpty", "Character name cannot be empty.");
			return false;
		}
		if (Name.Len() < 2)
		{
			OutError = NSLOCTEXT("LoginFlow", "CharNameTooShort", "Character name must be at least 2 characters.");
			return false;
		}
		if (Name.Len() > 20)
		{
			OutError = NSLOCTEXT("LoginFlow", "CharNameTooLong", "Character name must be 20 characters or fewer.");
			return false;
		}
		static const FRegexPattern NamePattern(TEXT("^[A-Za-z ]+$"));
		FRegexMatcher Matcher(NamePattern, Name);
		if (!Matcher.FindNext())
		{
			OutError = NSLOCTEXT("LoginFlow", "CharNameInvalid", "Name may only contain letters and spaces.");
			return false;
		}
		return true;
	}
}
