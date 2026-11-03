#include "VSHUD.h"

#include "Engine/Canvas.h"

void AVSHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas)
    {
        return;
    }

    const float X = Canvas->SizeX * 0.5f;
    const float Y = Canvas->SizeY * 0.5f;
    const FLinearColor Color(0.3f, 1.0f, 0.8f);
    DrawLine(X - 10.0f, Y, X - 4.0f, Y, Color, 2.0f);
    DrawLine(X + 4.0f, Y, X + 10.0f, Y, Color, 2.0f);
    DrawLine(X, Y - 10.0f, X, Y - 4.0f, Color, 2.0f);
    DrawLine(X, Y + 4.0f, X, Y + 10.0f, Color, 2.0f);

    DrawText(TEXT("ValStrike | Movement prototype"), FLinearColor::White, 24.0f, 24.0f);
    DrawText(TEXT("WASD: move   Mouse: look   Shift: walk   Ctrl: crouch   Space: jump"),
        FLinearColor::White, 24.0f, Canvas->SizeY - 40.0f);
}
