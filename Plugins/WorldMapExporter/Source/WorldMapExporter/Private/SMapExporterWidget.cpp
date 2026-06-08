#include "SMapExporterWidget.h"

#include "Editor.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"

#include "IDesktopPlatform.h"
#include "DesktopPlatformModule.h"

#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Engine/Light.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PrimitiveComponent.h"

#include "HAL/IConsoleManager.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHelpers.h"
#include "WorldPartition/WorldPartitionActorDescInstance.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "SMapExporterWidget"

// ─────────────────────────────────────────────────────────────────────────────
//  Construct
// ─────────────────────────────────────────────────────────────────────────────

void SMapExporterWidget::Construct(const FArguments& InArgs)
{
    // Texture size dropdown options
    TextureSizeOptions.Add(MakeShared<int32>(1024));
    TextureSizeOptions.Add(MakeShared<int32>(2048));
    TextureSizeOptions.Add(MakeShared<int32>(4096));
    TextureSizeOptions.Add(MakeShared<int32>(8192));
    SelectedTextureSize = TextureSizeOptions[2]; // default 4096

    const FMargin RowPadding(8.f, 4.f);
    const FMargin LabelPadding(0.f, 0.f, 8.f, 0.f);
    const float LabelWidth = 90.f;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(12.f)
        [
            SNew(SScrollBox)

            // ── Header ────────────────────────────────────────────────────────
            + SScrollBox::Slot().Padding(0.f, 0.f, 0.f, 8.f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("Header", "World Map Exporter"))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
            ]

            + SScrollBox::Slot().Padding(0.f, 0.f, 0.f, 10.f)
            [
                SNew(SSeparator)
            ]

            // ── Output Path ───────────────────────────────────────────────────
            + SScrollBox::Slot().Padding(0.f, 2.f)
            [
                MakeInputRow(
                    LOCTEXT("OutputPathLabel", "Output Dir"),
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1.f)
                    [
                        SAssignNew(OutputPathBox, SEditableTextBox)
                        .HintText(LOCTEXT("OutputHint", "D:/webeditor/public/map"))
                        .MinDesiredWidth(200.f)
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f, 0.f, 0.f)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("BrowseBtn", "..."))
                        .OnClicked_Lambda([this]() -> FReply
                        {
                            OnBrowseOutputPath();
                            return FReply::Handled();
                        })
                    ]
                )
            ]

            // ── Section: World Bounds + Auto-detect checkbox ────────────────────────
            + SScrollBox::Slot().Padding(0.f, 10.f, 0.f, 4.f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("BoundsHeader", "World Bounds  (UE units)"))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(14.f, 0.f, 0.f, 0.f)
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this]() -> ECheckBoxState
                    {
                        return bAutoDetectBounds ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                    })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
                    {
                        bAutoDetectBounds = (State == ECheckBoxState::Checked);
                    })
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("AutoDetectLabel", "Auto-detect from scene"))
                    ]
                ]
            ]

            + SScrollBox::Slot().Padding(0.f, 2.f)
            [
                MakeInputRow(
                    LOCTEXT("MinXLabel", "Min X"),
                    SAssignNew(MinXBox, SEditableTextBox)
                    .HintText(LOCTEXT("MinXHint", "-50000"))
                    .Text(FText::FromString(TEXT("-50000")))
                    .IsEnabled_Lambda([this]() { return !bAutoDetectBounds; })
                )
            ]
            + SScrollBox::Slot().Padding(0.f, 2.f)
            [
                MakeInputRow(
                    LOCTEXT("MaxXLabel", "Max X"),
                    SAssignNew(MaxXBox, SEditableTextBox)
                    .HintText(LOCTEXT("MaxXHint", "50000"))
                    .Text(FText::FromString(TEXT("50000")))
                    .IsEnabled_Lambda([this]() { return !bAutoDetectBounds; })
                )
            ]
            + SScrollBox::Slot().Padding(0.f, 2.f)
            [
                MakeInputRow(
                    LOCTEXT("MinYLabel", "Min Y"),
                    SAssignNew(MinYBox, SEditableTextBox)
                    .HintText(LOCTEXT("MinYHint", "-50000"))
                    .Text(FText::FromString(TEXT("-50000")))
                    .IsEnabled_Lambda([this]() { return !bAutoDetectBounds; })
                )
            ]
            + SScrollBox::Slot().Padding(0.f, 2.f)
            [
                MakeInputRow(
                    LOCTEXT("MaxYLabel", "Max Y"),
                    SAssignNew(MaxYBox, SEditableTextBox)
                    .HintText(LOCTEXT("MaxYHint", "50000"))
                    .Text(FText::FromString(TEXT("50000")))
                    .IsEnabled_Lambda([this]() { return !bAutoDetectBounds; })
                )
            ]

            // ── Texture Size ──────────────────────────────────────────────────
            + SScrollBox::Slot().Padding(0.f, 10.f, 0.f, 2.f)
            [
                MakeInputRow(
                    LOCTEXT("TextureSizeLabel", "Texture Size"),
                    SAssignNew(TextureSizeCombo, SComboBox<TSharedPtr<int32>>)
                    .OptionsSource(&TextureSizeOptions)
                    .InitiallySelectedItem(SelectedTextureSize)
                    .OnSelectionChanged_Lambda([this](TSharedPtr<int32> Item, ESelectInfo::Type)
                    {
                        SelectedTextureSize = Item;
                    })
                    .OnGenerateWidget_Lambda([](TSharedPtr<int32> Item) -> TSharedRef<SWidget>
                    {
                        return SNew(STextBlock).Text(FText::AsNumber(*Item));
                    })
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() -> FText
                        {
                            return SelectedTextureSize
                                ? FText::AsNumber(*SelectedTextureSize)
                                : FText::GetEmpty();
                        })
                    ]
                )
            ]

            // ── Unlit mode ───────────────────────────────────────────────────
            + SScrollBox::Slot().Padding(0.f, 8.f, 0.f, 0.f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SCheckBox)
                    .IsChecked_Lambda([this]() -> ECheckBoxState
                    {
                        return bUnlitMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                    })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
                    {
                        bUnlitMode = (State == ECheckBoxState::Checked);
                    })
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("UnlitLabel", "Unlit mode  (flat mesh colours, no lighting)"))
                    ]
                ]
            ]

            // ── Spacer ────────────────────────────────────────────────────────
            + SScrollBox::Slot().Padding(0.f, 8.f, 0.f, 0.f)
            [
                SNew(SSeparator)
            ]

            // ── Export Button ─────────────────────────────────────────────────
            + SScrollBox::Slot().Padding(0.f, 10.f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1.f)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "PrimaryButton")
                    .ContentPadding(FMargin(24.f, 6.f))
                    .Text(LOCTEXT("ExportBtn", "Export World Map"))
                    .OnClicked(this, &SMapExporterWidget::OnExportClicked)
                ]
                + SHorizontalBox::Slot().FillWidth(1.f)
            ]

            // ── Status ────────────────────────────────────────────────────────
            + SScrollBox::Slot().Padding(0.f, 4.f)
            [
                SNew(SBox).MinDesiredHeight(20.f)
                [
                    SAssignNew(StatusText, STextBlock)
                    .Text(LOCTEXT("StatusReady", "Ready."))
                    .AutoWrapText(true)
                ]
            ]

            // ── Info note ─────────────────────────────────────────────────────
            + SScrollBox::Slot().Padding(0.f, 8.f, 0.f, 0.f)
            [
                SNew(SSeparator)
            ]
            + SScrollBox::Slot().Padding(0.f, 6.f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("InfoNote",
                    "Outputs worldmap.png + worldmap_meta.json.\n"
                    "flip_y=true in metadata: the web editor must vertically flip\n"
                    "pixel coords (UE5 X+ = North/up, Y+ = East/right)."))
                .Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
                .AutoWrapText(true)
                .ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)))
            ]
        ]
    ];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Layout helper
// ─────────────────────────────────────────────────────────────────────────────

TSharedRef<SWidget> SMapExporterWidget::MakeInputRow(const FText& Label, TSharedRef<SWidget> Input)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
          .AutoWidth()
          .VAlign(VAlign_Center)
          .Padding(0.f, 0.f, 8.f, 0.f)
        [
            SNew(SBox).WidthOverride(90.f)
            [
                SNew(STextBlock)
                .Text(Label)
            ]
        ]
        + SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
        [
            Input
        ];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Browse folder dialog
// ─────────────────────────────────────────────────────────────────────────────

void SMapExporterWidget::OnBrowseOutputPath()
{
    IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
    if (!DesktopPlatform)
    {
        return;
    }

    FString FolderPath;
    const void* ParentWindow = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());

    if (DesktopPlatform->OpenDirectoryDialog(
            ParentWindow,
            TEXT("Select Output Folder"),
            OutputPathBox->GetText().ToString(),
            FolderPath))
    {
        OutputPathBox->SetText(FText::FromString(FolderPath));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Export button callback
// ─────────────────────────────────────────────────────────────────────────────

FReply SMapExporterWidget::OnExportClicked()
{
    const FString OutputPath = OutputPathBox->GetText().ToString().TrimStartAndEnd();
    if (OutputPath.IsEmpty())
    {
        SetStatus(TEXT("Error: output path is empty."), true);
        return FReply::Handled();
    }

    float MinX = 0.f, MaxX = 0.f, MinY = 0.f, MaxY = 0.f;

    if (bAutoDetectBounds)
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        FString BoundsError;
        float DummyMaxZ = 0.f;
        if (!ComputeWorldBounds(World, MinX, MaxX, MinY, MaxY, DummyMaxZ, BoundsError))
        {
            SetStatus(BoundsError, true);
            return FReply::Handled();
        }
        // Show what was detected so the user can review / copy it
        MinXBox->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), MinX)));
        MaxXBox->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), MaxX)));
        MinYBox->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), MinY)));
        MaxYBox->SetText(FText::FromString(FString::Printf(TEXT("%.0f"), MaxY)));
    }
    else
    {
        MinX = GetFloat(MinXBox);
        MaxX = GetFloat(MaxXBox);
        MinY = GetFloat(MinYBox);
        MaxY = GetFloat(MaxYBox);
    }

    if (MaxX <= MinX || MaxY <= MinY)
    {
        SetStatus(TEXT("Error: invalid bounds — Max must be greater than Min."), true);
        return FReply::Handled();
    }

    if (!SelectedTextureSize)
    {
        SetStatus(TEXT("Error: no texture size selected."), true);
        return FReply::Handled();
    }

    if (GEditor && GEditor->IsPlayingSessionInEditor())
    {
        SetStatus(TEXT("Error: stop the PIE session before exporting."), true);
        return FReply::Handled();
    }

    SetStatus(TEXT("Capturing — please wait..."));

    FString Error;
    const bool bOk = DoExport(OutputPath, MinX, MaxX, MinY, MaxY, *SelectedTextureSize, bUnlitMode, Error);

    if (bOk)
    {
        SetStatus(FString::Printf(TEXT("Done! Check Output Log ([WME]) for diagnostics. Files: %s"), *OutputPath));
    }
    else
    {
        SetStatus(Error, true);
    }

    return FReply::Handled();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Core export logic
// ─────────────────────────────────────────────────────────────────────────────

bool SMapExporterWidget::DoExport(
    const FString& OutputPath,
    float MinX, float MaxX,
    float MinY, float MaxY,
    int32 TextureSize,
    bool  bUnlit,
    FString& OutError)
{
    // ── 1. Get the editor world ───────────────────────────────────────────────
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        OutError = TEXT("Error: failed to get editor world.");
        return false;
    }

    if (!World->Scene)
    {
        OutError = TEXT("Error: editor world scene is not initialized. Open a level first.");
        return false;
    }

    // ── 0. Force-load all World Partition cells ─────────────────────────────
    if (UWorldPartition* WorldPartition = World->GetWorldPartition())
    {
        UE_LOG(LogTemp, Log, TEXT("[WME] World Partition detected — loading all cells..."));
        FWorldPartitionHelpers::FForEachActorWithLoadingParams LoadParams;
        FWorldPartitionHelpers::FForEachActorWithLoadingResult LoadResult;
        FWorldPartitionHelpers::ForEachActorWithLoading(
            WorldPartition,
            [](const FWorldPartitionActorDescInstance*) { return true; },
            LoadParams,
            LoadResult);
        FlushRenderingCommands();
        UE_LOG(LogTemp, Log, TEXT("[WME] World Partition loading complete. %d actors loaded (0 = all in persistent level)."),
            LoadResult.ActorReferences.Num());
    }

    // ── 0b. Force-generate all PCG volumes ───────────────────────────────────
    {
        UClass* PCGComponentClass = FindObject<UClass>(nullptr, TEXT("/Script/PCG.PCGComponent"));
        if (PCGComponentClass)
        {
            int32 PCGGenerated = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                TArray<UActorComponent*> Comps;
                It->GetComponents(PCGComponentClass, Comps);
                for (UActorComponent* Comp : Comps)
                {
                    UFunction* GenFunc = Comp->FindFunction(FName("Generate"));
                    if (GenFunc)
                    {
                        uint8* Params = GenFunc->ParmsSize > 0
                            ? (uint8*)FMemory_Alloca(GenFunc->ParmsSize)
                            : nullptr;
                        if (Params)
                        {
                            FMemory::Memzero(Params, GenFunc->ParmsSize);
                        }
                        Comp->ProcessEvent(GenFunc, Params);
                        ++PCGGenerated;
                    }
                }
            }
            FlushRenderingCommands();
            UE_LOG(LogTemp, Log, TEXT("[WME] PCG generation triggered on %d volumes."), PCGGenerated);
        }
    }

    // ── 1a. Compute maximum Z of geometry (for camera placement) ─────────────
    // Scan all primitive components in the world to find the tallest point.
    float CaptureMaxZ = 0.f;
    {
        constexpr float MaxSaneHalfExtent = 200000.f;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (!IsValid(Actor) || Actor->IsHidden()) continue;

            TArray<UPrimitiveComponent*> Prims;
            Actor->GetComponents<UPrimitiveComponent>(Prims);
            for (UPrimitiveComponent* Prim : Prims)
            {
                if (!Prim || !Prim->IsRegistered() || !Prim->IsVisible()) continue;
                if (Prim->IsA<UExponentialHeightFogComponent>() ||
                    Prim->IsA<USkyLightComponent>() ||
                    Prim->IsA<USkyAtmosphereComponent>()) continue;
                FBoxSphereBounds B = Prim->CalcBounds(Prim->GetComponentTransform());
                if (B.BoxExtent.GetAbsMax() > MaxSaneHalfExtent) continue;
                CaptureMaxZ = FMath::Max(CaptureMaxZ, B.Origin.Z + B.BoxExtent.Z);
            }
        }
        UE_LOG(LogTemp, Log, TEXT("[WME] Geometry MaxZ = %.0f (%.1f m)"), CaptureMaxZ, CaptureMaxZ / 100.f);
    }

    // Warn about suspiciously large OrthoWidth — usually caused by distant SkyAtmosphere /
    // DirectionalLight / SkyLight actors inflating auto-detected bounds.
    const float OrthoWidthKm = (MaxX - MinX) / 100000.f; // UE unit = 1 cm → / 100 = metres / 100 = km
    if ((MaxX - MinX) > 1000000.f) // > 10 km
    {
        UE_LOG(LogTemp, Warning, TEXT("[WME] OrthoWidth=%.0f cm (%.1f km) is very large. "
            "Auto-detect probably picked up a distant Sky/Light actor. "
            "Consider setting bounds manually to get useful resolution."),
            (MaxX - MinX), OrthoWidthKm);
        // Surface the warning in the status bar, but don't abort
    }

    UE_LOG(LogTemp, Log, TEXT("[WME] World: %s | Mode: %s | Bounds: X[%.0f..%.0f] Y[%.0f..%.0f]"),
        *World->GetMapName(),
        bUnlit ? TEXT("Unlit") : TEXT("Lit"),
        MinX, MaxX, MinY, MaxY);

    // ── 1b. Destroy any leftover capture actor from a previous run ────────────
    for (TActorIterator<ASceneCapture2D> It(World); It; ++It)
    {
        if (It->Tags.Contains(TEXT("WME_Capture")))
        {
            UE_LOG(LogTemp, Log, TEXT("[WME] Destroying leftover capture actor."));
            It->Destroy();
        }
    }
    GEngine->ForceGarbageCollection(true);
    FlushRenderingCommands();

    // ── 2. Create render target (float 16f — works with FinalToneCurveHDR) ─────
    // RTF_RGBA8 + SCS_FinalColorLDR is unreliable in non-PIE editor because the
    // tone-mapping pass doesn't always run. FinalToneCurveHDR writes a half-float
    // result that we manually convert to FColor.
    UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
    RT->ClearColor         = FLinearColor::Black;
    RT->RenderTargetFormat = RTF_RGBA16f;
    RT->bGPUSharedFlag     = false;
    RT->InitAutoFormat(TextureSize, TextureSize);
    RT->UpdateResourceImmediate(true);

    UE_LOG(LogTemp, Log, TEXT("[WME] RT created: %dx%d format=RTF_RGBA16f"), RT->SizeX, RT->SizeY);

    // ── 3. Square the bounds & spawn SceneCapture2D ──────────────────────────
    //  Camera FRotator(-90,0,0) maps:  image X+ → world Y+,  image Y+ → world X-.
    //  OrthoWidth controls the horizontal image span (= world Y range) and for a
    //  square render target the vertical span equals OrthoWidth too.
    //  To keep pixel-to-world mapping trivial we expand the smaller axis so both
    //  ranges are equal, then OrthoWidth covers everything exactly.
    const float RangeX = MaxX - MinX;
    const float RangeY = MaxY - MinY;
    if (RangeX > RangeY)
    {
        const float Half = (RangeX - RangeY) * 0.5f;
        MinY -= Half;
        MaxY += Half;
    }
    else if (RangeY > RangeX)
    {
        const float Half = (RangeY - RangeX) * 0.5f;
        MinX -= Half;
        MaxX += Half;
    }

    const float CenterX    = (MinX + MaxX) * 0.5f;
    const float CenterY    = (MinY + MaxY) * 0.5f;
    const float OrthoWidth = MaxX - MinX;   // == MaxY - MinY after squaring

    FActorSpawnParameters SpawnParams;
    SpawnParams.bNoFail     = true;
    SpawnParams.ObjectFlags = RF_Transient;

    // Place camera above the tallest geometry found in the Z-scan.
    const float CaptureZ = CaptureMaxZ + 50000.f;   // 500 m above tallest geometry

    ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(
        FVector(CenterX, CenterY, CaptureZ),
        FRotator(-90.f, 0.f, 0.f),
        SpawnParams
    );

    if (!CaptureActor)
    {
        OutError = TEXT("Error: failed to spawn ASceneCapture2D.");
        return false;
    }
    CaptureActor->Tags.Add(TEXT("WME_Capture"));

    UE_LOG(LogTemp, Log, TEXT("[WME] CaptureActor spawned at (%.0f, %.0f, %.0f) OrthoWidth=%.0f"),
        CenterX, CenterY, CaptureZ, OrthoWidth);

    // ── 4. Configure capture component ───────────────────────────────────────
    USceneCaptureComponent2D* Comp = CaptureActor->GetCaptureComponent2D();
    Comp->ProjectionType               = ECameraProjectionMode::Orthographic;
    Comp->OrthoWidth                   = OrthoWidth;
    Comp->TextureTarget                = RT;
    Comp->bCaptureEveryFrame           = false;
    Comp->bCaptureOnMovement           = false;
    Comp->bAlwaysPersistRenderingState = true;

    // Force every mesh to render regardless of per-object MaxDrawDistance / cull-distance settings.
    // 0 = "no override" (respects default culling); use a huge value to truly disable all distance culling.
    Comp->MaxViewDistanceOverride      = 100000000.f;
    Comp->LODDistanceFactor            = 0.001f; // treat camera as very close for LOD selection

    // Explicitly show all mesh types and distance-culled primitives.
    Comp->ShowFlags.SetDistanceCulledPrimitives(true);
    Comp->ShowFlags.SetStaticMeshes(true);
    Comp->ShowFlags.SetSkeletalMeshes(true);
    Comp->ShowFlags.SetLandscape(true);
    Comp->ShowFlags.SetInstancedFoliage(true);
    Comp->ShowFlags.SetInstancedStaticMeshes(true);
    Comp->ShowFlags.SetNaniteMeshes(true);
    Comp->ShowFlags.SetTemporalAA(false);

    if (bUnlit)
    {
        // SCS_BaseColor reads directly from the GBuffer albedo — no lighting,
        // no tonemapping, no auto-exposure. Most reliable path in non-PIE editor.
        Comp->CaptureSource = SCS_BaseColor;
        Comp->ShowFlags.SetLighting(false);
        Comp->ShowFlags.SetGlobalIllumination(false);
        Comp->ShowFlags.SetReflectionEnvironment(false);
        Comp->ShowFlags.SetScreenSpaceReflections(false);
        Comp->ShowFlags.SetAtmosphere(false);
        Comp->ShowFlags.SetFog(false);
        Comp->ShowFlags.SetVolumetricFog(false);
    }
    else
    {
        // SCS_SceneColorHDR captures the fully lit HDR scene before tone mapping.
        // This includes translucent objects (water planes), directional shadows,
        // and sky-light ambient — essential for visual depth and material
        // differentiation in the exported map.
        // We apply Reinhard tone mapping + sRGB gamma in the pixel conversion pass.
        Comp->CaptureSource = SCS_SceneColorHDR;
        Comp->ShowFlags.SetLighting(true);
        Comp->ShowFlags.SetDirectionalLights(true);
        Comp->ShowFlags.SetPointLights(true);
        Comp->ShowFlags.SetSpotLights(true);
        Comp->ShowFlags.SetSkyLighting(true);
        Comp->ShowFlags.SetGlobalIllumination(true);
        Comp->ShowFlags.SetReflectionEnvironment(true);
        Comp->ShowFlags.SetScreenSpaceReflections(true);
        Comp->ShowFlags.SetAmbientOcclusion(true);
        Comp->ShowFlags.SetTranslucency(true);
        Comp->ShowFlags.SetBloom(false);
        Comp->ShowFlags.SetMotionBlur(false);
        Comp->ShowFlags.SetLensFlares(false);
        Comp->ShowFlags.SetDepthOfField(false);
        Comp->ShowFlags.SetTemporalAA(false);
        Comp->ShowFlags.SetAtmosphere(false);
        Comp->ShowFlags.SetFog(false);
        Comp->ShowFlags.SetVolumetricFog(false);
    }

    UE_LOG(LogTemp, Log, TEXT("[WME] Component visible: %s | Scene: %s"),
        Comp->IsVisible() ? TEXT("YES") : TEXT("NO"),
        Comp->GetScene()  ? TEXT("valid") : TEXT("NULL"));

    UE_LOG(LogTemp, Log, TEXT("[WME] CaptureSource: %s"),
        Comp->CaptureSource == SCS_BaseColor          ? TEXT("SCS_BaseColor")         :
        Comp->CaptureSource == SCS_SceneColorHDR      ? TEXT("SCS_SceneColorHDR")      :
        Comp->CaptureSource == SCS_FinalToneCurveHDR  ? TEXT("SCS_FinalToneCurveHDR")  : TEXT("Other"));

    // ── 5. Multi-frame capture ─────────────────────────────────────────────
    // A single CaptureScene() after spawning often produces black because
    // UE5's GPU-driven rendering pipeline (Nanite, GPU Scene, Virtual Shadow Maps)
    // needs at least one full rendered frame to populate the GPU Scene buffer for
    // the new viewpoint.  We enable bCaptureEveryFrame and pump several editor
    // frames so the renderer has time to build acceleration structures.
    //
    // Before capture, temporarily disable minimum-screen-radius culling so
    // small foliage instances (trees, bushes, grass) are not dropped in the
    // orthographic top-down view where every instance projects to a tiny area.

    static IConsoleVariable* CVarMinScreenRadius = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MinScreenRadiusForPrimitives"));
    static IConsoleVariable* CVarFoliageScreenSize = IConsoleManager::Get().FindConsoleVariable(TEXT("foliage.MinimumScreenSizeGpu"));
    static IConsoleVariable* CVarFoliageVSMCull   = IConsoleManager::Get().FindConsoleVariable(TEXT("foliage.CullAllInVertexShader"));
    static IConsoleVariable* CVarForceLOD         = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForceLOD"));

    const float OldMinScreenRadius = CVarMinScreenRadius ? CVarMinScreenRadius->GetFloat() : 0.02f;
    const float OldFoliageScreenSize = CVarFoliageScreenSize ? CVarFoliageScreenSize->GetFloat() : 0.05f;
    const int32 OldFoliageVSMCull   = CVarFoliageVSMCull   ? CVarFoliageVSMCull->GetInt()   : 1;
    const int32 OldForceLOD         = CVarForceLOD         ? CVarForceLOD->GetInt()         : -1;

    if (CVarMinScreenRadius) CVarMinScreenRadius->Set(0.0f);
    if (CVarFoliageScreenSize) CVarFoliageScreenSize->Set(0.0f);
    if (CVarFoliageVSMCull)   CVarFoliageVSMCull->Set(0, ECVF_SetByCode);
    if (CVarForceLOD)         CVarForceLOD->Set(0, ECVF_SetByCode);

    UE_LOG(LogTemp, Log, TEXT("[WME] Disabled foliage culling: r.MinScreenRadius=%.4f→0, foliage.MinScreenSizeGpu=%.4f→0, foliage.CullAllInVS=%d→0, r.ForceLOD=%d→0"),
        OldMinScreenRadius, OldFoliageScreenSize, OldFoliageVSMCull, OldForceLOD);
    Comp->bCaptureEveryFrame = true;
    Comp->bCaptureOnMovement = true;

    static constexpr int32 WarmUpFrames = 30;
    for (int32 i = 0; i < WarmUpFrames; ++i)
    {
        GEditor->RedrawAllViewports(/*bInvalidateHitProxies=*/false);
        FlushRenderingCommands();
    }

    // Final explicit capture + full GPU sync
    Comp->bCaptureEveryFrame = false;
    Comp->bCaptureOnMovement = false;
    Comp->CaptureScene();

    FlushRenderingCommands();
    {
        FRenderCommandFence Fence;
        Fence.BeginFence();
        Fence.Wait(/*bProcessGameThreadTasks=*/true);
    }

    // Restore cvar defaults
    if (CVarMinScreenRadius) CVarMinScreenRadius->Set(OldMinScreenRadius);
    if (CVarFoliageScreenSize) CVarFoliageScreenSize->Set(OldFoliageScreenSize);
    if (CVarFoliageVSMCull)   CVarFoliageVSMCull->Set(OldFoliageVSMCull, ECVF_SetByCode);
    if (CVarForceLOD)         CVarForceLOD->Set(OldForceLOD, ECVF_SetByCode);

    // ── 6. Read float pixels and convert to FColor ────────────────────────────
    // RTF_RGBA16f → ReadLinearColorPixels; then ToFColor(bSRGB=true) applies gamma.
    FTextureRenderTarget2DResource* RTResource =
        static_cast<FTextureRenderTarget2DResource*>(RT->GameThread_GetRenderTargetResource());

    TArray<FLinearColor> LinearPixels;
    const bool bReadOk = RTResource && RTResource->ReadLinearColorPixels(LinearPixels);

    CaptureActor->Destroy();

    if (!bReadOk || LinearPixels.Num() == 0)
    {
        OutError = TEXT("Error: failed to read float pixels from render target. See Output Log.");
        UE_LOG(LogTemp, Error, TEXT("[WME] ReadLinearColorPixels failed. RTResource=%s Count=%d"),
            RTResource ? TEXT("valid") : TEXT("NULL"), LinearPixels.Num());
        return false;
    }

    // Convert to BGRA8 with sRGB gamma.
    // Unlit (SCS_BaseColor): values are already in sRGB space; direct ToFColor.
    // Lit   (SCS_SceneColorHDR): linear HDR → Reinhard tone map → sRGB gamma.
    // Force alpha to 255: SCS_BaseColor writes alpha=0 in the GBuffer,
    // which makes the entire PNG fully transparent (appears black in viewers).
    static constexpr float ExposureCompensation = 2.0f;
    TArray<FColor> Pixels;
    Pixels.Reserve(LinearPixels.Num());
    for (const FLinearColor& LC : LinearPixels)
    {
        FLinearColor Processed;
        if (bUnlit)
        {
            Processed = LC;
        }
        else
        {
            const FLinearColor Exposed = LC * ExposureCompensation;
            Processed = FLinearColor(
                Exposed.R / (1.0f + Exposed.R),
                Exposed.G / (1.0f + Exposed.G),
                Exposed.B / (1.0f + Exposed.B),
                1.0f);
        }
        FColor C = Processed.ToFColor(/*bSRGB=*/true);
        C.A = 255;
        Pixels.Add(C);
    }

    // Diagnostics
    int32 NonBlack = 0;
    for (const FColor& P : Pixels) { if (P.R > 4 || P.G > 4 || P.B > 4) ++NonBlack; }
    UE_LOG(LogTemp, Log, TEXT("[WME] Pixels: %d total, %d non-black (%.1f%%). First=[R=%d G=%d B=%d A=%d]"),
        Pixels.Num(), NonBlack, 100.f * NonBlack / FMath::Max(1, Pixels.Num()),
        Pixels[0].R, Pixels[0].G, Pixels[0].B, Pixels[0].A);

    if (NonBlack == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[WME] STILL ALL BLACK. "
            "Unlit mode uses SCS_BaseColor (should always work). "
            "Lit mode uses SCS_SceneColorHDR + manual Reinhard tone map. "
            "If lit is black, check that the level has at least one directional/sky light."));
    }

    // ── 6. Ensure output directory exists ─────────────────────────────────────
    IFileManager::Get().MakeDirectory(*OutputPath, /*Tree=*/true);

    // ── 7. Encode and save PNG ─────────────────────────────────────────────────
    IImageWrapperModule& IWM =
        FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
    TSharedPtr<IImageWrapper> PngWrapper = IWM.CreateImageWrapper(EImageFormat::PNG);

    if (!PngWrapper.IsValid() ||
        !PngWrapper->SetRaw(Pixels.GetData(),
                            Pixels.Num() * sizeof(FColor),
                            TextureSize, TextureSize,
                            ERGBFormat::BGRA, 8))
    {
        OutError = TEXT("Error: failed to encode PNG.");
        return false;
    }

    const TArray64<uint8>& Compressed = PngWrapper->GetCompressed(/*Quality=*/0);
    const FString PngPath = OutputPath / TEXT("worldmap.png");

    // Write via platform file to support TArray64 sizes properly
    {
        TUniquePtr<IFileHandle> FileHandle(
            FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*PngPath));
        if (!FileHandle)
        {
            OutError = FString::Printf(TEXT("Error: cannot write file — %s"), *PngPath);
            return false;
        }
        FileHandle->Write(Compressed.GetData(), Compressed.Num());
    }

    // ── 8. Save JSON metadata ─────────────────────────────────────────────────
    //
    // Coordinate conventions (camera Pitch=-90, Yaw=0, Roll=0):
    //   image X+ (right) = world Y+ (East)
    //   image Y+ (down)  = world X- (South)
    //
    // Bounds are squared so OrthoWidth covers both axes identically.
    // Web client conversion (pixel ↔ world):
    //   world_y = world_min_y + (pixel_x / image_width)  * (world_max_y - world_min_y)
    //   world_x = world_max_x - (pixel_y / image_height) * (world_max_x - world_min_x)
    //
    const float UnitsPerPixel = OrthoWidth / static_cast<float>(TextureSize);

    TSharedPtr<FJsonObject> Meta = MakeShared<FJsonObject>();
    Meta->SetNumberField(TEXT("world_min_x"),     MinX);
    Meta->SetNumberField(TEXT("world_max_x"),     MaxX);
    Meta->SetNumberField(TEXT("world_min_y"),     MinY);
    Meta->SetNumberField(TEXT("world_max_y"),     MaxY);
    Meta->SetNumberField(TEXT("image_width"),     TextureSize);
    Meta->SetNumberField(TEXT("image_height"),    TextureSize);
    Meta->SetNumberField(TEXT("units_per_pixel"), UnitsPerPixel);
    Meta->SetStringField(TEXT("image_x_axis"),    TEXT("+Y"));   // image right  → world +Y (East)
    Meta->SetStringField(TEXT("image_y_axis"),    TEXT("-X"));   // image down   → world -X (South)
    Meta->SetStringField(TEXT("ue_version"),      TEXT("5.7"));

    FString MetaString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MetaString);
    FJsonSerializer::Serialize(Meta.ToSharedRef(), Writer);
    FFileHelper::SaveStringToFile(MetaString, *(OutputPath / TEXT("worldmap_meta.json")));

    UE_LOG(LogTemp, Log, TEXT("[WorldMapExporter] Export complete -> %s"), *PngPath);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Auto world bounds detection
// ─────────────────────────────────────────────────────────────────────────────

bool SMapExporterWidget::ComputeWorldBounds(
    UWorld* World,
    float& OutMinX, float& OutMaxX,
    float& OutMinY, float& OutMaxY,
    float& OutMaxZ,
    FString& OutError)
{
    if (!World)
    {
        OutError = TEXT("Error: editor world is not available.");
        return false;
    }

    // Maximum allowed half-extent for a single actor in any axis (UE units = cm).
    // A sky sphere / atmospheric dome is typically 2,000,000+ units in radius.
    // Real level geometry is rarely larger than 200,000 units (~2 km) per axis.
    constexpr float MaxSaneHalfExtent = 200000.f;

    FBox Bounds(ForceInit);
    int32 Accepted  = 0;
    int32 Rejected  = 0;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor) || Actor->IsHidden()) continue;

        // ── Hard-exclude known non-geometry actor types ──────────────────────
        if (Actor->IsA<ASceneCapture2D>() ||
            Actor->IsA<ACameraActor>()    ||
            Actor->IsA<ALight>())
        {
            continue;
        }

        // ── Only accumulate bounds from geometry-producing components ─────────
        // Check ALL primitive components (static meshes, skeletal meshes,
        // landscape, instanced foliage, etc.) and filter out known non-geometry
        // types (lights, fog, sky, atmosphere).
        FBox ActorGeomBox(ForceInit);
        {
            TArray<UPrimitiveComponent*> Prims;
            Actor->GetComponents<UPrimitiveComponent>(Prims);
            for (UPrimitiveComponent* Prim : Prims)
            {
                if (!Prim || !Prim->IsRegistered() || !Prim->IsVisible()) continue;
                if (Prim->IsA<UExponentialHeightFogComponent>() ||
                    Prim->IsA<USkyLightComponent>() ||
                    Prim->IsA<USkyAtmosphereComponent>()) continue;

                FBoxSphereBounds PrimBounds = Prim->CalcBounds(Prim->GetComponentTransform());

                // Reject individual components with suspiciously large extents
                // (sky spheres, atmospheric meshes).
                if (PrimBounds.BoxExtent.GetAbsMax() > MaxSaneHalfExtent) continue;

                ActorGeomBox += PrimBounds.GetBox();
            }
        }

        if (!ActorGeomBox.IsValid)
        {
            ++Rejected;
            continue;
        }

        Bounds += ActorGeomBox;
        ++Accepted;
    }

    UE_LOG(LogTemp, Log, TEXT("[WME] ComputeWorldBounds: %d actors accepted, %d rejected (too large or non-geometry)"),
        Accepted, Rejected);

    if (!Bounds.IsValid)
    {
        OutError = TEXT("Error: no geometry actors found. Make sure static meshes / landscape are visible.");
        return false;
    }

    // 5% padding so map edges are not clipped
    OutMaxZ = Bounds.Max.Z;  // Save Z max BEFORE XY padding
    Bounds = Bounds.ExpandBy(Bounds.GetSize() * 0.05f);

    OutMinX = Bounds.Min.X;
    OutMaxX = Bounds.Max.X;
    OutMinY = Bounds.Min.Y;
    OutMaxY = Bounds.Max.Y;

    UE_LOG(LogTemp, Log, TEXT("[WME] Auto-bounds result: X[%.0f..%.0f] Y[%.0f..%.0f] = %.1f x %.1f m"),
        OutMinX, OutMaxX, OutMinY, OutMaxY,
        (OutMaxX - OutMinX) / 100.f, (OutMaxY - OutMinY) / 100.f);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

void SMapExporterWidget::SetStatus(const FString& Message, bool bIsError)
{
    const FLinearColor Color = bIsError
        ? FLinearColor(1.f, 0.3f, 0.3f)
        : FLinearColor(0.6f, 1.f, 0.6f);

    StatusText->SetText(FText::FromString(Message));
    StatusText->SetColorAndOpacity(FSlateColor(Color));
}

float SMapExporterWidget::GetFloat(const TSharedPtr<SEditableTextBox>& Box) const
{
    return Box.IsValid() ? FCString::Atof(*Box->GetText().ToString()) : 0.f;
}

#undef LOCTEXT_NAMESPACE
