#include "Player/CourierActionTimeline.h"

void FCourierActionTimeline::Start(ECourierAction Next)
{ Action=Next; Queued=ECourierAction::None; Elapsed=0; InstanceId=FGuid::NewGuid(); }
void FCourierActionTimeline::Cancel()
{ Action=Queued=ECourierAction::None; Elapsed=0; ConsecutiveHits=0; HitGap=0; }
void FCourierActionTimeline::ConfigureRecovery(double FallDuration,double RiseDuration)
{
    if(FMath::IsFinite(FallDuration) && FallDuration>0 && FMath::IsFinite(RiseDuration) && RiseDuration>0)
    { FallLength=FallDuration; RiseLength=RiseDuration; bRecoveryEnabled=true; }
}
bool FCourierActionTimeline::RequestAttack(bool bHeavy)
{
    if(Action==ECourierAction::None) { Start(bHeavy?ECourierAction::Heavy:ECourierAction::LightL); return true; }
    if((Action!=ECourierAction::LightL && Action!=ECourierAction::LightR) || Elapsed<.18 || Elapsed>=.40) return false;
    if(Action==ECourierAction::LightR && !bHeavy) return false;
    Queued=bHeavy?ECourierAction::Heavy:ECourierAction::LightR; return true;
}
void FCourierActionTimeline::ReceiveHit(bool bFront)
{
    if(IsRecovering()) return;
    ConsecutiveHits=HitGap<=1.2?FMath::Min(ConsecutiveHits+1,3):1; HitGap=0;
    if(bRecoveryEnabled && ConsecutiveHits>=3) { ConsecutiveHits=0; Start(ECourierAction::Knockdown); }
    else if(!IsReacting()) Start(bFront?ECourierAction::HitFront:ECourierAction::HitBack);
}
TArray<FActionSlice> FCourierActionTimeline::Advance(double Delta)
{
    TArray<FActionSlice> Slices;
    if(!FMath::IsFinite(Delta) || Delta<=0) return Slices;
    HitGap+=Delta;
    while(Action!=ECourierAction::None && Delta>0)
    {
        const bool bHeavy=Action==ECourierAction::Heavy;
        const double Duration=Action==ECourierAction::Knockdown?FallLength:Action==ECourierAction::GroundHold?.6:
            Action==ECourierAction::GetUp?RiseLength:IsReacting()?.40:bHeavy?.95:.55;
        const bool bChain=Queued!=ECourierAction::None;
        const double Boundary=bChain?.44:Duration;
        const double End=FMath::Min(Elapsed+Delta,Boundary);
        if(IsAttacking())
        {
            const double From=FMath::Max(Elapsed,bHeavy?.34:.14), To=FMath::Min(End,bHeavy?.46:.22);
            if(To>From) Slices.Add({Action,InstanceId,From,To});
        }
        Delta-=End-Elapsed; Elapsed=End;
        if(Elapsed>=Boundary)
        {
            if(bChain) Start(Queued);
            else if(Action==ECourierAction::Knockdown) Start(ECourierAction::GroundHold);
            else if(Action==ECourierAction::GroundHold) Start(ECourierAction::GetUp);
            else if(Action==ECourierAction::GetUp) Cancel();
            else { Action=Queued=ECourierAction::None; Elapsed=0; }
        }
    }
    return Slices;
}
