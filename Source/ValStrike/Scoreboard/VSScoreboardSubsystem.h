#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VSScoreboardModel.h"
#include "VSScoreboardSubsystem.generated.h"

class APlayerController;
class AVSScoreboardState;
class SVSScoreboard;
class ULocalPlayer;

/**
 * Local presentation of the replicated scoreboard: shown while Tab is held by
 * the local player and hidden whenever the Escape menu is open. Reads input
 * state only; it binds no keys and never takes input focus.
 */
UCLASS()
class VALSTRIKE_API UVSScoreboardSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void RegisterState(AVSScoreboardState* InState);
    void UnregisterState(AVSScoreboardState* InState);
    void MarkDirty() { bDirty = true; }

    AVSScoreboardState* GetState() const { return State.Get(); }
    bool IsScoreboardVisible() const { return bVisible; }
    const FVSScoreboardModel& GetShownModel() const { return ShownModel; }
    TSharedPtr<SVSScoreboard> GetWidget() const { return Widget; }
    FVSScoreboardModel BuildModel() const;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // Development harness only: show without Tab (still hidden by an open menu).
    void DevelopmentForceVisible(bool bForce) { bForceVisible = bForce; }
#endif

private:
    APlayerController* FindLocalController() const;
    bool WantsVisible(APlayerController* Controller) const;
    void EnsureWidget(APlayerController* Controller);
    void RemoveWidget();

    TWeakObjectPtr<AVSScoreboardState> State;
    TSharedPtr<SVSScoreboard> Widget;
    TWeakObjectPtr<ULocalPlayer> WidgetOwner;
    FVSScoreboardModel ShownModel;
    double NextRefreshSeconds = 0.0;
    bool bVisible = false;
    bool bDirty = true;
    bool bForceVisible = false;
};
