#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Player/CourierAnimInstance.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimClassInterface.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "AnimNodes/AnimNode_LayeredBoneBlend.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchDatabase.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicLocomotionTest,"CityCourier.EpicAnimation.Graph.UsesRetargetedLocomotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicLocomotionTest::RunTest(const FString&)
{
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto* Config=LoadObject<UCourierEpicAnimationConfig>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation.DA_CourierEpicAnimation"));
    if(!TestNotNull(TEXT("Generated courier config"),Config)) return false;
#if WITH_EDITOR
    for(auto* Database:{Config->LocomotionDatabase.LoadSynchronous(),Config->SprintDatabase.LoadSynchronous(),Config->AirDatabase.LoadSynchronous()})
    {
        using namespace UE::PoseSearch;
        if(!TestNotNull(TEXT("Retargeted database"),Database)) return false;
        TestTrue(TEXT("Cold database index completes before sampling"),FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database,ERequestAsyncBuildFlag::NewRequest|ERequestAsyncBuildFlag::WaitForCompletion)==EAsyncBuildIndexResult::Success);
    }
#endif
    FStationFixture F; F.Rider->DispatchBeginPlay();
    // Exercise the graph independently while final traversal/rig/physics gates remain closed.
    auto* Class=Config->AnimationClass.LoadSynchronous();
    if(!TestNotNull(TEXT("Generated Epic graph class"),Class)) return false;
    F.Rider->EpicAnimationConfig=Config;
    F.Rider->GetMesh()->SetAnimInstanceClass(Class);
    auto* Anim=Cast<UCourierAnimInstance>(F.Rider->GetMesh()->GetAnimInstance());
    if(!TestNotNull(TEXT("Courier animation instance"),Anim)) return false;
    TestTrue(TEXT("Generated graph uses standard proxy"),Anim->IsEpicAnimationEnabled());
    TestEqual(TEXT("Saved graph includes the required stationary idle branch"),Anim->EpicGraphVersion,UCourierAnimInstance::RequiredEpicGraphVersion);
    {
        auto* Defaults=CastChecked<UCourierAnimInstance>(Class->GetDefaultObject());
        TGuardValue<int32> OldGraph(Defaults->EpicGraphVersion,0);
        FText Reason;
        TestFalse(TEXT("A saved old graph cannot silently enable running at rest"),Config->ValidateFor(F.Rider->GetMesh()->GetSkeletalMeshAsset(),Reason));
        TestTrue(TEXT("Old graph reports the regeneration requirement"),Reason.ToString().Contains(TEXT("out of date")));
    }
    int32 MatchNodes=0;
    for(const auto* Property:IAnimClassInterface::GetFromClass(Anim->GetClass())->GetAnimNodeProperties())
        if(Property->Struct==FAnimNode_MotionMatching::StaticStruct())
        {
            ++MatchNodes;
            const void* Node=Property->ContainerPtrToValuePtr<void>(Anim);
            auto* DatabaseProperty=FindFProperty<FObjectPropertyBase>(Property->Struct,TEXT("Database"));
            TestNotNull(TEXT("Every compiled motion matching node retains its database"),DatabaseProperty->GetObjectPropertyValue_InContainer(Node));
        }
    TestEqual(TEXT("Separate jog, sprint and air matching nodes compiled"),MatchNodes,3);
    int32 LegLayers=0;
    for(const auto* Property:IAnimClassInterface::GetFromClass(Anim->GetClass())->GetAnimNodeProperties())
        if(Property->Struct==FAnimNode_LayeredBoneBlend::StaticStruct()) ++LegLayers;
    TestEqual(TEXT("Moving punches have a compiled layer preserving locomotion legs"),LegLayers,1);
    auto CheckGroundBank=[&](UPoseSearchDatabase* DB,const TCHAR* Expected)
    {
        if(!TestNotNull(TEXT("Explicit gait database"),DB)) return;
        TestEqual(TEXT("Only the forward steady gait is eligible"),DB->GetNumAnimationAssets(),1);
        if(DB->GetNumAnimationAssets()==1) TestEqual(TEXT("Starts, strafes and pivots cannot enter forward movement"),DB->GetAnimationAsset(0)->GetName(),FString(Expected));
    };
    CheckGroundBank(Config->LocomotionDatabase.LoadSynchronous(),TEXT("CC_M_Neutral_Run_Loop_F"));
    CheckGroundBank(Config->SprintDatabase.LoadSynchronous(),TEXT("CC_M_Neutral_Sprint_Loop_F"));
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    F.Rider->GetCharacterMovement()->Velocity=FVector(350,0,0);
    for(int32 Frame=0;Frame<8;++Frame)
    {
        ++GFrameCounter; F.Rider->GetMesh()->TickAnimation(.1f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
        TestTrue(TEXT("Evaluated root keeps courier units across matching blends"),F.Rider->GetMesh()->GetSocketTransform(TEXT("CourierRig"),RTS_Component).GetScale3D().Equals(FVector(100),.02));
    }
    TestTrue(TEXT("Locomotion consumes gameplay velocity"),Anim->GroundSpeed>300);
    TestFalse(TEXT("Skeleton receives an evaluated pose"),F.Rider->GetMesh()->GetComponentSpaceTransforms().IsEmpty());
    bool bMatched=false;
    for(const auto* Property:IAnimClassInterface::GetFromClass(Anim->GetClass())->GetAnimNodeProperties())
        if(Property->Struct==FAnimNode_MotionMatching::StaticStruct())
        {
            const auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(Anim);
            bMatched|=Node->GetMotionMatchingState().SearchResult.SelectedAnim!=nullptr;
        }
    TestTrue(TEXT("Motion matching actually selects an indexed animation pose"),bMatched);
    F.Rider->GetCharacterMovement()->Velocity=FVector::ZeroVector;
    for(int32 Frame=0;Frame<12;++Frame)
    {
        ++GFrameCounter; F.Rider->GetMesh()->TickAnimation(.1f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
        TestEqual(TEXT("Stationary character explicitly selects library idle instead of running search results"),Anim->EpicPostureIndex,6);
    }
    // A zero-speed field alone cannot prove the graph stopped evaluating a run.
    const FVector IdleLeft=F.Rider->GetMesh()->GetSocketLocation(TEXT("foot_l"));
    const FVector IdleRight=F.Rider->GetMesh()->GetSocketLocation(TEXT("foot_r"));
    for(int32 Frame=0;Frame<30;++Frame)
    {
        ++GFrameCounter; F.Rider->GetMesh()->TickAnimation(1.f/30,false); F.Rider->GetMesh()->RefreshBoneTransforms();
        TestTrue(TEXT("Standing left foot does not keep taking running steps"),FVector::Dist(IdleLeft,F.Rider->GetMesh()->GetSocketLocation(TEXT("foot_l")))<5.);
        TestTrue(TEXT("Standing right foot does not keep taking running steps"),FVector::Dist(IdleRight,F.Rider->GetMesh()->GetSocketLocation(TEXT("foot_r")))<5.);
    }
    F.Rider->GetCharacterMovement()->Velocity=FVector(10,0,0);
    ++GFrameCounter; F.Rider->GetMesh()->TickAnimation(.1f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
    TestEqual(TEXT("Idle threshold boundary does not select a running search result"),Anim->EpicPostureIndex,6);
    F.Rider->GetCharacterMovement()->Velocity=FVector(350,0,0);
    ++GFrameCounter; F.Rider->GetMesh()->TickAnimation(.1f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
    TestEqual(TEXT("Moving again releases idle to GASP locomotion"),Anim->EpicPostureIndex,0);
    F.Rider->GetCharacterMovement()->Velocity=FVector(420,0,0);
    ++GFrameCounter; F.Rider->GetMesh()->TickAnimation(.1f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
    TestFalse(TEXT("Ordinary movement uses jog bank"),Anim->bUseSprintDatabase);
    TestTrue(TEXT("Jog cadence matches capsule speed"),FMath::IsNearlyEqual(Anim->JogPlayRate*Config->JogReferenceSpeed,420.f,.1f));
    F.Rider->GetCharacterMovement()->Velocity=FVector(700,0,0);
    for(int32 Frame=0;Frame<12;++Frame)
    { ++GFrameCounter; F.Rider->GetMesh()->TickAnimation(.1f,false); F.Rider->GetMesh()->RefreshBoneTransforms(); }
    TestTrue(TEXT("Fast movement switches to a distinct sprint bank"),Anim->bUseSprintDatabase);
    TestTrue(TEXT("Sprint cadence matches capsule speed"),FMath::IsNearlyEqual(Anim->SprintPlayRate*Config->SprintReferenceSpeed,700.f,.1f));
    bool bSprintSelected=false;
    for(const auto* Property:IAnimClassInterface::GetFromClass(Anim->GetClass())->GetAnimNodeProperties())
        if(Property->Struct==FAnimNode_MotionMatching::StaticStruct())
        {
            const auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(Anim);
            const auto* Selected=Node->GetMotionMatchingState().SearchResult.SelectedAnim.Get();
            bSprintSelected|=Selected && Selected->GetName()==TEXT("CC_M_Neutral_Sprint_Loop_F");
        }
    TestTrue(TEXT("Actual graph selects the forward sprint clip"),bSprintSelected);
    F.Rider->SetMounted(true);
    F.Rider->GetMesh()->TickAnimation(.1f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
    TestEqual(TEXT("Existing seated posture retained"),Anim->AnimationIndex,3);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicRootUnitsTest,"CityCourier.EpicAnimation.Assets.RootUnitsMatchCourier",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicRootUnitsTest::RunTest(const FString&)
{
    auto* Config=LoadObject<UCourierEpicAnimationConfig>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation.DA_CourierEpicAnimation"));
    if(!TestNotNull(TEXT("Generated config"),Config)) return false;
    int32 Checked=0;
    for(const auto& Ref:Config->RetargetedSequences)
    {
        const auto* Clip=Ref.LoadSynchronous(); if(!Clip || Clip->GetAdditiveAnimType()!=AAT_None) continue;
        // Constant reference tracks can be omitted by compression; the single-
        // bone compressed accessor then returns identity. Inspect authored keys
        // here and validate the fully evaluated root separately in the graph test.
        FTransform Root; Clip->GetBoneTransform(Root,FSkeletonPoseBoneIndex(0),FAnimExtractContext(0.),true);
        const FVector Expected=Clip->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[0].GetScale3D();
        if(!Root.GetScale3D().Equals(Expected,.01))
        {
            AddError(FString::Printf(TEXT("Root units differ: %s track %s reference %s"),*Clip->GetName(),*Root.GetScale3D().ToString(),*Expected.ToString()));
            TArray<FName> Names; Clip->GetDataModel()->GetBoneTrackNames(Names);
            AddInfo(FString::Printf(TEXT("Root bone %s; model has %d tracks"),*Clip->GetSkeleton()->GetReferenceSkeleton().GetBoneName(0).ToString(),Names.Num()));
            for(int32 Index=0;Index<FMath::Min(3,Names.Num());++Index)
                AddInfo(FString::Printf(TEXT("Model %s = %s"),*Names[Index].ToString(),*Clip->GetDataModel()->GetBoneTrackTransform(Names[Index],FFrameNumber(0)).ToString()));
            FTransform Raw; Clip->GetBoneTransform(Raw,FSkeletonPoseBoneIndex(0),FAnimExtractContext(0.),true);
            AddInfo(TEXT("Raw root = ")+Raw.ToString());
            return false;
        }
        if(Clip->bUseNormalizedRootMotionScale)
        { AddError(TEXT("Normalized root scale would multiply target root travel by authored unit conversion")); return false; }
        const double Travel=Clip->ExtractRootMotionFromRange(0,FMath::Min(.1,double(Clip->GetPlayLength())),FAnimExtractContext(0.)).GetTranslation().Size();
        if(Travel>200) { AddError(FString::Printf(TEXT("Unbounded root trajectory %.2f cm per .1s: %s"),Travel,*Clip->GetName())); return false; }
        ++Checked;
    }
    TestTrue(TEXT("All retargeted full-pose clips checked"),Checked>500);
    return true;
}
#endif
