#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SCheckBox.h"

/**
 * Slate widget for the World Map Exporter tool window.
 *
 * Spawns an ASceneCapture2D actor in the editor world, captures an orthographic
 * top-down image of the entire world, saves worldmap.png and worldmap_meta.json
 * to the specified output folder.
 *
 * Coordinate convention written to metadata:
 *   UE5 top-down: X+ = North (up in image), Y+ = East (right in image)
 *   flip_y = true → web editors with Y-down should vertically flip the image
 *             when converting between pixel and world coordinates.
 */
class SMapExporterWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMapExporterWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    // ─── UI callbacks ─────────────────────────────────────────────────────────
    void        OnBrowseOutputPath();
    FReply      OnExportClicked();

    // ─── Export implementation ─────────────────────────────────────────────────
    bool        DoExport(const FString& OutputPath,
                         float MinX, float MaxX,
                         float MinY, float MaxY,
                         int32 TextureSize,
                         bool  bUnlit,
                         FString& OutError);
    /** Iterates all visible primitive actors and returns an AABB with 5% padding. */
    bool        ComputeWorldBounds(UWorld* World,
                                   float& OutMinX, float& OutMaxX,
                                   float& OutMinY, float& OutMaxY,
                                   float& OutMaxZ,
                                   FString& OutError);
    void        SetStatus(const FString& Message, bool bIsError = false);

    // ─── Text box helpers ──────────────────────────────────────────────────────
    float       GetFloat(const TSharedPtr<SEditableTextBox>& Box) const;

    // ─── Input widgets ─────────────────────────────────────────────────────────
    TSharedPtr<SEditableTextBox>          OutputPathBox;
    TSharedPtr<SEditableTextBox>          MinXBox;
    TSharedPtr<SEditableTextBox>          MaxXBox;
    TSharedPtr<SEditableTextBox>          MinYBox;
    TSharedPtr<SEditableTextBox>          MaxYBox;

    TArray<TSharedPtr<int32>>             TextureSizeOptions;
    TSharedPtr<int32>                     SelectedTextureSize;
    TSharedPtr<SComboBox<TSharedPtr<int32>>> TextureSizeCombo;

    TSharedPtr<STextBlock>               StatusText;

    // Whether the user wants bounds computed automatically from scene actors
    bool                                  bAutoDetectBounds = false;

    // Capture in unlit mode (flat mesh base colours, no lighting dependency)
    bool                                  bUnlitMode = false;

    // ─── Layout helper ─────────────────────────────────────────────────────────
    /** Builds a single "Label | Input" row with consistent padding. */
    TSharedRef<SWidget> MakeInputRow(const FText& Label, TSharedRef<SWidget> Input);
};
