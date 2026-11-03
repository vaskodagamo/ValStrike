#include "SVSScoreboard.h"

#include "Engine/UserInterfaceSettings.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
// Same family as the HUD and session menu; unique names for the unity build.
const FLinearColor VSBoardPanel(0.010f, 0.018f, 0.030f, 0.90f);
const FLinearColor VSBoardStripe(0.030f, 0.050f, 0.070f, 0.95f);
const FLinearColor VSBoardLocal(0.30f, 0.95f, 0.79f, 0.14f);
const FLinearColor VSBoardInk(0.94f, 0.97f, 0.98f);
const FLinearColor VSBoardMuted(0.62f, 0.71f, 0.75f);
const FLinearColor VSBoardDead(1.0f, 0.42f, 0.36f);
const FLinearColor VSBoardTeamOne(0.30f, 0.95f, 0.79f);
const FLinearColor VSBoardTeamTwo(1.0f, 0.72f, 0.38f);

constexpr float VSBoardNameWidth = 220.0f;
constexpr float VSBoardStatWidth = 50.0f;
constexpr float VSBoardDamageWidth = 64.0f;
constexpr float VSBoardStatusWidth = 104.0f;

FLinearColor VSBoardTeamColor(uint8 TeamId)
{
    return TeamId == 1 ? VSBoardTeamOne : TeamId == 2 ? VSBoardTeamTwo : VSBoardInk;
}

TSharedRef<STextBlock> VSBoardText(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false,
    ETextJustify::Type Justify = ETextJustify::Left)
{
    return SNew(STextBlock)
        .Text(FText::FromString(Text))
        .Font(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size))
        .ColorAndOpacity(Color)
        .Justification(Justify)
        .ShadowOffset(FVector2D(1.0f, 1.0f))
        .ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
}

TSharedRef<SWidget> VSBoardCell(const FString& Text, float Width, int32 Size, const FLinearColor& Color, bool bBold,
    ETextJustify::Type Justify)
{
    return SNew(SBox).WidthOverride(Width).VAlign(VAlign_Center)[VSBoardText(Text, Size, Color, bBold, Justify)];
}

FString VSBoardLifeLabel(EVSScoreboardLife Life)
{
    switch (Life)
    {
    case EVSScoreboardLife::Alive: return TEXT("ALIVE");
    case EVSScoreboardLife::Dead: return TEXT("DEAD");
    default: return TEXT("SPECTATING");
    }
}
}

void SVSScoreboard::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    SetVisibility(EVisibility::Collapsed);
    SetCanTick(false);
    ChildSlot
    [
        // Mirrors the session menu: undo the engine DPI curve, which halves Slate
        // at 540p, so the board stays readable at 960 x 540 without config changes.
        SNew(SDPIScaler).DPIScale_Lambda([this]() {
            int32 Width = 960, Height = 540;
            if (Controller.IsValid()) { Controller->GetViewportSize(Width, Height); }
            Width = FMath::Max(Width, 1);
            Height = FMath::Max(Height, 1);
            const float EngineScale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Width, Height));
            const float ReadableScale = FMath::Clamp(FMath::Min(Width / 1280.0f, Height / 720.0f), 1.0f, 1.4f);
            return ReadableScale / FMath::Max(EngineScale, 0.01f);
        })
        [
            // Below the round HUD's score strip so team scores stay visible.
            SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(16.0f, 112.0f, 16.0f, 16.0f))
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(VSBoardPanel)
                .Padding(FMargin(18.0f, 14.0f))
                [
                    SAssignNew(Body, SVerticalBox)
                ]
            ]
        ]
    ];
}

TSharedRef<SWidget> SVSScoreboard::MakeLine(const FVSScoreboardLine& Line, bool bHeader) const
{
    const int32 Size = bHeader ? 11 : 13;
    const bool bDead = !bHeader && Line.Life != EVSScoreboardLife::Alive;
    const FLinearColor RowColor = bHeader ? VSBoardMuted : (bDead ? VSBoardMuted : VSBoardInk);
    const auto Number = [&](int32 Value, const TCHAR* Header) { return bHeader ? FString(Header) : FString::FromInt(Value); };
    const FString Ping = bHeader ? FString(TEXT("PING")) : (Line.PingMs >= 0 ? FString::FromInt(Line.PingMs) : FString(TEXT("—")));
    const FLinearColor StatusColor = bHeader ? VSBoardMuted :
        Line.Life == EVSScoreboardLife::Alive ? VSBoardInk : Line.Life == EVSScoreboardLife::Dead ? VSBoardDead : VSBoardMuted;
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(!bHeader && Line.bLocal ? VSBoardLocal : FLinearColor::Transparent)
        .Padding(FMargin(8.0f, bHeader ? 2.0f : 4.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [ VSBoardCell(bHeader ? FString(TEXT("PLAYER")) : Line.Name, VSBoardNameWidth, Size, RowColor, !bHeader && Line.bLocal, ETextJustify::Left) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ VSBoardCell(Number(Line.Kills, TEXT("K")), VSBoardStatWidth, Size, RowColor, !bHeader, ETextJustify::Right) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ VSBoardCell(Number(Line.Deaths, TEXT("D")), VSBoardStatWidth, Size, RowColor, false, ETextJustify::Right) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ VSBoardCell(Number(Line.Assists, TEXT("A")), VSBoardStatWidth, Size, RowColor, false, ETextJustify::Right) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ VSBoardCell(Number(Line.HeadshotKills, TEXT("HS")), VSBoardStatWidth, Size, RowColor, false, ETextJustify::Right) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ VSBoardCell(Number(Line.DamageDealt, TEXT("DMG")), VSBoardDamageWidth, Size, RowColor, false, ETextJustify::Right) ]
            + SHorizontalBox::Slot().AutoWidth()
            [ VSBoardCell(Ping, VSBoardStatWidth + 8.0f, Size, RowColor, false, ETextJustify::Right) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(FMargin(14.0f, 0.0f, 0.0f, 0.0f))
            [ VSBoardCell(bHeader ? FString(TEXT("STATUS")) : VSBoardLifeLabel(Line.Life), VSBoardStatusWidth, Size, StatusColor, !bHeader && !bDead, ETextJustify::Left) ]
        ];
}

TSharedRef<SWidget> SVSScoreboard::MakeTeam(const FVSScoreboardTeam& Team) const
{
    const FLinearColor Color = VSBoardTeamColor(Team.TeamId);
    TSharedRef<SVerticalBox> Section = SNew(SVerticalBox);
    const FString Heading = Team.Role.IsEmpty() ? Team.Title : Team.Title + TEXT("  ·  ") + Team.Role;
    Section->AddSlot().AutoHeight()
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(VSBoardStripe)
        .Padding(FMargin(8.0f, 5.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [ VSBoardText(Heading, 14, Color, true) ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [ VSBoardText(Team.Score >= 0 ? FString::FromInt(Team.Score) : FString(), 20, Color, true, ETextJustify::Right) ]
        ]
    ];
    Section->AddSlot().AutoHeight().Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))[MakeLine(FVSScoreboardLine(), true)];
    if (Team.Lines.IsEmpty())
    {
        Section->AddSlot().AutoHeight().Padding(FMargin(8.0f, 4.0f))[VSBoardText(TEXT("No players"), 12, VSBoardMuted)];
    }
    for (const FVSScoreboardLine& Line : Team.Lines)
    {
        Section->AddSlot().AutoHeight()[MakeLine(Line, false)];
    }
    return Section;
}

void SVSScoreboard::SetModel(const FVSScoreboardModel& Model)
{
    const FString Next = Model.Signature();
    if (Next == Signature || !Body.IsValid())
    {
        return;
    }
    Signature = Next;
    Body->ClearChildren();
    Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 0.0f, 0.0f, 10.0f))
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [ VSBoardText(TEXT("SCOREBOARD"), 17, VSBoardInk, true) ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ VSBoardText(Model.Status, 12, VSBoardMuted, false, ETextJustify::Right) ]
    ];
    if (!Model.bHasStats)
    {
        Body->AddSlot().AutoHeight()[VSBoardText(TEXT("Waiting for match statistics…"), 13, VSBoardMuted)];
        return;
    }
    for (int32 Index = 0; Index < Model.Teams.Num(); ++Index)
    {
        Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, Index == 0 ? 0.0f : 12.0f, 0.0f, 0.0f))[MakeTeam(Model.Teams[Index])];
    }
    Body->AddSlot().AutoHeight().Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
    [ VSBoardText(TEXT("K kills  ·  D deaths  ·  A assists  ·  HS headshot kills  ·  DMG damage dealt"), 10, VSBoardMuted) ];
}
