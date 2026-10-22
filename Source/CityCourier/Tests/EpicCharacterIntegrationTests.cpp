#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Player/CourierAnimInstance.h"
#include "Player/CourierActionComponent.h"
#include "Player/CourierActionAnimation.h"
#include "Player/Epic/CourierEpicAnimationConfig.h"
#include "Player/Epic/CourierTraversalComponent.h"
#include "Player/Epic/CourierObjectInteractionComponent.h"
#include "Player/Epic/CourierPhysicalReactionComponent.h"
#include "World/CourierGrabbable.h"
#include "World/CourierInteractionButton.h"
#include "World/CourierTrainingTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "GameFramework/WorldSettings.h"
#include "Animation/AnimMontage.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicLandingTest,"CityCourier.EpicCharacterIntegration.JumpLanding",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicLandingTest::RunTest(const FString&)
{
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto* Config=LoadObject<UCourierEpicAnimationConfig>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation.DA_CourierEpicAnimation"));
    for(auto* DB:{Config->LocomotionDatabase.LoadSynchronous(),Config->SprintDatabase.LoadSynchronous(),Config->AirDatabase.LoadSynchronous()})
        UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest|UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
    FStationFixture F;
    auto* Class=LoadClass<ACourierCharacter>(nullptr,TEXT("/Game/CityCourier/Blueprints/BP_CourierCharacter.BP_CourierCharacter_C"));
    auto* Rider=F.World->SpawnActor<ACourierCharacter>(Class,FVector(0,0,90),FRotator::ZeroRotator);
    F.Controller->Possess(Rider); F.Rider->Destroy(); Rider->DispatchBeginPlay();
    auto* Mesh=Rider->GetMesh(); auto* Anim=Cast<UCourierAnimInstance>(Mesh->GetAnimInstance());
    if(!TestTrue(TEXT("Production Epic graph enabled"),Anim && Anim->IsEpicAnimationEnabled())) return false;
    auto Step=[&](float Delta) { ++GFrameCounter; Rider->GetActions()->TickComponent(Delta,LEVELTICK_All,nullptr); Mesh->TickAnimation(Delta,false); Mesh->RefreshBoneTransforms(); };
    for(float Speed:{0.f,350.f})
    {
        Rider->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
        Rider->GetCharacterMovement()->Velocity=FVector(Speed,0,-200); Step(.2f);
        Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Rider->GetCharacterMovement()->Velocity=FVector(Speed,0,0); Step(.05f);
        auto* Montage=Anim->GetCurrentActiveMontage();
        if(!TestNotNull(TEXT("Ground contact starts a landing montage"),Montage)) return false;
        const auto* Clip=Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference().Get();
        TestTrue(TEXT("Landing uses owned GASP clip"),Clip && Clip->GetPathName().Contains(TEXT("/CityCourier/Animation/Epic/Clips/CC_")) && Clip->GetName().Contains(TEXT("_Land_")));
        const float Before=Anim->Montage_GetPosition(Montage);
        const FVector Pelvis=Mesh->GetSocketLocation(TEXT("pelvis"));
        Step(.1f);
        TestTrue(TEXT("Landing time advances after grounding"),Anim->Montage_GetPosition(Montage)>Before+.05f);
        TestTrue(TEXT("Evaluated landing pose progresses"),FVector::Dist(Pelvis,Mesh->GetSocketLocation(TEXT("pelvis")))>.01);
        Step(1.f);
        TestFalse(TEXT("Landing releases full body slot"),Anim->Montage_IsPlaying(Montage));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicRealPlayerTest,"CityCourier.EpicCharacterIntegration.RealPlayerFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicRealPlayerTest::RunTest(const FString&)
{
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto* Config=LoadObject<UCourierEpicAnimationConfig>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation.DA_CourierEpicAnimation"));
    if(!TestNotNull(TEXT("Courier config"),Config)) return false;
    for(auto* DB:{Config->LocomotionDatabase.LoadSynchronous(),Config->SprintDatabase.LoadSynchronous(),Config->AirDatabase.LoadSynchronous()})
        UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest|UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
    FStationFixture F;
    auto* Class=LoadClass<ACourierCharacter>(nullptr,TEXT("/Game/CityCourier/Blueprints/BP_CourierCharacter.BP_CourierCharacter_C"));
    auto* Rider=F.World->SpawnActor<ACourierCharacter>(Class,FVector(0,0,90),FRotator::ZeroRotator);
    F.Controller->Possess(Rider); F.Rider->Destroy(); Rider->DispatchBeginPlay();
    if(!TestEqual(TEXT("Production BP owns validated config"),Rider->EpicAnimationConfig.Get(),Config)) return false;
    auto* Anim=Cast<UCourierAnimInstance>(Rider->GetMesh()->GetAnimInstance());
    if(!TestTrue(TEXT("Production BP actually enables Epic graph"),Anim && Anim->IsEpicAnimationEnabled())) return false;
    F.World->InitializeActorsForPlay(FURL()); F.World->BeginPlay(); F.World->GetWorldSettings()->NotifyBeginPlay();
    Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking); Rider->GetActions()->TickComponent(.3f,LEVELTICK_All,nullptr);
    auto Step=[&](float Delta) { ++GFrameCounter; Rider->GetObjectInteraction()->TickComponent(Delta,LEVELTICK_All,nullptr); Rider->GetMesh()->TickAnimation(Delta,false); Rider->GetMesh()->RefreshBoneTransforms(); };
    Rider->GetCharacterMovement()->Velocity=FVector(350,0,0); Step(.1f);
    TestTrue(TEXT("Same rider drives locomotion"),Anim->GroundSpeed>300);
    Rider->GetCharacterMovement()->Velocity=FVector::ZeroVector;
    auto* Crate=F.World->SpawnActor<ACourierGrabbable>(FVector(90,0,70),FRotator::ZeroRotator);
    TestTrue(TEXT("Same rider grabs"),Rider->GetObjectInteraction()->TryGrab(Crate->Body).bSucceeded);
    TestTrue(TEXT("E releases held object"),Rider->TryInteract());
    TestFalse(TEXT("Grab released"),Rider->GetObjectInteraction()->IsBusy());
    Crate->Destroy(); Step(.05f);
    const FVector Hand=Rider->GetMesh()->GetSocketLocation(TEXT("hand_r"));
    auto* Button=F.World->SpawnActor<ACourierInteractionButton>(Hand+FVector(8,0,0),FRotator::ZeroRotator); Button->HandContact->SetWorldLocation(Hand);
    TestTrue(TEXT("Same rider presses"),Rider->GetObjectInteraction()->TryPress(Button).bSucceeded);
    double Closest=MAX_dbl;
    for(int32 Frame=0;Frame<75;++Frame)
    {
        Step(1.f/60);
        Closest=FMath::Min(Closest,FVector::Dist(Rider->GetMesh()->GetSocketLocation(TEXT("hand_r")),Hand));
        if(Frame==20 || Frame==40 || Frame==74)
            AddInfo(FString::Printf(TEXT("frame %d busy %d alpha %.3f error %.3f mode %d action %d goal %s hand %s"),Frame,Rider->GetObjectInteraction()->IsBusy(),Anim->RightHandAlpha,FVector::Dist(Rider->GetMesh()->GetSocketLocation(TEXT("hand_r")),Hand),int32(Rider->GetCharacterMovement()->MovementMode),int32(Anim->CurrentAction),*Hand.ToString(),*Rider->GetMesh()->GetSocketLocation(TEXT("hand_r")).ToString()));
    }
    AddInfo(FString::Printf(TEXT("closest integrated contact %.3f"),Closest));
    TestEqual(TEXT("Exactly one contact in actual graph"),Button->GetPressCount(),1); Button->Destroy();
    auto* Obstacle=F.Box(FVector(140,0,45),FVector(40,100,45)); Obstacle->GetRootComponent()->SetMobility(EComponentMobility::Static);
    Rider->GetActions()->TickComponent(.3f,LEVELTICK_All,nullptr);
    if(TestTrue(TEXT("Same rider starts root motion traversal"),Rider->TryJumpWithFeedback().bSucceeded))
    {
        TestTrue(TEXT("Jump chose authored traversal"),Rider->GetTraversal()->IsActive());
        for(int32 Frame=0;Frame<180;++Frame)
        {
            ++GFrameCounter;
            Rider->GetCharacterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
            Rider->GetMesh()->RefreshBoneTransforms(); Rider->GetActions()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
            Rider->GetTraversal()->TickComponent(1.f/60,LEVELTICK_All,nullptr);
        }
        TestTrue(TEXT("Same production rider actually clears obstacle"),Rider->GetActorLocation().X>215);
        TestFalse(TEXT("Traversal releases ownership"),Rider->GetTraversal()->IsActive());
    }
    TestTrue(TEXT("Same rider has local physics reaction"),Rider->GetPhysicalReaction()->ApplyLocalImpulse(TEXT("upperarm_r"),FVector(600,0,0)));
    Rider->SetMounted(true);
    TestFalse(TEXT("Mount cancels local reaction"),Rider->GetPhysicalReaction()->IsActive()); Step(.2f);
    TestEqual(TEXT("Same rider retains seated posture"),Anim->EpicPostureIndex,1);
    Rider->SetMounted(false); Step(.2f);
    TestFalse(TEXT("Unmount releases seated posture"),Anim->bRiding);
    FText Reason; TestTrue(TEXT("All interruption states restore ground action ownership"),Rider->CanPerformGroundAction(Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicActionAuthorityTest,"CityCourier.EpicAnimation.Graph.ActionsRemainAuthoritative",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicActionAuthorityTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->DispatchBeginPlay();
    auto* Config=LoadObject<UCourierEpicAnimationConfig>(nullptr,TEXT("/Game/CityCourier/Animation/Epic/DA_CourierEpicAnimation.DA_CourierEpicAnimation"));
    for(auto* DB:{Config->LocomotionDatabase.LoadSynchronous(),Config->SprintDatabase.LoadSynchronous(),Config->AirDatabase.LoadSynchronous()})
        UE::PoseSearch::FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(DB,UE::PoseSearch::ERequestAsyncBuildFlag::NewRequest|UE::PoseSearch::ERequestAsyncBuildFlag::WaitForCompletion);
    if(!TestTrue(TEXT("Validated graph enabled"),F.Rider->TryEnableEpicAnimation(Config).bSucceeded)) return false;
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking); F.Rider->GetActions()->TickComponent(.3f,LEVELTICK_All,nullptr);
    auto* Mesh=F.Rider->GetMesh(); auto* Actions=F.Rider->GetActions();
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Step=[&](float Delta) { ++GFrameCounter; Mesh->TickAnimation(Delta,false); Mesh->RefreshBoneTransforms(); };
    Step(.1f);
    if(!TestTrue(TEXT("Existing attack input accepted"),Actions->QueueAttack(false))) return false;
    Actions->TickComponent(.06f,LEVELTICK_All,nullptr); Step(.12f);
    FVector Expected;
    FCourierActionAnimation::SampleFist(Mesh,FCourierActionAnimation::Load(TEXT("A_LightPunch_L")),true,.18,Expected);
    TestTrue(TEXT("Epic graph preserves final authored fist"),FVector::Dist(Mesh->GetSocketLocation(TEXT("PunchTrace_L")),Expected)<1);
    auto* Target=F.World->SpawnActor<ACourierTrainingTarget>(Expected,FRotator::ZeroRotator);
    Actions->TickComponent(.2f,LEVELTICK_All,nullptr); Actions->TickComponent(.05f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Gameplay action timeline still delivers one hit"),Target->GetHitCount(),1);
    return true;
}
#endif
