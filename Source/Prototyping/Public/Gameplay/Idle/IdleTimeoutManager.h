#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Framework/Application/IInputProcessor.h"
#include "IdleTimeoutManager.generated.h"

class UMyGameInstance;
class UUIManager;
class APlayerController;

UCLASS()
class PROTOTYPING_API UIdleTimeoutManager : public UObject
{
	GENERATED_BODY()

public:
	void Configure(UMyGameInstance* InGameInstance, float InIdleTimeoutSeconds, float InIdleWarningSeconds);

	void StartTracking(APlayerController* InPC, UUIManager* InUIManager);
	void StopTracking();
	void ResetLastActivityTime();

private:
	void OnCheckTimer();

	float IdleTimeoutSeconds = 300.0f;
	float IdleWarningSeconds = 60.0f;

	TSharedPtr<IInputProcessor> InputProcessor;
	FTimerHandle CheckTimer;
	double LastActivityTime = 0.0;
	bool bWarningShown = false;
	bool bIsTracking = false;

	TWeakObjectPtr<UMyGameInstance> GameInstance;
	TWeakObjectPtr<UUIManager> UIManagerRef;
};
