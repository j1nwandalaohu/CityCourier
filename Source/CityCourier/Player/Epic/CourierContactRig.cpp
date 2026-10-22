#include "Player/Epic/CourierContactRig.h"
#include "Rigs/RigHierarchy.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "UObject/UnrealType.h"
#include "TwoBoneIK.h"

bool UCourierContactRig::Execute_Internal(const FName& EventName)
{
    const bool bResult=Super::Execute_Internal(EventName);
    if(EventName!=FRigUnit_BeginExecution::EventName || ReadWeight(TEXT("ContactEnabled"))<=0) return bResult;
    auto* Hierarchy=GetHierarchy();
    const FRigElementKey Pelvis(TEXT("pelvis"),ERigElementType::Bone);
    if(Hierarchy && Hierarchy->GetIndex(Pelvis)!=INDEX_NONE)
    {
        auto Transform=Hierarchy->GetGlobalTransform(Pelvis);
        Transform.AddToTranslation(FVector(0,0,FMath::Clamp(ReadWeight(TEXT("ContactPelvisOffset")),-25.f,0.f)));
        Hierarchy->SetGlobalTransform(Pelvis,Transform);
    }
    SolveChain(TEXT("thigh_l"),TEXT("shin_l"),TEXT("foot_l"),ReadGoal(TEXT("LeftFootGoal")),ReadWeight(TEXT("LeftFootAlpha")),FVector(1,0,0));
    SolveChain(TEXT("thigh_r"),TEXT("shin_r"),TEXT("foot_r"),ReadGoal(TEXT("RightFootGoal")),ReadWeight(TEXT("RightFootAlpha")),FVector(1,0,0));
    SolveChain(TEXT("upperarm_l"),TEXT("forearm_l"),TEXT("hand_l"),ReadGoal(TEXT("LeftHandGoal")),ReadWeight(TEXT("LeftHandAlpha")),FVector(0,-1,0));
    SolveChain(TEXT("upperarm_r"),TEXT("forearm_r"),TEXT("hand_r"),ReadGoal(TEXT("RightHandGoal")),ReadWeight(TEXT("RightHandAlpha")),FVector(0,1,0));
    return bResult;
}
FTransform UCourierContactRig::ReadGoal(FName Name) const
{
    if(const auto* Property=FindFProperty<FStructProperty>(GetClass(),Name))
        if(Property->Struct==TBaseStructure<FTransform>::Get()) return *Property->ContainerPtrToValuePtr<FTransform>(this);
    return FTransform::Identity;
}
float UCourierContactRig::ReadWeight(FName Name) const
{
    if(const auto* Property=FindFProperty<FFloatProperty>(GetClass(),Name)) return Property->GetPropertyValue_InContainer(this);
    return 0;
}
void UCourierContactRig::SolveChain(FName Root,FName Joint,FName End,const FTransform& Goal,float Weight,FVector FallbackPole)
{
    auto* Hierarchy=GetHierarchy();
    const FRigElementKey A(Root,ERigElementType::Bone),B(Joint,ERigElementType::Bone),C(End,ERigElementType::Bone);
    if(!Hierarchy || Weight<=KINDA_SMALL_NUMBER || Goal.ContainsNaN() || Hierarchy->GetIndex(A)==INDEX_NONE || Hierarchy->GetIndex(B)==INDEX_NONE || Hierarchy->GetIndex(C)==INDEX_NONE) return;
    FTransform Upper=Hierarchy->GetGlobalTransform(A),Lower=Hierarchy->GetGlobalTransform(B),Tip=Hierarchy->GetGlobalTransform(C);
    const bool bFoot=End==TEXT("foot_l") || End==TEXT("foot_r");
    if(bFoot)
    {
        // Use the incoming animation pose, not last frame's corrected foot, so
        // the contact solver cannot feed back and pin a swinging foot down.
        const float Height=FMath::Max(0.,Tip.GetLocation().Z-Goal.GetLocation().Z);
        Weight*=1.f-FMath::SmoothStep(5.f,25.f,Height);
        if(Weight<=KINDA_SMALL_NUMBER) return;
    }
    const FQuat IncomingRotation=Tip.GetRotation();
    const double Length=FVector::Dist(Upper.GetLocation(),Lower.GetLocation())+FVector::Dist(Lower.GetLocation(),Tip.GetLocation());
    // Surface traces came from the previous evaluated pose. Preserve the new
    // animation's horizontal stride; ground IK adjusts height and surface tilt.
    const FVector Contact=bFoot?FVector(Tip.GetLocation().X,Tip.GetLocation().Y,Goal.GetLocation().Z):Goal.GetLocation();
    const FVector Effector=Upper.GetLocation()+(FMath::Lerp(Tip.GetLocation(),Contact,FMath::Clamp(Weight,0.f,1.f))-Upper.GetLocation()).GetClampedToMaxSize(Length*.999);
    const FVector Axis=(Effector-Upper.GetLocation()).GetSafeNormal();
    FVector Pole=FVector::VectorPlaneProject(Lower.GetLocation()-Upper.GetLocation(),Axis).GetSafeNormal();
    if(Pole.IsNearlyZero()) Pole=FVector::VectorPlaneProject(FallbackPole,Axis).GetSafeNormal();
    AnimationCore::SolveTwoBoneIK(Upper,Lower,Tip,Upper.GetLocation()+Pole*Length,Effector,false,1.,1.);
    const FQuat Rotation=bFoot?Goal.GetRotation()*IncomingRotation:Goal.GetRotation();
    Tip.SetRotation(FQuat::Slerp(IncomingRotation,Rotation,FMath::Clamp(Weight,0.f,1.f)).GetNormalized());
    Hierarchy->SetGlobalTransform(A,Upper); Hierarchy->SetGlobalTransform(B,Lower); Hierarchy->SetGlobalTransform(C,Tip);
}
