#include "Ambient/AmbientPedestrian.h"
#include "Ambient/AmbientTrafficManager.h"
#include "Ambient/CitizenAnimInstance.h"
#include "Ambient/CitizenCrowdCoordination.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierActionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarCitizenDebug(TEXT("Courier.CitizenDebug"),0,TEXT("Show citizen behavior diagnostics."),ECVF_Cheat);
#endif

void AAmbientPedestrian::SetPatrol(const TArray<FVector>& Points)
{ ResetCitizenRuntime();Super::SetPatrol(Points); }
void AAmbientPedestrian::EndPlay(EEndPlayReason::Type Reason)
{CancelCombat(true);Threat.Reset();Navigation.Reset();Super::EndPlay(Reason);}
void AAmbientPedestrian::ResetCitizenRuntime()
{
    CancelCombat(true);++ActivationGeneration;++PathSerial;Threat.Reset();BumpTarget.Reset();
    Behavior.Reset(ActivationGeneration,BehaviorConfig);Navigation.Reset();Combat.Reset();
    RecentStrikes.Empty();CombatClock=0;LastLightTime=-100;SenseAge=0;FleeGoalAge=0;
    BumpCount=0;BumpWindow=NextBumpSample=0;bHasFleeGoal=false;bNextLeft=true;bHitThisAttack=false;
    RejectedFleeGoals.Reset();RejectedReturnPoints.Reset();PathRequestCount=0;
    SetReaction(ECitizenReaction::Walking);
}
void AAmbientPedestrian::InitializeCitizen(AAmbientTrafficManager* Manager,int32 Slot)
{
    Crowd=Manager;CrowdSlot=Slot;Random.Initialize(int32(HashCombine(GetTypeHash(Slot),GetTypeHash(ActivationGeneration))));
    const float Choice=Random.FRand();
    if(!bOverrideBehaviorConfig)BehaviorConfig=FCitizenBehaviorConfig::ForPersonality(Choice<.3f?ECitizenPersonality::Cautious:
        Choice<.8f?ECitizenPersonality::Ordinary:ECitizenPersonality::Aggressive);
    Behavior.Reset(Random.RandHelper(MAX_int32),BehaviorConfig);SenseAge=-Slot*.02f;
}
bool AAmbientPedestrian::ReceiveCourierStrikeContext(const FCourierStrikeContext& C)
{
    if(C.Instigator.Get()==this)return false;
    if(!ReceiveCourierStrike(C.InstanceId,C.Origin,C.Action))return false;
    AActor* Attacker=C.Instigator.Get();
    if(!IsValid(Attacker) || Attacker->IsActorBeingDestroyed() || Attacker->GetWorld()!=GetWorld())return true;
    Threat=Attacker;
    Behavior.RegisterThreat(C.InstanceId,C.Origin,C.Action==ECourierAction::Heavy || Reaction==ECitizenReaction::Falling,true);
    bHasFleeGoal=false;Navigation.Reset();++PathSerial;
    if(Crowd.IsValid())Crowd->NotifyCitizenAssault(*this,C);
    return true;
}
void AAmbientPedestrian::ReceiveWitnessEvent(const FCitizenWitnessEvent& E)
{
    if(!IsPopulationActive() || !IsBodyAvailable() || Behavior.bDirectThreat || E.HopCount!=1 || !E.Attacker.IsValid() || E.Attacker.Get()==this)return;
    Threat=E.Attacker;
    Behavior.RegisterThreat(E.EventId,E.Origin,false,false);
}
bool AAmbientPedestrian::CanSeeTarget(const AActor* Target) const
{
    if(!IsValid(Target) || FVector::DistSquared2D(GetActorLocation(),Target->GetActorLocation())>FMath::Square(6000.))return false;
    const FVector Eye=Person->GetComponentLocation()+FVector(0,0,150);
    FVector End=Target->GetActorLocation();End.Z+=45;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CitizenSight),false,this);FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,End,ECC_Visibility,Q) || Hit.GetActor()==Target;
}
bool AAmbientPedestrian::CanAttackTarget(const AActor* Target) const
{
    if(!IsValid(Target) || Target==this || Target->IsActorBeingDestroyed())return false;
    if(const auto* Rider=Cast<ACourierCharacter>(Target))
        return !Rider->IsMounted() && !Rider->GetActions()->IsSuppressed();
    if(const auto* Citizen=Cast<AAmbientPedestrian>(Target))return Citizen->IsPopulationActive() && Citizen->IsBodyAvailable();
    return false;
}
FCitizenPerception AAmbientPedestrian::Sense() const
{
    FCitizenPerception P;P.SelfPosition=GetActorLocation();P.bBodyAvailable=IsBodyAvailable();P.bPathFailed=Navigation.bFailed;
    const auto* Anim=Cast<UCitizenAnimInstance>(Person->GetAnimInstance());P.bAttackReady=Anim && Anim->HasCombatClips();
    AActor* Target=Threat.Get();
    P.bTargetValid=IsValid(Target) && !Target->IsActorBeingDestroyed() && Target->GetWorld()==GetWorld();
    if(const auto* Citizen=Cast<AAmbientPedestrian>(Target))P.bTargetValid&=Citizen->IsPopulationActive();
    P.bTargetVisible=P.bTargetValid && CanSeeTarget(Target);
    if(P.bTargetVisible)P.VisibleTargetPosition=Target->GetActorLocation();
    const FVector Known=P.bTargetVisible?P.VisibleTargetPosition:Behavior.LastSeenPosition;
    P.TargetDistanceCm=FVector::Dist2D(GetActorLocation(),Known);
    P.TargetAngleDeg=FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,(Known-GetActorLocation()).Rotation().Yaw));
    if(P.bTargetVisible)if(const auto* Rider=Cast<ACourierCharacter>(Target))P.bTargetMounted=Rider->IsMounted();
    P.bAtPatrol=FVector::Dist2D(GetActorLocation(),PatrolReturnPoint())<60;
    return P;
}
void AAmbientPedestrian::AdvanceBehavior(float Delta)
{
    SenseAge+=Delta;FleeGoalAge+=Delta;
    if(SenseAge>=.25f)
    {
        const auto Before=Behavior.State;
        Behavior.Advance(SenseAge,Sense());SenseAge=0;
        if(Before!=Behavior.State)
        {
            const bool bSameEscape=(Before==ECitizenBehaviorState::Flee || Before==ECitizenBehaviorState::Cower)
                && (Behavior.State==ECitizenBehaviorState::Flee || Behavior.State==ECitizenBehaviorState::Cower);
            if(!bSameEscape){Navigation.Points.Reset();Navigation.bWaiting=false;++PathSerial;}
            if(Behavior.State!=ECitizenBehaviorState::Fight)CancelCombat(true);
            if(Behavior.State==ECitizenBehaviorState::Disengage || Behavior.State==ECitizenBehaviorState::Patrol)
            {Threat.Reset();Navigation.Reset();bHasFleeGoal=false;}
        }
    }
#if !UE_BUILD_SHIPPING
    if(CVarCitizenDebug.GetValueOnGameThread())
    {
        const auto* Anim=Cast<UCitizenAnimInstance>(Person->GetAnimInstance());
        const bool bToken=Threat.IsValid() && Crowd.IsValid() && Crowd->OwnsCitizenAttack(*this,*Threat.Get());
        const FString Text=FString::Printf(TEXT("body=%d state=%d type=%d target=%s\nage=%.1f/%.1f pressure=%.0f path=%d requests=%d exit=%d\ntoken=%d art=%s"),
            int32(Reaction),int32(Behavior.State),int32(BehaviorConfig.Personality),*GetNameSafe(Threat.Get()),
            Behavior.IncidentAge,BehaviorConfig.ChaseSeconds,Behavior.Pressure,Navigation.Points.Num(),PathRequestCount,int32(Behavior.ExitReason),
            bToken,Anim && Anim->HasCombatClips()?TEXT("ready"):TEXT("missing/incompatible guard or punch"));
        DrawDebugString(GetWorld(),GetActorLocation()+FVector(0,0,115),Text,nullptr,FColor::Yellow,0,true);
        if(Behavior.bHasThreat)DrawDebugSphere(GetWorld(),Behavior.LastSeenPosition,15,8,FColor::Orange,false,0);
    }
#endif
}
FVector AAmbientPedestrian::PatrolReturnPoint() const
{
    FVector Best=Patrol.IsEmpty()?GetActorLocation():Patrol[0],Fallback=Best;
    double Distance=MAX_dbl,FallbackDistance=MAX_dbl;
    TArray<FVector> Candidates=Patrol;
    for(int32 I=1;I<Patrol.Num();++I)
        Candidates.Add(FMath::ClosestPointOnSegment(GetActorLocation(),Patrol[I-1],Patrol[I]));
    for(const FVector& Candidate:Candidates)
    {
        const double D=FVector::DistSquared2D(GetActorLocation(),Candidate);
        if(D<FallbackDistance){FallbackDistance=D;Fallback=Candidate;}
        if(RejectedReturnPoints.ContainsByPredicate([&](const auto& R){return CombatClock-R.Value<5 && FVector::DistSquared2D(Candidate,R.Key)<FMath::Square(100.);}))continue;
        if(D<Distance){Distance=D;Best=Candidate;}
    }
    return Distance==MAX_dbl?Fallback:Best;
}
void AAmbientPedestrian::ObserveBump(AActor* Actor)
{
    if(!Cast<ACourierCharacter>(Actor) || Behavior.bHasThreat || CombatClock<NextBumpSample)return;
    NextBumpSample=CombatClock+.5f;
    if(BumpTarget.Get()!=Actor || CombatClock-BumpWindow>2){BumpTarget=Actor;BumpWindow=CombatClock;BumpCount=0;}
    if(++BumpCount>=3){Threat=Actor;Behavior.Warn();BumpCount=0;}
}
