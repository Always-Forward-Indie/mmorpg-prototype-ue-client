#pragma once

#include "Modules/ModuleManager.h"

class FWorldMapExporterModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void OnOpenExporter();
};
