#include "Vehicles/ScooterPawn.h"
#include "Vehicles/ScooterMovementComponent.h"
#include "Vehicles/VehicleRules.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "UObject/ConstructorHelpers.h"

AScooterPawn::AScooterPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision")); RootComponent = Collision;
    Collision->SetBoxExtent(FVector(80,36,50)); Collision->SetCollisionProfileName(TEXT("Pawn"));
    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot")); VisualRoot->SetupAttachment(RootComponent);
    BodyMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh")); BodyMesh->SetupAttachment(VisualRoot);
    BodyMesh->SetRelativeLocation(FVector(0,0,-52)); BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    auto Anchor=[this](const TCHAR* Name,FVector Position) { auto* C=CreateDefaultSubobject<USceneComponent>(Name); C->SetupAttachment(VisualRoot); C->SetRelativeLocation(Position); return C; };
    RiderSeat=Anchor(TEXT("RiderSeat"),FVector(-16,0,27)); LeftGrip=Anchor(TEXT("LeftGrip"),FVector(35,-30,59)); RightGrip=Anchor(TEXT("RightGrip"),FVector(35,30,59));
    LeftPedal=Anchor(TEXT("LeftPedal"),FVector(10,-20,-7)); RightPedal=Anchor(TEXT("RightPedal"),FVector(10,20,-7));
    Movement = CreateDefaultSubobject<UScooterMovementComponent>(TEXT("ScooterMovement")); Movement->SetUpdatedComponent(RootComponent);
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom")); CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 520; CameraBoom->SocketOffset = FVector(0,30,170); CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->ProbeSize = 18; CameraBoom->bEnableCameraLag = true; CameraBoom->CameraLagSpeed = 8;
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera")); FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->FieldOfView = 85;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    const FVector Positions[] = {{0,0,-15}, {-25,0,35}, {55,0,30}, {60,0,75}, {-60,0,-25}, {60,0,-25}, {-65,0,70}};
    const FVector Scales[] = {{1.4,.55,.30}, {.7,.55,.18}, {.15,.45,.9}, {.12,.85,.12}, {.50,.50,.14}, {.50,.50,.14}, {.55,.55,.55}};
    for (int32 I = 0; I < 7; ++I)
    {
        auto* Part = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ScooterPrototype%d"),I));
        Part->SetupAttachment(VisualRoot); Part->SetStaticMesh(I == 4 || I == 5 ? Cylinder.Object : Cube.Object);
        Part->SetRelativeLocation(Positions[I]); Part->SetRelativeScale3D(Scales[I]);
        if (I == 4 || I == 5) Part->SetRelativeRotation(FRotator(0,0,90));
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision); PrototypeParts.Add(Part);
    }
}
void AScooterPawn::BeginPlay()
{
    Super::BeginPlay();
    if(auto* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/CityCourier/Art/Scooter/SM_Scooter.SM_Scooter")))
    { BodyMesh->SetStaticMesh(Mesh); for(UStaticMeshComponent* Part:PrototypeParts) Part->SetVisibility(false); }
}
UPawnMovementComponent* AScooterPawn::GetMovementComponent() const { return Movement; }
float AScooterPawn::GetSpeedKmh() const { return Movement->Velocity.Size2D() * .036f; }
void AScooterPawn::SetDriveInput(float Throttle, float Steering, bool bBrake)
{
    if (const auto* PC=Cast<ACourierController>(Controller))
        if (PC->GetInterfaceMode()!=ECourierInterfaceMode::Gameplay) { Throttle=0; Steering=0; bBrake=true; }
    Movement->SetDriveInput(Rider.IsValid() ? Throttle : 0, Rider.IsValid() ? Steering : 0, bBrake || !Rider.IsValid());
}
void AScooterPawn::ClearDriveInput() { DriveAxis=FVector2D::ZeroVector; bBrakeHeld=false; SetDriveInput(0,0,true); }
void AScooterPawn::SetBatteryFraction(float Fraction)
{
    if (FMath::IsFinite(Fraction)) BatteryFraction = FMath::Clamp(Fraction,0.f,1.f);
}
void AScooterPawn::ConsumeTravel(double Distance)
{
    const float Before = BatteryFraction;
    BatteryFraction = FVehicleRules::ConsumeDistance(BatteryFraction, Distance, BatteryLevel);
    if (Before >= .2f && BatteryFraction < .2f) OnLowBattery.Broadcast();
}
void AScooterPawn::ApplyUpgrades(const FPlayerProfile& Profile)
{
    Movement->MaxForwardSpeedCm = FVehicleRules::GetMaxSpeedKmh(Profile.PowerLevel)/.036f;
    BatteryLevel = Profile.BatteryLevel >= 0 && Profile.BatteryLevel <= 2 ? Profile.BatteryLevel : 0;
    Movement->HandlingMultiplier = 1.f + .1f * FMath::Clamp(Profile.HandlingLevel,0,2);
}
bool AScooterPawn::CanInteract(ACourierCharacter* Courier, FText& OutReason) const
{
    return !Rider.IsValid() && GetSpeedKmh() <= 3 && IsValid(Courier) && Courier->GetController() &&
        Courier->CanReachInteraction(this, GetActorLocation(), OutReason);
}
bool AScooterPawn::TryMount(ACourierCharacter* Courier)
{
    FText Reason; if (!CanInteract(Courier,Reason)) return false;
    AController* RiderController = Courier->GetController();
    Rider = Courier; Courier->SetMounted(true); Courier->GetCharacterMovement()->DisableMovement();
    Courier->SetActorEnableCollision(false); Courier->AttachToComponent(VisualRoot, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    Courier->SetActorRelativeLocation(RiderSeat->GetRelativeLocation()+FVector(0,0,9));
    RiderController->Possess(this); RiderController->SetControlRotation(FRotator(-12,GetActorRotation().Yaw,0));
    DriveAxis = FVector2D::ZeroVector; bBrakeHeld = false; SetDriveInput(0,0,true); return true;
}
bool AScooterPawn::FindDismountLocation(FVector& Out) const
{
    if (!Rider.IsValid()) return false;
    const auto* Capsule = Rider->GetCapsuleComponent();
    const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ScooterDismount), false, this); Params.AddIgnoredActor(Rider.Get());
    for (float Side : {-1.f,1.f})
    {
        const FVector Candidate = GetActorLocation() + GetActorRightVector() * 110 * Side;
        FHitResult Ground;
        if (!GetWorld()->LineTraceSingleByChannel(Ground, Candidate+FVector(0,0,150),Candidate-FVector(0,0,200),ECC_Visibility,Params) || Ground.Normal.Z < .7) continue;
        // A horizontal obstacle top is not a safe adjacent footstep.
        if (FMath::Abs(Ground.ImpactPoint.Z - (GetActorLocation().Z - 52)) > 35) continue;
        const FVector Position = Ground.ImpactPoint + FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()+2);
        if (GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,Shape,Params)) continue;
        FHitResult Passage;
        if (GetWorld()->SweepSingleByChannel(Passage, FVector(GetActorLocation().X,GetActorLocation().Y,Position.Z),Position,
            FQuat::Identity,ECC_Pawn,Shape,Params)) continue;
        Out = Position; return true;
    }
    return false;
}
bool AScooterPawn::TryDismount()
{
    if (!Rider.IsValid() || !Controller || GetSpeedKmh() > 3.f) return false;
    FVector Location; if (!FindDismountLocation(Location)) return false;
    ACourierCharacter* Courier = Rider.Get(); AController* RiderController = Controller;
    StopDrive(); Movement->Velocity = FVector::ZeroVector;
    Courier->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    Courier->SetActorLocationAndRotation(Location, FRotator(0,GetActorRotation().Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
    Courier->SetActorEnableCollision(true); Courier->SetMounted(false); Courier->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    RiderController->Possess(Courier); Rider.Reset(); return true;
}
void AScooterPawn::UpdateVisualDrive(float Steering, float Pitch, float Delta)
{
    const FRotator Target(FMath::Clamp(Pitch,-25.f,25.f),0,-Steering*FMath::Clamp(GetSpeedKmh()/36.f,0.f,1.f)*12);
    VisualRoot->SetRelativeRotation(FMath::RInterpTo(VisualRoot->GetRelativeRotation(), Target, Delta, 7));
}
void AScooterPawn::Tick(float Delta)
{
    Super::Tick(Delta);
    if (Controller && GetSpeedKmh() > 3 && GetWorld()->GetTimeSeconds() - LastLookTime > 2)
    {
        FRotator View = Controller->GetControlRotation();
        View.Yaw = FMath::FixedTurn(View.Yaw,GetActorRotation().Yaw,60*Delta);
        Controller->SetControlRotation(View);
    }
}
void AScooterPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input); auto* Enhanced = CastChecked<UEnhancedInputComponent>(Input);
    auto Action = [this](const TCHAR* Name)
    {
        auto* A = LoadObject<UInputAction>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Input/IA_%s.IA_%s"),Name,Name));
        if (A) InputActions.Add(A); return A;
    };
    if (auto* A = Action(TEXT("Move")))
    {
        Enhanced->BindAction(A,ETriggerEvent::Triggered,this,&AScooterPawn::Drive);
        Enhanced->BindAction(A,ETriggerEvent::Completed,this,&AScooterPawn::StopDrive);
        Enhanced->BindAction(A,ETriggerEvent::Canceled,this,&AScooterPawn::StopDrive);
    }
    if (auto* A = Action(TEXT("Look"))) Enhanced->BindAction(A,ETriggerEvent::Triggered,this,&AScooterPawn::Look);
    if (auto* A = Action(TEXT("Brake")))
    {
        Enhanced->BindAction(A,ETriggerEvent::Started,this,&AScooterPawn::BrakeStart);
        Enhanced->BindAction(A,ETriggerEvent::Completed,this,&AScooterPawn::BrakeStop);
        Enhanced->BindAction(A,ETriggerEvent::Canceled,this,&AScooterPawn::BrakeStop);
    }
    if (auto* A = Action(TEXT("Interact"))) Enhanced->BindAction(A,ETriggerEvent::Started,this,&AScooterPawn::DismountPressed);
}
void AScooterPawn::Drive(const FInputActionValue& Value)
{
    if (const auto* PC = Cast<ACourierController>(Controller)) if (PC->GetInterfaceMode() != ECourierInterfaceMode::Gameplay) { StopDrive(); return; }
    DriveAxis = Value.Get<FVector2D>(); SetDriveInput(DriveAxis.Y, DriveAxis.X,bBrakeHeld);
}
void AScooterPawn::StopDrive() { DriveAxis = FVector2D::ZeroVector; SetDriveInput(0,0,true); }
void AScooterPawn::BrakeStart() { bBrakeHeld = true; SetDriveInput(DriveAxis.Y,DriveAxis.X,true); }
void AScooterPawn::BrakeStop() { bBrakeHeld = false; SetDriveInput(DriveAxis.Y,DriveAxis.X,false); }
void AScooterPawn::DismountPressed() { TryDismount(); }
void AScooterPawn::Look(const FInputActionValue& Value)
{
    if (const auto* PC = Cast<ACourierController>(Controller)) if (PC->GetInterfaceMode() != ECourierInterfaceMode::Gameplay) return;
    const FVector2D Axis = Value.Get<FVector2D>(); AddControllerYawInput(Axis.X); AddControllerPitchInput(Axis.Y);
    LastLookTime = GetWorld()->GetTimeSeconds();
}
