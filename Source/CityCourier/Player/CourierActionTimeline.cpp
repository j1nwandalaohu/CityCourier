#include "Player/CourierActionTimeline.h"

void FCourierActionTimeline::Start(ECourierAction Next)
{ Action=Next; Queued=ECourierAction::None; Elapsed=0; InstanceId=FGuid::NewGuid(); }
void FCourierActionTimeline::Cancel()
{ Action=Queued=ECourierAction::None; Elapsed=0; }
bool FCourierActionTimeline::RequestAttack(bool bHeavy)
{
    if(Action==ECourierAction::None) { Start(bHeavy?ECourierAction::Heavy:ECourierAction::LightL); return true; }
    if((Action!=ECourierAction::LightL && Action!=ECourierAction::LightR) || Elapsed<.18 || Elapsed>=.40) return false;
    if(Action==ECourierAction::LightR && !bHeavy) return false;
    Queued=bHeavy?ECourierAction::Heavy:ECourierAction::LightR; return true;
}
void FCourierActionTimeline::ReceiveHit(bool bFront)
{ if(!IsReacting()) Start(bFront?ECourierAction::HitFront:ECourierAction::HitBack); }
TArray<FActionSlice> FCourierActionTimeline::Advance(double Delta)
{
    TArray<FActionSlice> Slices;
    if(!FMath::IsFinite(Delta) || Delta<=0) return Slices;
    while(Action!=ECourierAction::None && Delta>0)
    {
        const bool bHeavy=Action==ECourierAction::Heavy;
        const double Duration=IsReacting()?.40:bHeavy?.95:.55;
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
            else Cancel();
        }
    }
    return Slices;
}
