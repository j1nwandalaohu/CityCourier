#include "Player/CourierAnimInstance.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierActionComponent.h"
#include "Player/CourierActionAnimation.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

/** Samples authored clips with a short crossfade; UObject state is read only in PreUpdate. */
struct FCourierAnimProxy final : FAnimInstanceProxy
{
    using FAnimInstanceProxy::FAnimInstanceProxy;
    UAnimSequence* Clips[8]={};
    int32 Active=0,Previous=0;
    float Clock=0,PreviousClock=0,Blend=1,Rate=1,PreviousRate=1;
    float MovementInfluence=1;
    float SlotWeight=0,SourceWeight=1,TotalWeight=0;
    virtual void PreUpdate(UAnimInstance* Instance,float Delta) override
    {
        FAnimInstanceProxy::PreUpdate(Instance,Delta);
        auto* Anim=CastChecked<UCourierAnimInstance>(Instance);
        const auto* Rider=Cast<ACourierCharacter>(Anim->GetOwningActor());
        Anim->NativeUpdateAnimation(Delta);
        for(int32 I=0;I<4;++I) Clips[I]=Anim->Clips.IsValidIndex(I)?Anim->Clips[I].Get():nullptr;
        for(int32 I=0;I<4;++I) Clips[I+4]=Anim->PostureClips.IsValidIndex(I)?Anim->PostureClips[I].Get():nullptr;
        const int32 Next=Anim->AnimationIndex;
        if(Next!=Active)
        {
            Previous=Active; PreviousClock=Clock; PreviousRate=Rate;
            // Jog and sprint share left/right foot phases. Keep the same step
            // when accelerating instead of snapping to the first sprint frame.
            const bool bGait=(Active==1 || Active==2) && (Next==1 || Next==2);
            Clock=bGait && Clips[Active] && Clips[Next] && Clips[Active]->GetPlayLength()>0
                ? FMath::Fmod(Clock,Clips[Active]->GetPlayLength())/Clips[Active]->GetPlayLength()*Clips[Next]->GetPlayLength() : 0;
            Active=Next; Blend=0;
        }
        Rate=Active==1?FMath::Clamp(Anim->GroundSpeed/473.2f,.5f,1.5f):Active==2?FMath::Clamp(Anim->GroundSpeed/715.9f,.5f,1.5f)
            :Active==5?FMath::Clamp(Anim->GroundSpeed/100.f,.3f,1.3f):Active==7?FMath::Clamp(Anim->GroundSpeed/60.f,.3f,1.3f):1;
        // The short land clip has planted feet. At running speed retain the
        // stride underneath its compression; combat still uses the full slot.
        const bool bLanding=Rider && !Anim->bAirborne && Anim->CurrentAction==ECourierAction::None
            && Rider->GetActions()->GetLandingElapsed()<.37f;
        const float LandingEnvelope=bLanding?FMath::Clamp((.37f-Rider->GetActions()->GetLandingElapsed())/.12f,0.f,1.f):0.f;
        MovementInfluence=FMath::Lerp(1.f,.3f,FMath::Clamp(Anim->GroundSpeed/420.f,0.f,1.f)*LandingEnvelope);
    }
    virtual void Update(float Delta) override
    {
        Clock+=Delta*Rate; PreviousClock+=Delta*PreviousRate; Blend=FMath::Min(1.f,Blend+Delta/.18f);
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
        {
            FPoseContext ActionPose(Output);
            FAnimationPoseData ActionData(ActionPose);
            // Slot evaluation reads montage instance weights internally. Blend
            // its normalized result, rather than changing only its source weight.
            SlotEvaluatePose(TEXT("FullBodyAction"),Source,SourceWeight,ActionData,SlotWeight,TotalWeight);
            FAnimationRuntime::BlendTwoPosesTogether(ActionData,Source,MovementInfluence,Result);
        }
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
void UCourierAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();
    PostureClips.Reset();
    for(FName Name:{FName(TEXT("A_CrouchIdle")),FName(TEXT("A_CrouchWalk")),FName(TEXT("A_ProneIdle")),FName(TEXT("A_ProneCrawl"))})
        PostureClips.Add(FCourierActionAnimation::Load(Name));
}
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
    if(!bRiding && !bAirborne && PostureClips.Num()==4)
    {
        const int32 Posture=Rider->IsProne()?(GroundSpeed>5?7:6):Rider->IsCrouched()?(GroundSpeed>5?5:4):-1;
        if(Posture>=4 && PostureClips[Posture-4]) AnimationIndex=Posture;
    }
}
