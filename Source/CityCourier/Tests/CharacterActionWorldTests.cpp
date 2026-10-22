#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Player/CourierAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Player/CourierActionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterJumpTest,"CityCourier.CharacterActions.World.JumpAndHeadroom",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterJumpTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->SetActorLocation(FVector(0,0,90));
    auto* Move=F.Rider->GetCharacterMovement(); Move->SetMovementMode(MOVE_Walking);
    F.Box(FVector(0,0,200),FVector(100,100,20));
    TestFalse(TEXT("Low ceiling rejects jump"),F.Rider->TryJumpWithFeedback().bSucceeded);
    F.Rider->SetActorLocation(FVector(500,0,90));
    TestTrue(TEXT("Clear ground jump succeeds"),F.Rider->TryJumpWithFeedback().bSucceeded);
    TestTrue(TEXT("Actual physics impulse and falling mode"),Move->Velocity.Z>400 && Move->IsFalling());
    TestFalse(TEXT("Cannot jump again airborne"),F.Rider->TryJumpWithFeedback().bSucceeded);
    F.Controller->SetInterfaceMode(ECourierInterfaceMode::Phone);
    const double Before=Move->Velocity.Z; F.Rider->ClearMovementInput();
    TestEqual(TEXT("Opening and repeatedly clearing UI retains vertical momentum"),Move->Velocity.Z,Before);
    TestFalse(TEXT("UI blocks jump"),F.Rider->TryJumpWithFeedback().bSucceeded);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterActionModesTest,"CityCourier.CharacterActions.World.ModesAndInterruptions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCharacterActionModesTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->DispatchBeginPlay(); auto* A=F.Rider->GetActions();
    // Borrow authored Walk only as a functional timing fixture until attack art is delivered.
    auto* Fixture=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/CityCourier/Art/Characters/A_Walk.A_Walk"));
    for(auto Action:{ECourierAction::LightL,ECourierAction::LightR,ECourierAction::Heavy}) A->ActionAnimations.Add(Action,Fixture);
    F.Rider->GetMesh()->TickAnimation(.01f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    A->QueueAttack(false); A->QueueAttack(true); A->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Same frame heavy takes priority"),A->GetTimeline().GetAction()==ECourierAction::Heavy);
    TestEqual(TEXT("Attack retains ordinary movement speed"),F.Rider->GetCharacterMovement()->MaxWalkSpeed,420.f);
    TestFalse(TEXT("Cannot jump during attack"),F.Rider->TryJumpWithFeedback().bSucceeded);
    TestFalse(TEXT("Cannot interact during attack"),F.Rider->TryInteractWithFeedback().bSucceeded);
    const auto Id=A->GetTimeline().GetInstanceId(); const double Time=A->GetTimeline().GetElapsed();
    F.Controller->SetInterfaceMode(ECourierInterfaceMode::Pause); A->TickComponent(.2f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Pause retains attack ID"),A->GetTimeline().GetInstanceId()==Id);
    TestEqual(TEXT("Pause does not advance action"),A->GetTimeline().GetElapsed(),Time);
    F.Controller->SetInterfaceMode(ECourierInterfaceMode::Gameplay);
    TestTrue(TEXT("Unpause retains action"),A->GetTimeline().IsAttacking());
    F.Controller->SetInterfaceMode(ECourierInterfaceMode::Phone);
    TestTrue(TEXT("Phone cancels strike"),A->GetTimeline().GetAction()==ECourierAction::None);
    A->QueueAttack(false); F.Controller->SetInterfaceMode(ECourierInterfaceMode::Gameplay); A->TickComponent(.02f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("UI clicks cannot become attacks"),A->GetTimeline().GetAction()==ECourierAction::None);
    A->QueueAttack(false); A->TickComponent(.2f,LEVELTICK_All,nullptr);
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    TestTrue(TEXT("Ledge departure cancels attack"),A->GetTimeline().GetAction()==ECourierAction::None);
    TestFalse(TEXT("Cannot mount while airborne"),F.Bike->TryMount(F.Rider));
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    TestFalse(TEXT("Landing immediately blocks action"),A->QueueAttack(false));
    A->TickComponent(.121f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Landing releases at .12s"),A->QueueAttack(false));
    A->TickComponent(.01f,LEVELTICK_All,nullptr);
    F.Controller->SetInterfaceMode(ECourierInterfaceMode::Settlement);
    TestTrue(TEXT("Settlement closes action"),A->GetTimeline().GetAction()==ECourierAction::None);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierMovingPunchTest,"CityCourier.CharacterActions.World.MovingPunchKeepsMomentum",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCourierMovingPunchTest::RunTest(const FString&)
{
    for(bool bHeavy:{false,true})
    {
        FStationFixture F; F.Rider->DispatchBeginPlay();
        auto* Movement=F.Rider->GetCharacterMovement(); Movement->SetMovementMode(MOVE_Walking);
        F.Rider->SetSprinting(true); Movement->Velocity=FVector(700,0,0);
        F.Rider->GetMesh()->TickAnimation(.01f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
        TestTrue(TEXT("Running punch accepts input"),F.Rider->GetActions()->QueueAttack(bHeavy));
        F.Rider->GetActions()->TickComponent(.01f,LEVELTICK_All,nullptr);
        TestEqual(TEXT("Punch does not lower sprint speed limit"),Movement->MaxWalkSpeed,700.f);
        TestTrue(TEXT("Punch does not truncate existing horizontal momentum"),FMath::IsNearlyEqual(Movement->Velocity.Size2D(),700.,.1));
        F.Rider->GetMesh()->TickAnimation(.01f,false);
        if(auto* Anim=Cast<UCourierAnimInstance>(F.Rider->GetMesh()->GetAnimInstance()))
            TestEqual(TEXT("Moving punch retains locomotion legs"),Anim->LocomotionLegWeight,1.f);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierFlightPhasesTest,"CityCourier.CharacterActions.World.OrderedLibraryFlight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCourierFlightPhasesTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->DispatchBeginPlay();
    auto* Movement=F.Rider->GetCharacterMovement();
    auto* Actions=F.Rider->GetActions();
    Movement->SetMovementMode(MOVE_Falling); Actions->JumpStarted();
    auto Check=[&](float Delta,double Vertical,const TCHAR* Expected)
    {
        Movement->Velocity=FVector(420,0,Vertical);
        Actions->TickComponent(Delta,LEVELTICK_All,nullptr);
        auto* Montage=F.Rider->GetMesh()->GetAnimInstance()->GetCurrentActiveMontage();
        if(TestNotNull(TEXT("Flight phase has explicit montage playback"),Montage))
        {
            auto* Clip=Montage->GetFirstAnimReference();
            if(TestNotNull(TEXT("Flight montage references a real library clip"),Clip)) TestEqual(TEXT("Flight follows physical phase order"),Clip->GetName(),FString(Expected));
            TestFalse(TEXT("Flight phase holds its final pose until the next physical phase"),Montage->bEnableAutoBlendOut);
        }
    };
    Check(.05f,400,TEXT("A_JumpStart"));
    Check(.20f,120,TEXT("A_JumpLoop"));
    Check(.10f,-150,TEXT("A_JumpFall"));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierCrouchCrawlTest,"CityCourier.CharacterActions.World.CrouchAndCrawl",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCourierCrouchCrawlTest::RunTest(const FString&)
{
    FStationFixture F; auto* Rider=F.Rider; auto* Movement=Rider->GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Walking);
    const float StandingHalfHeight=Rider->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
    Rider->SetCrouching(true);
    Movement->TickComponent(.016f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Crouch reduces collision capsule height"),Rider->IsCrouched() && Rider->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()<StandingHalfHeight);
    TestEqual(TEXT("Crouched movement uses crawl speed"),Movement->MaxWalkSpeed,100.f);
    Rider->SetCrouching(false);
    Movement->TickComponent(.016f,LEVELTICK_All,nullptr);
    Rider->UpdateActionMovement();
    TestFalse(TEXT("Releasing crouch stands up when clear"),Rider->IsCrouched());
    TestEqual(TEXT("Standing movement speed is restored"),Movement->MaxWalkSpeed,420.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierJumpPunchTest,"CityCourier.CharacterActions.World.JumpPunch",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCourierJumpPunchTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->DispatchBeginPlay(); auto* Actions=F.Rider->GetActions();
    auto* Fixture=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/CityCourier/Art/Characters/A_Walk.A_Walk"));
    Actions->ActionAnimations.Add(ECourierAction::LightL,Fixture);
    F.Rider->GetMesh()->TickAnimation(.01f,false); F.Rider->GetMesh()->RefreshBoneTransforms();
    auto* Movement=F.Rider->GetCharacterMovement(); Movement->SetMovementMode(MOVE_Falling);
    TestTrue(TEXT("Light punch starts while airborne"),Actions->QueueAttack(false));
    TestTrue(TEXT("Jump punch uses the light punch timeline"),Actions->GetTimeline().GetAction()==ECourierAction::LightL);
    TestFalse(TEXT("Heavy punch remains grounded-only"),Actions->QueueAttack(true));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierRecoveryWorldTest,"CityCourier.CharacterActions.World.ConsecutiveHitsUseLibraryRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCourierRecoveryWorldTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->DispatchBeginPlay();
    auto* A=F.Rider->GetActions();
    const FVector Origin=F.Rider->GetActorLocation()+F.Rider->GetActorForwardVector()*100;
    const FGuid First=FGuid::NewGuid();
    TestTrue(TEXT("First real hit is accepted"),A->ReceiveStrike(First,Origin));
    TestFalse(TEXT("Repeated delivery of same strike is ignored"),A->ReceiveStrike(First,Origin));
    TestTrue(TEXT("Second distinct hit is accepted"),A->ReceiveStrike(FGuid::NewGuid(),Origin));
    TestFalse(TEXT("Duplicate did not increment streak"),A->GetTimeline().IsRecovering());
    TestTrue(TEXT("Third distinct hit is accepted"),A->ReceiveStrike(FGuid::NewGuid(),Origin));
    TestTrue(TEXT("Real component starts library fall"),A->GetTimeline().GetAction()==ECourierAction::Knockdown);
    TestTrue(TEXT("Movement is locked during authored recovery"),A->IsMovementLocked());
    TestFalse(TEXT("Hit while down cannot restart fall"),A->ReceiveStrike(FGuid::NewGuid(),Origin));
    A->Cancel();
    TestFalse(TEXT("Cancellation releases recovery lock"),A->IsMovementLocked());
    A->ReceiveStrike(FGuid::NewGuid(),Origin);
    TestFalse(TEXT("New strike after interruption starts a fresh streak"),A->GetTimeline().IsRecovering());
    return true;
}
#endif
