#include "Art/CourierAnimInstance.h"
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
    virtual void PreUpdate(UAnimInstance* Instance,float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Delta);
        auto* Anim=CastChecked<UCourierAnimInstance>(Instance);
        const auto* Rider=Cast<ACourierCharacter>(Anim->GetOwningActor());
        Anim->GroundSpeed=Rider?Rider->GetVelocity().Size2D():0;
        Anim->bRiding=Rider && Rider->IsMounted();
        for(int32 I=0;I<4;++I) Clips[I]=Anim->Clips.IsValidIndex(I)?Anim->Clips[I].Get():nullptr;
        const int32 Next=UCourierAnimInstance::SelectClip(Anim->GroundSpeed,Anim->bRiding);
        if(Next!=Active) { Previous=Active; PreviousClock=Clock; Active=Next; Clock=0; Blend=0; }
        Rate=Active==1?FMath::Clamp(Anim->GroundSpeed/420.f,.5f,1.5f):Active==2?FMath::Clamp(Anim->GroundSpeed/700.f,.5f,1.5f):1;
    }
    virtual void Update(float Delta) override
    {
        Clock+=Delta*Rate; PreviousClock+=Delta; Blend=FMath::Min(1.f,Blend+Delta/.18f);
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
        FPoseContext Current(Output),Old(Output);
        Sample(Clips[Active],Clock,Current); Sample(Clips[Previous],PreviousClock,Old);
        FAnimationPoseData A(Current),B(Old),Result(Output);
        FAnimationRuntime::BlendTwoPosesTogether(A,B,Blend,Result);
        return true;
    }
};
FAnimInstanceProxy* UCourierAnimInstance::CreateAnimInstanceProxy() { return new FCourierAnimProxy(this); }
void UCourierAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
