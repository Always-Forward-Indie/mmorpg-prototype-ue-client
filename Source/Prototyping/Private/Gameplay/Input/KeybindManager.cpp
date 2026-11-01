#include "Gameplay/Input/KeybindManager.h"
#include "Prototyping.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "InputMappingContext.h"

#define LOCTEXT_NAMESPACE "Keybinds"

void UKeybindManager::Initialize(UMyGameInstance* InGameInstance)
{
	GameInstance = InGameInstance;
}

FString UKeybindManager::ActionKey(const UInputAction* Action)
{
	return Action ? Action->GetFName().ToString() : FString();
}

TArray<FKeybindActionEntry> UKeybindManager::CollectActions(ABasicPlayer* Player) const
{
	TArray<FKeybindActionEntry> Out;
	if (Player)
		Player->GetRemappableActions(Out);
	return Out;
}

FKey UKeybindManager::GetPrimaryKey(UInputMappingContext* IMC, const UInputAction* Action) const
{
	if (!IMC || !Action) return FKey();
	const TArray<FEnhancedActionKeyMapping>& Maps = IMC->GetMappings();
	for (FEnhancedActionKeyMapping M : Maps)
	{
		if (M.Action == Action)
			return M.Key;
	}
	return FKey();
}

bool UKeybindManager::RemapAction(UInputMappingContext* IMC, const UInputAction* Action, const FKey& NewKey)
{
	if (!IMC || !Action || !NewKey.IsValid()) return false;

	// Reject duplicates (another action already owns the key).
	for (FEnhancedActionKeyMapping M : IMC->GetMappings())
	{
		if (M.Action != Action && M.Key == NewKey)
		{
			if (GameInstance && GameInstance->AudioManager)
				GameInstance->AudioManager->PlayUISound(EUISoundEvent::ErrorSound);
			return false;
		}
	}

	const FString Name = ActionKey(Action);
	if (!DefaultKeys.Contains(Name))
		DefaultKeys.Add(Name, GetPrimaryKey(IMC, Action));

	// Replace the primary binding (index 0); alternates are preserved.
	const TArray<FEnhancedActionKeyMapping>& Maps = IMC->GetMappings();
	bool bReplaced = false;
	for (int32 i = 0; i < Maps.Num(); ++i)
	{
		if (Maps[i].Action == Action)
		{
			IMC->UnmapKey(Action, Maps[i].Key);
			IMC->MapKey(Action, NewKey);
			bReplaced = true;
			break;
		}
	}
	if (!bReplaced)
		IMC->MapKey(Action, NewKey);

	RemappedKeys.Add(Name, NewKey.ToString());
	SaveConfig();
	return true;
}

void UKeybindManager::ResetAction(UInputMappingContext* IMC, const UInputAction* Action)
{
	if (!IMC || !Action) return;
	const FString Name = ActionKey(Action);
	RemappedKeys.Remove(Name);

	const FKey Default = DefaultKeys.FindRef(Name);
	const FKey Current = GetPrimaryKey(IMC, Action);
	if (Current.IsValid())
		IMC->UnmapKey(Action, Current);
	if (Default.IsValid())
		IMC->MapKey(Action, Default);
	SaveConfig();
}

void UKeybindManager::ResetAll(UInputMappingContext* IMC)
{
	if (!IMC) return;
	TArray<FString> Names;
	RemappedKeys.GetKeys(Names);
	for (const FString& Name : Names)
	{
		for (FEnhancedActionKeyMapping M : IMC->GetMappings())
		{
			if (M.Action && ActionKey(M.Action.Get()) == Name)
			{
				ResetAction(IMC, M.Action.Get());
				break;
			}
		}
	}
	RemappedKeys.Empty();
	SaveConfig();
}

void UKeybindManager::ApplySavedBindings(UInputMappingContext* IMC)
{
	if (!IMC) return;
	for (const auto& Pair : RemappedKeys)
	{
		const FKey NewKey(*Pair.Value);
		if (!NewKey.IsValid()) continue;
		for (FEnhancedActionKeyMapping M : IMC->GetMappings())
		{
			if (M.Action && ActionKey(M.Action.Get()) == Pair.Key)
			{
				if (M.Key != NewKey)
				{
					if (!DefaultKeys.Contains(Pair.Key))
						DefaultKeys.Add(Pair.Key, M.Key);
					IMC->UnmapKey(M.Action.Get(), M.Key);
					IMC->MapKey(M.Action.Get(), NewKey);
				}
				break;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
