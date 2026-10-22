#pragma once
#include "ControlRig.h"
#include "CourierContactRig.generated.h"

/** Human bone solver; all inputs are copied by the anim node, no World access. */
UCLASS(Blueprintable)
class CITYCOURIER_API UCourierContactRig : public UControlRig
{
    GENERATED_BODY()
public:
    virtual bool Execute_Internal(const FName& EventName) override;
private:
    FTransform ReadGoal(FName Name) const;
    float ReadWeight(FName Name) const;
    void SolveChain(FName Root,FName Joint,FName End,const FTransform& Goal,float Weight,FVector FallbackPole);
};
