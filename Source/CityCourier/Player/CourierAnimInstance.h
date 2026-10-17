#pragma once
#include "Animation/AnimInstance.h"
#include "CourierAnimInstance.generated.h"
UCLASS()
class CITYCOURIER_API UCourierAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly,Category="Courier") float GroundSpeed=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier") bool bRiding=false;
    UPROPERTY(BlueprintReadOnly,Category="Courier") float SteeringAmount=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier") int32 AnimationIndex=0;
};
