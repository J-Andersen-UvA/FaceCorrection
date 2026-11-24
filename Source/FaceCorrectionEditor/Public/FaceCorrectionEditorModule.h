#pragma once

#include "Modules/ModuleManager.h"

class FFaceCorrectionEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    TSharedPtr<class FFaceCorrectionTimelineExtension> timelineExtension;
};
