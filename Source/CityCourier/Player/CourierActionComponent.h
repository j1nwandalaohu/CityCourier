#pragma once
#include "Components/ActorComponent.h"
#include "Player/CourierActionTimeline.h"
#include "CourierActionComponent.generated.h"

UCLASS(ClassGroup=Courier, meta=(BlueprintSpawnableComponent))
class CITYCOURIER_API UCourierActionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCourierActionComponent();
    virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
    bool QueueAttack(bool bHeavy);
    void Cancel();
    void SetSuppressed(bool bValue,bool bPreserveAction);
    void MovementModeChanged(bool bAirborne);
    void JumpStarted();
    bool IsGroundActionBlocked() const;
    const FCourierActionTimeline& GetTimeline() const { return Timeline; }
    bool IsMovementLocked() const { return Timeline.IsReacting() || LandingElapsed<.12; }
    bool IsSuppressed() const { return bSuppressed; }
    float GetJumpElapsed() const { return JumpElapsed; }
    float GetLandingElapsed() const { return LandingElapsed; }
    bool IsJumpStart() const { return bJumpStarted && JumpElapsed<.18; }
    UPROPERTY(EditDefaultsOnly,Category="Courier|Actions") TMap<ECourierAction,TObjectPtr<class UAnimSequence>> ActionAnimations;
private:
    FCourierActionTimeline Timeline;
    uint8 PendingEdges=0;
    bool bSuppressed=false,bPreserve=false,bJumpStarted=false;
    float JumpElapsed=0,LandingElapsed=1;
};
