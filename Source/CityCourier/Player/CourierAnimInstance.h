#pragma once
#include "Animation/AnimInstance.h"
#include "CourierAnimInstance.generated.h"
UCLASS()
class CITYCOURIER_API UCourierAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Courier") TArray<TObjectPtr<class UAnimSequence>> Clips;
    static int32 SelectClip(float Speed,bool bMounted) { return bMounted?3:Speed<8?0:Speed<520?1:2; }
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(BlueprintReadOnly,Category="Courier") float GroundSpeed=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier") bool bRiding=false;
    UPROPERTY(BlueprintReadOnly,Category="Courier") float SteeringAmount=0;
    UPROPERTY(BlueprintReadOnly,Category="Courier") int32 AnimationIndex=0;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
