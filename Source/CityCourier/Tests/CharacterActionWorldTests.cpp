#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"
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
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    A->QueueAttack(false); A->QueueAttack(true); A->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Same frame heavy takes priority"),A->GetTimeline().GetAction()==ECourierAction::Heavy);
    TestEqual(TEXT("Attack caps sprint speed"),F.Rider->GetCharacterMovement()->MaxWalkSpeed,120.f);
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
#endif
