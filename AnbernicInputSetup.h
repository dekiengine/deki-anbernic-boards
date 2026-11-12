#pragma once

#include <deki/SetupComponent.h>
#include <deki/reflection/Property.h>
#include "AnbernicPackage.h"

namespace DekiAnbernic
{

/// Starts the handheld's buttons. Add it to the boot scene; it runs as part of
/// the boot sequence. See AnbernicInput.h for which key each button reports.
DEKI_CATEGORY("Anbernic")
DEKI_DESCRIPTION("Feeds an Anbernic handheld's buttons to the engine as keys.")
class DEKI_ANBERNIC_API AnbernicInputSetup : public Deki::SetupComponent
{
public:
    void Setup(SetupCallback onComplete) override;
    const char* GetSetupName() const override { return "Anbernic Input"; }
};

}  // namespace DekiAnbernic
