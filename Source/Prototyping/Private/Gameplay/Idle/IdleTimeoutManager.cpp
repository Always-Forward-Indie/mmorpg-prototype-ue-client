#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "MyGameInstance.h"
#include "UI/UIManager.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"

class FIdleTimeoutInputProcessor : public IInputProcessor
{
public:
	explicit FIdleTimeoutInputProcessor(UIdleTimeoutManager* InOwner)
		: Owner(InOwner)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		Owner->ResetLastActivityTime();
		return false;
	}

	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetCursorDelta().SizeSquared() > 0.0f)
		{
			Owner->ResetLastActivityTime();
		}
		return false;
	}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		Owner->ResetLastActivityTime();
		return false;
	}

	virtual bool HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent) override
	{
		Owner->ResetLastActivityTime();
		return false;
	}

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override
	{
		Owner->ResetLastActivityTime();
		return false;
	}

private:
	UIdleTimeoutManager* Owner;
};

void UIdleTimeoutManager::Configure(UMyGameInstance* InGameInstance, float InIdleTimeoutSeconds, float InIdleWarningSeconds)
{
	GameInstance = InGameInstance;
	IdleTimeoutSeconds = InIdleTimeoutSeconds;
	IdleWarningSeconds = InIdleWarningSeconds;
}

void UIdleTimeoutManager::StartTracking(APlayerController* InPC, UUIManager* InUIManager)
{
	if (bIsTracking)
	{
		return;
	}
	if (!InPC || !InUIManager)
	{
		return;
	}

	UIManagerRef = InUIManager;
	LastActivityTime = FPlatformTime::Seconds();
	bWarningShown = false;
	bIsTracking = true;

	InputProcessor = MakeShareable(new FIdleTimeoutInputProcessor(this));
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);

	UWorld* World = InPC->GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(CheckTimer, this, &UIdleTimeoutManager::OnCheckTimer, 1.0f, true);
	}
}

void UIdleTimeoutManager::StopTracking()
{
	if (!bIsTracking)
	{
		return;
	}

	bIsTracking = false;

	if (InputProcessor.IsValid())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		InputProcessor.Reset();
	}

	if (UWorld* World = GameInstance.IsValid() ? GameInstance->GetWorld() : nullptr)
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}

	if (bWarningShown && UIManagerRef.IsValid())
	{
		UIManagerRef->HideIdleWarning();
	}
	bWarningShown = false;
}

void UIdleTimeoutManager::ResetLastActivityTime()
{
	if (!bIsTracking)
	{
		return;
	}
	LastActivityTime = FPlatformTime::Seconds();

	if (bWarningShown && UIManagerRef.IsValid())
	{
		UIManagerRef->HideIdleWarning();
		bWarningShown = false;
	}
}

void UIdleTimeoutManager::OnCheckTimer()
{
	if (!bIsTracking || !GameInstance.IsValid())
	{
		return;
	}

	const double Elapsed = FPlatformTime::Seconds() - LastActivityTime;

	if (Elapsed >= IdleTimeoutSeconds)
	{
		StopTracking();
		GameInstance->ReturnToLoginLevel();
		return;
	}

	if (IdleWarningSeconds > 0.0f && Elapsed >= IdleTimeoutSeconds - IdleWarningSeconds)
	{
		const int32 SecondsRemaining = FMath::CeilToInt32(IdleTimeoutSeconds - Elapsed);

		if (!bWarningShown && UIManagerRef.IsValid())
		{
			UIManagerRef->ShowIdleWarning(SecondsRemaining);
			bWarningShown = true;
		}
		else if (bWarningShown && UIManagerRef.IsValid())
		{
			UIManagerRef->UpdateIdleCountdown(SecondsRemaining);
		}
	}
}
