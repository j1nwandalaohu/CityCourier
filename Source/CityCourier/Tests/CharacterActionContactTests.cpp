#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Player/CourierActionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/CourierActionAnimation.h"
#include "World/CourierTrainingTarget.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Components/BoxComponent.h"
#include "Animation/AnimInstance.h"
#include "Player/CourierAnimInstance.h"
// Existing authored Walk is deliberately borrowed as a trajectory fixture, not claimed as punching art.
struct FActionContactFixture : FStationFixture
{
    UAnimSequence* Sequence;
    FActionContactFixture()
    {
        Rider->SetActorLocation(FVector(0,0,90)); Rider->DispatchBeginPlay();
        Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Sequence=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/CityCourier/Art/Characters/A_Walk.A_Walk"));
        for(auto Action:{ECourierAction::LightL,ECourierAction::LightR,ECourierAction::Heavy,ECourierAction::HitFront,ECourierAction::HitBack})
            Rider->GetActions()->ActionAnimations.Add(Action,Sequence);
    }
    FVector Fist(double Time=.18)
    {
        FVector Out; FCourierActionAnimation::SampleFist(Rider->GetMesh(),Sequence,true,Time,Out); return Out;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterContactTest,"CityCourier.CharacterActions.Contact.TrajectoryAndOcclusion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterContactTest::RunTest(const FString&)
{
    FActionContactFixture F;
    const FVector Position=F.Fist();
    auto* Target=F.World->SpawnActor<ACourierTrainingTarget>(Position,FRotator::ZeroRotator);
    Target->Body->SetBoxExtent(FVector(5,5,10));
    auto* A=F.Rider->GetActions(); A->QueueAttack(false); A->TickComponent(.30f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Skipped complete active window still contacts authored fist trajectory"),Target->GetHitCount(),1);
    A->TickComponent(.1f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Recovery does not repeat contact"),Target->GetHitCount(),1);
    A->Cancel();
    // Put a thin wall between capsule center at fist height and the fist/target.
    const FVector Center(0,0,Position.Z); const FVector Mid=(Center+Position)*.5;
    auto* Wall=F.Box(Mid,FVector(200,2,100));
    for(double T:{.14,.18,.22})
    {
        const FVector Point=F.Fist(T); FHitResult Hit;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(ContactFixture),false,F.Rider);
        const bool bBlocked=F.World->LineTraceSingleByChannel(Hit,FVector(0,0,Point.Z),Point,ECC_Visibility,Query);
        TestTrue(TEXT("Fixture wall blocks every sampled fist ray"),bBlocked && Hit.GetActor()==Wall);
    }
    A->QueueAttack(false); A->TickComponent(.3f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Wall blocks target even when sweep begins beyond it"),Target->GetHitCount(),1);
    Wall->Destroy(); Target->Destroy(); A->Cancel();
    auto* Far=F.World->SpawnActor<ACourierTrainingTarget>(FVector(400,0,Position.Z),FRotator::ZeroRotator);
    A->QueueAttack(true); A->TickComponent(.6f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("No remote contact beyond maximum reach"),Far->GetHitCount(),0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterReceiverTest,"CityCourier.CharacterActions.Contact.ReactionsAndMissingAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterReceiverTest::RunTest(const FString&)
{
    FActionContactFixture F; auto* A=F.Rider->GetActions(); const auto Id=FGuid::NewGuid();
    TestTrue(TEXT("Valid frontal strike received"),A->ReceiveStrike(Id,FVector(100,0,90)));
    TestTrue(TEXT("Correct front reaction"),A->GetTimeline().GetAction()==ECourierAction::HitFront);
    TestFalse(TEXT("Attack ID deduplicated"),A->ReceiveStrike(Id,FVector(100,0,90)));
    A->TickComponent(.2f,LEVELTICK_All,nullptr); A->ReceiveStrike(FGuid::NewGuid(),FVector(-100,0,90)); A->TickComponent(.201f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Repeated hits do not restart stun timer"),A->GetTimeline().GetAction()==ECourierAction::None);
    A->ReceiveStrike(FGuid::NewGuid(),FVector(-100,0,90));
    TestTrue(TEXT("Back reaction selected"),A->GetTimeline().GetAction()==ECourierAction::HitBack);
    A->Cancel(); A->ActionAnimations.Empty();
    TestFalse(TEXT("Missing art rejects invisible attack"),A->QueueAttack(false));
    A->TickComponent(1,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Missing sequence cannot leave permanent action"),A->GetTimeline().GetAction()==ECourierAction::None);
    TestTrue(TEXT("Missing art has explicit feedback"),!F.Controller->GetActionFeedback().IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterContactEdgesTest,"CityCourier.CharacterActions.Contact.FrameFacingRangeAndDedup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterContactEdgesTest::RunTest(const FString&)
{
    FActionContactFixture F; auto* A=F.Rider->GetActions();
    auto* Target=F.World->SpawnActor<ACourierTrainingTarget>(F.Fist(),FRotator::ZeroRotator);
    A->QueueAttack(false); A->TickComponent(.15f,LEVELTICK_All,nullptr);
    for(int I=0;I<6;++I) A->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Multiple active frames still one receiver event"),Target->GetHitCount(),1);
    Target->Destroy(); A->Cancel();
    F.Controller->SetControlRotation(FRotator(0,90,0));
    A->QueueAttack(false); A->TickComponent(.8f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Frame spanning whole attack still captures camera facing"),FMath::IsNearlyEqual(F.Rider->GetActorRotation().Yaw,90.,.1));
    A->Cancel(); F.Controller->SetControlRotation(FRotator::ZeroRotator); F.Rider->SetActorRotation(FRotator::ZeroRotator);
    F.Rider->GetMesh()->AddRelativeLocation(FVector(400,0,0));
    FVector Right; TestTrue(TEXT("Real displaced trajectory sampled"),FCourierActionAnimation::SampleFist(F.Rider->GetMesh(),F.Sequence,false,.4,Right));
    auto* Far=F.World->SpawnActor<ACourierTrainingTarget>(Right,FRotator::ZeroRotator);
    A->QueueAttack(true); A->TickComponent(.5f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Malformed displaced fist cannot bypass reach ceiling"),Far->GetHitCount(),0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterActionBridgeTest,"CityCourier.CharacterActions.Contact.AnimationBridge",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterActionBridgeTest::RunTest(const FString&)
{
    FActionContactFixture F; auto* Mesh=F.Rider->GetMesh();
    TestTrue(TEXT("Named left fist attachment persists"),Mesh->DoesSocketExist(TEXT("PunchTrace_L")));
    TestTrue(TEXT("Named right fist attachment persists"),Mesh->DoesSocketExist(TEXT("PunchTrace_R")));
    Mesh->TickAnimation(.1f,false); Mesh->RefreshBoneTransforms();
    F.Rider->GetActions()->QueueAttack(false); F.Rider->GetActions()->TickComponent(.15f,LEVELTICK_All,nullptr);
    Mesh->TickAnimation(.05f,false); Mesh->RefreshBoneTransforms();
    auto* Anim=Cast<UCourierAnimInstance>(Mesh->GetAnimInstance());
    if(TestNotNull(TEXT("Native action animation bridge"),Anim))
    {
        TestTrue(TEXT("Action state available to animation"),Anim->CurrentAction==ECourierAction::LightL);
        TestTrue(TEXT("Full-body slot actually contributes montage pose"),Anim->GetSlotMontageGlobalWeight(TEXT("FullBodyAction"))>.5f);
    }
    return true;
}
#endif
