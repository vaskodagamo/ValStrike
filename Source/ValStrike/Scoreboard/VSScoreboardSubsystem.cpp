#include "VSScoreboardSubsystem.h"

#include "SVSScoreboard.h"
#include "VSScoreboardState.h"
#include "../VSPlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"

namespace
{
// Below the session menu (100) so an open menu always draws on top.
constexpr int32 VSBoardZOrder = 40;
constexpr double VSBoardRefreshSeconds = 0.1;
}

bool UVSScoreboardSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UVSScoreboardSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UVSScoreboardSubsystem, STATGROUP_Tickables);
}

void UVSScoreboardSubsystem::Deinitialize()
{
    RemoveWidget();
    State.Reset();
    Super::Deinitialize();
}

void UVSScoreboardSubsystem::RegisterState(AVSScoreboardState* InState)
{
    State = InState;
    bDirty = true;
}

void UVSScoreboardSubsystem::UnregisterState(AVSScoreboardState* InState)
{
    if (State.Get() == InState)
    {
        State.Reset();
        bDirty = true;
    }
}

APlayerController* UVSScoreboardSubsystem::FindLocalController() const
{
    const UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return nullptr;
    }
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Controller = It->Get();
        if (Controller && Controller->IsLocalController() && Controller->GetLocalPlayer())
        {
            return Controller;
        }
    }
    return nullptr;
}

bool UVSScoreboardSubsystem::WantsVisible(APlayerController* Controller) const
{
    if (!Controller)
    {
        return false;
    }
    if (const AVSPlayerController* Session = Cast<AVSPlayerController>(Controller))
    {
        // The Escape menu owns focus; never compete with it.
        if (Session->IsSessionMenuOpen()) { return false; }
    }
    return bForceVisible || Controller->IsInputKeyDown(EKeys::Tab);
}

FVSScoreboardModel UVSScoreboardSubsystem::BuildModel() const
{
    const UWorld* World = GetWorld();
    const APlayerController* Controller = FindLocalController();
    return VSScoreboardModel::Build(State.Get(), World ? World->GetGameState() : nullptr,
        Controller ? Controller->PlayerState.Get() : nullptr);
}

void UVSScoreboardSubsystem::EnsureWidget(APlayerController* Controller)
{
    ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
    UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!Player || !Viewport)
    {
        return;
    }
    if (Widget.IsValid() && WidgetOwner.Get() == Player)
    {
        return;
    }
    RemoveWidget();
    SAssignNew(Widget, SVSScoreboard).Controller(Controller);
    Viewport->AddViewportWidgetForPlayer(Player, Widget.ToSharedRef(), VSBoardZOrder);
    WidgetOwner = Player;
    bDirty = true;
}

void UVSScoreboardSubsystem::RemoveWidget()
{
    if (Widget.IsValid())
    {
        UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
        if (Viewport && WidgetOwner.IsValid())
        {
            Viewport->RemoveViewportWidgetForPlayer(WidgetOwner.Get(), Widget.ToSharedRef());
        }
    }
    Widget.Reset();
    WidgetOwner.Reset();
    bVisible = false;
}

void UVSScoreboardSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    APlayerController* Controller = FindLocalController();
    const bool bWant = WantsVisible(Controller);
    if (bWant)
    {
        EnsureWidget(Controller);
    }
    if (!Widget.IsValid())
    {
        bVisible = false;
        return;
    }
    const double Now = FPlatformTime::Seconds();
    if (bWant && (bDirty || !bVisible || Now >= NextRefreshSeconds))
    {
        ShownModel = BuildModel();
        Widget->SetModel(ShownModel);
        NextRefreshSeconds = Now + VSBoardRefreshSeconds;
        bDirty = false;
    }
    if (bWant != bVisible)
    {
        Widget->SetVisibility(bWant ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
        bVisible = bWant;
    }
}
