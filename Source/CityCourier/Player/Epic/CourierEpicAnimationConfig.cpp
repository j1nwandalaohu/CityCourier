#include "Player/Epic/CourierEpicAnimationConfig.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimClassInterface.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Player/CourierAnimInstance.h"
#include "ControlRig.h"
#include "Player/Epic/CourierContactRig.h"
#include "Animation/AnimSequence.h"
#include "Chooser.h"
#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedData.h"
#endif

bool UCourierEpicAnimationConfig::ValidateFor(const USkeletalMesh* Mesh,FText& OutReason) const
{
    auto Fail=[&](const TCHAR* Message) { OutReason=FText::FromString(Message); return false; };
    if(!Mesh || TargetMesh.LoadSynchronous()!=Mesh || !Mesh->GetSkeleton()) return Fail(TEXT("Epic animation target mesh is missing or mismatched"));
    if(ValidationFingerprint.IsEmpty()) return Fail(TEXT("Epic animation assets have not passed editor validation"));
    const auto* Skeleton=Mesh->GetSkeleton();
    for(const auto& Reference:{StandingLanding,MovingLanding})
    {
        const auto* Sequence=Reference.LoadSynchronous();
        if(!Sequence || Sequence->GetSkeleton()!=Skeleton || Sequence->GetAdditiveAnimType()!=AAT_None)
            return Fail(TEXT("Courier landing clips are missing or incompatible"));
    }
    if(!DatabaseChooser.LoadSynchronous()) return Fail(TEXT("Courier ground/air chooser is missing"));
    if(!FMath::IsFinite(JogReferenceSpeed) || !FMath::IsFinite(SprintReferenceSpeed) || JogReferenceSpeed<=0 || SprintReferenceSpeed<=JogReferenceSpeed)
        return Fail(TEXT("Forward gait reference speeds are missing or invalid"));
    for(const auto& Reference:{LocomotionDatabase,SprintDatabase,AirDatabase})
    {
        const auto* DB=Reference.LoadSynchronous();
        if(!DB || !DB->Schema || DB->Schema->GetSkeleton(DB->Schema->GetDefaultRole())!=Skeleton || DB->GetNumAnimationAssets()==0)
            return Fail(TEXT("Pose Search database is missing, empty or uses a different skeleton"));
        for(int32 Index=0;Index<DB->GetNumAnimationAssets();++Index)
        {
            const auto* Sequence=Cast<UAnimSequence>(DB->GetAnimationAsset(Index));
            if(!Sequence || Sequence->GetSkeleton()!=Skeleton || Sequence->GetAdditiveAnimType()!=AAT_None)
                return Fail(TEXT("Pose Search entries must be full poses retargeted to the courier"));
        }
#if WITH_EDITOR
        using namespace UE::PoseSearch;
        if(FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,ERequestAsyncBuildFlag::ContinueRequest)!=EAsyncBuildIndexResult::Success)
            return Fail(TEXT("Pose Search index is not ready"));
#endif
    }
    for(const auto& Reference:{VaultMontage,MantleMontage})
    {
        const auto* Montage=Reference.LoadSynchronous();
        if(!Montage || Montage->GetSkeleton()!=Skeleton || !Montage->HasRootMotion())
            return Fail(TEXT("Traversal montage must use the courier skeleton and root motion"));
    }
    auto* Class=AnimationClass.LoadSynchronous();
    const auto* AnimClass=IAnimClassInterface::GetFromClass(Class);
    const auto* Defaults=Class?Cast<UCourierAnimInstance>(Class->GetDefaultObject()):nullptr;
    if(!Defaults || !Defaults->IsEpicAnimationEnabled() || !AnimClass || AnimClass->GetTargetSkeleton()!=Skeleton)
        return Fail(TEXT("Epic animation graph class is missing or incompatible"));
    if(Defaults->EpicGraphVersion!=UCourierAnimInstance::RequiredEpicGraphVersion)
        return Fail(TEXT("Epic animation graph is out of date; regenerate its grounded idle branch"));
    if(!ContactRig.LoadSynchronous() || !PhysicsAsset.LoadSynchronous() || PhysicsAsset.LoadSynchronous()->SkeletalBodySetups.IsEmpty())
        return Fail(TEXT("Courier contact rig or physics bodies are missing"));
    if(!ContactRig.LoadSynchronous()->IsChildOf(UCourierContactRig::StaticClass())) return Fail(TEXT("Contact rig must solve the courier human skeleton"));
    for(FName Bone:{FName(TEXT("pelvis")),FName(TEXT("chest")),FName(TEXT("upperarm_l")),FName(TEXT("upperarm_r")),FName(TEXT("forearm_l")),FName(TEXT("forearm_r"))})
        if(PhysicsAsset.LoadSynchronous()->FindBodyIndex(Bone)==INDEX_NONE) return Fail(TEXT("Courier upper-body physics coverage is incomplete"));
    for(const auto& Body:PhysicsAsset.LoadSynchronous()->SkeletalBodySetups)
        if(!Body || Mesh->GetRefSkeleton().FindBoneIndex(Body->BoneName)==INDEX_NONE)
            return Fail(TEXT("Physical animation body does not belong to the courier skeleton"));
    OutReason=FText::GetEmpty();
    return true;
}
