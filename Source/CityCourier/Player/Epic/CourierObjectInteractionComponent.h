#pragma once
#include "Components/ActorComponent.h"
#include "Core/CourierActionResult.h"
#include "Player/Epic/CourierInteractionPose.h"
#include "CourierObjectInteractionComponent.generated.h"

UCLASS(ClassGroup=(Courier),meta=(BlueprintSpawnableComponent))
class CITYCOURIER_API UCourierObjectInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UCourierObjectInteractionComponent();
    FCourierActionResult TryGrab(class UPrimitiveComponent* Target);
    FCourierActionResult TryPress(AActor* Target);
    void Release();
    void Cancel();
    bool IsBusy() const { return State!=EState::None; }
    bool IsHolding() const { return State==EState::Holding; }
    FCourierInteractionPose GetPose() const;
    bool CanReachHand(FVector Point,bool bLeft=false) const;
    UPROPERTY(EditAnywhere,Category="Interaction",meta=(ClampMin="0.1")) float MaximumMassKg=20;
    UPROPERTY(EditAnywhere,Category="Interaction",meta=(ClampMin="0.05")) float ContactDelay=.25f;
    virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function) override;
    virtual void EndPlay(EEndPlayReason::Type Reason) override;
private:
    enum class EState:uint8 { None,Holding,Pressing,Recovering };
    EState State=EState::None;
    bool CheckTarget(AActor* Actor,FVector Point,FText& Reason) const;
    TWeakObjectPtr<UPrimitiveComponent> Held;
    TWeakObjectPtr<AActor> PressTarget;
    ECollisionResponse SavedPawnResponse=ECR_Block;
    ECollisionEnabled::Type SavedCollision=ECollisionEnabled::QueryAndPhysics;
    bool bSavedSimulating=false;
    bool bSubmitted=false;
    float Elapsed=0;
    float RecoveryStarted=0;
    uint64 Generation=0;
};
