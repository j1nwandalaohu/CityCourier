#pragma once
#include "Animation/AnimInstance.h"
#include "CourierAnimInstance.generated.h"

UCLASS(Transient, Blueprintable)
class CITYCOURIER_API UCourierAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Courier") TArray<TObjectPtr<class UAnimSequence>> Clips;
    UPROPERTY(BlueprintReadOnly, Category="Courier") float GroundSpeed=0;
    UPROPERTY(BlueprintReadOnly, Category="Courier") bool bRiding=false;
    static int32 SelectClip(float Speed,bool bMounted) { return bMounted?3:Speed<8?0:Speed<520?1:2; }
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) override;
};
