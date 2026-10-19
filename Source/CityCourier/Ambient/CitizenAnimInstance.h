#pragma once
#include "Animation/AnimInstance.h"
#include "CitizenAnimInstance.generated.h"

/** Citizen-only clips; no dependency on the independently authored rider rig. */
UCLASS()
class CITYCOURIER_API UCitizenAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> Clips;
    UPROPERTY(BlueprintReadOnly) int32 AnimationIndex=0;
    UPROPERTY(BlueprintReadOnly) float SampleTime=0;
    UPROPERTY(BlueprintReadOnly) float WalkRate=0;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
