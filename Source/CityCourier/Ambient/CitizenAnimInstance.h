#pragma once
#include "Animation/AnimInstance.h"
#include "CitizenAnimInstance.generated.h"
enum class ECitizenClip : uint8 { Idle, Walk, Hit, Fall, GetUp, Guard, PunchL, PunchR, Run, Cower, Count };

/** Citizen-only clips; no dependency on the independently authored rider rig. */
UCLASS()
class CITYCOURIER_API UCitizenAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeInitializeAnimation() override;
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
    bool HasCombatClips() const;
    class UAnimSequence* GetPunchClip(bool bLeft) const;
    static bool IsLooping(int32 Index);
    UPROPERTY(Transient) TArray<TObjectPtr<class UAnimSequence>> Clips;
    UPROPERTY(BlueprintReadOnly) int32 AnimationIndex=0;
    UPROPERTY(BlueprintReadOnly) float SampleTime=0;
    UPROPERTY(BlueprintReadOnly) float WalkRate=0;
    FGuid AttackInstance;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
