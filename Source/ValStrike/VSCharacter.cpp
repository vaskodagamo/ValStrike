#include "VSCharacter.h"

#include "Camera/CameraComponent.h"
#include "Combat/VSHealthComponent.h"
#include "Combat/VSPistolComponent.h"
#include "Combat/VSRifleComponent.h"
#include "Combat/VSWeaponLoadoutComponent.h"
#include "Combat/VSCombatSmoke.h"
#include "Presentation/VSPistolPresentationComponent.h"
#include "Presentation/VSRiflePresentationComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
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
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "ValStrike.h"

AVSCharacter::AVSCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 88.0f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
    Health = CreateDefaultSubobject<UVSHealthComponent>(TEXT("Health"));
    Rifle = CreateDefaultSubobject<UVSRifleComponent>(TEXT("Rifle"));
    Pistol = CreateDefaultSubobject<UVSPistolComponent>(TEXT("Pistol"));
    Loadout = CreateDefaultSubobject<UVSWeaponLoadoutComponent>(TEXT("Loadout"));
    RiflePresentation = CreateDefaultSubobject<UVSRiflePresentationComponent>(TEXT("RiflePresentation"));
    PistolPresentation = CreateDefaultSubobject<UVSPistolPresentationComponent>(TEXT("PistolPresentation"));
    BodyHitRegion = CreateDefaultSubobject<UBoxComponent>(TEXT("BodyHitRegion"));
    HeadHitRegion = CreateDefaultSubobject<USphereComponent>(TEXT("HeadHitRegion"));
    for (UShapeComponent* Region : {static_cast<UShapeComponent*>(BodyHitRegion), static_cast<UShapeComponent*>(HeadHitRegion)})
    {
        Region->SetupAttachment(GetCapsuleComponent());
        Region->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Region->SetCollisionResponseToAllChannels(ECR_Ignore);
        Region->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Region->SetGenerateOverlapEvents(false);
        Region->SetCanEverAffectNavigation(false);
    }
    HeadHitRegion->SetSphereRadius(16.0f);
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
    FVSPlayerHUDData Data;
    Data.bHasHealth = true;
    Data.Health = Health->GetHealth();
    Data.MaxHealth = Health->GetMaxHealth();
    Data.bAlive = Health->IsAlive();
    // Ammo and reload fields describe the equipped weapon (the rifle by default).
    if (const UVSWeaponComponent* Weapon = GetEquippedWeapon())
    {
        Data.bHasWeapon = true;
        Data.MagazineAmmo = Weapon->GetState().Magazine;
        Data.MagazineCapacity = Weapon->GetMagazineCapacity();
        Data.ReserveAmmo = Weapon->GetState().Reserve;
        Data.bReloading = Weapon->GetState().bReloading;
        Data.WeaponName = Weapon->GetDisplayName();
        Data.bEquipping = Loadout->IsEquipping();
    }
    return Data;
}

UVSWeaponComponent* AVSCharacter::GetWeapon(EVSWeaponSlot Slot) const
{
    return Loadout ? Loadout->GetWeapon(Slot) : nullptr;
}

UVSWeaponComponent* AVSCharacter::GetEquippedWeapon() const
{
    return Loadout ? Loadout->GetEquippedWeapon() : nullptr;
}

EVSWeaponSlot AVSCharacter::GetEquippedWeaponSlot() const
{
    return Loadout ? Loadout->GetEquippedSlot() : EVSWeaponSlot::Rifle;
}

uint32 AVSCharacter::GetAcceptedShotCount(EVSWeaponSlot Slot) const
{
    const UVSWeaponComponent* Weapon = GetWeapon(Slot);
    return Weapon ? Weapon->GetState().ShotsFired : 0;
}

UVSWeaponPresentationComponent* AVSCharacter::GetWeaponPresentation(EVSWeaponSlot Slot) const
{
    switch (Slot)
    {
    case EVSWeaponSlot::Rifle: return RiflePresentation;
    case EVSWeaponSlot::Pistol: return PistolPresentation;
    }
    return nullptr;
}

void AVSCharacter::StopCombatInput()
{
    Rifle->StopFire();
    Pistol->StopFire();
}

bool AVSCharacter::CanMatchMove() const
{
    return bMatchMovementAllowed && Health && Health->IsAlive();
}

bool AVSCharacter::CanMatchCombat() const
{
    return bMatchCombatAllowed && Health && Health->IsAlive();
}

void AVSCharacter::SetMatchGates(bool bMove, bool bCombat)
{
    if (!HasAuthority()) { return; }
    const bool bChanged = bMatchMovementAllowed != bMove || bMatchCombatAllowed != bCombat;
    bMatchMovementAllowed = bMove;
    bMatchCombatAllowed = bCombat;
    OnRep_MatchGates();
    if (bChanged) { ForceNetUpdate(); }
}

void AVSCharacter::OnRep_MatchGates()
{
    if (!CanMatchCombat())
    {
        // Also cancels an authoritative reload; opening a gate never resumes
        // previously held fire or a canceled reload.
        Rifle->OwnerDied();
        Pistol->OwnerDied();
    }
    UCharacterMovementComponent* Movement = GetCharacterMovement();
    if (!CanMatchMove())
    {
        bWalkHeld = false;
        StopJumping();
        UnCrouch();
        ConsumeMovementInputVector();
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
    }
    else if (Movement->MovementMode == MOVE_None)
    {
        Movement->SetMovementMode(MOVE_Walking);
    }
}

void AVSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AVSCharacter, bMatchMovementAllowed);
    DOREPLIFETIME(AVSCharacter, bMatchCombatAllowed);
}

void AVSCharacter::BeginPlay()
{
    Super::BeginPlay();
    Health->OnLifeStateChanged.AddUObject(this, &AVSCharacter::HandleHealthChanged);
    UpdatePrototypeVisuals();
    HandleHealthChanged();
    // Weapon visuals are cosmetic; a dedicated server never creates them.
    if (GetNetMode() != NM_DedicatedServer)
    {
        if (!RiflePresentation->Initialize(this, FirstPersonCamera, GetMesh()))
        {
            UE_LOG(LogValStrike, Error, TEXT("Rifle presentation failed to initialize on %s; no rifle visual is shown."), *GetName());
        }
        if (!PistolPresentation->Initialize(this, FirstPersonCamera, GetMesh()))
        {
            UE_LOG(LogValStrike, Error, TEXT("Pistol presentation failed to initialize on %s; no pistol visual is shown."), *GetName());
        }
    }
    ValStrikeTests::StartCombatSmoke();
}

void AVSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCombatInput();
    Health->OnLifeStateChanged.RemoveAll(this);
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
    FireAction = MakeAction(TEXT("Fire"), EInputActionValueType::Boolean);
    ReloadAction = MakeAction(TEXT("Reload"), EInputActionValueType::Boolean);
    EquipRifleAction = MakeAction(TEXT("EquipRifle"), EInputActionValueType::Boolean);
    EquipPistolAction = MakeAction(TEXT("EquipPistol"), EInputActionValueType::Boolean);
    EquipLastAction = MakeAction(TEXT("EquipLast"), EInputActionValueType::Boolean);

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
    MovementMapping->MapKey(FireAction, EKeys::LeftMouseButton);
    MovementMapping->MapKey(ReloadAction, EKeys::R);
    MovementMapping->MapKey(EquipRifleAction, EKeys::One);
    MovementMapping->MapKey(EquipPistolAction, EKeys::Two);
    MovementMapping->MapKey(EquipLastAction, EKeys::Q);
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
    Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AVSCharacter::BeginJump);
    Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
    Input->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ACharacter::StopJumping);
    Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AVSCharacter::BeginCrouch);
    Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AVSCharacter::EndCrouch);
    Input->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &AVSCharacter::EndCrouch);
    Input->BindAction(WalkAction, ETriggerEvent::Started, this, &AVSCharacter::BeginWalk);
    Input->BindAction(WalkAction, ETriggerEvent::Completed, this, &AVSCharacter::EndWalk);
    Input->BindAction(WalkAction, ETriggerEvent::Canceled, this, &AVSCharacter::EndWalk);

    Input->BindAction(FireAction, ETriggerEvent::Started, this, &AVSCharacter::BeginFire);
    Input->BindAction(FireAction, ETriggerEvent::Completed, this, &AVSCharacter::StopCombatInput);
    Input->BindAction(FireAction, ETriggerEvent::Canceled, this, &AVSCharacter::StopCombatInput);
    Input->BindAction(ReloadAction, ETriggerEvent::Started, this, &AVSCharacter::BeginReload);
    Input->BindAction(EquipRifleAction, ETriggerEvent::Started, this, &AVSCharacter::EquipRifle);
    Input->BindAction(EquipPistolAction, ETriggerEvent::Started, this, &AVSCharacter::EquipPistol);
    Input->BindAction(EquipLastAction, ETriggerEvent::Started, this, &AVSCharacter::EquipLast);

    PlayerController->SetInputMode(FInputModeGameOnly());
    PlayerController->bShowMouseCursor = false;
}

void AVSCharacter::Move(const FInputActionValue& Value)
{
    if (!Controller || !CanMatchMove())
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
    AddControllerPitchInput(Value.Get<float>() * MouseSensitivity);
}

void AVSCharacter::BeginFire()
{
    if (UVSWeaponComponent* Weapon = GetEquippedWeapon(); Weapon && CanMatchCombat()) { Weapon->StartFire(); }
}
void AVSCharacter::BeginReload()
{
    if (UVSWeaponComponent* Weapon = GetEquippedWeapon(); Weapon && CanMatchCombat()) { Weapon->Reload(); }
}
void AVSCharacter::EquipRifle() { Loadout->RequestEquip(EVSEquipRequest::Rifle); }
void AVSCharacter::EquipPistol() { Loadout->RequestEquip(EVSEquipRequest::Pistol); }
void AVSCharacter::EquipLast() { Loadout->RequestEquip(EVSEquipRequest::Last); }
void AVSCharacter::BeginJump() { if (CanMatchMove()) { Jump(); } }
void AVSCharacter::BeginCrouch() { if (CanMatchMove()) { Crouch(); } }
void AVSCharacter::EndCrouch() { if (CanMatchMove()) { UnCrouch(); } }
void AVSCharacter::BeginWalk() { bWalkHeld = CanMatchMove(); }
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
    BodyHitRegion->SetBoxExtent(FVector(25.0f, 20.0f, BodyHeight * 0.5f));
    BodyHitRegion->SetRelativeLocation(FVector(0.0f, 0.0f, -16.0f));
    HeadHitRegion->SetRelativeLocation(FVector(0.0f, 0.0f, Height * 0.5f - 16.0f));
}

bool AVSCharacter::IsHeadHit(const UPrimitiveComponent* Component) const
{
    return Component == HeadHitRegion;
}

FVector AVSCharacter::GetPawnViewLocation() const
{
    // Independent of the rendered camera and mesh smoothing on the server.
    return GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 24.0f);
}

void AVSCharacter::HandleHealthChanged()
{
    const bool bAlive = Health->IsAlive();
    BodyHitRegion->SetCollisionEnabled(bAlive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    HeadHitRegion->SetCollisionEnabled(bAlive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    OnRep_MatchGates();
}
