#include "VSHUD.h"

#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "VSCharacter.h"
#include "VSPlayerController.h"

namespace
{
const FLinearColor Panel(0.015f, 0.026f, 0.04f, 0.88f);
const FLinearColor Ink(0.94f, 0.97f, 0.98f);
const FLinearColor Muted(0.66f, 0.76f, 0.80f);
const FLinearColor Accent(0.30f, 0.95f, 0.79f);
const FLinearColor Warning(1.0f, 0.40f, 0.34f);
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

void AVSHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !PlayerOwner)
    {
        return;
    }

    const AVSPlayerController* Controller = Cast<AVSPlayerController>(PlayerOwner);
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

    const AVSCharacter* Character = Cast<AVSCharacter>(PlayerOwner->GetPawn());
    if (!Character)
    {
        return;
    }
    const FVSPlayerHUDData Data = Character->GetHUDData();
    // Availability is explicit: the movement-only baseline must never look dead.
    const bool bDead = Data.bHasHealth && !Data.bAlive;

    if (!bDead)
    {
        const float Gap = 4.0f * Scale;
        const float Reach = 10.0f * Scale;
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
        DrawLabel(TEXT("AMMO"), AmmoX + 16.0f * Scale, PanelY + 11.0f * Scale, 13.0f * Scale, Muted);
        DrawLabel(FString::FromInt(FMath::Max(0, Data.MagazineAmmo)), AmmoX + 16.0f * Scale,
            PanelY + 31.0f * Scale, 34.0f * Scale, Data.MagazineAmmo <= 0 ? Warning : Ink);
        DrawLabel(FString::Printf(TEXT("/ %d"), FMath::Max(0, Data.MagazineCapacity)),
            AmmoX + 89.0f * Scale, PanelY + 44.0f * Scale, 16.0f * Scale, Muted);
        DrawLabel(FString::Printf(TEXT("%d RESERVE"), FMath::Max(0, Data.ReserveAmmo)),
            AmmoX + 16.0f * Scale, PanelY + 72.0f * Scale, 12.0f * Scale, Muted);
    }

    if (bDead)
    {
        const float BannerY = CenterY + 48.0f * Scale;
        DrawRect(Panel, CenterX - 160.0f * Scale, BannerY, 320.0f * Scale, 52.0f * Scale);
        DrawRect(Warning, CenterX - 160.0f * Scale, BannerY, 3.0f * Scale, 52.0f * Scale);
        DrawLabel(TEXT("ELIMINATED"), CenterX, BannerY + 15.0f * Scale, 22.0f * Scale, Ink, true);
    }
    else if (Data.bHasWeapon && Data.bReloading)
    {
        const float BannerY = CenterY + 42.0f * Scale;
        DrawRect(Panel, CenterX - 86.0f * Scale, BannerY, 172.0f * Scale, 32.0f * Scale);
        DrawLabel(TEXT("RELOADING"), CenterX, BannerY + 8.0f * Scale, 16.0f * Scale, Accent, true);
    }

    if (!bDead)
    {
        const float HelpY = Canvas->SizeY - Margin - 48.0f * Scale;
        DrawRect(Panel, CenterX - 198.0f * Scale, HelpY, 396.0f * Scale, 48.0f * Scale);
        DrawLabel(TEXT("WASD  MOVE    MOUSE  LOOK    SPACE  JUMP"), CenterX,
            HelpY + 8.0f * Scale, 12.0f * Scale, Muted, true);
        DrawLabel(Data.bHasWeapon ? TEXT("SHIFT  WALK    CTRL  CROUCH    R  RELOAD") : TEXT("SHIFT  WALK    CTRL  CROUCH"),
            CenterX, HelpY + 27.0f * Scale, 12.0f * Scale, Muted, true);
    }
}
