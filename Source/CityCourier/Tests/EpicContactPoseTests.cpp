#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "ControlRig.h"
#include "Player/Epic/CourierContactRig.h"
#include "Rigs/RigHierarchy.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Tests/StationTestSupport.h"
#include "Player/CourierAnimInstance.h"
#include "Player/CourierActionComponent.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#include "Player/Epic/CourierObjectInteractionComponent.h"
#include "World/CourierInteractionButton.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "AnimNode_ControlRig.h"
#include "Animation/AnimClassInterface.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"

struct FEpicContactFixture : FStationFixture
{
    TGuardValue<uint64> FrameGuard{GFrameCounter,GFrameCounter};
    UCourierEpicAnimationConfig* Config;
    UCourierAnimInstance* Anim;
    FEpicContactFixture()
    {
        Rider->SetActorLocation(FVector(0,0,90)); Rider->DispatchBeginPlay();
        Config=LoadObject<UCourierEpicAnimationConfig>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation.DA_CourierEpicAnimation"));
        Rider->EpicAnimationConfig=Config;
        Rider->GetMesh()->SetAnimInstanceClass(Config->AnimationClass.LoadSynchronous());
        Anim=CastChecked<UCourierAnimInstance>(Rider->GetMesh()->GetAnimInstance());
        Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Rider->GetActions()->TickComponent(.3f,LEVELTICK_All,nullptr);
        using namespace UE::PoseSearch;
        for(auto* DB:{Config->LocomotionDatabase.LoadSynchronous(),Config->SprintDatabase.LoadSynchronous(),Config->AirDatabase.LoadSynchronous()})
            FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,ERequestAsyncBuildFlag::NewRequest|ERequestAsyncBuildFlag::WaitForCompletion);
        Step(.05f);
    }
    void Step(float Delta)
    {
        ++GFrameCounter;
        Rider->GetObjectInteraction()->TickComponent(Delta,LEVELTICK_All,nullptr);
        Rider->GetMesh()->TickAnimation(Delta,false); Rider->GetMesh()->RefreshBoneTransforms();
    }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicHandReachTest,"CityCourier.EpicAnimation.Contact.HandReach",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicHandReachTest::RunTest(const FString&)
{
    FEpicContactFixture F;
    for(auto* Property:IAnimClassInterface::GetFromClass(F.Anim->GetClass())->GetAnimNodeProperties())
    {
        if(Property->Struct==FAnimNode_MotionMatching::StaticStruct())
        {
            const auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(F.Anim);
            AddInfo(FString::Printf(TEXT("Selected %s"),*GetPathNameSafe(Node->GetMotionMatchingState().SearchResult.SelectedAnim)));
            if(const auto* Clip=Cast<UAnimSequence>(Node->GetMotionMatchingState().SearchResult.SelectedAnim))
                for(FName Bone:{FName(TEXT("CourierRig")),FName(TEXT("root")),FName(TEXT("pelvis"))})
                {
                    const int32 Index=Clip->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Bone);
                    FTransform Local; Clip->GetBoneTransform(Local,FSkeletonPoseBoneIndex(Index),FAnimExtractContext(0.),false);
                    AddInfo(FString::Printf(TEXT("Track %s sample %s reference %s"),*Bone.ToString(),*Local.ToString(),*Clip->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[Index].ToString()));
                }
        }
        if(Property->Struct==FAnimNode_ControlRig::StaticStruct())
        {
            auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_ControlRig>(F.Anim);
            auto* Rig=Node->GetControlRig();
            AddInfo(FString::Printf(TEXT("Rig %s bones %d hand %s"),*GetPathNameSafe(Rig),Rig?Rig->GetHierarchy()->Num():0,Rig?*Rig->GetHierarchy()->GetGlobalTransform(FRigElementKey(TEXT("hand_r"),ERigElementType::Bone)).ToString():TEXT("absent")));
        }
    }
    if(!TestNotNull(TEXT("Courier-specific generated contact rig"),F.Config->ContactRig.LoadSynchronous())) return false;
    const FVector Initial=F.Rider->GetMesh()->GetSocketLocation(TEXT("hand_r"));
    const FVector Shoulder=F.Rider->GetMesh()->GetSocketLocation(TEXT("upperarm_r"));
    TestTrue(TEXT("Retargeted torso remains inside character capsule height"),FMath::Abs(Shoulder.Z-F.Rider->GetActorLocation().Z)<150);
    const FVector Goal=FMath::Lerp(Initial,Shoulder,.35)+FVector(25,0,0);
    auto* Button=F.World->SpawnActor<ACourierInteractionButton>(Goal+FVector(8,0,0),FRotator::ZeroRotator);
    Button->HandContact->SetWorldLocation(Goal);
    TestTrue(TEXT("Goal requires a visible reach"),FVector::Dist(Initial,Goal)>20);
    const auto Result=F.Rider->GetObjectInteraction()->TryPress(Button);
    AddInfo(FString::Printf(TEXT("Press result: %s, shoulder %s hand %s goal %s"),*Result.Reason.ToString(),*Shoulder.ToString(),*Initial.ToString(),*Goal.ToString()));
    if(!TestTrue(TEXT("Reachable button accepted"),Result.bSucceeded)) return false;
    double Closest=MAX_dbl;
    for(int32 Frame=0;Frame<50;++Frame)
    {
        F.Step(1.f/60);
        Closest=FMath::Min(Closest,FVector::Dist(F.Rider->GetMesh()->GetSocketLocation(TEXT("hand_r")),Goal));
    }
    AddInfo(FString::Printf(TEXT("Final evaluated hand closest error %.3f cm"),Closest));
    TestTrue(TEXT("Rendered hand bone reaches within five cm"),Closest<=5);
    TestEqual(TEXT("Only actual contact activates button"),Button->GetPressCount(),1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicFootWeightsTest,"CityCourier.EpicAnimation.Contact.FootContactStateWeights",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicFootWeightsTest::RunTest(const FString&)
{
    FEpicContactFixture F; F.Step(.2f);
    const float Ground=F.Anim->LeftFootAlpha;
    TestTrue(TEXT("Grounded foot contact active"),Ground>.5f);
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Falling); F.Step(.025f);
    TestTrue(TEXT("Leaving ground fades rather than snapping"),F.Anim->LeftFootAlpha>0 && F.Anim->LeftFootAlpha<Ground);
    F.Step(.3f); TestTrue(TEXT("Airborne foot contact released"),F.Anim->LeftFootAlpha<.01f);
    F.Rider->SetMounted(true); F.Step(.2f); TestTrue(TEXT("Seated feet are not ground locked"),F.Anim->RightFootAlpha<.01f);
    F.Rider->SetMounted(false); F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking); F.Step(.3f);
    F.Rider->SetInputSuppressed(true); F.Step(.3f);
    TestTrue(TEXT("Interface suppression releases contact weights"),F.Anim->LeftFootAlpha<.01f && F.Anim->RightFootAlpha<.01f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicContactRenderTest,"CityCourier.EpicAnimation.Contact.RenderedHandContact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FEpicContactRenderTest::RunTest(const FString&)
{
    FEpicContactFixture F;
    const FVector Initial=F.Rider->GetMesh()->GetSocketLocation(TEXT("hand_r"));
    const FVector Shoulder=F.Rider->GetMesh()->GetSocketLocation(TEXT("upperarm_r"));
    const FVector Goal=FMath::Lerp(Initial,Shoulder,.35)+FVector(25,0,0);
    auto* Button=F.World->SpawnActor<ACourierInteractionButton>(Goal+FVector(8,0,0),FRotator::ZeroRotator);
    Button->HandContact->SetWorldLocation(Goal);
    auto* Camera=F.World->SpawnActor<ASceneCapture2D>(FVector(220,220,160),FRotator::ZeroRotator);
    Camera->SetActorRotation((FVector(0,0,95)-Camera->GetActorLocation()).Rotation());
    auto* Capture=Camera->GetCaptureComponent2D();
    auto* Target=NewObject<UTextureRenderTarget2D>(Camera); Target->InitAutoFormat(640,640); Target->UpdateResourceImmediate(true);
    Capture->TextureTarget=Target; Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
    Capture->CaptureSource=ESceneCaptureSource::SCS_BaseColor; Capture->FOVAngle=45;
    Capture->ShowFlags.SetAtmosphere(false); Capture->ShowFlags.SetFog(false);
    F.World->UpdateWorldComponents(false,false);
    const FString Folder=FPaths::ProjectSavedDir()/TEXT("Validation/EpicSamples/Contact");
    IFileManager::Get().MakeDirectory(*Folder,true);
    auto Save=[&](const TCHAR* Name)
    {
        Capture->CaptureScene(); FlushRenderingCommands();
        FBufferArchive Image;
        const bool bExport=FImageUtils::ExportRenderTarget2DAsPNG(Target,Image) && FFileHelper::SaveArrayToFile(Image,*(Folder/Name));
        TestTrue(TEXT("Offscreen pose image exported"),bExport);
        TArray<FColor> Pixels; Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels);
        int32 Visible=0; for(const auto& Pixel:Pixels) if(Pixel.R>15 || Pixel.G>15 || Pixel.B>15) ++Visible;
        TestTrue(TEXT("Capture contains visible geometry"),Visible>500);
    };
    Save(TEXT("before.png"));
    if(!TestTrue(TEXT("Real reach starts"),F.Rider->GetObjectInteraction()->TryPress(Button).bSucceeded)) return false;
    for(int32 Frame=0;Frame<75 && Button->GetPressCount()==0;++Frame) F.Step(1.f/60);
    const FVector Hand=F.Rider->GetMesh()->GetSocketLocation(TEXT("hand_r"));
    TestEqual(TEXT("Rendered reach submits once"),Button->GetPressCount(),1);
    TestTrue(TEXT("Captured hand touches target"),FVector::Dist(Hand,Goal)<=5);
    Save(TEXT("contact.png"));
    FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"initial\":\"%s\",\"target\":\"%s\",\"hand\":\"%s\",\"error_cm\":%.4f,\"press_count\":%d}"),*Initial.ToString(),*Goal.ToString(),*Hand.ToString(),FVector::Dist(Hand,Goal),Button->GetPressCount()),*(Folder/TEXT("state.json")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicFootSwingTest,"CityCourier.EpicAnimation.Contact.FootSwingAndSlope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicFootSwingTest::RunTest(const FString&)
{
    auto* Config=LoadObject<UCourierEpicAnimationConfig>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation.DA_CourierEpicAnimation"));
    auto* Class=Config?Config->ContactRig.LoadSynchronous():nullptr;
    if(!TestNotNull(TEXT("Generated human rig"),Class)) return false;
    auto* Rig=NewObject<UCourierContactRig>(GetTransientPackage(),Class); Rig->Initialize(true); Rig->Execute(FRigUnit_BeginExecution::EventName);
    auto* Hierarchy=Rig->GetHierarchy(); const FRigElementKey Foot(TEXT("foot_l"),ERigElementType::Bone);
    if(!TestTrue(TEXT("Human foot exists"),Hierarchy->GetIndex(Foot)!=INDEX_NONE)) return false;
    auto SetFloat=[&](FName Name,float Value) { auto* Property=FindFProperty<FFloatProperty>(Class,Name); check(Property); Property->SetPropertyValue_InContainer(Rig,Value); };
    auto SetGoal=[&](const FTransform& Value) { auto* Property=FindFProperty<FStructProperty>(Class,TEXT("LeftFootGoal")); check(Property); *Property->ContainerPtrToValuePtr<FTransform>(Rig)=Value; };
    SetFloat(TEXT("ContactEnabled"),1); SetFloat(TEXT("LeftFootAlpha"),1);
    const double GroundZ=Hierarchy->GetGlobalTransform(Foot).GetLocation().Z;
    const FRigElementKey Hip(TEXT("thigh_l"),ERigElementType::Bone);
    FTransform HipPose=Hierarchy->GetGlobalTransform(Hip);
    HipPose.SetRotation(FQuat(FVector::RightVector,FMath::DegreesToRadians(60.))*HipPose.GetRotation());
    Hierarchy->SetGlobalTransform(Hip,HipPose);
    FTransform Incoming=Hierarchy->GetGlobalTransform(Foot);
    FTransform Goal=Incoming; Goal.SetLocation(FVector(Incoming.GetLocation().X,Incoming.GetLocation().Y,GroundZ)); Goal.SetRotation(FQuat::Identity); SetGoal(Goal);
    Rig->Execute_Internal(FRigUnit_BeginExecution::EventName);
    TestTrue(TEXT("Swing fixture has a lifted foot with unchanged limb lengths"),Incoming.GetLocation().Z>GroundZ+25);
    TestTrue(TEXT("Swing foot keeps its authored trajectory"),Hierarchy->GetGlobalTransform(Foot).GetLocation().Equals(Incoming.GetLocation(),.1));
    Hierarchy->ResetPoseToInitial(ERigElementType::Bone);
    Incoming=Hierarchy->GetGlobalTransform(Foot);
    Goal=Incoming; const FQuat Slope(FVector::ForwardVector,FMath::DegreesToRadians(15.)); Goal.SetRotation(Slope); SetGoal(Goal);
    Rig->Execute_Internal(FRigUnit_BeginExecution::EventName);
    TestTrue(TEXT("Planted foot follows surface tilt"),Hierarchy->GetGlobalTransform(Foot).GetRotation().AngularDistance(Slope*Incoming.GetRotation())<.02);
    Hierarchy->ResetPoseToInitial(ERigElementType::Bone);
    Incoming=Hierarchy->GetGlobalTransform(Foot);
    Goal=Incoming; Goal.AddToTranslation(FVector(35,-25,0)); Goal.SetRotation(FQuat::Identity); SetGoal(Goal);
    Rig->Execute_Internal(FRigUnit_BeginExecution::EventName);
    TestTrue(TEXT("Previous-frame ground trace cannot pull a planted foot sideways"),
        FVector::DistSquared2D(Hierarchy->GetGlobalTransform(Foot).GetLocation(),Incoming.GetLocation())<.25*.25);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicHandOrientationTest,"CityCourier.EpicAnimation.Contact.HandGoalOrientation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicHandOrientationTest::RunTest(const FString&)
{
    auto* Config=LoadObject<UCourierEpicAnimationConfig>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation.DA_CourierEpicAnimation"));
    auto* Class=Config?Config->ContactRig.LoadSynchronous():nullptr;
    if(!TestNotNull(TEXT("Generated human rig"),Class)) return false;
    auto* Rig=NewObject<UCourierContactRig>(GetTransientPackage(),Class); Rig->Initialize(true); Rig->Execute(FRigUnit_BeginExecution::EventName);
    const FRigElementKey Hand(TEXT("hand_r"),ERigElementType::Bone);
    FTransform Goal=Rig->GetHierarchy()->GetGlobalTransform(Hand);
    Goal.SetRotation(FQuat(FVector::UpVector,FMath::DegreesToRadians(30.))*Goal.GetRotation());
    for(FName Name:{FName(TEXT("ContactEnabled")),FName(TEXT("RightHandAlpha"))}) FindFProperty<FFloatProperty>(Class,Name)->SetPropertyValue_InContainer(Rig,1);
    *FindFProperty<FStructProperty>(Class,TEXT("RightHandGoal"))->ContainerPtrToValuePtr<FTransform>(Rig)=Goal;
    Rig->Execute_Internal(FRigUnit_BeginExecution::EventName);
    TestTrue(TEXT("Authored contact orientation reaches the wrist"),Rig->GetHierarchy()->GetGlobalTransform(Hand).GetRotation().AngularDistance(Goal.GetRotation())<.02);
    return true;
}
#endif
