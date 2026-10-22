#include "Player/CourierActionAnimation.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Misc/PackageName.h"

FName FCourierActionAnimation::SequenceName(ECourierAction Action)
{
    switch(Action)
    {
    case ECourierAction::LightL:return TEXT("A_LightPunch_L");
    case ECourierAction::LightR:return TEXT("A_LightPunch_R");
    case ECourierAction::Heavy:return TEXT("A_HeavyPunch_R");
    case ECourierAction::HitFront:return TEXT("A_HitReact_Front");
    case ECourierAction::HitBack:return TEXT("A_HitReact_Back");
    case ECourierAction::Knockdown:
    case ECourierAction::GroundHold:return TEXT("A_Knockdown");
    case ECourierAction::GetUp:return TEXT("A_GetUp");
    default:return NAME_None;
    }
}
UAnimSequence* FCourierActionAnimation::Load(FName Name)
{
    if(Name.IsNone()) return nullptr;
    FString Library=TEXT("/Game/CityCourier/Animation/Library/")+Name.ToString();
    if(Name==TEXT("A_RunStart")) Library=TEXT("/Game/CityCourier/Animation/Epic/Clips/CC_M_Neutral_Sprint_Start_F_Lfoot");
    if(Name==TEXT("A_RunStop")) Library=TEXT("/Game/CityCourier/Animation/Epic/Clips/CC_M_Neutral_Sprint_Stop_F_Lfoot");
    if(FPackageName::DoesPackageExist(Library))
        return LoadObject<UAnimSequence>(nullptr,*(Library+TEXT(".")+FPackageName::GetShortName(Library)));
    // Missing replacements must not silently revive an original authored clip.
    for(const TCHAR* Required:{TEXT("A_IdleRelaxed"),TEXT("A_Ride"),TEXT("A_JumpStart"),TEXT("A_JumpLoop"),TEXT("A_JumpFall"),TEXT("A_Land"),
        TEXT("A_HitReact_Front"),TEXT("A_HitReact_Back"),TEXT("A_Knockdown"),TEXT("A_GetUp"),TEXT("A_RunStart"),TEXT("A_RunStop")})
        if(Name==FName(Required)) return nullptr;
    const FString Package=TEXT("/Game/CityCourier/Art/Characters/")+Name.ToString();
    return FPackageName::DoesPackageExist(Package)?LoadObject<UAnimSequence>(nullptr,*(Package+TEXT(".")+Name.ToString())):nullptr;
}
bool FCourierActionAnimation::Compatible(const USkeletalMeshComponent* Mesh,const UAnimSequence* Sequence,double MinimumDuration)
{
    return Mesh && Mesh->GetSkeletalMeshAsset() && Sequence && Sequence->GetSkeleton() &&
        Sequence->GetSkeleton()==Mesh->GetSkeletalMeshAsset()->GetSkeleton() && !Sequence->bEnableRootMotion &&
        Sequence->GetPlayLength()+.002>=MinimumDuration;
}
bool FCourierActionAnimation::SampleFist(const USkeletalMeshComponent* Mesh,const UAnimSequence* Sequence,bool bLeft,double Time,FVector& Out,bool bUseLocomotionPelvis)
{
    if(!Compatible(Mesh,Sequence) || !FMath::IsFinite(Time)) return false;
    const FName SocketName=bLeft?TEXT("PunchTrace_L"):TEXT("PunchTrace_R");
    const auto* Socket=Mesh->GetSocketByName(SocketName);
    const FName Bone=Socket?Socket->BoneName:bLeft?TEXT("hand_l"):TEXT("hand_r");
    const auto& Ref=Sequence->GetSkeleton()->GetReferenceSkeleton();
    int32 Index=Ref.FindBoneIndex(Bone); if(Index==INDEX_NONE) return false;
    FTransform Chain=Socket?Socket->GetSocketLocalTransform():FTransform::Identity;
    const FAnimExtractContext Context(FMath::Clamp(Time,0.,double(Sequence->GetPlayLength())));
    while(Index!=INDEX_NONE)
    {
        if(bUseLocomotionPelvis && Ref.GetBoneName(Index)==TEXT("pelvis"))
        {
            Out=(Chain*Mesh->GetSocketTransform(TEXT("pelvis"))).GetLocation();
            return !Out.ContainsNaN();
        }
        FTransform Local; Sequence->GetBoneTransform(Local,FSkeletonPoseBoneIndex(Index),Context,false);
        Chain=Chain*Local; Index=Ref.GetParentIndex(Index);
    }
    if(!Socket) Chain.AddToTranslation(Chain.GetRotation().GetAxisX()*5.f);
    Out=(Chain*Mesh->GetComponentTransform()).GetLocation();
    return !Out.ContainsNaN();
}
