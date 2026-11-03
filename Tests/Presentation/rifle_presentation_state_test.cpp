#include "../../Source/ValStrike/Presentation/VSRiflePresentationState.h"

#include <cstdio>
#include <limits>

namespace
{
using VSRiflePresentation::Snapshot;
using VSRiflePresentation::State;
using VSRiflePresentation::Update;

int Checks = 0;
int Failures = 0;

void Expect(const char* Name, const Update& Actual, const Update& Expected)
{
    ++Checks;
    if (Actual.shotPulse != Expected.shotPulse
        || Actual.reloadStarted != Expected.reloadStarted
        || Actual.reloadStopped != Expected.reloadStopped
        || Actual.lifeChanged != Expected.lifeChanged
        || Actual.visible != Expected.visible
        || Actual.reloading != Expected.reloading)
    {
        ++Failures;
        std::fprintf(stderr,
            "FAIL %s: shot/start/stop/life/visible/reload actual=%d/%d/%d/%d/%d/%d expected=%d/%d/%d/%d/%d/%d\n",
            Name, Actual.shotPulse, Actual.reloadStarted, Actual.reloadStopped,
            Actual.lifeChanged, Actual.visible, Actual.reloading,
            Expected.shotPulse, Expected.reloadStarted, Expected.reloadStopped,
            Expected.lifeChanged, Expected.visible, Expected.reloading);
    }
}

const Update Visible{false, false, false, false, true, false};
const Update ReloadPose{false, false, false, false, true, true};
const Update Shot{true, false, false, false, true, false};
const Update ReloadStart{false, true, false, false, true, true};
const Update ReloadStop{false, false, true, false, true, false};

void AttachmentAndRejoin()
{
    State View;
    Expect("attach suppresses historical shots", View.Initialize({true, true, false, 700}), Visible);
    Expect("unchanged attached state is quiet", View.Observe({true, true, false, 700}), Visible);
    Expect("new accepted shot after attachment", View.Observe({true, true, false, 701}), Shot);
    Expect("reattach establishes a new pawn baseline", View.Initialize({true, true, false, 20}), Visible);
    Expect("reattach does not replay lower counter", View.Observe({true, true, false, 20}), Visible);
    Expect("new pawn can subsequently fire", View.Observe({true, true, false, 21}), Shot);
    Expect("join during reload shows pose without start", View.Initialize({true, true, true, 900}), ReloadPose);
    Expect("joined reload does not repeat a start", View.Observe({true, true, true, 900}), ReloadPose);
    Expect("joined reload ending clears pose", View.Observe({true, true, false, 900}), ReloadStop);
}

void FirstObservationAndUnavailableState()
{
    State View;
    Expect("first Observe is an attachment baseline", View.Observe({true, true, true, 11}), ReloadPose);
    Expect("unavailable data clears reload and visibility", View.Observe({false, true, true, 1000}),
        {false, false, true, false, false, false});
    Expect("unavailable changes have no effects", View.Observe({false, false, true, 2000}), {});
    Expect("availability return suppresses history", View.Observe({true, true, true, 3000}), ReloadPose);
    Expect("availability return is not a reload start", View.Observe({true, true, true, 3000}), ReloadPose);
    Expect("returned reload may subsequently stop", View.Observe({true, true, false, 3000}), ReloadStop);
    Expect("new shot after availability return", View.Observe({true, true, false, 3001}), Shot);

    State InitiallyUnavailable;
    Expect("unavailable attachment ignores raw alive/reload", InitiallyUnavailable.Initialize({false, true, true, 50}), {});
    Expect("first available state has no life or shot replay", InitiallyUnavailable.Observe({true, true, false, 55}), Visible);
}

void DuplicateBatchAndResetCounters()
{
    State View;
    View.Initialize({true, true, false, 0});
    Expect("one accepted shot pulses", View.Observe({true, true, false, 1}), Shot);
    Expect("duplicate accepted count does not pulse", View.Observe({true, true, false, 1}), Visible);
    Expect("batch of accepted shots coalesces", View.Observe({true, true, false, 40}), Shot);
    Expect("batch has no queued second pulse", View.Observe({true, true, false, 40}), Visible);
    Expect("rollback suppresses shot", View.Observe({true, true, false, 9}), Visible);
    Expect("next shot after rollback uses new baseline", View.Observe({true, true, false, 10}), Shot);
    Expect("reset to zero suppresses shot", View.Observe({true, true, false, 0}), Visible);
    Expect("reset duplicate stays quiet", View.Observe({true, true, false, 0}), Visible);
    Expect("first shot after reset works", View.Observe({true, true, false, 1}), Shot);

    View.Initialize({true, true, false, std::numeric_limits<std::uint32_t>::max()});
    Expect("counter wrap is a reset rather than a burst", View.Observe({true, true, false, 0}), Visible);
    Expect("counter works after wrap", View.Observe({true, true, false, 1}), Shot);
}

void ReloadTransitions()
{
    State View;
    View.Initialize({true, true, false, 4});
    Expect("reload starts once", View.Observe({true, true, true, 4}), ReloadStart);
    Expect("identical reload state does not restart", View.Observe({true, true, true, 4}), ReloadPose);
    Expect("reload ends once", View.Observe({true, true, false, 4}), ReloadStop);
    Expect("identical ended state does not restop", View.Observe({true, true, false, 4}), Visible);
    Expect("shot and reload batched together emit one of each", View.Observe({true, true, true, 5}),
        {true, true, false, false, true, true});
    Expect("batched events do not replay", View.Observe({true, true, true, 5}), ReloadPose);
    Expect("reset while reloading stops pose without shot", View.Observe({true, true, false, 0}), ReloadStop);
}

void DeathAndRevival()
{
    State View;
    View.Initialize({true, true, false, 4});
    Expect("active reload before death", View.Observe({true, true, true, 4}), ReloadStart);
    Expect("death overrides batched shot and stale reload", View.Observe({true, false, true, 5}),
        {false, false, true, true, false, false});
    Expect("repeated death has no duplicate events", View.Observe({true, false, true, 5}), {});
    Expect("late accepted count while dead cannot flash", View.Observe({true, false, false, 6}), {});
    Expect("revival suppresses stale shot and reload-start", View.Observe({true, true, true, 100}),
        {false, false, false, true, true, true});
    Expect("revival event does not repeat", View.Observe({true, true, true, 100}), ReloadPose);
    Expect("revived reload ends normally", View.Observe({true, true, false, 100}), ReloadStop);
    Expect("new shot after revival pulses", View.Observe({true, true, false, 101}), Shot);
    Expect("death without reload emits only life event", View.Observe({true, false, false, 102}),
        {false, false, false, true, false, false});
    Expect("attaching a dead pawn suppresses all history", View.Initialize({true, false, true, 999}), {});
    Expect("fresh pawn attachment clears old death baseline", View.Initialize({true, true, false, 0}), Visible);
}
}

int main()
{
    AttachmentAndRejoin();
    FirstObservationAndUnavailableState();
    DuplicateBatchAndResetCounters();
    ReloadTransitions();
    DeathAndRevival();
    std::printf("Rifle presentation state: %d/%d checks passed\n", Checks - Failures, Checks);
    return Failures == 0 ? 0 : 1;
}
