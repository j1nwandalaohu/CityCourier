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
    default:return NAME_None;
    }
}
UAnimSequence* FCourierActionAnimation::Load(FName Name)
{
    if(Name.IsNone()) return nullptr;
    const FString Package=TEXT("/Game/CityCourier/Art/Characters/")+Name.ToString();
    return FPackageName::DoesPackageExist(Package)?LoadObject<UAnimSequence>(nullptr,*(Package+TEXT(".")+Name.ToString())):nullptr;
}
bool FCourierActionAnimation::Compatible(const USkeletalMeshComponent* Mesh,const UAnimSequence* Sequence,double MinimumDuration)
{
    return Mesh && Mesh->GetSkeletalMeshAsset() && Sequence && Sequence->GetSkeleton() &&
        Sequence->GetSkeleton()==Mesh->GetSkeletalMeshAsset()->GetSkeleton() && !Sequence->bEnableRootMotion &&
        Sequence->GetPlayLength()+.002>=MinimumDuration;
}
bool FCourierActionAnimation::SampleFist(const USkeletalMeshComponent* Mesh,const UAnimSequence* Sequence,bool bLeft,double Time,FVector& Out)
{
    if(!Compatible(Mesh,Sequence) || !FMath::IsFinite(Time)) return false;
    const FName SocketName=bLeft?TEXT("PunchTrace_L"):TEXT("PunchTrace_R");
    const auto* Socket=Mesh->GetSocketByName(SocketName);
    const FName Bone=Socket?Socket->BoneName:bLeft?TEXT("hand_l"):TEXT("hand_r");
    const auto& Ref=Sequence->GetSkeleton()->GetReferenceSkeleton();
    int32 Index=Ref.FindBoneIndex(Bone); if(Index==INDEX_NONE) return false;
    FTransform Chain=Socket?Socket->GetSocketLocalTransform():FTransform(FVector(5,0,0));
    const FAnimExtractContext Context(FMath::Clamp(Time,0.,double(Sequence->GetPlayLength())));
    while(Index!=INDEX_NONE)
    {
        FTransform Local; Sequence->GetBoneTransform(Local,FSkeletonPoseBoneIndex(Index),Context,false);
        Chain=Chain*Local; Index=Ref.GetParentIndex(Index);
    }
    Out=(Chain*Mesh->GetComponentTransform()).GetLocation();
    return !Out.ContainsNaN();
}
