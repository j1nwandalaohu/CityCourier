#include "Ambient/AmbientPedestrian.h"
#include "Ambient/AmbientTrafficManager.h"
#include "Ambient/CitizenAnimInstance.h"
#include "Player/CourierActionAnimation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Player/CourierCharacter.h"
#include "Engine/World.h"

bool AAmbientPedestrian::CanHoldAttack() const
{return IsPopulationActive() && IsBodyAvailable() && Behavior.State==ECitizenBehaviorState::Fight && CanAttackTarget(Threat.Get());}
float AAmbientPedestrian::CloseFightDistance() const
{
    float Radius=34;
    if(const auto* Rider=Cast<ACourierCharacter>(Threat.Get()))Radius=Rider->GetCapsuleComponent()->GetScaledCapsuleRadius();
    else if(const auto* Citizen=Cast<AAmbientPedestrian>(Threat.Get()))Radius=Citizen->Body->GetScaledBoxExtent().X;
    return Body->GetScaledBoxExtent().X+Radius+2;
}
bool AAmbientPedestrian::CanParticipateInFight() const
{
    return IsPopulationActive() && IsBodyAvailable() && Behavior.bHasThreat && CanAttackTarget(Threat.Get())
        && (Behavior.State==ECitizenBehaviorState::Fight || Behavior.State==ECitizenBehaviorState::Chase || Behavior.State==ECitizenBehaviorState::Confront);
}
bool AAmbientPedestrian::IsReadyForAttackToken() const
{
    if(!CanHoldAttack() || Combat.bActive || Combat.Cooldown>0)return false;
    const FVector To=Threat->GetActorLocation()-GetActorLocation();
    return To.Size2D()<=FMath::Min(BehaviorConfig.AttackDistance,CloseFightDistance()+2) && FMath::Abs(To.Z)<140
        && FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,To.Rotation().Yaw))<=BehaviorConfig.AttackAngle && CanSeeTarget(Threat.Get());
}
void AAmbientPedestrian::CancelCombat(bool bLeaveGroup)
{
    Combat.Cancel();bHitThisAttack=false;
    if(Crowd.IsValid())Crowd->ReleaseCitizenAttack(*this,bLeaveGroup);
}
void AAmbientPedestrian::AdvanceCombat(float Delta)
{
    if(!FMath::IsFinite(Delta) || Delta<=0)return;
    AActor* Target=Threat.Get();
    const auto* Anim=Cast<UCitizenAnimInstance>(Person->GetAnimInstance());
    if(!CanHoldAttack() || !Anim || !Anim->HasCombatClips())
    {CancelCombat();Combat.Advance(Delta);return;}
    if(Combat.bActive && Crowd.IsValid() && !Crowd->OwnsCitizenAttack(*this,*Target))
    {CancelCombat();Combat.Advance(Delta);return;}
    if(!Combat.bActive && Combat.Cooldown<=0 && CanSeeTarget(Target))
    {
        const FVector To=Target->GetActorLocation()-GetActorLocation();
        const float Angle=FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,To.Rotation().Yaw));
        if(To.Size2D()<=FMath::Min(BehaviorConfig.AttackDistance,CloseFightDistance()+2) && FMath::Abs(To.Z)<140 && Angle<=BehaviorConfig.AttackAngle
            && (!Crowd.IsValid() || Crowd->TryAcquireCitizenAttack(*this,*Target)))
        {
            if(Combat.TryStart(bNextLeft,GetActorRotation().Yaw,Random.FRandRange(1.f,1.5f)))
            {bNextLeft=!bNextLeft;bHitThisAttack=false;}
        }
    }
    if(Combat.bActive)SetActorRotation(FRotator(0,Combat.FacingYaw,0));
    const uint32 Generation=Combat.Generation;
    const bool bWasActive=Combat.bActive;
    for(const auto& Slice:Combat.Advance(Delta))TraceCounterpunch(Slice,Generation);
    if(bWasActive && !Combat.bActive && Crowd.IsValid())Crowd->ReleaseCitizenAttack(*this,false);
}
void AAmbientPedestrian::TraceCounterpunch(const FCitizenAttackSlice& Slice,uint32 Generation)
{
    const auto* Anim=Cast<UCitizenAnimInstance>(Person->GetAnimInstance());
    auto* Clip=Anim?Anim->GetPunchClip(Slice.bLeft):nullptr;
    if(bHitThisAttack || !FCourierActionAnimation::Compatible(Person,Clip,.8))return;
    FVector Previous;
    if(!FCourierActionAnimation::SampleFist(Person,Clip,Slice.bLeft,Slice.Start,Previous))return;
    const int32 Steps=FMath::Max(1,FMath::CeilToInt((Slice.End-Slice.Start)*120));
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CitizenCounterpunch),false,this);
    for(int32 I=1;I<=Steps;++I)
    {
        if(Combat.Generation!=Generation || !CanHoldAttack() || bHitThisAttack)return;
        if(Crowd.IsValid() && !Crowd->OwnsCitizenAttack(*this,*Threat.Get()))return;
        FVector Next;
        if(!FCourierActionAnimation::SampleFist(Person,Clip,Slice.bLeft,FMath::Lerp(Slice.Start,Slice.End,float(I)/Steps),Next))return;
        if(FVector::DistSquared2D(Next,GetActorLocation())<=FMath::Square(175.))
        {
            TArray<FHitResult> Hits;
            GetWorld()->SweepMultiByChannel(Hits,Previous,Next,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(12),Q);
            Hits.Sort([](const auto& A,const auto& B){return A.Time==B.Time?A.bBlockingHit && !B.bBlockingHit:A.Time<B.Time;});
            for(const auto& Hit:Hits)
            {
                if(Hit.GetActor()==Threat.Get())
                {
                    const FVector Contact=FMath::Lerp(Previous,Next,float(Hit.Time));
                    FHitResult Wall;
                    const bool bBlocked=GetWorld()->LineTraceSingleByChannel(Wall,
                        FVector(GetActorLocation().X,GetActorLocation().Y,Contact.Z),Contact,ECC_Visibility,Q);
                    if(!bBlocked || Wall.GetActor()==Hit.GetActor())
                    {
                        if(auto* Receiver=Cast<ICourierStrikeReceiver>(Hit.GetActor()))
                        {
                            bHitThisAttack=true;
                            Receiver->ReceiveCourierStrikeContext({Slice.InstanceId,this,GetActorLocation(),
                                Slice.bLeft?ECourierAction::LightL:ECourierAction::LightR});
                            return;
                        }
                    }
                }
                if(Hit.bBlockingHit)break;
            }
        }
        Previous=Next;
    }
}
