#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
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
#include "Components/SkeletalMeshComponent.h"
#include "Player/CourierActionComponent.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "Ambient/AmbientVehicle.h"
#include "Components/BoxComponent.h"

ACourierCharacter::ACourierCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    Actions=CreateDefaultSubobject<UCourierActionComponent>(TEXT("CharacterActions"));
    GetCapsuleComponent()->InitCapsuleSize(34, 88);
    bUseControllerRotationYaw = false;
    auto* Movement = GetCharacterMovement();
    Movement->bOrientRotationToMovement = true; Movement->RotationRate = FRotator(0,540,0);
    Movement->MaxWalkSpeed = 420; Movement->MaxStepHeight = 35; Movement->BrakingDecelerationWalking = 1600;
    Movement->JumpZVelocity=420; Movement->AirControl=.25; JumpMaxCount=1; JumpMaxHoldTime=0;
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
void ACourierCharacter::BeginPlay()
{
    Super::BeginPlay();
    if(auto* RiderAsset=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/CityCourier/Art/Characters/SK_Courier.SK_Courier")))
    {
        GetMesh()->SetSkeletalMesh(RiderAsset); GetMesh()->SetRelativeLocation(FVector(0,0,-88));
        GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,TEXT("/Game/CityCourier/Blueprints/ABP_Courier.ABP_Courier_C")));
        for(UStaticMeshComponent* Part:PrototypeParts) Part->SetVisibility(false);
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
    if(auto* A=Action(TEXT("Jump")))
    {
        Enhanced->BindAction(A,ETriggerEvent::Started,this,&ACourierCharacter::JumpPressed);
        Enhanced->BindAction(A,ETriggerEvent::Completed,this,&ACourierCharacter::JumpReleased);
        Enhanced->BindAction(A,ETriggerEvent::Canceled,this,&ACourierCharacter::JumpReleased);
    }
    if(auto* A=Action(TEXT("LightPunch"))) Enhanced->BindAction(A,ETriggerEvent::Started,this,&ACourierCharacter::LightPressed);
    if(auto* A=Action(TEXT("HeavyPunch"))) Enhanced->BindAction(A,ETriggerEvent::Started,this,&ACourierCharacter::HeavyPressed);
}
void ACourierCharacter::Move(const FInputActionValue& Value)
{
    if (bInputSuppressed || bMounted || !Controller || Actions->IsMovementLocked()) return;
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
void ACourierCharacter::InteractPressed()
{ const auto Result=TryInteractWithFeedback(); if(auto* PC=Cast<ACourierController>(Controller)) PC->ShowActionFeedback(Result.Reason); }
void ACourierCharacter::SetSprinting(bool bValue)
{
    bSprintHeld=bValue; UpdateActionMovement();
}
void ACourierCharacter::UpdateActionMovement()
{
    const auto& State=Actions->GetTimeline();
    const float Limit=Actions->IsMovementLocked()?0.f:State.IsAttacking()?120.f:bSprintHeld && !bMounted && !bInputSuppressed?700.f:420.f;
    auto* Movement=GetCharacterMovement(); Movement->MaxWalkSpeed=Limit;
    if(State.IsAttacking() || Actions->IsMovementLocked())
    {
        FVector Horizontal=Movement->Velocity; Horizontal.Z=0;
        Horizontal=Horizontal.GetClampedToMaxSize(Limit); Horizontal.Z=Movement->Velocity.Z; Movement->Velocity=Horizontal;
    }
    Movement->bOrientRotationToMovement=!State.IsAttacking() && !State.IsReacting();
}
void ACourierCharacter::ClearMovementInput()
{
    ConsumeMovementInputVector();
    const double Vertical=GetCharacterMovement()->Velocity.Z;
    GetCharacterMovement()->StopMovementImmediately();
    if(GetCharacterMovement()->IsFalling()) GetCharacterMovement()->Velocity.Z=Vertical;
    StopJumping(); SetSprinting(false);
}
void ACourierCharacter::SetMounted(bool bValue)
{ bMounted=bValue; Actions->Cancel(); ClearMovementInput(); RefreshInteraction(); }
void ACourierCharacter::SetInputSuppressed(bool bValue,bool bPreserveAction)
{ bInputSuppressed=bValue; Actions->SetSuppressed(bValue,bPreserveAction); ClearMovementInput(); RefreshInteraction(); }
void ACourierCharacter::OnMovementModeChanged(EMovementMode Prev,uint8 Custom)
{
    Super::OnMovementModeChanged(Prev,Custom);
    if(Actions && (GetCharacterMovement()->IsFalling() || Prev==MOVE_Falling)) Actions->MovementModeChanged(GetCharacterMovement()->IsFalling());
}
void ACourierCharacter::OnJumped_Implementation()
{ Super::OnJumped_Implementation(); Actions->JumpStarted(); }
bool ACourierCharacter::CanPerformGroundAction(FText& Reason,bool bAllowAttack) const
{
    if(bMounted || bInputSuppressed) { Reason=FText::FromString(TEXT("请先下车并关闭界面")); return false; }
    if(!GetCharacterMovement()->IsMovingOnGround()) { Reason=FText::FromString(TEXT("请落地后再操作")); return false; }
    if(Actions->IsGroundActionBlocked() && !(bAllowAttack && Actions->GetTimeline().IsAttacking()))
    { Reason=FText::FromString(TEXT("请等待当前动作结束")); return false; }
    return true;
}
FCourierActionResult ACourierCharacter::TryJumpWithFeedback()
{
    FText Reason; if(!CanPerformGroundAction(Reason)) return {false,Reason};
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierJumpHeadroom),false,this);
    const auto* Capsule=GetCapsuleComponent();
    if(GetWorld()->SweepSingleByChannel(Hit,GetActorLocation()+FVector(0,0,2),GetActorLocation()+FVector(0,0,50),FQuat::Identity,ECC_Pawn,
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius()-1,Capsule->GetScaledCapsuleHalfHeight()-1),Params))
        return FCourierActionResult::Failure(TEXT("头顶空间不足，无法跳跃"));
    auto* Movement=GetCharacterMovement();
    float JumpSpeed=Movement->JumpZVelocity;
    const FVector Feet=GetActorLocation()-FVector(0,0,Capsule->GetScaledCapsuleHalfHeight());
    FHitResult Ahead;
    if(GetWorld()->LineTraceSingleByChannel(Ahead,Feet+FVector(0,0,60),Feet+GetActorForwardVector()*180+FVector(0,0,60),ECC_Pawn,Params))
        if(const auto* Car=Cast<AAmbientVehicle>(Ahead.GetActor()))
            if(Car->Hood->GetCollisionEnabled()!=ECollisionEnabled::NoCollision)
            {
                const FVector Local=Car->GetActorTransform().InverseTransformPosition(Feet);
                const double Height=Car->Hood->GetComponentLocation().Z+Car->Hood->GetScaledBoxExtent().Z-Feet.Z;
                // A small contextual lift at the front makes the authored
                // 115cm hood reachable, without changing normal 90cm jumps.
                if(Local.X>Car->Hood->GetRelativeLocation().X && FMath::Abs(Local.Y)<120 && Height>0 && Height<140)
                    JumpSpeed=FMath::Max(JumpSpeed,float(FMath::Min(520.,FMath::Sqrt(2*FMath::Abs(Movement->GetGravityZ())*(Height+20)))));
            }
    TGuardValue<float> RestoreJumpSpeed(Movement->JumpZVelocity,JumpSpeed);
    Jump(); CheckJumpInput(0); StopJumping();
    return GetCharacterMovement()->IsFalling()?FCourierActionResult::Success():FCourierActionResult::Failure(TEXT("当前无法起跳"));
}
void ACourierCharacter::JumpPressed()
{ const auto R=TryJumpWithFeedback(); if(!R.bSucceeded) if(auto* PC=Cast<ACourierController>(Controller)) PC->ShowActionFeedback(R.Reason); }
void ACourierCharacter::JumpReleased() { StopJumping(); }
void ACourierCharacter::LightPressed() { Actions->QueueAttack(false); }
void ACourierCharacter::HeavyPressed() { Actions->QueueAttack(true); }
bool ACourierCharacter::ReceiveCourierStrike(FGuid Id,const FVector& Origin,ECourierAction Action)
{ return Actions->ReceiveStrike(Id,Origin); }
bool ACourierCharacter::CanReachInteraction(const AActor* Target, FVector Point, FText& Reason, bool bAllowSuppressed) const
{
    Reason = FText::GetEmpty();
    if(GetCharacterMovement()->IsFalling() || Actions->IsGroundActionBlocked()) { Reason=FText::FromString(TEXT("请落地并等待当前动作结束")); return false; }
    if (!IsValid(Target) || bMounted || (bInputSuppressed && !bAllowSuppressed)) { Reason = FText::FromString(TEXT("请先下车并关闭界面")); return false; }
    if (FVector::DistSquared(GetActorLocation(), Point) > 200.0 * 200.0) { Reason = FText::FromString(TEXT("请靠近至2米内")); return false; }
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierInteraction), false, this);
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, GetPawnViewLocation(), Point + FVector(0,0,50), ECC_Visibility, Params);
    if (bHit && Hit.GetActor() != Target) { Reason = FText::FromString(TEXT("目标被遮挡")); return false; }
    return true;
}
TArray<AActor*> ACourierCharacter::InteractionCandidates() const
{
    if(!GetWorld()) return {};
    const auto* Interiors=GetWorld()->GetSubsystem<UCourierBuildingSubsystem>();
    if(Interiors && Interiors->IsEnabled()) return Interiors->GetInteractionCandidates(GetActorLocation());
    TArray<AActor*> Result; for(TActorIterator<AActor> It(GetWorld());It;++It) if(Cast<ICourierInteractable>(*It)) Result.Add(*It);
    return Result;
}
void ACourierCharacter::RefreshInteraction()
{
    InteractionTarget.Reset(); InteractionPrompt = FText::GetEmpty();
    if (bMounted || bInputSuppressed || !GetWorld()) return;
    int32 BestPriority = MIN_int32; double BestDistance = MAX_dbl;
    for (AActor* Candidate:InteractionCandidates())
    {
        auto* Target = Cast<ICourierInteractable>(Candidate); if (!Target) continue;
        FText Reason;
        if (!CanReachInteraction(Candidate, Target->GetInteractionLocation(), Reason) || !Target->CanInteract(this, Reason)) continue;
        const int32 Priority = Target->GetInteractionPriority();
        const double Distance = FVector::DistSquared(GetActorLocation(), Target->GetInteractionLocation());
        if (Priority > BestPriority || (Priority == BestPriority && Distance < BestDistance))
        {
            BestPriority = Priority; BestDistance = Distance; InteractionTarget = Candidate; InteractionPrompt = Target->GetInteractionText();
        }
    }
}
bool ACourierCharacter::TryInteract()
{ return TryInteractWithFeedback().bSucceeded; }
FCourierActionResult ACourierCharacter::TryInteractWithFeedback()
{
    RefreshInteraction(); auto* Target = Cast<ICourierInteractable>(InteractionTarget.Get());
    FText Reason;
    if(Target)
    {
        if(!Target->CanInteract(this,Reason)) return {false,Reason};
        return Target->Interact(this)?FCourierActionResult::Success():FCourierActionResult::Failure(TEXT("当前订单无法在此操作，请检查取餐或送达目标"));
    }
    if(bMounted || bInputSuppressed) return FCourierActionResult::Failure(TEXT("请先下车并关闭界面"));
    int32 Priority=MIN_int32; double Distance=MAX_dbl;
    for(AActor* Actor:InteractionCandidates())
    {
        const auto* Candidate=Cast<ICourierInteractable>(Actor); if(!Candidate) continue;
        const double D=FVector::DistSquared(GetActorLocation(),Candidate->GetInteractionLocation());
        if(D>500*500 || Candidate->GetInteractionPriority()<Priority || (Candidate->GetInteractionPriority()==Priority && D>=Distance)) continue;
        FText Rejected;
        if(!CanReachInteraction(Actor,Candidate->GetInteractionLocation(),Rejected) || !Candidate->CanInteract(this,Rejected))
        { Priority=Candidate->GetInteractionPriority(); Distance=D; Reason=Rejected; }
    }
    return Reason.IsEmpty()?FCourierActionResult::Failure(TEXT("附近没有可交互目标")):FCourierActionResult{false,Reason};
}
