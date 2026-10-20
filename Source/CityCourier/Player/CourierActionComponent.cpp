#include "Player/CourierActionComponent.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/CourierActionAnimation.h"
#include "Player/CourierStrikeReceiver.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"

UCourierActionComponent::UCourierActionComponent()
{ PrimaryComponentTick.bCanEverTick=true; }
void UCourierActionComponent::BeginPlay()
{
    Super::BeginPlay();
    for(auto Action:{ECourierAction::LightL,ECourierAction::LightR,ECourierAction::Heavy,ECourierAction::HitFront,ECourierAction::HitBack})
        if(!ActionAnimations.Contains(Action)) if(auto* Sequence=FCourierActionAnimation::Load(FCourierActionAnimation::SequenceName(Action))) ActionAnimations.Add(Action,Sequence);
    for(FName Name:{FName(TEXT("A_JumpStart")),FName(TEXT("A_JumpLoop")),FName(TEXT("A_JumpFall")),FName(TEXT("A_Land")),FName(TEXT("A_RunStart")),FName(TEXT("A_RunStop"))})
        if(!MovementAnimations.Contains(Name)) if(auto* Sequence=FCourierActionAnimation::Load(Name)) MovementAnimations.Add(Name,Sequence);
}
void UCourierActionComponent::Feedback(const FText& Message) const
{ if(auto* Rider=Cast<ACourierCharacter>(GetOwner())) if(auto* PC=Cast<ACourierController>(Rider->GetController())) PC->ShowActionFeedback(Message); }
bool UCourierActionComponent::HasAttackAnimation(bool bHeavy) const
{
    const auto* Rider=Cast<ACourierCharacter>(GetOwner());
    const auto* Ptr=ActionAnimations.Find(bHeavy?ECourierAction::Heavy:ECourierAction::LightL);
    return IsPlaybackReady() && Rider && Ptr && FCourierActionAnimation::Compatible(Rider->GetMesh(),Ptr->Get(),bHeavy?.95:.55);
}
bool UCourierActionComponent::IsPlaybackReady() const
{
    const auto* Rider=Cast<ACourierCharacter>(GetOwner());
    const auto* Anim=Rider?Rider->GetMesh()->GetAnimInstance():nullptr;
    return Anim && Anim->GetSlotNodeGlobalWeight(TEXT("FullBodyAction"))>SMALL_NUMBER;
}
bool UCourierActionComponent::QueueAttack(bool bHeavy)
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); FText Reason;
    if(Rider && Rider->GetCharacterMovement()->IsFalling()) return bHeavy?false:QueueJumpPunch();
    if(bSuppressed || !Rider || !Rider->CanPerformGroundAction(Reason,true)) { if(!Reason.IsEmpty()) Feedback(Reason); return false; }
    const auto Next=bHeavy?ECourierAction::Heavy:Timeline.GetAction()==ECourierAction::LightL?ECourierAction::LightR:ECourierAction::LightL;
    auto* Ptr=ActionAnimations.Find(Next);
    if(!IsPlaybackReady() || !Ptr || !FCourierActionAnimation::Compatible(Rider->GetMesh(),Ptr->Get(),bHeavy?.95:.55))
    { Feedback(FText::FromString(TEXT("出拳动画尚未就绪，请等待美术资源接入"))); return false; }
    PendingEdges|=bHeavy?2:1; return true;
}
bool UCourierActionComponent::QueueJumpPunch()
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner());
    if(bSuppressed || !Rider || !Rider->GetCharacterMovement()->IsFalling() || Rider->IsMounted() || Rider->IsProne() || Timeline.GetAction()!=ECourierAction::None) return false;
    const auto* Ptr=ActionAnimations.Find(ECourierAction::LightL);
    if(!IsPlaybackReady() || !Ptr || !FCourierActionAnimation::Compatible(Rider->GetMesh(),Ptr->Get(),.55)) return false;
    return Timeline.RequestAttack(false);
}
void UCourierActionComponent::Cancel()
{
    ++InterruptionGeneration;
    TGuardValue<bool> Changing(bChangingMontage,true);
    Timeline.Cancel(); PendingEdges=0; Struck.Empty();
    if(PlayingMontage) if(auto* Rider=Cast<ACourierCharacter>(GetOwner())) if(auto* Anim=Rider->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.05f,PlayingMontage);
    PlayingMontage=nullptr; PlayingSequence=nullptr; PlayingMovement=NAME_None; PlayingInstance.Invalidate();
    bPlaybackFinishedNaturally=false;
    if(auto* Rider=Cast<ACourierCharacter>(GetOwner())) Rider->UpdateActionMovement();
}
void UCourierActionComponent::SetSuppressed(bool bValue,bool bPreserveAction)
{
    bSuppressed=bValue; bPreserve=bValue && bPreserveAction;
    if(bValue) ++InterruptionGeneration;
    PendingEdges=0; Timeline.ClearQueuedInput();
    if(bValue && !bPreserveAction) Cancel();
}
void UCourierActionComponent::MovementModeChanged(bool bAirborne)
{
    FallElapsed=0;
    if(bAirborne) { Cancel(); JumpElapsed=0; bJumpStarted=false; }
    else { bJumpStarted=false; LandingElapsed=0; }
}
void UCourierActionComponent::JumpStarted()
{ bJumpStarted=true; JumpElapsed=0; }
bool UCourierActionComponent::IsGroundActionBlocked() const
{ return Timeline.GetAction()!=ECourierAction::None || LandingElapsed<.12; }
void UCourierActionComponent::TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta,TickType,Function);
    if(!FMath::IsFinite(Delta) || Delta<=0 || bPreserve || (GetWorld() && GetWorld()->IsPaused())) return;
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); if(!IsValid(Rider) || Rider->IsActorBeingDestroyed()) return;
    auto* Movement=Rider->GetCharacterMovement();
    if(Movement->IsFalling()) JumpElapsed+=Delta; else LandingElapsed+=Delta;
    if(Movement->IsFalling() && !IsJumpStart() && Movement->Velocity.Z<-80) FallElapsed+=Delta;
    else FallElapsed=0;
    if(PendingEdges)
    {
        FText Reason;
        if(!bSuppressed && Rider->CanPerformGroundAction(Reason,true)) Timeline.RequestAttack((PendingEdges&2)!=0);
        PendingEdges=0;
    }
    if(Timeline.IsAttacking())
    {
        auto* Ptr=ActionAnimations.Find(Timeline.GetAction());
        if(!Ptr || !FCourierActionAnimation::Compatible(Rider->GetMesh(),Ptr->Get()) || !IsPlaybackReady()) Cancel();
    }
    // Start or validate playback before emitting any authoritative contact slices.
    if(Timeline.IsAttacking() || Timeline.IsReacting()) SyncAnimation(0);
    const uint64 Generation=InterruptionGeneration;
    auto CaptureFacing=[&](FGuid Id)
    {
        if(Id!=FacingInstance) { FacingInstance=Id; AttackFacing=FRotator(0,Rider->GetControlRotation().Yaw,0); }
    };
    if(Timeline.IsAttacking()) CaptureFacing(Timeline.GetInstanceId());
    const auto Slices=Timeline.Advance(Delta);
    for(const auto& Slice:Slices)
    {
        if(!CanProcessStrike(Generation)) break;
        CaptureFacing(Slice.InstanceId); Rider->SetActorRotation(AttackFacing); TraceSlice(Slice,Generation);
    }
    if(!IsValid(Rider) || Rider->IsActorBeingDestroyed()) return;
    if(Timeline.IsAttacking())
    {
        CaptureFacing(Timeline.GetInstanceId());
        Rider->SetActorRotation(FMath::RInterpConstantTo(Rider->GetActorRotation(),AttackFacing,Delta,1440));
    }
    for(auto It=Struck.CreateIterator();It;++It) if(It.Key()!=Timeline.GetInstanceId() || !Timeline.IsAttacking()) It.RemoveCurrent();
    Rider->UpdateActionMovement();
    SyncAnimation(Delta);
}
void UCourierActionComponent::EndPlay(EEndPlayReason::Type Reason)
{ Cancel(); Super::EndPlay(Reason); }

bool UCourierActionComponent::ReceiveStrike(FGuid Id,const FVector& Origin)
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner());
    if(!Id.IsValid() || Received.Contains(Id) || Origin.ContainsNaN() || !Rider || bSuppressed || !Rider->GetCharacterMovement()->IsMovingOnGround()) return false;
    Received.Add(Id);
    ++InterruptionGeneration;
    const bool bFront=FVector::DotProduct((Origin-Rider->GetActorLocation()).GetSafeNormal2D(),Rider->GetActorForwardVector())>=0;
    const auto Action=bFront?ECourierAction::HitFront:ECourierAction::HitBack;
    PendingEdges=0; Timeline.ClearQueuedInput();
    auto* Ptr=ActionAnimations.Find(Action);
    if(Ptr && FCourierActionAnimation::Compatible(Rider->GetMesh(),Ptr->Get(),.4)) Timeline.ReceiveHit(bFront);
    else Cancel();
    Rider->UpdateActionMovement(); return true;
}
bool UCourierActionComponent::CanProcessStrike(uint64 Generation) const
{
    const auto* Rider=Cast<ACourierCharacter>(GetOwner());
    return Generation==InterruptionGeneration && !bSuppressed && IsValid(Rider) && !Rider->IsActorBeingDestroyed() && !Rider->IsMounted();
}
void UCourierActionComponent::TraceSlice(const FActionSlice& Slice,uint64 Generation)
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); if(!Rider) return;
    auto* Ptr=ActionAnimations.Find(Slice.Action); if(!Ptr) return;
    const double Reach=Slice.Action==ECourierAction::Heavy?160:130;
    const bool bLeft=Slice.Action==ECourierAction::LightL;
    const int32 Steps=FMath::Max(1,FMath::CeilToInt((Slice.End-Slice.Start)*120));
    FVector Previous; if(!FCourierActionAnimation::SampleFist(Rider->GetMesh(),Ptr->Get(),bLeft,Slice.Start,Previous)) return;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CourierFist),false,Rider);
    for(int32 I=1;I<=Steps;++I)
    {
        if(!CanProcessStrike(Generation)) return;
        FVector Next;
        if(!FCourierActionAnimation::SampleFist(Rider->GetMesh(),Ptr->Get(),bLeft,FMath::Lerp(Slice.Start,Slice.End,double(I)/Steps),Next)) return;
        if(FVector::DistSquared2D(Rider->GetActorLocation(),Previous)<=Reach*Reach && FVector::DistSquared2D(Rider->GetActorLocation(),Next)<=Reach*Reach)
        {
            TArray<FHitResult> Hits;
            GetWorld()->SweepMultiByChannel(Hits,Previous,Next,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(12),Params);
            Hits.Sort([](const FHitResult& A,const FHitResult& B)
            {
                // Make the observed blocker-first behavior explicit for tied initial overlaps.
                return A.Time==B.Time ? A.bBlockingHit && !B.bBlockingHit : A.Time<B.Time;
            });
            for(const auto& Hit:Hits)
            {
                if(!CanProcessStrike(Generation)) return;
                auto* Target=Hit.GetActor(); auto* Receiver=Cast<ICourierStrikeReceiver>(Target);
                if(Receiver && !Struck.FindOrAdd(Slice.InstanceId).Contains(Target))
                {
                    FHitResult Obstacle;
                    // Initial overlaps can have an unset ImpactPoint. Use the actual swept fist center.
                    const FVector Contact=FMath::Lerp(Previous,Next,double(Hit.Time));
                    const FVector Center(Rider->GetActorLocation().X,Rider->GetActorLocation().Y,Contact.Z);
                    const bool bBlocked=GetWorld()->LineTraceSingleByChannel(Obstacle,Center,Contact,ECC_Visibility,Params);
                    if(!bBlocked || Obstacle.GetActor()==Target)
                    {
                        Struck.FindOrAdd(Slice.InstanceId).Add(Target);
                        const bool bReceived=Receiver->ReceiveCourierStrike(Slice.InstanceId,Rider->GetActorLocation(),Slice.Action);
                        if(!CanProcessStrike(Generation)) return;
                        if(bReceived) OnStrike.Broadcast(Target,Slice.InstanceId,Slice.Action);
                        if(!CanProcessStrike(Generation)) return;
                    }
                }
                if(Hit.bBlockingHit) break;
            }
        }
        Previous=Next;
    }
}
void UCourierActionComponent::SyncAnimation(float Delta)
{
    auto* Rider=Cast<ACourierCharacter>(GetOwner()); auto* Mesh=Rider?Rider->GetMesh():nullptr;
    auto* Anim=Mesh?Mesh->GetAnimInstance():nullptr;
    if(!Anim) { if(Timeline.GetAction()!=ECourierAction::None) Cancel(); return; }
    UAnimSequence* Desired=nullptr; FName Movement; float Time=0; bool bLoop=false;
    const auto Action=Timeline.GetAction();
    if(Action!=ECourierAction::None)
    { if(auto* P=ActionAnimations.Find(Action)) Desired=P->Get(); Time=Timeline.GetElapsed(); }
    else if(!Rider->IsMounted())
    {
        if(Rider->GetCharacterMovement()->IsFalling())
        {
            const bool bDescending=FallElapsed>0;
            Movement=IsJumpStart()?TEXT("A_JumpStart"):bDescending?TEXT("A_JumpFall"):TEXT("A_JumpLoop");
            Time=IsJumpStart()?JumpElapsed:bDescending?FallElapsed:FMath::Max(0.f,JumpElapsed-.18f);
            bLoop=Movement==TEXT("A_JumpLoop");
        }
        else if(LandingElapsed<.25f && !Rider->IsProne() && !Rider->IsCrouched()) { Movement=TEXT("A_Land"); Time=LandingElapsed; }
        else if(!Rider->IsProne() && !Rider->IsCrouched())
        {
            const float Speed=Rider->GetVelocity().Size2D(); const bool bMoving=!Rider->GetLastMovementInputVector().IsNearlyZero() || !Rider->GetPendingMovementInputVector().IsNearlyZero();
            if(bMoving && !bWasMoving && PreviousSpeed<10 && Rider->GetCharacterMovement()->MaxWalkSpeed>500) { RunTransition=TEXT("A_RunStart"); RunTransitionElapsed=0; }
            else if(!bMoving && bWasMoving && PreviousSpeed>500) { RunTransition=TEXT("A_RunStop"); RunTransitionElapsed=0; }
            if(bMoving && RunTransition==TEXT("A_RunStop")) RunTransition=NAME_None;
            if(!RunTransition.IsNone())
            {
                RunTransitionElapsed+=Delta; const float Length=RunTransition==TEXT("A_RunStart")?.2f:.25f;
                if(RunTransitionElapsed<Length) { Movement=RunTransition; Time=RunTransitionElapsed; } else RunTransition=NAME_None;
            }
            bWasMoving=bMoving; PreviousSpeed=Speed;
        }
        if(auto* P=MovementAnimations.Find(Movement)) Desired=P->Get();
    }
    if(!FCourierActionAnimation::Compatible(Mesh,Desired))
    {
        TGuardValue<bool> Changing(bChangingMontage,true);
        if(PlayingMontage) Anim->Montage_Stop(PlayingMovement.IsNone()?.05f:.12f,PlayingMontage);
        if(Action!=ECourierAction::None) Cancel();
        PlayingMontage=nullptr; PlayingSequence=nullptr; PlayingMovement=NAME_None; PlayingInstance.Invalidate(); return;
    }
    const FGuid Instance=Action==ECourierAction::None?FGuid():Timeline.GetInstanceId();
    if(Desired!=PlayingSequence || Instance!=PlayingInstance || Movement!=PlayingMovement)
    {
        TGuardValue<bool> Changing(bChangingMontage,true);
        bPlaybackFinishedNaturally=false;
        const float BlendIn=Movement.IsNone()?.03f:Movement==TEXT("A_RunStop")?.15f:.10f;
        const float BlendOutTime=Movement.IsNone()?.06f:.12f;
        PlayingMontage=Anim->PlaySlotAnimationAsDynamicMontage(Desired,TEXT("FullBodyAction"),BlendIn,BlendOutTime,1,bLoop?100000:1,-1,bLoop?FMath::Fmod(Time,Desired->GetPlayLength()):FMath::Min(Time,Desired->GetPlayLength()));
        if(!PlayingMontage) { Cancel(); Feedback(FText::FromString(TEXT("动作播放失败，请检查动画资源"))); return; }
        // Hold the short takeoff until the flight state replaces it. Automatic
        // blend-out otherwise starts before takeoff has even finished blending
        // in, briefly exposing the running pose while the character is airborne.
        if(Movement==TEXT("A_JumpStart") || Movement==TEXT("A_JumpFall"))
        {
            PlayingMontage->bEnableAutoBlendOut=false;
            if(auto* Playback=Anim->GetActiveInstanceForMontage(PlayingMontage)) Playback->bEnableAutoBlendOut=false;
        }
        FOnMontageBlendingOutStarted BlendOut;
        BlendOut.BindUObject(this,&UCourierActionComponent::MontageBlendingOut);
        Anim->Montage_SetBlendingOutDelegate(BlendOut,PlayingMontage);
        PlayingSequence=Desired; PlayingInstance=Instance; PlayingMovement=Movement;
    }
    else if(Action!=ECourierAction::None && (!PlayingMontage || !Anim->Montage_IsActive(PlayingMontage) || !Anim->Montage_IsPlaying(PlayingMontage)) && !bPlaybackFinishedNaturally)
    { Cancel(); Feedback(FText::FromString(TEXT("动作已中断"))); return; }
    else if(PlayingMontage && !bLoop) Anim->Montage_SetPosition(PlayingMontage,FMath::Min(Time,Desired->GetPlayLength()));
}
void UCourierActionComponent::MontageBlendingOut(UAnimMontage* Montage,bool bInterrupted)
{
    if(bChangingMontage || Montage!=PlayingMontage) return;
    if(bInterrupted) Cancel();
    else bPlaybackFinishedNaturally=true;
}
