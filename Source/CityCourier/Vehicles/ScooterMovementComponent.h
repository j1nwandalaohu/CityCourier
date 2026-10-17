#pragma once
#include "GameFramework/PawnMovementComponent.h"
#include "ScooterMovementComponent.generated.h"

UCLASS(ClassGroup=Movement, meta=(BlueprintSpawnableComponent))
class CITYCOURIER_API UScooterMovementComponent : public UPawnMovementComponent
{
    GENERATED_BODY()
public:
    virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    void SetDriveInput(float Throttle, float Steering, bool bBrake);
    void ResetAfterTeleport() { StopMovementImmediately(); FallingSpeed=0; SetDriveInput(0,0,true); }
    float MaxForwardSpeedCm = 1000;
    float HandlingMultiplier = 1;
private:
    void SimulateStep(float DeltaSeconds);
    float ThrottleInput = 0;
    float SteeringInput = 0;
    bool bBraking = true;
    float FallingSpeed = 0;
};
