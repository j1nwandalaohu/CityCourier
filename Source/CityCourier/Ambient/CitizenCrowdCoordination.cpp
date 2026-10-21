#include "Ambient/CitizenCrowdCoordination.h"
#include "Ambient/AmbientPedestrian.h"
bool FCitizenCrowdCoordination::RememberEvent(FGuid Id,double Now)
{
    Prune(Now);
    if(!Id.IsValid() || Witnessed.Contains(Id))return false;
    if(Witnessed.Num()>=1024)
    {
        FGuid Oldest;double Time=MAX_dbl;
        for(const auto& Pair:Witnessed)if(Pair.Value<Time){Oldest=Pair.Key;Time=Pair.Value;}
        Witnessed.Remove(Oldest);
    }
    Witnessed.Add(Id,Now);return true;
}
void FCitizenCrowdCoordination::Prune(double Now)
{
    for(auto It=Witnessed.CreateIterator();It;++It)if(Now-It.Value()>60)It.RemoveCurrent();
    for(auto It=Groups.CreateIterator();It;++It)
    {
        auto& G=It.Value();
        G.Participants.RemoveAll([&](const auto& P){return !P.IsValid() || !P->CanParticipateInFight() || P->GetThreat()!=It.Key().Get();});
        if(!G.Holder.IsValid() || !G.Holder->CanHoldAttack() || G.Expires<=Now)G.Holder.Reset();
        if(!It.Key().IsValid() || G.Participants.IsEmpty())It.RemoveCurrent();
    }
}
bool FCitizenCrowdCoordination::TryJoin(AAmbientPedestrian& P,AActor& Target,double Now)
{
    Prune(Now);auto& G=Groups.FindOrAdd(&Target);
    if(!G.Participants.Contains(&P))
    {if(G.Participants.Num()>=2)return false;G.Participants.Add(&P);}
    return true;
}
bool FCitizenCrowdCoordination::TryAcquire(AAmbientPedestrian& P,AActor& Target,double Now)
{
    if(!TryJoin(P,Target,Now))return false;
    auto& G=Groups.FindChecked(&Target);
    if(G.Holder.IsValid())return G.Holder==&P;
    AAmbientPedestrian* Ready=nullptr;
    for(const auto& Waiting:G.Participants)
        if(Waiting.IsValid() && Waiting->IsReadyForAttackToken()){Ready=Waiting.Get();break;}
    if(Ready!=&P)return false;
    G.Holder=&P;G.LastHolder=&P;G.Expires=Now+1.2;
    return true;
}
void FCitizenCrowdCoordination::Release(AAmbientPedestrian& P,bool bLeaveGroup)
{
    for(auto& Pair:Groups)
    {
        if(Pair.Value.Holder==&P)
        {
            Pair.Value.Holder.Reset();
            Pair.Value.Participants.Remove(&P);
            if(!bLeaveGroup)Pair.Value.Participants.Add(&P);
        }
        if(bLeaveGroup)Pair.Value.Participants.Remove(&P);
    }
}
bool FCitizenCrowdCoordination::Owns(const AAmbientPedestrian& P,const AActor& Target,double Now) const
{
    const auto* G=Groups.Find(const_cast<AActor*>(&Target));
    return G && G->Holder.Get()==&P && G->Expires>Now;
}
