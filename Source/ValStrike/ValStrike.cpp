#include "ValStrike.h"

#include "Modules/ModuleManager.h"
#include "Tests/VSMovementSmoke.h"

DEFINE_LOG_CATEGORY(LogValStrike);

class FValStrikeModule : public FDefaultGameModuleImpl
{
public:
    virtual void StartupModule() override
    {
        FDefaultGameModuleImpl::StartupModule();
        ValStrikeTests::StartMovementSmoke();
    }

    virtual void ShutdownModule() override
    {
        ValStrikeTests::StopMovementSmoke();
        FDefaultGameModuleImpl::ShutdownModule();
    }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FValStrikeModule, ValStrike, "ValStrike");
