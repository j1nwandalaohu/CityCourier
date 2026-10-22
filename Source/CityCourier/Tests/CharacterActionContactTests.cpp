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
        Rider->GetMesh()->TickAnimation(.01f,false); Rider->GetMesh()->RefreshBoneTransforms();
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
    auto* A=F.Rider->GetActions(); TestTrue(TEXT("Trajectory fixture admits attack"),A->QueueAttack(false)); A->TickComponent(.30f,LEVELTICK_All,nullptr);
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
    // Directional standing recoil is a separate strike after the combo gap.
    // A third hit inside 1.2 seconds intentionally enters knockdown now.
    A->TickComponent(1.201f,LEVELTICK_All,nullptr);
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
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Step=[&](float Time){ ++GFrameCounter; Mesh->TickAnimation(Time,false); Mesh->RefreshBoneTransforms(); };
    // A visibly different authored pose exposes a native locomotion evaluator
    // bypassing the montage even when slot weights report successful playback.
    auto* PoseFixture=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/CityCourier/Art/Characters/A_Ride.A_Ride"));
    F.Rider->GetActions()->ActionAnimations.Add(ECourierAction::LightL,PoseFixture);
    TestTrue(TEXT("Named left fist attachment persists"),Mesh->DoesSocketExist(TEXT("PunchTrace_L")));
    TestTrue(TEXT("Named right fist attachment persists"),Mesh->DoesSocketExist(TEXT("PunchTrace_R")));
    Step(.1f);
    TestTrue(TEXT("Imported rig scaling keeps fist socket five centimeters from the hand"),FMath::IsNearlyEqual(FVector::Distance(Mesh->GetSocketLocation(TEXT("PunchTrace_L")),Mesh->GetBoneLocation(TEXT("hand_l"))),5.,.1));
    // Let the 0.10s attack blend finish, while still sampling contact at 0.20s.
    F.Rider->GetActions()->QueueAttack(false); F.Rider->GetActions()->TickComponent(.09f,LEVELTICK_All,nullptr);
    Step(.11f);
    auto* Anim=Cast<UCourierAnimInstance>(Mesh->GetAnimInstance());
    if(TestNotNull(TEXT("Native action animation bridge"),Anim))
    {
        TestTrue(TEXT("Action state available to animation"),Anim->CurrentAction==ECourierAction::LightL);
        TestTrue(TEXT("Full-body slot actually contributes montage pose"),Anim->GetSlotMontageGlobalWeight(TEXT("FullBodyAction"))>.5f);
        FVector Expected;
        TestTrue(TEXT("Authored montage fist can be sampled"),FCourierActionAnimation::SampleFist(Mesh,PoseFixture,true,.2,Expected));
        TestTrue(TEXT("Rendered full-body pose matches the montage rather than locomotion"),FVector::Distance(Mesh->GetSocketLocation(TEXT("PunchTrace_L")),Expected)<1.f);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterCancelledSliceTest,"CityCourier.CharacterActions.Review.InterruptDuringContact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterCancelledSliceTest::RunTest(const FString&)
{
    for(int Mode=0;Mode<3;++Mode)
    {
        FActionContactFixture F; auto* A=F.Rider->GetActions();
        auto* First=F.World->SpawnActor<ACourierTrainingTarget>(F.Fist(),FRotator::ZeroRotator);
        auto* Second=F.World->SpawnActor<ACourierTrainingTarget>(F.Fist(),FRotator::ZeroRotator);
        A->OnStrike.AddLambda([&](AActor*,FGuid,ECourierAction)
        {
            if(Mode==0) A->Cancel();
            else if(Mode==1) F.Controller->SetInterfaceMode(ECourierInterfaceMode::Phone);
            else F.Rider->Destroy();
        });
        A->QueueAttack(false); A->TickComponent(.3f,LEVELTICK_All,nullptr);
        TestEqual(FString::Printf(TEXT("Interruption %d stops local hit/substep loops immediately"),Mode),First->GetHitCount()+Second->GetHitCount(),1);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterThinWallTest,"CityCourier.CharacterActions.Review.InitialOverlapThinWall",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterThinWallTest::RunTest(const FString&)
{
    FActionContactFixture F;
    const FVector Start=F.Fist(.14);
    auto* Target=F.World->SpawnActor<ACourierTrainingTarget>(Start+FVector(10,0,0),FRotator::ZeroRotator);
    Target->Body->SetBoxExtent(FVector(2,2,5));
    auto* Wall=F.Box(Start+FVector(6,0,0),FVector(1,100,100));
    FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(ThinWallFixture),false,F.Rider);
    TestFalse(TEXT("Fist center is still on player side of wall"),F.World->LineTraceSingleByChannel(Hit,FVector(0,0,Start.Z),Start,ECC_Visibility,Query));
    TArray<FHitResult> Contacts;
    F.World->SweepMultiByChannel(Contacts,Start,Start+FVector(.01,0,0),FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(12),Query);
    TestTrue(TEXT("Wall initially overlaps sphere"),Contacts.ContainsByPredicate([&](const FHitResult& H){return H.GetActor()==Wall && H.Time==0;}));
    TestTrue(TEXT("Target also initially overlaps sphere"),Contacts.ContainsByPredicate([&](const FHitResult& H){return H.GetActor()==Target && H.Time==0;}));
    F.Rider->GetActions()->QueueAttack(false); F.Rider->GetActions()->TickComponent(.15f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Initial blocking wall wins over tied receiver overlap"),Target->GetHitCount(),0);
    TArray<ACourierTrainingTarget*> Targets{Target};
    for(int Extra=0;Extra<32;++Extra)
    {
        auto* Other=F.World->SpawnActor<ACourierTrainingTarget>(Start+FVector(10,0,0),FRotator::ZeroRotator);
        Other->Body->SetBoxExtent(FVector(2,2,5)); Targets.Add(Other);
        F.Rider->GetActions()->Cancel(); TestTrue(TEXT("Straddling fixture admits real strike"),F.Rider->GetActions()->QueueAttack(false));
        F.Rider->GetActions()->TickComponent(.15f,LEVELTICK_All,nullptr);
        int Count=0; for(auto* Item:Targets) Count+=Item->GetHitCount();
        TestEqual(TEXT("Blocker priority survives varying equal-time overlap list sizes"),Count,0);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterPlaybackFailureTest,"CityCourier.CharacterActions.Review.PlaybackFailure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterPlaybackFailureTest::RunTest(const FString&)
{
    for(int Mode=0;Mode<3;++Mode)
    {
        FActionContactFixture F; auto* A=F.Rider->GetActions(); auto* Mesh=F.Rider->GetMesh();
        auto* Target=F.World->SpawnActor<ACourierTrainingTarget>(F.Fist(),FRotator::ZeroRotator);
        Mesh->TickAnimation(.01f,false); Mesh->RefreshBoneTransforms();
        if(Mode==0) Mesh->SetAnimInstanceClass(nullptr);
        else if(Mode==1) Mesh->SetAnimInstanceClass(UAnimInstance::StaticClass()); // Instance has no action slot graph.
        const bool bAccepted=A->QueueAttack(false);
        if(Mode<2) TestFalse(TEXT("Unavailable playback cannot admit attack"),bAccepted);
        else
        {
            TestTrue(TEXT("Valid graph admits windup"),bAccepted); A->TickComponent(.05f,LEVELTICK_All,nullptr);
            Mesh->GetAnimInstance()->Montage_Stop(0.f);
        }
        A->TickComponent(.3f,LEVELTICK_All,nullptr);
        TestEqual(FString::Printf(TEXT("Playback failure %d cannot produce invisible contact"),Mode),Target->GetHitCount(),0);
        TestTrue(TEXT("Playback failure releases action lock"),A->GetTimeline().GetAction()==ECourierAction::None);
    }
    return true;
}
#endif
