#include "Vehicles/ScooterMovementComponent.h"
#include "Vehicles/ScooterPawn.h"
#include "Player/CourierCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

void UScooterMovementComponent::SetDriveInput(float Throttle, float Steering, bool bBrake)
{
    ThrottleInput = FMath::IsFinite(Throttle) ? FMath::Clamp(Throttle,-1.f,1.f) : 0;
    SteeringInput = FMath::IsFinite(Steering) ? FMath::Clamp(Steering,-1.f,1.f) : 0; bBraking = bBrake;
}
void UScooterMovementComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, TickType, Function);
    if (!UpdatedComponent || !PawnOwner || !FMath::IsFinite(Delta) || Delta <= 0 || ShouldSkipUpdate(Delta)) return;
    // Bounded substeps make acceleration, steering and sweeps stable at 30/60 FPS.
    for (float Remaining = FMath::Min(Delta, .25f); Remaining > UE_SMALL_NUMBER;)
    {
        const float Step = FMath::Min(Remaining, 1.f/120.f); SimulateStep(Step); Remaining -= Step;
    }
    UpdateComponentVelocity();
}
void UScooterMovementComponent::SimulateStep(float Delta)
{
    auto* Bike = CastChecked<AScooterPawn>(PawnOwner);
    const FVector Start = UpdatedComponent->GetComponentLocation();
    const FVector Forward = Bike->GetActorForwardVector();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ScooterGround), false, Bike);
    if (Bike->GetRider()) Params.AddIgnoredActor(Bike->GetRider());
    bool bGrounded = false; double GroundZ = -MAX_dbl; double FrontZ = 0, BackZ = 0;
    const FVector Right = Bike->GetActorRightVector();
    // Probe the footprint corners as well as the wheel line. At diagonal curb approaches,
    // a box corner reaches the curb before the center-line front probe does.
    const FVector ProbeOffsets[] = {-Forward*100, FVector::ZeroVector, Forward*100,
        Forward*95+Right*46, Forward*95-Right*46, -Forward*95+Right*46, -Forward*95-Right*46};
    for (const FVector& Offset : ProbeOffsets)
    {
        FHitResult Floor; const FVector Probe = Start + Offset;
        if (GetWorld()->LineTraceSingleByChannel(Floor, Probe + FVector(0,0,75), Probe - FVector(0,0,180), ECC_Visibility, Params) &&
            Floor.Normal.Z > .65 && Floor.ImpactPoint.Z + 52 - Start.Z <= 35)
        {
            bGrounded = true; GroundZ = FMath::Max(GroundZ, Floor.ImpactPoint.Z);
            if (FVector::DotProduct(Offset,Forward) > 80) FrontZ = Floor.ImpactPoint.Z;
            if (FVector::DotProduct(Offset,Forward) < -80) BackZ = Floor.ImpactPoint.Z;
        }
    }
    FHitResult VerticalHit;
    if (bGrounded)
    {
        FallingSpeed = 0;
        SafeMoveUpdatedComponent(FVector(0,0,FMath::Max(GroundZ+52-Start.Z, -400.0*Delta)), UpdatedComponent->GetComponentQuat(), true, VerticalHit);
    }
    else
    {
        FallingSpeed -= 980 * Delta;
        SafeMoveUpdatedComponent(FVector(0,0,FallingSpeed*Delta), UpdatedComponent->GetComponentQuat(), true, VerticalHit);
        if (VerticalHit.bBlockingHit) FallingSpeed = 0;
    }
    float Speed = FVector::DotProduct(Velocity, Forward);
    const bool bCanDrive = Bike->GetRider() && Bike->GetBatteryFraction() > 0 && bGrounded;
    if (bBraking || !Bike->GetRider()) Speed = FMath::FInterpConstantTo(Speed, 0.f, Delta, 900.f);
    else if (!bCanDrive || FMath::IsNearlyZero(ThrottleInput)) Speed = FMath::FInterpConstantTo(Speed, 0.f, Delta, 85.f);
    else if (ThrottleInput > 0) Speed += 260.f * ThrottleInput * Delta;
    else if (Speed > 30) Speed = FMath::FInterpConstantTo(Speed, 0.f, Delta, 750.f);
    else Speed += 180.f * ThrottleInput * Delta;
    Speed = FMath::Clamp(Speed, -6.f/.036f, MaxForwardSpeedCm);
    FRotator Rotation = Bike->GetActorRotation();
    Rotation.Yaw += SteeringInput * 75.f * HandlingMultiplier * FMath::Clamp(FMath::Abs(Speed)/350.f,0.f,1.f) * (Speed < 0 ? -1 : 1) * Delta;
    if (GetWorld()->OverlapBlockingTestByChannel(UpdatedComponent->GetComponentLocation(), Rotation.Quaternion(), ECC_Pawn,
        FCollisionShape::MakeBox(Bike->Collision->GetScaledBoxExtent()), Params)) Rotation = Bike->GetActorRotation();
    Velocity = Rotation.Vector() * Speed;
    const FVector Travel = Velocity * Delta; FHitResult Hit;
    SafeMoveUpdatedComponent(Travel, Rotation.Quaternion(), true, Hit);
    if (Hit.IsValidBlockingHit())
    {
        const float ClosingSpeed = FMath::Max(0.0, -FVector::DotProduct(Velocity, Hit.Normal)) * .036;
        if (ClosingSpeed > .01f) Bike->OnEffectiveImpact.Broadcast(ClosingSpeed);
        SlideAlongSurface(Travel, 1.f - Hit.Time, Hit.Normal, Hit, true);
        Velocity = FVector::VectorPlaneProject(Velocity, Hit.Normal);
    }
    // Only movement performed inside this simulation step counts. External teleports are excluded.
    Bike->ConsumeTravel(FVector::Distance(Start, UpdatedComponent->GetComponentLocation()) / 100.0);
    Bike->UpdateVisualDrive(SteeringInput, -FMath::RadiansToDegrees(FMath::Atan2(FrontZ-BackZ,200.0)), Delta);
}
