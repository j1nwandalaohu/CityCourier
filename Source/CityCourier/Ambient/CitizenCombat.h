#pragma once
#include "CoreMinimal.h"
enum class ECitizenAttackPhase : uint8 { None, Windup, Contact, Recovery };
struct FCitizenAttackSlice
{ FGuid InstanceId;bool bLeft=false;float Start=0,End=0; };
struct FCitizenCombat
{
    FGuid InstanceId;
    float Elapsed=0,Cooldown=0,FacingYaw=0;
    bool bLeft=false,bActive=false;
    uint32 Generation=0;
    bool TryStart(bool bUseLeft,float Yaw,float Interval);
    TArray<FCitizenAttackSlice> Advance(float Delta);
    void Cancel();
    void Reset();
    ECitizenAttackPhase Phase() const;
};
