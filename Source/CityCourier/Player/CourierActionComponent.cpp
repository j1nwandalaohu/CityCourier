#include "Player/CourierActionComponent.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "GameFramework/CharacterMovementComponent.h"

UCourierActionComponent::UCourierActionComponent()
{ PrimaryComponentTick.bCanEverTick=true; }
bool UCourierActionComponent::QueueAttack(bool bHeavy)
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); FText Reason;
    if(bSuppressed || !Rider || !Rider->CanPerformGroundAction(Reason,true)) return false;
    PendingEdges|=bHeavy?2:1; return true;
}
void UCourierActionComponent::Cancel()
{ Timeline.Cancel(); PendingEdges=0; }
void UCourierActionComponent::SetSuppressed(bool bValue,bool bPreserveAction)
{
    bSuppressed=bValue; bPreserve=bValue && bPreserveAction;
    PendingEdges=0; Timeline.ClearQueuedInput();
    if(bValue && !bPreserveAction) Cancel();
}
void UCourierActionComponent::MovementModeChanged(bool bAirborne)
{
    if(bAirborne) { Cancel(); JumpElapsed=0; bJumpStarted=false; }
    else { bJumpStarted=false; LandingElapsed=0; }
}
void UCourierActionComponent::JumpStarted()
{ bJumpStarted=true; JumpElapsed=0; }
bool UCourierActionComponent::IsGroundActionBlocked() const
{ return Timeline.GetAction()!=ECourierAction::None || LandingElapsed<.12; }
void UCourierActionComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,TickType,Function);
    if(!FMath::IsFinite(Delta) || Delta<=0 || bPreserve || (GetWorld() && GetWorld()->IsPaused())) return;
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); if(!Rider) return;
    auto* Movement=Rider->GetCharacterMovement();
    if(Movement->IsFalling()) JumpElapsed+=Delta; else LandingElapsed+=Delta;
    if(PendingEdges)
    {
        FText Reason;
        if(!bSuppressed && Rider->CanPerformGroundAction(Reason,true)) Timeline.RequestAttack((PendingEdges&2)!=0);
        PendingEdges=0;
    }
    Timeline.Advance(Delta);
    Rider->UpdateActionMovement();
}
void UCourierActionComponent::EndPlay(EEndPlayReason::Type Reason)
{ Cancel(); Super::EndPlay(Reason); }
