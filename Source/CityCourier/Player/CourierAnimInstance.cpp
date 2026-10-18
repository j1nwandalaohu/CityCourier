#include "Player/CourierAnimInstance.h"
#include "Player/CourierCharacter.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

/** Samples authored clips with a short crossfade; UObject state is read only in PreUpdate. */
struct FCourierAnimProxy final : FAnimInstanceProxy
{
    using FAnimInstanceProxy::FAnimInstanceProxy;
    UAnimSequence* Clips[4]={};
    int32 Active=0,Previous=0;
    float Clock=0,PreviousClock=0,Blend=1,Rate=1;
    float SlotWeight=0,SourceWeight=1,TotalWeight=0;
    virtual void PreUpdate(UAnimInstance* Instance,float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Delta);
        auto* Anim=CastChecked<UCourierAnimInstance>(Instance);
        const auto* Rider=Cast<ACourierCharacter>(Anim->GetOwningActor());
        Anim->NativeUpdateAnimation(Delta);
        for(int32 I=0;I<4;++I) Clips[I]=Anim->Clips.IsValidIndex(I)?Anim->Clips[I].Get():nullptr;
        const int32 Next=Anim->AnimationIndex;
        if(Next!=Active) { Previous=Active; PreviousClock=Clock; Active=Next; Clock=0; Blend=0; }
        Rate=Active==1?FMath::Clamp(Anim->GroundSpeed/420.f,.5f,1.5f):Active==2?FMath::Clamp(Anim->GroundSpeed/700.f,.5f,1.5f):1;
    }
    virtual void Update(float Delta) override
    {
        Clock+=Delta*Rate; PreviousClock+=Delta; Blend=FMath::Min(1.f,Blend+Delta/.18f);
        // The authored graph registers FullBodyAction. Keep its montage weights
        // in sync while the native proxy supplies the locomotion source pose.
        GetSlotWeight(TEXT("FullBodyAction"),SlotWeight,SourceWeight,TotalWeight);
        UpdateSlotNodeWeight(TEXT("FullBodyAction"),SlotWeight,1.f);
    }
    static void Sample(UAnimSequence* Clip,float Time,FPoseContext& Pose)
    {
        Pose.ResetToRefPose();
        if(Clip && Clip->GetPlayLength()>0)
        {
            FAnimationPoseData Data(Pose);
            Clip->GetAnimationPose(Data,FAnimExtractContext(static_cast<double>(FMath::Fmod(Time,Clip->GetPlayLength())),false));
        }
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        FPoseContext Current(Output),Old(Output),Locomotion(Output);
        Sample(Clips[Active],Clock,Current); Sample(Clips[Previous],PreviousClock,Old);
        FAnimationPoseData A(Current),B(Old),Source(Locomotion),Result(Output);
        FAnimationRuntime::BlendTwoPosesTogether(A,B,Blend,Source);
        // A custom Evaluate bypasses the generated AnimGraph, including its slot.
        // Evaluate that slot explicitly so visible poses agree with strike timing.
        if(SlotWeight>ZERO_ANIMWEIGHT_THRESH)
            SlotEvaluatePose(TEXT("FullBodyAction"),Source,SourceWeight,Result,SlotWeight,TotalWeight);
        else Output=Locomotion;
        return true;
    }
};
FAnimInstanceProxy* UCourierAnimInstance::CreateAnimInstanceProxy() { return new FCourierAnimProxy(this); }
void UCourierAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }

#include "Player/CourierAnimInstance.h"
#include "Player/CourierCharacter.h"
#include "Vehicles/ScooterPawn.h"
#include "Player/CourierActionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
void UCourierAnimInstance::NativeUpdateAnimation(float Delta)
{
    Super::NativeUpdateAnimation(Delta);
    auto* Rider=Cast<ACourierCharacter>(TryGetPawnOwner()); if(!Rider) return;
    bRiding=Rider->IsMounted(); GroundSpeed=Rider->GetVelocity().Size2D(); SteeringAmount=0;
    bAirborne=Rider->GetCharacterMovement()->IsFalling();
    CurrentAction=Rider->GetActions()->GetTimeline().GetAction(); ActionElapsed=Rider->GetActions()->GetTimeline().GetElapsed();
    if(auto* Bike=Cast<AScooterPawn>(Rider->GetAttachParentActor()))
    { GroundSpeed=Bike->GetSpeedKmh()/ .036f; SteeringAmount=FMath::Clamp(-Bike->VisualRoot->GetRelativeRotation().Roll/12.f,-1.f,1.f); }
    AnimationIndex=bRiding?3:GroundSpeed>500?2:GroundSpeed>10?1:0;
}
