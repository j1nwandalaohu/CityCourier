#pragma once
#include "CoreMinimal.h"
class AAmbientPedestrian;
struct FCitizenWitnessEvent
{
    FGuid EventId;
    TWeakObjectPtr<AActor> Attacker,Victim;
    FVector Origin=FVector::ZeroVector;
    uint8 HopCount=0;
};
struct FCitizenCrowdCoordination
{
    struct FGroup
    {
        TArray<TWeakObjectPtr<AAmbientPedestrian>> Participants;
        TWeakObjectPtr<AAmbientPedestrian> Holder,LastHolder;
        double Expires=0;
    };
    TMap<TWeakObjectPtr<AActor>,FGroup> Groups;
    TMap<FGuid,double> Witnessed;
    bool RememberEvent(FGuid Id,double Now);
    bool TryAcquire(AAmbientPedestrian& Citizen,AActor& Target,double Now);
    bool TryJoin(AAmbientPedestrian& Citizen,AActor& Target,double Now);
    bool Owns(const AAmbientPedestrian& Citizen,const AActor& Target,double Now) const;
    void Release(AAmbientPedestrian& Citizen,bool bLeaveGroup=true);
    void Prune(double Now);
};
