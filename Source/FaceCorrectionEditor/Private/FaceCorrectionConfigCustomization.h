#pragma once

#include "IDetailCustomization.h"

class IPropertyUtilities;
class UFaceCorrectionConfig;

class FFaceCorrectionConfigCustomization final : public IDetailCustomization
{
public:
    static TSharedRef<IDetailCustomization> makeInstance();
    virtual void CustomizeDetails(IDetailLayoutBuilder& detailBuilder) override;

private:
    FReply populateFromFolder();
    FReply populatePhonemeMappings();

    TWeakObjectPtr<UFaceCorrectionConfig> config;
    TWeakPtr<IPropertyUtilities> propertyUtilities;
};
