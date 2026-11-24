#include "FaceCorrectionEditorModule.h"

#include "FaceCorrectionConfig.h"
#include "FaceCorrectionConfigCustomization.h"
#include "PropertyEditorModule.h"

#if WITH_HANDPOSE_TIMELINE
#include "FaceCorrectionTimelineExtension.h"
#include "HandposeTimelineModule.h"
#endif

void FFaceCorrectionEditorModule::StartupModule()
{
    FPropertyEditorModule& propertyEditor =
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
    propertyEditor.RegisterCustomClassLayout(
        UFaceCorrectionConfig::StaticClass()->GetFName(),
        FOnGetDetailCustomizationInstance::CreateStatic(
            &FFaceCorrectionConfigCustomization::makeInstance));
    propertyEditor.NotifyCustomizationModuleChanged();

#if WITH_HANDPOSE_TIMELINE
    IHandposeTimelineModule& module =
        FModuleManager::LoadModuleChecked<IHandposeTimelineModule>(TEXT("HandposeTimeline"));
    timelineExtension = MakeShared<FFaceCorrectionTimelineExtension>();
    module.registerExtension(timelineExtension.ToSharedRef());
#endif
}

void FFaceCorrectionEditorModule::ShutdownModule()
{
#if WITH_HANDPOSE_TIMELINE
    if (FModuleManager::Get().IsModuleLoaded(TEXT("HandposeTimeline")))
    {
        if (IHandposeTimelineModule* module =
            FModuleManager::GetModulePtr<IHandposeTimelineModule>(TEXT("HandposeTimeline")))
        {
            module->unregisterExtension(TEXT("FaceCorrection"));
        }
    }
    timelineExtension.Reset();
#endif

    if (FPropertyEditorModule* propertyEditor =
        FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
    {
        propertyEditor->UnregisterCustomClassLayout(
            UFaceCorrectionConfig::StaticClass()->GetFName());
        propertyEditor->NotifyCustomizationModuleChanged();
    }
}

IMPLEMENT_MODULE(FFaceCorrectionEditorModule, FaceCorrectionEditor)
