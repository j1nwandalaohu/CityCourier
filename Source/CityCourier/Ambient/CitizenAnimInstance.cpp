#include "Ambient/CitizenAnimInstance.h"
#include "Ambient/AmbientPedestrian.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "Player/CourierActionAnimation.h"

struct FCitizenAnimProxy final : FAnimInstanceProxy
{
    using FAnimInstanceProxy::FAnimInstanceProxy;
    UAnimSequence* Clips[int32(ECitizenClip::Count)]={};
    FGuid AttackInstance;
    int32 Active=0,Previous=0;
    float Clock=0,PreviousClock=0,Rate=1,PreviousRate=1,Blend=1;
    virtual void PreUpdate(UAnimInstance* Instance,float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Delta);
        auto* Anim=CastChecked<UCitizenAnimInstance>(Instance);
        Anim->NativeUpdateAnimation(Delta);
        for(int32 I=0;I<int32(ECitizenClip::Count);++I)Clips[I]=Anim->Clips.IsValidIndex(I)?Anim->Clips[I].Get():nullptr;
        if(Active!=Anim->AnimationIndex || AttackInstance!=Anim->AttackInstance)
        {
            Previous=Active;PreviousClock=Clock;PreviousRate=Rate;
            Active=Anim->AnimationIndex;Clock=0;Blend=0;
            AttackInstance=Anim->AttackInstance;
        }
        Rate=Active==1 || Active==int32(ECitizenClip::Run)?Anim->WalkRate:UCitizenAnimInstance::IsLooping(Active)?1:0;
        if(!UCitizenAnimInstance::IsLooping(Active))Clock=Anim->SampleTime;
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
        Sample(Clips[Active],Clock,UCitizenAnimInstance::IsLooping(Active),Current);
        Sample(Clips[Previous],PreviousClock,UCitizenAnimInstance::IsLooping(Previous),Old);
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
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Reactions/A_CitizenGetUp"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Behavior/A_CitizenGuard"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Behavior/A_CitizenPunchL"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Behavior/A_CitizenPunchR"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Behavior/A_CitizenRun"),
        TEXT("/Game/CityCourier/Art/Ambient/CharMorph/Behavior/A_CitizenCower")})
        Clips.Add(LoadObject<UAnimSequence>(nullptr,Path));
}
void UCitizenAnimInstance::NativeUpdateAnimation(float Delta)
{
    Super::NativeUpdateAnimation(Delta);
    const auto* Walker=Cast<AAmbientPedestrian>(GetOwningActor());if(!Walker)return;
    WalkRate=Walker->GetSpeedCm()/160.f;
    AttackInstance.Invalidate();
    switch(Walker->GetReaction())
    {
        case ECitizenReaction::Walking:AnimationIndex=WalkRate>.02f?1:0;break;
        case ECitizenReaction::Staggering:AnimationIndex=2;break;
        case ECitizenReaction::Falling:case ECitizenReaction::Down:AnimationIndex=3;break;
        case ECitizenReaction::GettingUp:AnimationIndex=4;break;
    }
    SampleTime=Walker->GetReaction()==ECitizenReaction::Down?MAX_flt:Walker->GetReactionElapsed();
    if(!Walker->IsBodyAvailable())return;
    const auto& Combat=Walker->GetCombat();
    if(Combat.bActive)
    {
        AnimationIndex=int32(Combat.bLeft?ECitizenClip::PunchL:ECitizenClip::PunchR);
        AttackInstance=Combat.InstanceId;SampleTime=Combat.Elapsed;return;
    }
    const auto State=Walker->GetBehavior().State;
    if(State==ECitizenBehaviorState::Cower)AnimationIndex=int32(ECitizenClip::Cower);
    else if(Walker->GetSpeedCm()>180)
    {AnimationIndex=int32(ECitizenClip::Run);WalkRate=Walker->GetSpeedCm()/380.f;}
    else if(WalkRate>.02f)AnimationIndex=int32(ECitizenClip::Walk);
    else if(State==ECitizenBehaviorState::Fight || State==ECitizenBehaviorState::Confront || State==ECitizenBehaviorState::AssessThreat)
        AnimationIndex=int32(ECitizenClip::Guard);
    if(!Clips.IsValidIndex(AnimationIndex) || !Clips[AnimationIndex])AnimationIndex=0;
}
bool UCitizenAnimInstance::IsLooping(int32 I)
{return I==0 || I==1 || I==int32(ECitizenClip::Guard) || I==int32(ECitizenClip::Run) || I==int32(ECitizenClip::Cower);}
UAnimSequence* UCitizenAnimInstance::GetPunchClip(bool bLeft) const
{const int32 I=int32(bLeft?ECitizenClip::PunchL:ECitizenClip::PunchR);return Clips.IsValidIndex(I)?Clips[I].Get():nullptr;}
bool UCitizenAnimInstance::HasCombatClips() const
{
    const auto* P=Cast<AAmbientPedestrian>(GetOwningActor());
    return P && FCourierActionAnimation::Compatible(P->Person,GetPunchClip(true),.8)
        && FCourierActionAnimation::Compatible(P->Person,GetPunchClip(false),.8)
        && Clips.IsValidIndex(int32(ECitizenClip::Guard))
        && FCourierActionAnimation::Compatible(P->Person,Clips[int32(ECitizenClip::Guard)],1);
}
