#include "WorldMapExporterModule.h"
#include "SMapExporterWidget.h"
#include "ToolMenus.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "WorldMapExporter"

IMPLEMENT_MODULE(FWorldMapExporterModule, WorldMapExporter)

void FWorldMapExporterModule::StartupModule()
{
    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FWorldMapExporterModule::RegisterMenus));
}

void FWorldMapExporterModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
}

void FWorldMapExporterModule::RegisterMenus()
{
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
    FToolMenuSection& Section = Menu->AddSection(
        "WorldMapExporterSection",
        LOCTEXT("SectionLabel", "World Map Exporter"));

    Section.AddMenuEntry(
        "OpenWorldMapExporter",
        LOCTEXT("MenuLabel", "Export World Map..."),
        LOCTEXT("MenuTooltip", "Capture an orthographic top-down screenshot + coordinate metadata for the web level editor"),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"),
        FUIAction(FExecuteAction::CreateRaw(this, &FWorldMapExporterModule::OnOpenExporter))
    );
}

void FWorldMapExporterModule::OnOpenExporter()
{
    TSharedRef<SWindow> Window = SNew(SWindow)
        .Title(LOCTEXT("WindowTitle", "World Map Exporter"))
        .ClientSize(FVector2D(500, 400))
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        .SizingRule(ESizingRule::FixedSize);

    Window->SetContent(SNew(SMapExporterWidget));
    FSlateApplication::Get().AddWindow(Window);
}

#undef LOCTEXT_NAMESPACE
