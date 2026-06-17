#include "UI/GraphicsSettingsWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PCGComponent.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"

static const TArray<TPair<FString, EWindowMode::Type>> WindowModeOptions = {
	{ TEXT("Fullscreen"),         EWindowMode::Fullscreen },
	{ TEXT("Borderless Window"),  EWindowMode::WindowedFullscreen },
	{ TEXT("Windowed"),           EWindowMode::Windowed },
};

static const TArray<TPair<FString, int32>> QualityPresetOptions = {
	{ TEXT("Low"),    0 },
	{ TEXT("Medium"), 1 },
	{ TEXT("High"),   2 },
	{ TEXT("Epic"),   3 },
};

static const TArray<FString> QualityDescriptions = {
	TEXT("Low — best performance, lowest visual fidelity"),
	TEXT("Medium — balanced performance and quality"),
	TEXT("High — good visual quality"),
	TEXT("Epic — maximum visual fidelity"),
};

void UGraphicsSettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bDelegatesBound) return;
	bDelegatesBound = true;

	if (Quality_Combo)
	{
		Quality_Combo->OnSelectionChanged.AddDynamic(this, &UGraphicsSettingsWidget::OnQualityChanged);
	}

	if (WindowMode_Combo)
	{
		WindowMode_Combo->OnSelectionChanged.AddDynamic(this, &UGraphicsSettingsWidget::OnWindowModeChanged);
	}

	if (Resolution_Combo)
	{
		Resolution_Combo->OnSelectionChanged.AddDynamic(this, &UGraphicsSettingsWidget::OnResolutionChanged);
	}

	if (Apply_Button)
	{
		Apply_Button->OnClicked.AddDynamic(this, &UGraphicsSettingsWidget::ApplySettings);
		Apply_Button->SetIsEnabled(false);
	}

	PopulateWindowModes();
	PopulateQualityPresets();
	PopulateResolutions();
	InitializeSettings();
}

void UGraphicsSettingsWidget::PopulateWindowModes()
{
	if (!WindowMode_Combo) return;

	WindowMode_Combo->ClearOptions();
	for (const auto& Option : WindowModeOptions)
	{
		WindowMode_Combo->AddOption(Option.Key);
	}
}

void UGraphicsSettingsWidget::PopulateQualityPresets()
{
	if (!Quality_Combo) return;

	Quality_Combo->ClearOptions();
	for (const auto& Option : QualityPresetOptions)
	{
		Quality_Combo->AddOption(Option.Key);
	}
}

void UGraphicsSettingsWidget::PopulateResolutions()
{
	if (!Resolution_Combo) return;

	Resolution_Combo->ClearOptions();

	TArray<FIntPoint> Resolutions;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

	if (Resolutions.Num() == 0)
	{
		Resolution_Combo->AddOption(TEXT("1920x1080"));
		Resolution_Combo->AddOption(TEXT("1280x720"));
		return;
	}

	for (const FIntPoint& Res : Resolutions)
	{
		const FString Option = FString::Printf(TEXT("%dx%d"), Res.X, Res.Y);
		Resolution_Combo->AddOption(Option);
	}
}

void UGraphicsSettingsWidget::InitializeSettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	EWindowMode::Type CurrentMode = Settings->GetFullscreenMode();
	int32 CurrentQuality = Settings->GetOverallScalabilityLevel();
	FIntPoint CurrentRes = Settings->GetScreenResolution();

	FString ModeStr;
	for (const auto& Option : WindowModeOptions)
	{
		if (Option.Value == CurrentMode)
		{
			ModeStr = Option.Key;
			break;
		}
	}
	if (ModeStr.IsEmpty()) ModeStr = WindowModeOptions[0].Key;

	FString QualityStr;
	for (const auto& Option : QualityPresetOptions)
	{
		if (Option.Value == CurrentQuality)
		{
			QualityStr = Option.Key;
			break;
		}
	}
	if (QualityStr.IsEmpty()) QualityStr = QualityPresetOptions[1].Key;

	FString ResStr = FString::Printf(TEXT("%dx%d"), CurrentRes.X, CurrentRes.Y);

	if (WindowMode_Combo) WindowMode_Combo->SetSelectedOption(ModeStr);
	if (Quality_Combo)    Quality_Combo->SetSelectedOption(QualityStr);
	if (Resolution_Combo) Resolution_Combo->SetSelectedOption(ResStr);

	PendingWindowMode = ModeStr;
	PendingQuality    = QualityStr;
	PendingResolution = ResStr;

	OnQualityChanged(QualityStr, ESelectInfo::Direct);
	UpdateApplyButtonState();
}

void UGraphicsSettingsWidget::OnQualityChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (Quality_Label)
	{
		for (int32 i = 0; i < QualityPresetOptions.Num(); ++i)
		{
			if (QualityPresetOptions[i].Key == SelectedItem && i < QualityDescriptions.Num())
			{
				Quality_Label->SetText(FText::FromString(QualityDescriptions[i]));
				break;
			}
		}
	}
	UpdateApplyButtonState();
}

void UGraphicsSettingsWidget::OnWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateApplyButtonState();
}

void UGraphicsSettingsWidget::OnResolutionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateApplyButtonState();
}

void UGraphicsSettingsWidget::UpdateApplyButtonState()
{
	if (!Apply_Button) return;
	if (!WindowMode_Combo || !Quality_Combo || !Resolution_Combo) return;

	const bool bChanged =
		(WindowMode_Combo->GetSelectedOption() != PendingWindowMode) ||
		(Quality_Combo->GetSelectedOption()    != PendingQuality) ||
		(Resolution_Combo->GetSelectedOption() != PendingResolution);

	Apply_Button->SetIsEnabled(bChanged);
}

void UGraphicsSettingsWidget::ApplySettings()
{
	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (!Settings) return;

	const FString SelectedMode  = WindowMode_Combo  ? WindowMode_Combo->GetSelectedOption()  : TEXT("");
	const FString SelectedQual  = Quality_Combo     ? Quality_Combo->GetSelectedOption()     : TEXT("");
	const FString SelectedRes   = Resolution_Combo  ? Resolution_Combo->GetSelectedOption()  : TEXT("");

	EWindowMode::Type NewMode = EWindowMode::WindowedFullscreen;
	for (const auto& Option : WindowModeOptions)
	{
		if (Option.Key == SelectedMode) { NewMode = Option.Value; break; }
	}
	Settings->SetFullscreenMode(NewMode);

	int32 NewQuality = 1;
	for (const auto& Option : QualityPresetOptions)
	{
		if (Option.Key == SelectedQual) { NewQuality = Option.Value; break; }
	}
	Settings->SetOverallScalabilityLevel(NewQuality);

	int32 ResX = 0, ResY = 0;
	FString ResCopy = SelectedRes;
	ResCopy.ReplaceInline(TEXT("x"), TEXT(" "));
	TArray<FString> Parts;
	ResCopy.ParseIntoArray(Parts, TEXT(" "));
	if (Parts.Num() >= 2)
	{
		ResX = FCString::Atoi(*Parts[0]);
		ResY = FCString::Atoi(*Parts[1]);
	}
	if (ResX > 0 && ResY > 0)
	{
		Settings->SetScreenResolution(FIntPoint(ResX, ResY));
	}

	Settings->ApplySettings(false);
	Settings->SaveSettings();

	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTimerHandle PCGRefreshHandle;
		World->GetTimerManager().SetTimer(PCGRefreshHandle, [WeakWorld]()
		{
			if (!WeakWorld.IsValid()) return;

			// Guard: skip PCG refresh if the game world isn't ready yet
			// (World Partition still streaming = landscape data may be invalid).
			if (UMyGameInstance* GI = Cast<UMyGameInstance>(WeakWorld->GetGameInstance()))
			{
				if (!GI->IsGameWorldReady())
				{
					return;
				}
			}

			for (TActorIterator<AActor> It(WeakWorld.Get()); It; ++It)
			{
				TArray<UPCGComponent*> PCGComps;
				It->GetComponents<UPCGComponent>(PCGComps);
				for (UPCGComponent* PCGComp : PCGComps)
				{
					if (IsValid(PCGComp))
					{
						PCGComp->GenerateLocal(EPCGComponentGenerationTrigger::GenerateAtRuntime, true);
					}
				}
			}
		}, 0.15f, false);
	}

	PendingWindowMode = SelectedMode;
	PendingQuality    = SelectedQual;
	PendingResolution = SelectedRes;

	UpdateApplyButtonState();
}

void UGraphicsSettingsWidget::CloseWidget()
{
	if (WindowMode_Combo) WindowMode_Combo->SetSelectedOption(PendingWindowMode);
	if (Quality_Combo)    Quality_Combo->SetSelectedOption(PendingQuality);
	if (Resolution_Combo) Resolution_Combo->SetSelectedOption(PendingResolution);

	UpdateApplyButtonState();
	SetVisibility(ESlateVisibility::Collapsed);
}
