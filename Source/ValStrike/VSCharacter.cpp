#include "VSCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"
#include "ValStrike.h"

AVSCharacter::AVSCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    BaseEyeHeight = 64.0f;
    CrouchedEyeHeight = 32.0f;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = false;
    Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
    Movement->MaxWalkSpeed = 450.0f;
    Movement->MaxWalkSpeedCrouched = 180.0f;
    Movement->MinAnalogWalkSpeed = 0.0f;
    Movement->MaxAcceleration = 2048.0f;
    Movement->BrakingDecelerationWalking = 2048.0f;
    Movement->JumpZVelocity = 420.0f;
    Movement->AirControl = 0.15f;
    Movement->SetCrouchedHalfHeight(56.0f);

    FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
    FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, BaseEyeHeight));
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->FieldOfView = 90.0f;

    // The engine moves this mesh during network smoothing. Attach the temporary
    // static visuals to it so they follow the same presentation transform.
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    BodyVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyVisual"));
    BodyVisual->SetupAttachment(GetMesh());
    BodyVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BodyVisual->SetOwnerNoSee(true);

    HeadVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadVisual"));
    HeadVisual->SetupAttachment(GetMesh());
    HeadVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadVisual->SetOwnerNoSee(true);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Cube.Succeeded())
    {
        BodyVisual->SetStaticMesh(Cube.Object);
    }
    if (Sphere.Succeeded())
    {
        HeadVisual->SetStaticMesh(Sphere.Object);
    }
    UpdatePrototypeVisuals();
}

FVSPlayerHUDData AVSCharacter::GetHUDData() const
{
    return {}; // Combat workstream supplies replicated state when implemented.
}

void AVSCharacter::StopCombatInput()
{
    // Combat workstream releases held fire here. No weapon exists at this baseline.
}

void AVSCharacter::BeginPlay()
{
    Super::BeginPlay();
    UpdatePrototypeVisuals();
}

void AVSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (InputSubsystem.IsValid() && MovementMapping)
    {
        InputSubsystem->RemoveMappingContext(MovementMapping);
    }
    Super::EndPlay(EndPlayReason);
}

void AVSCharacter::CreateInputMappings()
{
    if (MovementMapping)
    {
        return;
    }

    MovementMapping = NewObject<UInputMappingContext>(this, TEXT("MovementMapping"));
    const auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
    {
        UInputAction* Action = NewObject<UInputAction>(this, FName(Name));
        Action->ValueType = Type;
        return Action;
    };

    MoveAction = MakeAction(TEXT("Move"), EInputActionValueType::Axis2D);
    MoveAction->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
    LookYawAction = MakeAction(TEXT("LookYaw"), EInputActionValueType::Axis1D);
    LookPitchAction = MakeAction(TEXT("LookPitch"), EInputActionValueType::Axis1D);
    JumpAction = MakeAction(TEXT("Jump"), EInputActionValueType::Boolean);
    CrouchAction = MakeAction(TEXT("Crouch"), EInputActionValueType::Boolean);
    WalkAction = MakeAction(TEXT("Walk"), EInputActionValueType::Boolean);

    const auto MapMovementKey = [this](FKey Key, bool bNegate, bool bUseYAxis)
    {
        FEnhancedActionKeyMapping& Mapping = MovementMapping->MapKey(MoveAction, Key);
        if (bNegate)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MovementMapping));
        }
        if (bUseYAxis)
        {
            UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(MovementMapping);
            Swizzle->Order = EInputAxisSwizzle::YXZ;
            Mapping.Modifiers.Add(Swizzle);
        }
    };
    MapMovementKey(EKeys::W, false, true);
    MapMovementKey(EKeys::S, true, true);
    MapMovementKey(EKeys::A, true, false);
    MapMovementKey(EKeys::D, false, false);
    MovementMapping->MapKey(LookYawAction, EKeys::MouseX);
    MovementMapping->MapKey(LookPitchAction, EKeys::MouseY);
    MovementMapping->MapKey(JumpAction, EKeys::SpaceBar);
    MovementMapping->MapKey(CrouchAction, EKeys::LeftControl);
    MovementMapping->MapKey(WalkAction, EKeys::LeftShift);
}

void AVSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    APlayerController* PlayerController = Cast<APlayerController>(GetController());
    if (!Input || !PlayerController || !PlayerController->GetLocalPlayer())
    {
        UE_LOG(LogValStrike, Warning, TEXT("Local movement input requires Enhanced Input and a local player."));
        return;
    }

    CreateInputMappings();
    InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
    if (InputSubsystem.IsValid())
    {
        InputSubsystem->RemoveMappingContext(MovementMapping);
        InputSubsystem->AddMappingContext(MovementMapping, 0);
    }

    Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AVSCharacter::Move);
    Input->BindAction(LookYawAction, ETriggerEvent::Triggered, this, &AVSCharacter::LookYaw);
    Input->BindAction(LookPitchAction, ETriggerEvent::Triggered, this, &AVSCharacter::LookPitch);
    Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
    Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
    Input->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ACharacter::StopJumping);
    Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AVSCharacter::BeginCrouch);
    Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AVSCharacter::EndCrouch);
    Input->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &AVSCharacter::EndCrouch);
    Input->BindAction(WalkAction, ETriggerEvent::Started, this, &AVSCharacter::BeginWalk);
    Input->BindAction(WalkAction, ETriggerEvent::Completed, this, &AVSCharacter::EndWalk);
    Input->BindAction(WalkAction, ETriggerEvent::Canceled, this, &AVSCharacter::EndWalk);

    PlayerController->SetInputMode(FInputModeGameOnly());
    PlayerController->bShowMouseCursor = false;
}

void AVSCharacter::Move(const FInputActionValue& Value)
{
    if (!Controller)
    {
        return;
    }

    const FVector2D Axis = Value.Get<FVector2D>();
    const FVector LocalDirection = FVector(Axis.Y, Axis.X, 0.0f).GetClampedToMaxSize(1.0f);
    const FRotator YawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
    // Analog magnitude is part of the engine's saved acceleration. This keeps
    // walking on the normal prediction path, including its lower acceleration.
    AddMovementInput(YawRotation.RotateVector(LocalDirection), bWalkHeld ? WalkInputScale : 1.0f);
}

void AVSCharacter::LookYaw(const FInputActionValue& Value)
{
    AddControllerYawInput(Value.Get<float>() * MouseSensitivity);
}

void AVSCharacter::LookPitch(const FInputActionValue& Value)
{
    AddControllerPitchInput(-Value.Get<float>() * MouseSensitivity);
}

void AVSCharacter::BeginCrouch() { Crouch(); }
void AVSCharacter::EndCrouch() { UnCrouch(); }
void AVSCharacter::BeginWalk() { bWalkHeld = true; }
void AVSCharacter::EndWalk() { bWalkHeld = false; }

void AVSCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    UpdatePrototypeVisuals();
}

void AVSCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    UpdatePrototypeVisuals();
}

void AVSCharacter::UpdatePrototypeVisuals()
{
    const float Height = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * 2.0f;
    const float BodyHeight = Height - 32.0f;
    BodyVisual->SetRelativeLocation(FVector(0.0f, 0.0f, BodyHeight * 0.5f));
    BodyVisual->SetRelativeScale3D(FVector(0.5f, 0.4f, BodyHeight / 100.0f));
    HeadVisual->SetRelativeLocation(FVector(0.0f, 0.0f, Height - 16.0f));
    HeadVisual->SetRelativeScale3D(FVector(0.32f));
    FirstPersonCamera->SetRelativeLocation(FVector(0.0f, 0.0f, Height * 0.5f - 24.0f));
}
