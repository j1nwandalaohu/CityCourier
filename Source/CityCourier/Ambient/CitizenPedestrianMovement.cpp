#include "Ambient/AmbientPedestrian.h"
#include "Ambient/AmbientTrafficManager.h"
#include "Components/BoxComponent.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

void AAmbientPedestrian::RequestPath(FVector Goal)
{
    if(!Crowd.IsValid() || Navigation.bWaiting || CombatClock-Navigation.LastRequest<.75f)return;
    Navigation.LastRequest=CombatClock;Navigation.RequestedGoal=Goal;Navigation.bWaiting=true;
    Navigation.RequestSerial=++PathSerial;
    ++PathRequestCount;
    Crowd->QueueCitizenPath({this,ActivationGeneration,PathSerial,Goal});
}
void AAmbientPedestrian::ApplyCitizenPath(const FCitizenPathResult& Result)
{
    if(!IsPopulationActive() || Result.ActivationGeneration!=ActivationGeneration || Result.RequestSerial!=PathSerial)return;
    Navigation.bWaiting=false;
    if(Result.Status==ECitizenPathStatus::Complete && Result.Points.Num()>0)
        Navigation.SetPath(Result.Points);
    else
    {
        Navigation.Points.Reset();
        if(Behavior.State==ECitizenBehaviorState::Flee || Behavior.State==ECitizenBehaviorState::Cower)
        {
            if(RejectedFleeGoals.Num()>=16)RejectedFleeGoals.RemoveAt(0);
            RejectedFleeGoals.Add({Navigation.RequestedGoal,CombatClock});
            bHasFleeGoal=false;Navigation.bFailed=true;FleeGoalAge=.5f;
        }
        else
        {
            if(Behavior.State==ECitizenBehaviorState::ReturnToPatrol)
            {
                if(RejectedReturnPoints.Num()>=16)RejectedReturnPoints.RemoveAt(0);
                RejectedReturnPoints.Add({Navigation.RequestedGoal,CombatClock});
            }
            if(++Navigation.Retries>2)Navigation.bFailed=true;
        }
    }
}
FVector AAmbientPedestrian::SelectFleeGoal()
{
    struct FCandidate {FVector Position;float Score;};TArray<FCandidate> Candidates;
    FVector Away=(GetActorLocation()-Behavior.LastSeenPosition).GetSafeNormal2D();
    if(Away.IsNearlyZero())Away=-GetActorForwardVector();
    RejectedFleeGoals.RemoveAll([&](const auto& R){return CombatClock-R.Value>=5;});
    const float CurrentDistance=FVector::Dist2D(GetActorLocation(),Behavior.LastSeenPosition);
    for(int32 I=0;I<8;++I)
    {
        const FVector Position=GetActorLocation()+Away.RotateAngleAxis(I*45.f,FVector::UpVector)*800;
        if(FVector::DotProduct((Position-GetActorLocation()).GetSafeNormal2D(),Away)<-.01
            || FVector::Dist2D(Position,Behavior.LastSeenPosition)<=CurrentDistance+100)continue;
        if(RejectedFleeGoals.ContainsByPredicate([&](const auto& R){return FVector::DistSquared2D(Position,R.Key)<FMath::Square(250.);}))continue;
        Candidates.Add({Position,float(FVector::DistSquared2D(Position,Behavior.LastSeenPosition))});
    }
    Candidates.Sort([](const auto& A,const auto& B){return A.Score>B.Score;});
    for(const auto& C:Candidates)
        if(FCitizenNavigation::CanMoveDirect(*this,C.Position,1000))return C.Position;
    // A blocked direct line may still have a safe route. Only the queued full
    // path query can approve it; projection alone never licenses movement.
    if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        for(const auto& C:Candidates)
        {
            FNavLocation Projected;
            if(Nav->ProjectPointToNavigation(C.Position-FVector(0,0,90),Projected,FVector(100,100,150)))
                return Projected.Location+FVector(0,0,90);
        }
    return GetActorLocation();
}
void AAmbientPedestrian::MoveBehavior(float Delta)
{
    if(Delta<=0)return;
    Body->ComponentVelocity=FVector::ZeroVector;
    FVector Goal=GetActorLocation();float DesiredSpeed=0;
    const bool bOuterGuard=CanParticipateInFight() && Crowd.IsValid() && !Crowd->TryJoinCitizenFight(*this,*Threat.Get());
    if(bOuterGuard)
    {
        const FVector Offset=FVector::ForwardVector.RotateAngleAxis(CrowdSlot*137.5f,FVector::UpVector)*260;
        Goal=Behavior.LastSeenPosition+Offset;DesiredSpeed=130;
        if(FVector::Dist2D(GetActorLocation(),Goal)<45){Speed=0;return;}
    }
    else switch(Behavior.State)
    {
    case ECitizenBehaviorState::Patrol:
        if(Patrol.Num()<2)return;
        if(FVector::Dist2D(GetActorLocation(),Patrol[NextPoint])<40)NextPoint=(NextPoint+1)%Patrol.Num();
        Goal=Patrol[NextPoint];DesiredSpeed=130;break;
    case ECitizenBehaviorState::ReturnToPatrol:
        if(Navigation.bFailed && CombatClock-Navigation.LastRequest<5){Speed=0;return;}
        Goal=PatrolReturnPoint();DesiredSpeed=130;break;
    case ECitizenBehaviorState::Chase:
    case ECitizenBehaviorState::Search:
        Goal=Behavior.LastSeenPosition;
        if(Behavior.State==ECitizenBehaviorState::Chase)
            Goal+=(GetActorLocation()-Goal).GetSafeNormal2D()*CloseFightDistance();
        DesiredSpeed=Behavior.State==ECitizenBehaviorState::Chase?BehaviorConfig.ChaseSpeed:BehaviorConfig.ApproachSpeed;break;
    case ECitizenBehaviorState::Flee:
    case ECitizenBehaviorState::Cower:
        if(FleeGoalAge>=.5f || (bHasFleeGoal && FVector::Dist2D(GetActorLocation(),FleeGoal)<80))
        {
            FleeGoalAge=0;FleeGoal=SelectFleeGoal();bHasFleeGoal=FVector::Dist2D(GetActorLocation(),FleeGoal)>100;
        }
        if(!bHasFleeGoal){Navigation.bFailed=true;Speed=0;return;}
        Goal=FleeGoal;DesiredSpeed=BehaviorConfig.ChaseSpeed;break;
    case ECitizenBehaviorState::Fight:
        if(Threat.IsValid() && CanSeeTarget(Threat.Get()) && FVector::Dist2D(GetActorLocation(),Behavior.LastSeenPosition)>CloseFightDistance()+1)
        {
            Goal=Behavior.LastSeenPosition+(GetActorLocation()-Behavior.LastSeenPosition).GetSafeNormal2D()*CloseFightDistance();
            DesiredSpeed=BehaviorConfig.ApproachSpeed;break;
        }
        [[fallthrough]];
    case ECitizenBehaviorState::Confront:
        {
            const FVector Known=Behavior.bHasThreat?Behavior.LastSeenPosition:
                Threat.IsValid() && CanSeeTarget(Threat.Get())?Threat->GetActorLocation():GetActorLocation()+GetActorForwardVector();
            SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(),(Known-GetActorLocation()).Rotation(),Delta,240));
        }
        Speed=0;return;
    default:Speed=0;return;
    }
    if(Navigation.StuckSeconds>=2.5f)
    {
        Navigation.StuckSeconds=0;Navigation.Points.Reset();
        if(++Navigation.Retries>2){Navigation.bFailed=true;Speed=0;return;}
        Navigation.bWaiting=false;++PathSerial;
    }
    // Reuse the direct segment for one perception period; every actual step
    // is swept and ground checked again by Advance.
    const bool bGoalChanged=FVector::DistSquared2D(Goal,Navigation.RequestedGoal)>FMath::Square(200.);
    if(bGoalChanged){Navigation.Points.Reset();Navigation.bWaiting=false;++PathSerial;}
    if(CombatClock-Navigation.DirectCheckedAt>=.25f || FVector::DistSquared2D(Goal,Navigation.DirectGoal)>FMath::Square(200.))
    {
        const FVector To=Goal-GetActorLocation();
        const FVector Ahead=GetActorLocation()+To*FMath::Min(1.,600./FMath::Max(1.,To.Size2D()));
        Navigation.bDirectClear=FCitizenNavigation::CanMoveDirect(*this,Ahead,650);
        Navigation.DirectCheckedAt=CombatClock;Navigation.DirectGoal=Goal;
    }
    const bool bDirect=Navigation.bDirectClear;
    if(bDirect)
    {
        Navigation.Points.Reset();Navigation.bFailed=false;Navigation.Retries=0;
        Navigation.RequestedGoal=Goal;
    }
    else if(!Navigation.Points.IsValidIndex(Navigation.Next))
    {
        RequestPath(Goal);
        if(Behavior.State==ECitizenBehaviorState::Cower){Speed=0;return;}
        // Local alternatives are short, swept moves, not teleporting around blockers.
        const FVector Direction=(Goal-GetActorLocation()).GetSafeNormal2D();
        for(float Angle:{45.f,-45.f,90.f,-90.f})
        {
            const FVector Side=GetActorLocation()+Direction.RotateAngleAxis(Angle,FVector::UpVector)*100;
            if(FCitizenNavigation::CanMoveDirect(*this,Side,150))
            {
                const float Unresolved=Navigation.StuckSeconds;
                Navigation.Advance(*this,Side,FMath::Min(DesiredSpeed,160.f),Delta);
                Navigation.StuckSeconds=Unresolved+Delta;return;
            }
        }
        Speed=0;Navigation.StuckSeconds+=Delta;return;
    }
    Navigation.Advance(*this,Goal,DesiredSpeed,Delta);
}
