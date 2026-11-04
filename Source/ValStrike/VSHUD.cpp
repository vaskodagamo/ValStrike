#include "VSHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "VSCharacter.h"
#include "VSPlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/VSWeaponComponent.h"
#include "Combat/VSWeaponLoadoutComponent.h"
#include "Feedback/VSHitFeedbackView.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Scoreboard/VSKillFeedModel.h"
#include "Scoreboard/VSScoreboardState.h"
#include "Scoreboard/VSScoreboardSubsystem.h"
#include "UI/Round/VSRoundHUDPresentation.h"
#include "UI/Round/VSRoundHUDPreview.h"

namespace
{
const FLinearColor Panel(0.015f, 0.026f, 0.04f, 0.88f);
const FLinearColor Ink(0.94f, 0.97f, 0.98f);
const FLinearColor Muted(0.66f, 0.76f, 0.80f);
const FLinearColor Accent(0.30f, 0.95f, 0.79f);
const FLinearColor Warning(1.0f, 0.40f, 0.34f);
const FLinearColor TeamTwo(1.0f, 0.72f, 0.38f);
const FLinearColor Headshot(1.0f, 0.82f, 0.25f);

FLinearColor WithAlpha(FLinearColor Color, float Alpha)
{
    Color.A *= FMath::Clamp(Alpha, 0.0f, 1.0f);
    return Color;
}
}

void AVSHUD::DrawLabel(const FString& Text, float X, float Y, float Height,
    const FLinearColor& Color, bool bCentered)
{
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    if (!Font)
    {
        return;
    }

    const float FontScale = FMath::Max(12.0f, Height) /
        FMath::Max(1.0f, static_cast<float>(Font->GetMaxCharHeight()));
    FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
    Item.Scale = FVector2D(FontScale);
    Item.bCentreX = bCentered;
    Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f), FVector2D(1.0f));
    Canvas->DrawItem(Item);
}

float AVSHUD::LabelWidth(const FString& Text, float Height) const
{
    UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
    if (!Font) return 0.0f;
    float Width = 0.0f, TextHeight = 0.0f;
    const float FontScale = FMath::Max(12.0f, Height) /
        FMath::Max(1.0f, static_cast<float>(Font->GetMaxCharHeight()));
    GetTextSize(Text, Width, TextHeight, Font, FontScale);
    return Width;
}

void AVSHUD::DrawBoundedLabel(const FString& Text, float X, float Y, float Height,
    float Width, const FLinearColor& Color, bool bCentered)
{
    // Keep the minimum readable font size. Long provider text is bounded to its
    // panel rather than shrinking into tiny text or covering adjacent UI.
    FString Visible = Text.Left(512);
    if (Visible.Len() < Text.Len() || LabelWidth(Visible, Height) > Width)
    {
        while (!Visible.IsEmpty() && LabelWidth(Visible + TEXT("..."), Height) > Width)
            Visible.LeftChopInline(1);
        Visible += TEXT("...");
    }
    DrawLabel(Visible, X, Y, Height, Color, bCentered);
}

void AVSHUD::DrawWrappedLabel(const FString& Text, float X, float Y, float Height,
    float Width, const FLinearColor& Color, bool bCentered)
{
    TArray<FString> Words;
    Text.Left(1024).ParseIntoArrayWS(Words);
    FString First, Rest;
    bool bSecondLine = false;
    for (const FString& Word : Words)
    {
        const FString Candidate = First.IsEmpty() ? Word : First + TEXT(" ") + Word;
        if (!bSecondLine && LabelWidth(Candidate, Height) <= Width) First = Candidate;
        else
        {
            bSecondLine = true;
            if (!Rest.IsEmpty()) Rest += TEXT(" ");
            Rest += Word;
        }
    }
    if (First.IsEmpty())
    {
        DrawBoundedLabel(Rest, X, Y, Height, Width, Color, bCentered);
        return;
    }
    DrawBoundedLabel(First, X, Y, Height, Width, Color, bCentered);
    if (!Rest.IsEmpty())
        DrawBoundedLabel(Rest, X, Y + FMath::Max(16.0f, Height + 4.0f), Height, Width, Color, bCentered);
}

void AVSHUD::DrawMatchHUD(const FVSMatchHUDData& Data, const FVSRoundHUDPresentation& View, float Scale)
{
    if (!View.bAvailable) return;
    const float CenterX = Canvas->SizeX * 0.5f;
    const float Margin = 24.0f * Scale;
    const float Width = 520.0f * Scale;
    const float Left = CenterX - Width * 0.5f;
    const float Cell = Width / 3.0f;
    const float HeaderHeight = FMath::Max(76.0f, 96.0f * Scale);
    const float Small = FMath::Max(12.0f, 14.0f * Scale);
    const float TeamOneX = Left + Cell * 0.5f;
    const float TeamTwoX = Left + Cell * 2.5f;
    const float CellTextWidth = Cell - 20.0f * Scale;
    DrawRect(Panel, Left, Margin, Width, HeaderHeight);
    DrawRect(Accent, Left, Margin, Cell, 3.0f * Scale);
    DrawRect(TeamTwo, Left + Cell * 2.0f, Margin, Cell, 3.0f * Scale);
    DrawBoundedLabel(Data.LocalTeam == EVSMatchHUDTeam::One ? TEXT("TEAM 1 / YOU") : TEXT("TEAM 1"),
        TeamOneX, Margin + 10.0f * Scale, Small, CellTextWidth, Accent, true);
    DrawBoundedLabel(Data.LocalTeam == EVSMatchHUDTeam::Two ? TEXT("TEAM 2 / YOU") : TEXT("TEAM 2"),
        TeamTwoX, Margin + 10.0f * Scale, Small, CellTextWidth, TeamTwo, true);
    DrawBoundedLabel(View.PhaseLabel, CenterX, Margin + 10.0f * Scale, Small, CellTextWidth, Ink, true);
    DrawBoundedLabel(View.TeamOneScore, TeamOneX, Margin + 32.0f * Scale,
        32.0f * Scale, CellTextWidth, Ink, true);
    DrawBoundedLabel(View.TeamTwoScore, TeamTwoX, Margin + 32.0f * Scale,
        32.0f * Scale, CellTextWidth, Ink, true);
    if (View.bShowTimer)
        DrawBoundedLabel(View.TimerText, CenterX, Margin + 35.0f * Scale,
            28.0f * Scale, CellTextWidth, Data.Phase == EVSMatchHUDPhase::Planted ? Warning : Ink, true);
    DrawBoundedLabel(View.TeamOneRole, TeamOneX, Margin + HeaderHeight - Small - 10.0f * Scale,
        Small, CellTextWidth, Muted, true);
    DrawBoundedLabel(View.TeamTwoRole, TeamTwoX, Margin + HeaderHeight - Small - 10.0f * Scale,
        Small, CellTextWidth, Muted, true);
    DrawBoundedLabel(View.RoundLabel, CenterX, Margin + HeaderHeight - Small - 10.0f * Scale,
        Small, CellTextWidth, Muted, true);
    const float CaptionHeight = View.bShowTimer ? Small + 8.0f * Scale : 0.0f;
    if (View.bShowTimer)
    {
        DrawRect(Panel, Left + Cell, Margin + HeaderHeight + 2.0f * Scale, Cell, CaptionHeight);
        DrawBoundedLabel(View.TimerLabel, CenterX, Margin + HeaderHeight + 6.0f * Scale,
            Small, Width, Ink, true);
    }

    if (!View.bShowResult)
    {
        const float Top = Margin + HeaderHeight + CaptionHeight + 8.0f * Scale;
        const bool bHeading = !View.ObjectiveLabel.IsEmpty();
        const bool bStatus = !View.StatusText.IsEmpty();
        if (bHeading || bStatus)
        {
            const float HeadingHeight = bHeading ? FMath::Max(22.0f, 24.0f * Scale) : 0.0f;
            const float StatusHeight = bStatus ? FMath::Max(32.0f, 36.0f * Scale) : 0.0f;
            DrawRect(Panel, Left, Top, Width, HeadingHeight + StatusHeight + 16.0f * Scale);
            if (bHeading)
                DrawBoundedLabel(View.ObjectiveLabel, CenterX, Top + 8.0f * Scale,
                    16.0f * Scale, Width - 24.0f * Scale, Data.bLocalCarrier ? Accent : Ink, true);
            if (bStatus)
                DrawWrappedLabel(View.StatusText, CenterX, Top + 8.0f * Scale + HeadingHeight,
                    14.0f * Scale, Width - 24.0f * Scale, Muted, true);
        }
    }

    if (View.bShowResult)
    {
        const float Top = Canvas->SizeY * 0.5f - 24.0f * Scale;
        const float HeadHeight = FMath::Max(26.0f, 32.0f * Scale);
        const float ObjectiveHeight = View.ObjectiveLabel.IsEmpty() ? 0.0f : FMath::Max(20.0f, 24.0f * Scale);
        const float StatusHeight = View.StatusText.IsEmpty() ? 0.0f : FMath::Max(38.0f, 42.0f * Scale);
        const float HintHeight = View.RematchHint.IsEmpty() ? 0.0f : FMath::Max(22.0f, 26.0f * Scale);
        const float Height = HeadHeight + ObjectiveHeight + StatusHeight + HintHeight + 28.0f * Scale;
        const FLinearColor ResultColor = View.bDraw ? Muted : View.bLocalWinner ? Accent : Ink;
        DrawRect(Panel, Left, Top, Width, Height);
        DrawRect(ResultColor, Left, Top, Width, 3.0f * Scale);
        DrawBoundedLabel(View.ResultHeadline, CenterX, Top + 12.0f * Scale,
            26.0f * Scale, Width - 24.0f * Scale, ResultColor, true);
        if (!View.ObjectiveLabel.IsEmpty())
            DrawBoundedLabel(View.ObjectiveLabel, CenterX, Top + 12.0f * Scale + HeadHeight,
                14.0f * Scale, Width - 24.0f * Scale, Muted, true);
        if (!View.StatusText.IsEmpty())
            DrawWrappedLabel(View.StatusText, CenterX, Top + 12.0f * Scale + HeadHeight + ObjectiveHeight,
                15.0f * Scale, Width - 24.0f * Scale, Ink, true);
        if (!View.RematchHint.IsEmpty())
            DrawBoundedLabel(View.RematchHint, CenterX, Top + 12.0f * Scale + HeadHeight + ObjectiveHeight + StatusHeight,
                16.0f * Scale, Width - 24.0f * Scale, Accent, true);
    }
    else if (View.bSpectating)
    {
        const float Top = Canvas->SizeY - 160.0f * Scale;
        DrawRect(Panel, Left, Top, Width, 54.0f * Scale);
        DrawBoundedLabel(TEXT("SPECTATING"), CenterX, Top + 8.0f * Scale,
            20.0f * Scale, Width - 24.0f * Scale, Ink, true);
        DrawBoundedLabel(View.LocalRole, CenterX, Top + 32.0f * Scale,
            Small, Width - 24.0f * Scale, Muted, true);
    }
    else if (!View.InteractionPrompt.IsEmpty() || View.bShowInteractionProgress)
    {
        const float Top = Canvas->SizeY - 190.0f * Scale;
        const float TextHeight = FMath::Max(32.0f, 36.0f * Scale);
        const float Height = TextHeight + (View.bShowInteractionProgress ? 24.0f : 12.0f) * Scale;
        DrawRect(Panel, Left, Top, Width, Height);
        if (!View.InteractionPrompt.IsEmpty())
            DrawWrappedLabel(View.InteractionPrompt, CenterX, Top + 6.0f * Scale,
                16.0f * Scale, Width - 24.0f * Scale, Accent, true);
        else
            DrawLabel(TEXT("INTERACTING"), CenterX, Top + 6.0f * Scale, 16.0f * Scale, Accent, true);
        if (View.bShowInteractionProgress)
        {
            const float BarWidth = Width - 28.0f * Scale;
            const float BarY = Top + TextHeight + 8.0f * Scale;
            DrawRect(FLinearColor(0.13f, 0.19f, 0.22f), Left + 14.0f * Scale, BarY, BarWidth, 5.0f * Scale);
            DrawRect(Accent, Left + 14.0f * Scale, BarY, BarWidth * View.InteractionProgress, 5.0f * Scale);
        }
    }
}

void AVSHUD::DrawKillFeed(float Scale)
{
    namespace VSFeed = ValStrike::KillFeed;
    const UWorld* World = GetWorld();
    const UVSScoreboardSubsystem* Scoreboard = World ? World->GetSubsystem<UVSScoreboardSubsystem>() : nullptr;
    const AVSScoreboardState* State = Scoreboard ? Scoreboard->GetState() : nullptr;
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    if (!State || !GameState || State->GetKillFeed().IsEmpty())
    {
        return;
    }
    const TArray<FVSKillFeedEntry>& Feed = State->GetKillFeed();
    std::vector<VSFeed::Stamp> Stamps;
    Stamps.reserve(static_cast<std::size_t>(Feed.Num()));
    for (const FVSKillFeedEntry& Entry : Feed)
    {
        Stamps.push_back({static_cast<VSFeed::Seq>(FMath::Max(0, Entry.Sequence)), Entry.ServerTimeSeconds});
    }
    // Expiry uses the replicated server clock, so late joiners skip old lines.
    const std::vector<VSFeed::Visible> Rows = VSFeed::Select(Stamps, GameState->GetServerWorldTimeSeconds());
    if (Rows.empty())
    {
        return;
    }
    const FVSScoreboardRow* LocalRow = PlayerOwner ? State->FindRow(PlayerOwner->PlayerState) : nullptr;
    const int32 LocalKey = LocalRow ? LocalRow->PlayerKey : 0;

    const auto TeamColor = [](uint8 Team) { return Team == 1 ? Accent : Team == 2 ? TeamTwo : Ink; };
    const FLinearColor LocalPanel(0.11f, 0.17f, 0.21f, 0.94f);
    const float Margin = 24.0f * Scale;
    const float Right = Canvas->SizeX - Margin;
    const float Top = Margin + 42.0f * Scale; // Below the 34 px ESC MENU box.
    const float RowHeight = 24.0f * Scale;
    const float RowStep = RowHeight + 3.0f * Scale;
    const float TextHeight = FMath::Max(12.0f, 13.0f * Scale);
    const float TextY = (RowHeight - TextHeight) * 0.5f;
    const float Pad = 10.0f * Scale;
    const float Gap = 8.0f * Scale;
    const float MaxName = 110.0f * Scale;

    // Rows come newest first; draw the newest at the bottom so older lines sit above it.
    for (int32 Slot = 0; Slot < static_cast<int32>(Rows.size()); ++Slot)
    {
        const VSFeed::Visible& Row = Rows[Rows.size() - 1 - static_cast<std::size_t>(Slot)];
        const FVSKillFeedEntry& Entry = Feed[static_cast<int32>(Row.Index)];
        const VSFeed::Kind FeedKind = static_cast<VSFeed::Kind>(Entry.Kind);
        const bool bHasKiller = (FeedKind == VSFeed::Kind::Kill || FeedKind == VSFeed::Kind::TeamKill ||
            FeedKind == VSFeed::Kind::World) && !Entry.KillerName.IsEmpty();
        // Text labels in place of icons (art can supply a weapon icon slot later).
        FString Middle = UTF8_TO_TCHAR(VSFeed::KindLabel(FeedKind));
        if (FeedKind == VSFeed::Kind::World && bHasKiller)
        {
            Middle.Reset(); // An untracked source such as a bot still reads as a kill.
        }
        if (FeedKind != VSFeed::Kind::SelfKill && (FeedKind != VSFeed::Kind::World || bHasKiller))
        {
            const FString WeaponText = UTF8_TO_TCHAR(VSFeed::WeaponLabel(static_cast<VSFeed::Weapon>(Entry.Weapon)));
            Middle = Middle.IsEmpty() ? WeaponText : WeaponText.IsEmpty() ? Middle : Middle + TEXT(" ") + WeaponText;
            if (Entry.bHeadshot)
            {
                Middle += Middle.IsEmpty() ? TEXT("HS") : TEXT(" HS");
            }
        }
        const FString Victim = Entry.VictimName.IsEmpty() ? FString(TEXT("?")) : Entry.VictimName;
        const float KillerWidth = bHasKiller ? FMath::Min(LabelWidth(Entry.KillerName, TextHeight), MaxName) : 0.0f;
        const float MiddleWidth = Middle.IsEmpty() ? 0.0f : LabelWidth(Middle, TextHeight);
        const float VictimWidth = FMath::Min(LabelWidth(Victim, TextHeight), MaxName);
        const float Width = Pad * 2.0f + KillerWidth + (bHasKiller ? Gap : 0.0f) + MiddleWidth +
            (Middle.IsEmpty() ? 0.0f : Gap) + VictimWidth;
        const float X = Right - Width;
        const float Y = Top + RowStep * static_cast<float>(Slot);
        const float Alpha = Row.Alpha;

        const bool bLocal = VSFeed::InvolvesPlayer(Entry.KillerKey, Entry.VictimKey, LocalKey);
        DrawRect(WithAlpha(bLocal ? LocalPanel : Panel, Alpha), X, Y, Width, RowHeight);
        if (bLocal)
        {
            // Own kills and deaths: brighter panel with an outline.
            const FLinearColor Edge = WithAlpha(Ink, Alpha * 0.9f);
            const float Line = FMath::Max(1.0f, 1.5f * Scale);
            DrawRect(Edge, X, Y, Width, Line);
            DrawRect(Edge, X, Y + RowHeight - Line, Width, Line);
            DrawRect(Edge, X, Y, 3.0f * Scale, RowHeight);
            DrawRect(Edge, X + Width - Line, Y, Line, RowHeight);
        }
        float Cursor = X + Pad;
        if (bHasKiller)
        {
            DrawBoundedLabel(Entry.KillerName, Cursor, Y + TextY, TextHeight, MaxName,
                WithAlpha(TeamColor(Entry.KillerTeam), Alpha));
            Cursor += KillerWidth + Gap;
        }
        if (!Middle.IsEmpty())
        {
            const FLinearColor MiddleColor = FeedKind == VSFeed::Kind::TeamKill ? Warning
                : Entry.bHeadshot ? Headshot : Muted;
            DrawLabel(Middle, Cursor, Y + TextY, TextHeight, WithAlpha(MiddleColor, Alpha));
            Cursor += MiddleWidth + Gap;
        }
        DrawBoundedLabel(Victim, Cursor, Y + TextY, TextHeight, MaxName, WithAlpha(TeamColor(Entry.VictimTeam), Alpha));
    }
}

void AVSHUD::DrawHitMarker(const FVSHitFeedbackView& Feedback, float CenterX, float CenterY, float Scale)
{
    using VSHitFeedback::MarkerKind;
    if (Feedback.Marker == MarkerKind::None || Feedback.MarkerAlpha <= 0.0f) return;
    const bool bKill = Feedback.Marker == MarkerKind::Kill;
    const FLinearColor Color = bKill ? Warning : Feedback.Marker == MarkerKind::Headshot ? Headshot : Ink;
    // Diagonal ticks outside the reticle cross; a kill is longer and heavier.
    const float DamageBoost = 1.0f + 0.25f * FMath::Clamp(Feedback.MarkerDamage / 100.0f, 0.0f, 1.0f);
    const float Inner = (bKill ? 8.0f : 7.0f) * Scale;
    const float Outer = Inner + (bKill ? 10.0f : 7.0f) * Scale * DamageBoost;
    const float Stroke = (bKill ? 3.0f : 2.0f) * Scale;
    const FLinearColor Shadow = WithAlpha(FLinearColor::Black, Feedback.MarkerAlpha * 0.8f);
    const FLinearColor Tint = WithAlpha(Color, Feedback.MarkerAlpha);
    for (const FVector2f Dir : {FVector2f(1.0f, 1.0f), FVector2f(-1.0f, 1.0f), FVector2f(1.0f, -1.0f), FVector2f(-1.0f, -1.0f)})
    {
        const FVector2f A = FVector2f(CenterX, CenterY) + Dir * (Inner * UE_INV_SQRT_2);
        const FVector2f B = FVector2f(CenterX, CenterY) + Dir * (Outer * UE_INV_SQRT_2);
        DrawLine(A.X, A.Y, B.X, B.Y, Shadow, Stroke + 2.0f);
        DrawLine(A.X, A.Y, B.X, B.Y, Tint, Stroke);
    }
}

void AVSHUD::DrawDamageIndicators(const FVSHitFeedbackView& Feedback, float CenterX, float CenterY, float Scale)
{
    const float Radius = 92.0f * Scale;
    const float HalfArc = 14.0f;
    const int32 Segments = 6;
    for (const FVSHitFeedbackView::FIndicator& Indicator : Feedback.Indicators)
    {
        // 0 degrees points up (ahead); positive yaw is to the right on screen.
        const FLinearColor Shadow = WithAlpha(FLinearColor::Black, Indicator.Alpha * 0.7f);
        const FLinearColor Tint = WithAlpha(Warning, Indicator.Alpha * 0.95f);
        const auto Point = [&](float Degrees, float R)
        {
            const float Radians = FMath::DegreesToRadians(Degrees);
            return FVector2f(CenterX + FMath::Sin(Radians) * R, CenterY - FMath::Cos(Radians) * R);
        };
        for (int32 Index = 0; Index < Segments; ++Index)
        {
            const float From = Indicator.RelativeYawDeg - HalfArc + 2.0f * HalfArc * Index / Segments;
            const float To = Indicator.RelativeYawDeg - HalfArc + 2.0f * HalfArc * (Index + 1) / Segments;
            const FVector2f A = Point(From, Radius);
            const FVector2f B = Point(To, Radius);
            DrawLine(A.X, A.Y, B.X, B.Y, Shadow, 7.0f * Scale);
            DrawLine(A.X, A.Y, B.X, B.Y, Tint, 4.0f * Scale);
        }
        // A short pointer outside the arc marks its centre.
        const FVector2f Tip = Point(Indicator.RelativeYawDeg, Radius + 12.0f * Scale);
        const FVector2f Left = Point(Indicator.RelativeYawDeg - 5.0f, Radius + 3.0f * Scale);
        const FVector2f Right = Point(Indicator.RelativeYawDeg + 5.0f, Radius + 3.0f * Scale);
        DrawLine(Left.X, Left.Y, Tip.X, Tip.Y, Tint, 3.0f * Scale);
        DrawLine(Right.X, Right.Y, Tip.X, Tip.Y, Tint, 3.0f * Scale);
    }
}

float AVSHUD::ReticleGap(const AVSCharacter& Character, float StaticGap, float Scale)
{
    const UVSWeaponComponent* Weapon = Character.GetEquippedWeapon();
    const UVSWeaponLoadoutComponent* Loadout = Character.GetLoadoutComponent();
    const bool bReady = Weapon && (!Loadout || Loadout->GetReadiness(Weapon) != EVSWeaponReadiness::Holstered);
    const float SpreadDeg = bReady ? Weapon->GetCurrentSpreadDegrees() : 0.0f;
    const float FovDeg = PlayerOwner && PlayerOwner->PlayerCameraManager
        ? PlayerOwner->PlayerCameraManager->GetFOVAngle() : 0.0f;
    if (!bReady || (!VSCrosshairRules::IsValidSpread(SpreadDeg) && !CrosshairGap.active))
    {
        // Nothing to show and nothing still closing: today's static reticle.
        CrosshairGap.Reset();
        return StaticGap;
    }
    // Presentation clock: same world time the client's spread recovers on.
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    const double DeltaSeconds = CrosshairGap.active ? Now - CrosshairGapTime : 0.0;
    CrosshairGapTime = Now;
    const double MaxGap = 60.0 * Scale;
    const double Target = VSCrosshairRules::TargetGapPx(SpreadDeg, FovDeg, Canvas->SizeX, StaticGap, MaxGap);
    const double Gap = VSCrosshairRules::Step(CrosshairGap, Target, DeltaSeconds, StaticGap, MaxGap);
    if (Gap == static_cast<double>(StaticGap) && Target == static_cast<double>(StaticGap))
    {
        // Fully recovered: hand back the exact static value, not a rounded copy.
        CrosshairGap.Reset();
        return StaticGap;
    }
    return static_cast<float>(Gap);
}

void AVSHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner)
    {
        return;
    }

    AVSPlayerController* Controller = Cast<AVSPlayerController>(PlayerOwner);
    FVSMatchHUDData MatchData = Controller ? Controller->GetMatchHUDData() : FVSMatchHUDData{};
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (VSRoundHUDPreview::Tick(Controller, MatchData))
    {
        const FString Marker = TEXT("PRESENTATION TEST / NOT A LIVE MATCH / ") + VSRoundHUDPreview::GetFixtureName();
        const float Y = Canvas->SizeY - 14.0f;
        DrawRect(FLinearColor(0.12f, 0.01f, 0.08f, 1.0f), 0.0f, Y - 2.0f, Canvas->SizeX, 16.0f);
        DrawBoundedLabel(Marker, Canvas->SizeX * 0.5f, Y, 12.0f, Canvas->SizeX - 24.0f,
            FLinearColor(1.0f, 0.65f, 0.86f), true);
    }
#endif
    if (Controller && Controller->IsSessionMenuOpen())
    {
        return;
    }

    // Logical 1280x720 layout. Minimum scale keeps the supported 960x540 view readable.
    const float Scale = FMath::Clamp(FMath::Min(Canvas->SizeX / 1280.0f, Canvas->SizeY / 720.0f), 0.75f, 2.0f);
    const float Margin = 24.0f * Scale;
    const float CenterX = Canvas->SizeX * 0.5f;
    const float CenterY = Canvas->SizeY * 0.5f;

    DrawRect(Panel, Margin, Margin, 150.0f * Scale, 34.0f * Scale);
    DrawRect(Accent, Margin, Margin, 3.0f * Scale, 34.0f * Scale);
    DrawLabel(TEXT("VALSTRIKE"), Margin + 15.0f * Scale, Margin + 8.0f * Scale, 18.0f * Scale, Ink);

    const float MenuX = Canvas->SizeX - Margin - 124.0f * Scale;
    DrawRect(Panel, MenuX, Margin, 124.0f * Scale, 34.0f * Scale);
    DrawLabel(TEXT("ESC  MENU"), MenuX + 14.0f * Scale, Margin + 9.0f * Scale, 16.0f * Scale, Ink);

    const FVSRoundHUDPresentation MatchView = VSRoundHUDPresentation::Build(MatchData);
    // Read the controller snapshot before considering pawn lifetime: late joins,
    // spectators and result screens still need the match HUD without a pawn.
    DrawMatchHUD(MatchData, MatchView, Scale);
    // Before the pawn check: spectators and dead players still follow the feed.
    DrawKillFeed(Scale);

    const AVSCharacter* Character = Cast<AVSCharacter>(PlayerOwner->GetPawn());
    if (!Character)
    {
        CrosshairGap.Reset();
        return;
    }
    const FVSPlayerHUDData Data = Character->GetHUDData();
    // Availability is explicit: the movement-only baseline must never look dead.
    const bool bDead = Data.bHasHealth && !Data.bAlive;
    // Only server-confirmed events for this pawn; never local predictions.
    const FVSHitFeedbackView Feedback = Controller ? Controller->GetHitFeedbackView() : FVSHitFeedbackView{};
    // Alive only: the death banner below the reticle takes the lower ring area.
    if (!bDead && !MatchView.bSpectating && !MatchView.bShowResult)
    {
        DrawDamageIndicators(Feedback, CenterX, CenterY, Scale);
    }

    if (!bDead && !MatchView.bSpectating && !MatchView.bShowResult)
    {
        // Only the inner gap follows spread; the arm length stays 6 px, and the
        // static gap yields exactly the static 4/10 px reticle.
        const float StaticGap = 4.0f * Scale;
        const float Gap = ReticleGap(*Character, StaticGap, Scale);
        const float Reach = 10.0f * Scale + (Gap - StaticGap);
        const float Stroke = 2.0f * Scale;
        const auto ReticleLine = [this, Stroke](float X1, float Y1, float X2, float Y2)
        {
            DrawLine(X1, Y1, X2, Y2, FLinearColor::Black, Stroke + 2.0f);
            DrawLine(X1, Y1, X2, Y2, Accent, Stroke);
        };
        ReticleLine(CenterX - Reach, CenterY, CenterX - Gap, CenterY);
        ReticleLine(CenterX + Gap, CenterY, CenterX + Reach, CenterY);
        ReticleLine(CenterX, CenterY - Reach, CenterX, CenterY - Gap);
        ReticleLine(CenterX, CenterY + Gap, CenterX, CenterY + Reach);
        DrawHitMarker(Feedback, CenterX, CenterY, Scale);
    }
    else
    {
        CrosshairGap.Reset();
    }

    const float PanelY = Canvas->SizeY - Margin - 90.0f * Scale;
    if (Data.bHasHealth)
    {
        const float Health = FMath::IsFinite(Data.Health) ? FMath::Max(0.0f, Data.Health) : 0.0f;
        const float MaxHealth = FMath::IsFinite(Data.MaxHealth) ? FMath::Max(0.0f, Data.MaxHealth) : 0.0f;
        const float Fraction = MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f;
        const FLinearColor HealthColor = bDead || (MaxHealth > 0.0f && Fraction <= 0.25f) ? Warning : Accent;
        DrawRect(Panel, Margin, PanelY, 190.0f * Scale, 90.0f * Scale);
        DrawLabel(TEXT("HEALTH"), Margin + 16.0f * Scale, PanelY + 11.0f * Scale, 13.0f * Scale, Muted);
        DrawLabel(FString::Printf(TEXT("%.0f"), Health), Margin + 16.0f * Scale,
            PanelY + 31.0f * Scale, 34.0f * Scale, Ink);
        if (MaxHealth > 0.0f)
        {
            DrawLabel(FString::Printf(TEXT("/ %.0f"), MaxHealth), Margin + 100.0f * Scale,
                PanelY + 44.0f * Scale, 16.0f * Scale, Muted);
            DrawRect(FLinearColor(0.13f, 0.19f, 0.22f), Margin + 16.0f * Scale,
                PanelY + 77.0f * Scale, 158.0f * Scale, 3.0f * Scale);
            DrawRect(HealthColor, Margin + 16.0f * Scale, PanelY + 77.0f * Scale,
                158.0f * Scale * Fraction, 3.0f * Scale);
        }
    }

    if (Data.bHasWeapon)
    {
        const float AmmoX = Canvas->SizeX - Margin - 230.0f * Scale;
        DrawRect(Panel, AmmoX, PanelY, 230.0f * Scale, 90.0f * Scale);
        // Equipped weapon name; muted with a status while its equip time runs.
        const FString WeaponTitle = Data.WeaponName.IsEmpty() ? FString(TEXT("AMMO")) : Data.WeaponName;
        DrawBoundedLabel(Data.bEquipping ? WeaponTitle + TEXT("  EQUIPPING") : WeaponTitle,
            AmmoX + 16.0f * Scale, PanelY + 11.0f * Scale, 13.0f * Scale, 198.0f * Scale,
            Data.bEquipping || Data.WeaponName.IsEmpty() ? Muted : Accent);
        DrawLabel(FString::FromInt(FMath::Max(0, Data.MagazineAmmo)), AmmoX + 16.0f * Scale,
            PanelY + 31.0f * Scale, 34.0f * Scale, Data.MagazineAmmo <= 0 ? Warning : Ink);
        DrawLabel(FString::Printf(TEXT("/ %d"), FMath::Max(0, Data.MagazineCapacity)),
            AmmoX + 89.0f * Scale, PanelY + 44.0f * Scale, 16.0f * Scale, Muted);
        DrawLabel(FString::Printf(TEXT("%d RESERVE"), FMath::Max(0, Data.ReserveAmmo)),
            AmmoX + 16.0f * Scale, PanelY + 72.0f * Scale, 12.0f * Scale, Muted);
    }
    else if (!Data.WeaponName.IsEmpty())
    {
        // Knife: its label only, no ammo, magazine or reload. The reticle stays
        // static because no firearm spread is equipped.
        const float AmmoX = Canvas->SizeX - Margin - 230.0f * Scale;
        DrawRect(Panel, AmmoX, PanelY, 230.0f * Scale, 90.0f * Scale);
        DrawBoundedLabel(Data.bEquipping ? Data.WeaponName + TEXT("  EQUIPPING") : Data.WeaponName,
            AmmoX + 16.0f * Scale, PanelY + 11.0f * Scale, 13.0f * Scale, 198.0f * Scale, Data.bEquipping ? Muted : Accent);
        DrawLabel(TEXT("LMB  SLASH    RMB  STAB"), AmmoX + 16.0f * Scale, PanelY + 44.0f * Scale, 12.0f * Scale, Muted);
    }

    if (bDead && !MatchView.bSpectating && !MatchView.bShowResult)
    {
        const float BannerY = CenterY + 48.0f * Scale;
        // The killer line stays until respawn; without a confirmed kill only the title shows.
        const bool bKiller = Feedback.bEliminated && !Feedback.EliminatedText.IsEmpty();
        const float BannerHeight = (bKiller ? 76.0f : 52.0f) * Scale;
        DrawRect(Panel, CenterX - 160.0f * Scale, BannerY, 320.0f * Scale, BannerHeight);
        DrawRect(Warning, CenterX - 160.0f * Scale, BannerY, 3.0f * Scale, BannerHeight);
        DrawLabel(TEXT("ELIMINATED"), CenterX, BannerY + 15.0f * Scale, 22.0f * Scale, Ink, true);
        if (bKiller)
            DrawBoundedLabel(Feedback.EliminatedText, CenterX, BannerY + 46.0f * Scale,
                14.0f * Scale, 296.0f * Scale, Muted, true);
    }
    else if (Data.bHasWeapon && Data.bReloading && !MatchView.bSpectating && !MatchView.bShowResult)
    {
        const float BannerY = CenterY + 42.0f * Scale;
        DrawRect(Panel, CenterX - 86.0f * Scale, BannerY, 172.0f * Scale, 32.0f * Scale);
        DrawLabel(TEXT("RELOADING"), CenterX, BannerY + 8.0f * Scale, 16.0f * Scale, Accent, true);
    }

    if (!bDead && !MatchView.bSpectating && !MatchView.bShowResult)
    {
        const float HelpY = Canvas->SizeY - Margin - 48.0f * Scale;
        DrawRect(Panel, CenterX - 198.0f * Scale, HelpY, 396.0f * Scale, 48.0f * Scale);
        // The knife is armed but has no ammo: weapon help without the reload key.
        const bool bKnife = !Data.bHasWeapon && !Data.WeaponName.IsEmpty();
        DrawLabel(Data.bHasWeapon || bKnife ? TEXT("WASD  MOVE    SPACE  JUMP    SHIFT  WALK    CTRL  CROUCH")
            : TEXT("WASD  MOVE    MOUSE  LOOK    SPACE  JUMP"), CenterX,
            HelpY + 8.0f * Scale, 12.0f * Scale, Muted, true);
        DrawLabel(Data.bHasWeapon ? TEXT("R  RELOAD    1  RIFLE    2  PISTOL    Q  LAST WEAPON")
            : bKnife ? TEXT("1  RIFLE    2  PISTOL    3  KNIFE    Q  LAST WEAPON") : TEXT("SHIFT  WALK    CTRL  CROUCH"),
            CenterX, HelpY + 27.0f * Scale, 12.0f * Scale, Muted, true);
    }
}
