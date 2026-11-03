#include "VSGameMode.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerStart.h"
#include "VSCharacter.h"
#include "VSHUD.h"
#include "VSPlayerController.h"
#include "ValStrike.h"

AVSGameMode::AVSGameMode()
{
    DefaultPawnClass = AVSCharacter::StaticClass();
    HUDClass = AVSHUD::StaticClass();
    PlayerControllerClass = AVSPlayerController::StaticClass();
}

AActor* AVSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    const UCapsuleComponent* Capsule = GetDefault<AVSCharacter>()->GetCapsuleComponent();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(
        Capsule->GetUnscaledCapsuleRadius(), Capsule->GetUnscaledCapsuleHalfHeight());
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VSSpawnCheck), false);
    if (Player && Player->GetPawn())
    {
        QueryParams.AddIgnoredActor(Player->GetPawn());
    }

    APlayerStart* BestStart = nullptr;
    double BestDistanceSquared = -1.0;
    for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
    {
        APlayerStart* Start = *It;
        if (GetWorld()->OverlapBlockingTestByChannel(Start->GetActorLocation(),
            FQuat::Identity, ECC_Pawn, Shape, QueryParams))
        {
            continue;
        }

        double NearestPlayerSquared = TNumericLimits<double>::Max();
        for (TActorIterator<AVSCharacter> CharacterIt(GetWorld()); CharacterIt; ++CharacterIt)
        {
            if (!Player || *CharacterIt != Player->GetPawn())
            {
                NearestPlayerSquared = FMath::Min(NearestPlayerSquared,
                    FVector::DistSquared(Start->GetActorLocation(), CharacterIt->GetActorLocation()));
            }
        }
        if (NearestPlayerSquared > BestDistanceSquared)
        {
            BestStart = Start;
            BestDistanceSquared = NearestPlayerSquared;
        }
    }

    if (!BestStart)
    {
        UE_LOG(LogValStrike, Warning, TEXT("No unobstructed PlayerStart; add free starts to the graybox map."));
    }
    return BestStart;
}
