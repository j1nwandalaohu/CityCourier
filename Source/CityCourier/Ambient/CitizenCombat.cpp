#include "Ambient/CitizenCombat.h"
bool FCitizenCombat::TryStart(bool bUseLeft,float Yaw,float Interval)
{
    if(bActive || Cooldown>0 || !FMath::IsFinite(Yaw))return false;
    ++Generation;InstanceId=FGuid::NewGuid();Elapsed=0;FacingYaw=Yaw;bLeft=bUseLeft;bActive=true;
    Cooldown=FMath::Clamp(Interval,1.f,1.5f);return true;
}
TArray<FCitizenAttackSlice> FCitizenCombat::Advance(float Delta)
{
    TArray<FCitizenAttackSlice> Slices;
    if(!FMath::IsFinite(Delta) || Delta<=0)return Slices;
    Cooldown=FMath::Max(0.f,Cooldown-Delta);
    if(!bActive)return Slices;
    const float Start=FMath::Max(Elapsed,.22f),End=FMath::Min(Elapsed+Delta,.38f);
    if(End>Start)Slices.Add({InstanceId,bLeft,Start,End});
    Elapsed=FMath::Min(.8f,Elapsed+Delta);
    if(Elapsed>=.8f)bActive=false;
    return Slices;
}
void FCitizenCombat::Cancel(){++Generation;bActive=false;InstanceId.Invalidate();Elapsed=0;}
void FCitizenCombat::Reset(){*this=FCitizenCombat();}
ECitizenAttackPhase FCitizenCombat::Phase() const
{ return !bActive?ECitizenAttackPhase::None:Elapsed<.22f?ECitizenAttackPhase::Windup:Elapsed<.38f?ECitizenAttackPhase::Contact:ECitizenAttackPhase::Recovery; }
