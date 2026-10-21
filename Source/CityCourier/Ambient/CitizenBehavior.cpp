#include "Ambient/CitizenBehavior.h"

FCitizenBehaviorConfig FCitizenBehaviorConfig::ForPersonality(ECitizenPersonality Personality)
{
    FCitizenBehaviorConfig C;C.Personality=Personality;
    if(Personality==ECitizenPersonality::Cautious)
    { C.Courage=.2f;C.FightChance=0;C.bCanChase=false;C.RetreatPressure=30; }
    else if(Personality==ECitizenPersonality::Aggressive)
    { C.Courage=.95f;C.FightChance=1;C.ChaseSeconds=30;C.LeashCm=5000;C.RetreatPressure=100; }
    return C;
}
void FCitizenBehavior::Reset(int32 Seed,const FCitizenBehaviorConfig& InConfig)
{
    *this=FCitizenBehavior();Config=InConfig;Random.Initialize(Seed);
}
void FCitizenBehavior::SetState(ECitizenBehaviorState Next)
{ if(State!=Next){State=Next;StateAge=0;} }
void FCitizenBehavior::RegisterThreat(FGuid EventId,FVector Origin,bool bHeavy,bool bDirect)
{
    if(!EventId.IsValid() || EventId==LastEvent || Origin.ContainsNaN() || (!bDirect && bDirectThreat))return;
    LastEvent=EventId;
    const bool bNewIncident=!bHasThreat;
    if(bNewIncident)
    {
        bWillFight=Random.FRand()<FMath::Clamp(Config.FightChance*(bDirect?1.f:Config.Courage),0.f,1.f);
        AssessSeconds=Random.FRandRange(.35f,.75f);
        EventOrigin=Origin;IncidentAge=0;UnseenAge=0;SafeAge=0;
    }
    bHasThreat=true;bDirectThreat|=bDirect;bWarningOnly=false;ExitReason=ECitizenExitReason::None;
    if(bDirect)Pressure=FMath::Min(150.f,Pressure+(bHeavy?45.f:20.f));
    LastSeenPosition=Origin;
    // New contacts update threat information, never the incident budget.
    if(bNewIncident || State==ECitizenBehaviorState::Patrol || State==ECitizenBehaviorState::ReturnToPatrol || State==ECitizenBehaviorState::Disengage)
        SetState(ECitizenBehaviorState::AssessThreat);
}
void FCitizenBehavior::Warn()
{ if(!bHasThreat && State==ECitizenBehaviorState::Patrol){bWarningOnly=true;SetState(ECitizenBehaviorState::Confront);} }
void FCitizenBehavior::Disengage(ECitizenExitReason Reason)
{
    bHasThreat=bDirectThreat=bWarningOnly=false;ExitReason=Reason;SafeAge=0;
    SetState(ECitizenBehaviorState::Disengage);
}
void FCitizenBehavior::Advance(float Delta,const FCitizenPerception& P)
{
    if(!FMath::IsFinite(Delta) || Delta<=0)return;
    StateAge+=Delta;
    if(!bHasThreat)
    {
        Pressure=FMath::Max(0.f,Pressure-5.f*Delta);
        if(bWarningOnly){if(StateAge>=1){bWarningOnly=false;SetState(ECitizenBehaviorState::ReturnToPatrol);}return;}
        if(State==ECitizenBehaviorState::Disengage && StateAge>=1)SetState(ECitizenBehaviorState::ReturnToPatrol);
        if(State==ECitizenBehaviorState::ReturnToPatrol && P.bAtPatrol)SetState(ECitizenBehaviorState::Patrol);
        return;
    }
    IncidentAge+=Delta;
    if(!P.bTargetValid){Disengage(ECitizenExitReason::TargetInvalid);return;}
    if(P.bTargetVisible && !(P.bTargetMounted && State==ECitizenBehaviorState::Search))
    {LastSeenPosition=P.VisibleTargetPosition;UnseenAge=0;}
    else UnseenAge+=Delta;
    // Assessment starts after flinching/get-up, while the total incident clock continues.
    if(!P.bBodyAvailable){if(State==ECitizenBehaviorState::AssessThreat)StateAge=0;return;}
    const bool bEscaping=State==ECitizenBehaviorState::Flee || State==ECitizenBehaviorState::Cower;
    if(!bEscaping && IncidentAge>=Config.ChaseSeconds){Disengage(ECitizenExitReason::Timeout);return;}
    if(!bEscaping && FVector::DistSquared2D(P.SelfPosition,EventOrigin)>FMath::Square(Config.LeashCm))
    {Disengage(ECitizenExitReason::LeashExceeded);return;}
    if(State==ECitizenBehaviorState::AssessThreat && StateAge<AssessSeconds)return;
    if(!bWillFight || Pressure>=Config.RetreatPressure || !P.bAttackReady)
        SetState(ECitizenBehaviorState::Flee);
    if(State==ECitizenBehaviorState::Flee || State==ECitizenBehaviorState::Cower)
    {
        // Without a visible threat, last-seen distance is a conservative safety estimate.
        SafeAge=(!P.bTargetVisible && P.TargetDistanceCm>=Config.SafeDistance)?SafeAge+Delta:0;
        if(SafeAge>=Config.SafeSeconds){Disengage(ECitizenExitReason::Safe);return;}
        SetState(P.bPathFailed?ECitizenBehaviorState::Cower:ECitizenBehaviorState::Flee);
        return;
    }
    if(P.bPathFailed){Disengage(ECitizenExitReason::Unreachable);return;}
    if(State==ECitizenBehaviorState::Search)
    {
        if(P.bTargetVisible && !P.bTargetMounted)SetState(ECitizenBehaviorState::Chase);
        else if(StateAge>=Config.SearchSeconds)Disengage(ECitizenExitReason::LostTarget);
        return;
    }
    if(UnseenAge>=Config.MemorySeconds){SetState(ECitizenBehaviorState::Search);return;}
    if(P.bTargetMounted)
    {SetState(ECitizenBehaviorState::Search);return;}
    if(State==ECitizenBehaviorState::Fight && P.bTargetVisible && P.TargetDistanceCm<=Config.LeaveFightDistance)return;
    if(P.bTargetVisible && P.TargetDistanceCm<=Config.AttackDistance)
        SetState(ECitizenBehaviorState::Fight);
    else if(Config.bCanChase)SetState(ECitizenBehaviorState::Chase);
    else Disengage(ECitizenExitReason::LostTarget);
}
