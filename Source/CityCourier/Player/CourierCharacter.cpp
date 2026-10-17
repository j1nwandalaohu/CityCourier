#include "Player/CourierCharacter.h"
#include "Player/CourierInteractable.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

ACourierCharacter::ACourierCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34, 88);
    bUseControllerRotationYaw = false;
    auto* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true; Movement->RotationRate = FRotator(0,540,0);
    Movement->MaxWalkSpeed = 420; Movement->MaxStepHeight = 35; Movement->BrakingDecelerationWalking = 1600;
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent); CameraBoom->TargetArmLength = 380;
    CameraBoom->SocketOffset = FVector(0,55,65); CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->ProbeSize = 14; CameraBoom->bEnableCameraLag = true; CameraBoom->CameraLagSpeed = 12;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); FollowCamera->FieldOfView = 80;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    const FVector Positions[] = {{0,0,8}, {0,0,64}, {0,-35,5}, {0,35,5}, {0,-14,-57}, {0,14,-57}};
    const FVector Scales[] = {{.35,.48,.70}, {.34,.34,.34}, {.18,.18,.65}, {.18,.18,.65}, {.23,.20,.58}, {.23,.20,.58}};
    for (int32 I = 0; I < 6; ++I)
    {
        auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("PrototypePart%d"), I));
        Part->SetupAttachment(RootComponent); Part->SetStaticMesh(I == 1 ? Sphere.Object : Cube.Object);
        Part->SetRelativeLocation(Positions[I]); Part->SetRelativeScale3D(Scales[I]);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); PrototypeParts.Add(Part);
    }
}
void ACourierCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); InteractionRefreshTime += DeltaSeconds;
    if (InteractionRefreshTime >= 0.1f) { InteractionRefreshTime = 0; RefreshInteraction(); }
}
void ACourierCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    auto* Enhanced = CastChecked<UEnhancedInputComponent>(Input);
    auto Action = [this](const TCHAR* Name)
    {
        auto* Result = LoadObject<UInputAction>(nullptr, *FString::Printf(TEXT("/Game/CityCourier/Input/IA_%s.IA_%s"), Name, Name));
        if (Result) InputActions.Add(Result); return Result;
    };
    if (auto* A = Action(TEXT("Move"))) Enhanced->BindAction(A, ETriggerEvent::Triggered, this, &ACourierCharacter::Move);
    if (auto* A = Action(TEXT("Look"))) Enhanced->BindAction(A, ETriggerEvent::Triggered, this, &ACourierCharacter::Look);
    if (auto* A = Action(TEXT("Sprint")))
    {
        Enhanced->BindAction(A, ETriggerEvent::Started, this, &ACourierCharacter::SprintStart);
        Enhanced->BindAction(A, ETriggerEvent::Completed, this, &ACourierCharacter::SprintStop);
        Enhanced->BindAction(A, ETriggerEvent::Canceled, this, &ACourierCharacter::SprintStop);
    }
    if (auto* A = Action(TEXT("Interact"))) Enhanced->BindAction(A, ETriggerEvent::Started, this, &ACourierCharacter::InteractPressed);
}
void ACourierCharacter::Move(const FInputActionValue& Value)
{
    if (bInputSuppressed || bMounted || !Controller) return;
    const FVector2D Axis = Value.Get<FVector2D>(); const FRotator Yaw(0, Controller->GetControlRotation().Yaw, 0);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), Axis.Y);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), Axis.X);
}
void ACourierCharacter::Look(const FInputActionValue& Value)
{
    if (bInputSuppressed) return;
    const FVector2D Axis = Value.Get<FVector2D>(); AddControllerYawInput(Axis.X); AddControllerPitchInput(Axis.Y);
}
void ACourierCharacter::SprintStart() { SetSprinting(true); }
void ACourierCharacter::SprintStop() { SetSprinting(false); }
void ACourierCharacter::InteractPressed() { TryInteract(); }
void ACourierCharacter::SetSprinting(bool bValue)
{
    GetCharacterMovement()->MaxWalkSpeed = bValue && !bMounted && !bInputSuppressed ? 700 : 420;
}
void ACourierCharacter::ClearMovementInput()
{
    ConsumeMovementInputVector(); GetCharacterMovement()->StopMovementImmediately(); SetSprinting(false);
}
bool ACourierCharacter::CanReachInteraction(const AActor* Target, FVector Point, FText& Reason, bool bAllowSuppressed) const
{
    Reason = FText::GetEmpty();
    if (!IsValid(Target) || bMounted || (bInputSuppressed && !bAllowSuppressed)) { Reason = FText::FromString(TEXT("请先下车并关闭界面")); return false; }
    if (FVector::DistSquared(GetActorLocation(), Point) > 200.0 * 200.0) { Reason = FText::FromString(TEXT("请靠近至2米内")); return false; }
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierInteraction), false, this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, GetPawnViewLocation(), Point + FVector(0,0,50), ECC_Visibility, Params);
    if (bHit && Hit.GetActor() != Target) { Reason = FText::FromString(TEXT("目标被遮挡")); return false; }
    return true;
}
void ACourierCharacter::RefreshInteraction()
{
    InteractionTarget.Reset(); InteractionPrompt = FText::GetEmpty();
    if (bMounted || bInputSuppressed || !GetWorld()) return;
    int32 BestPriority = MIN_int32; double BestDistance = MAX_dbl;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        auto* Target = Cast<ICourierInteractable>(*It); if (!Target) continue;
        FText Reason;
        if (!CanReachInteraction(*It, Target->GetInteractionLocation(), Reason) || !Target->CanInteract(this, Reason)) continue;
        const int32 Priority = Target->GetInteractionPriority();
        const double Distance = FVector::DistSquared(GetActorLocation(), Target->GetInteractionLocation());
        if (Priority > BestPriority || (Priority == BestPriority && Distance < BestDistance))
        {
            BestPriority = Priority; BestDistance = Distance; InteractionTarget = *It; InteractionPrompt = Target->GetInteractionText();
        }
    }
}
bool ACourierCharacter::TryInteract()
{
    RefreshInteraction(); auto* Target = Cast<ICourierInteractable>(InteractionTarget.Get());
    FText Reason;
    return Target && Target->CanInteract(this, Reason) && Target->Interact(this);
}
