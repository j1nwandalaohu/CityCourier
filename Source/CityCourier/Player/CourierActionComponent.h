#pragma once
#include "Components/ActorComponent.h"
#include "Player/CourierActionTimeline.h"
#include "CourierActionComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FCourierStrikeEvent,AActor*,FGuid,ECourierAction);

UCLASS(ClassGroup=Courier, meta=(BlueprintSpawnableComponent))
class CITYCOURIER_API UCourierActionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCourierActionComponent();
    virtual void BeginPlay() override;
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
    bool ReceiveStrike(FGuid InstanceId,const FVector& Origin);
    bool HasAttackAnimation(bool bHeavy) const;
    FCourierStrikeEvent OnStrike;
    float GetJumpElapsed() const { return JumpElapsed; }
    float GetLandingElapsed() const { return LandingElapsed; }
    bool IsJumpStart() const { return bJumpStarted && JumpElapsed<.18; }
    UPROPERTY(EditDefaultsOnly,Category="Courier|Actions") TMap<ECourierAction,TObjectPtr<class UAnimSequence>> ActionAnimations;
    UPROPERTY(EditDefaultsOnly,Category="Courier|Actions") TMap<FName,TObjectPtr<class UAnimSequence>> MovementAnimations;
private:
    void TraceSlice(const FActionSlice& Slice,uint64 Generation);
    bool CanProcessStrike(uint64 Generation) const;
    bool IsPlaybackReady() const;
    void MontageBlendingOut(class UAnimMontage* Montage,bool bInterrupted);
    void SyncAnimation(float Delta);
    void Feedback(const FText& Message) const;
    FCourierActionTimeline Timeline;
    TMap<FGuid,TSet<TWeakObjectPtr<AActor>>> Struck;
    TSet<FGuid> Received;
    UPROPERTY() TObjectPtr<class UAnimMontage> PlayingMontage;
    UPROPERTY() TObjectPtr<class UAnimSequence> PlayingSequence;
    FGuid PlayingInstance;
    FGuid FacingInstance;
    FName PlayingMovement,RunTransition;
    float RunTransitionElapsed=0,PreviousSpeed=0;
    bool bWasMoving=false;
    bool bChangingMontage=false,bPlaybackFinishedNaturally=false;
    uint64 InterruptionGeneration=0;
    FRotator AttackFacing;
    uint8 PendingEdges=0;
    bool bSuppressed=false,bPreserve=false,bJumpStarted=false;
    float JumpElapsed=0,LandingElapsed=1;
};
