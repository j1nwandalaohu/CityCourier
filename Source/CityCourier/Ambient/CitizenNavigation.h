#pragma once
#include "CoreMinimal.h"
class AAmbientPedestrian;
enum class ECitizenPathStatus : uint8 { Complete, Partial, Unreachable, NoNavigation };
struct FCitizenPathRequest
{
    TWeakObjectPtr<AAmbientPedestrian> Citizen;
    uint32 ActivationGeneration=0,RequestSerial=0;
    FVector Goal=FVector::ZeroVector;
};
struct FCitizenPathResult
{
    uint32 ActivationGeneration=0,RequestSerial=0;
    ECitizenPathStatus Status=ECitizenPathStatus::Unreachable;
    TArray<FVector> Points;
};
struct FCitizenNavigation
{
    TArray<FVector> Points;
    int32 Next=0,Retries=0;
    uint32 RequestSerial=0;
    bool bWaiting=false,bFailed=false;
    float LastRequest=-100,StuckSeconds=0;
    float DirectCheckedAt=-100;
    bool bDirectClear=false;
    FVector DirectGoal=FVector::ZeroVector;
    FVector RequestedGoal=FVector::ZeroVector;
    void Reset();
    void SetPath(const TArray<FVector>& Path);
    static bool CanMoveDirect(AAmbientPedestrian& Citizen,FVector Goal,float MaxLength=1500);
    float Advance(AAmbientPedestrian& Citizen,FVector Goal,float DesiredSpeed,float Delta);
};
