#include "Ambient/CitizenAnimInstance.h"
#include "Ambient/AmbientPedestrian.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

struct FCitizenAnimProxy final : FAnimInstanceProxy
{
    using FAnimInstanceProxy::FAnimInstanceProxy;
    UAnimSequence* Clips[5]={};
    int32 Active=0,Previous=0;
    float Clock=0,PreviousClock=0,Rate=1,PreviousRate=1,Blend=1;
    virtual void PreUpdate(UAnimInstance* Instance,float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Delta);
        auto* Anim=CastChecked<UCitizenAnimInstance>(Instance);
        Anim->NativeUpdateAnimation(Delta);
        for(int32 I=0;I<5;++I)Clips[I]=Anim->Clips.IsValidIndex(I)?Anim->Clips[I].Get():nullptr;
        if(Active!=Anim->AnimationIndex)
        {
            Previous=Active;PreviousClock=Clock;PreviousRate=Rate;
            Active=Anim->AnimationIndex;Clock=0;Blend=0;
        }
        Rate=Active==1?Anim->WalkRate:Active==0?1:0;
        if(Active>=2)Clock=Anim->SampleTime;
    }
    virtual void Update(float Delta) override
    {
        Clock+=Delta*Rate;
        PreviousClock+=Delta*PreviousRate;
        Blend=FMath::Min(1.f,Blend+Delta/.1f);
    }
    static void Sample(UAnimSequence* Clip,float Time,bool bLoop,FPoseContext& Pose)
    {
        Pose.ResetToRefPose();
        if(!Clip || Clip->GetPlayLength()<=0)return;
        const double T=bLoop?FMath::Fmod(Time,Clip->GetPlayLength()):FMath::Clamp(Time,0.f,Clip->GetPlayLength());
        FAnimationPoseData Data(Pose);
        Clip->GetAnimationPose(Data,FAnimExtractContext(T,false));
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        FPoseContext Current(Output),Old(Output);
        Sample(Clips[Active],Clock,Active<2,Current);
        Sample(Clips[Previous],PreviousClock,Previous<2,Old);
        FAnimationPoseData A(Current),B(Old),Result(Output);
        FAnimationRuntime::BlendTwoPosesTogether(A,B,Blend,Result);
        return true;
    }
};
FAnimInstanceProxy* UCitizenAnimInstance::CreateAnimInstanceProxy(){return new FCitizenAnimProxy(this);}
void UCitizenAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy){delete Proxy;}
void UCitizenAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    Clips.Empty();
    for(const TCHAR* Path:{
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Reactions/A_CitizenIdle"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Reactions/A_CitizenWalk"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Reactions/A_CitizenHit"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Reactions/A_CitizenFall"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Reactions/A_CitizenGetUp")})
        Clips.Add(LoadObject<UAnimSequence>(nullptr,Path));
}
void UCitizenAnimInstance::NativeUpdateAnimation(float Delta)
{
    Super::NativeUpdateAnimation(Delta);
    const auto* Walker=Cast<AAmbientPedestrian>(GetOwningActor());if(!Walker)return;
    WalkRate=Walker->GetSpeedCm()/160.f;
    switch(Walker->GetReaction())
    {
        case ECitizenReaction::Walking:AnimationIndex=WalkRate>.02f?1:0;break;
        case ECitizenReaction::Staggering:AnimationIndex=2;break;
        case ECitizenReaction::Falling:case ECitizenReaction::Down:AnimationIndex=3;break;
        case ECitizenReaction::GettingUp:AnimationIndex=4;break;
    }
    SampleTime=Walker->GetReaction()==ECitizenReaction::Down?MAX_flt:Walker->GetReactionElapsed();
}
