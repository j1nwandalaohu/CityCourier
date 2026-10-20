#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Player/CourierActionComponent.h"
#include "Player/CourierAnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputMappingContext.h"
#include "InputAction.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProneHeadroomTest,"CityCourier.CharacterActions.Posture.ProneHeadroomAndActions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProneHeadroomTest::RunTest(const FString&)
{
    FStationFixture F; auto* Rider=F.Rider; auto* Movement=Rider->GetCharacterMovement();
    Rider->SetActorLocation(FVector(0,0,90)); Movement->SetMovementMode(MOVE_Walking);
    Movement->TickComponent(.016f,LEVELTICK_All,nullptr);
    if(!TestTrue(TEXT("Clear space admits prone"),Rider->SetProne(true))) return false;
    TestTrue(TEXT("Prone collision fits below 70cm"),Rider->IsProne() && Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()<=35.f);
    Rider->SetSprinting(true); Movement->Velocity=FVector(700,0,0); Rider->UpdateActionMovement();
    TestTrue(TEXT("Sprint cannot override prone movement limit"),Movement->GetMaxSpeed()<=65.f && Movement->Velocity.Size2D()<=65.f);
    TestFalse(TEXT("Prone cannot jump"),Rider->TryJumpWithFeedback().bSucceeded);
    TestFalse(TEXT("Prone cannot punch"),Rider->GetActions()->QueueAttack(false));
    TestFalse(TEXT("Prone cannot mount a scooter"),F.Bike->TryMount(Rider));
    Rider->SetSprinting(false);
    auto* Ceiling=F.Box(FVector(0,0,125),FVector(200,200,40));
    TestFalse(TEXT("Releasing below ceiling cannot stand through it"),Rider->SetProne(false));
    TestTrue(TEXT("Blocked stand retains visible prone state"),Rider->IsProne());
    Ceiling->Destroy();
    TestTrue(TEXT("Standing succeeds when ceiling clears"),Rider->SetProne(false));
    TestFalse(TEXT("Standing clears prone state"),Rider->IsProne());
    TestTrue(TEXT("Standing restores full collision"),Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()>80.f);
    TestEqual(TEXT("Ordinary speed restored"),Movement->MaxWalkSpeed,420.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProneFootprintTest,"CityCourier.CharacterActions.Posture.FullBodyClearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProneFootprintTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->SetActorLocation(FVector(0,0,90));
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    F.Box(FVector(75,0,40),FVector(8,50,40));
    TestFalse(TEXT("Lying down requires room for head and feet beyond torso capsule"),F.Rider->SetProne(true));
    TestFalse(TEXT("Rejected prone does not change visible posture"),F.Rider->IsProne());
    F.Rider->SetInputSuppressed(true);
    TestFalse(TEXT("UI cannot enter prone"),F.Rider->SetProne(true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProneInterfaceTest,"CityCourier.CharacterActions.Posture.InterfaceTransitions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProneInterfaceTest::RunTest(const FString&)
{
    FStationFixture F; auto* Rider=F.Rider; Rider->SetActorLocation(FVector(0,0,90));
    auto* Movement=Rider->GetCharacterMovement(); Movement->SetMovementMode(MOVE_Walking); Movement->TickComponent(.016f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Enter prone"),Rider->SetProne(true));
    Rider->SetInputSuppressed(true,true);
    TestTrue(TEXT("Pause preserves prone pose"),Rider->IsProne());
    TestFalse(TEXT("Suppressed input cannot latch another prone press"),Rider->SetProne(true));
    Rider->SetInputSuppressed(false);
    auto* Ceiling=F.Box(FVector(0,0,125),FVector(200,200,40));
    Rider->SetInputSuppressed(true);
    TestTrue(TEXT("Opening UI below ceiling retains physical prone"),Rider->IsProne());
    TestFalse(TEXT("UI press cannot keep prone held"),Rider->SetProne(true));
    Ceiling->Destroy(); Rider->SetInputSuppressed(false); Rider->Tick(.016f);
    TestFalse(TEXT("Cleared UI request stands when headroom returns"),Rider->IsProne());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProneRenderedPoseTest,"CityCourier.CharacterActions.Posture.RenderedCrawl",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProneRenderedPoseTest::RunTest(const FString&)
{
    FStationFixture F;
    auto* Class=LoadClass<ACourierCharacter>(nullptr,TEXT("/Game/CityCourier/Blueprints/BP_CourierCharacter.BP_CourierCharacter_C"));
    auto* Rider=F.World->SpawnActor<ACourierCharacter>(Class,FVector(0,0,90),FRotator::ZeroRotator);
    F.Controller->Possess(Rider); F.Rider->Destroy(); Rider->DispatchBeginPlay();
    auto* Movement=Rider->GetCharacterMovement(); Movement->SetMovementMode(MOVE_Walking);
    auto* Mesh=Rider->GetMesh(); auto* Anim=Cast<UCourierAnimInstance>(Mesh->GetAnimInstance());
    if(!TestNotNull(TEXT("Production animation instance"),Anim)) return false;
    TGuardValue<uint64> FrameGuard(GFrameCounter,GFrameCounter);
    auto Step=[&](float Delta){Rider->Tick(Delta); ++GFrameCounter; Mesh->TickAnimation(Delta,false); Mesh->RefreshBoneTransforms();};
    TestTrue(TEXT("Production rider enters prone"),Rider->SetProne(true));
    Step(.25f); Step(.25f);
    const float Floor=Rider->GetActorLocation().Z-Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float HeadHeight=Mesh->GetSocketLocation(TEXT("head")).Z-Floor;
    AddInfo(FString::Printf(TEXT("Prone actor=%s capsule=%.2f mesh=%s offset=%s head=%s floor=%.2f index=%d"),
        *Rider->GetActorLocation().ToString(),Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
        *Mesh->GetRelativeLocation().ToString(),*Rider->GetBaseTranslationOffset().ToString(),
        *Mesh->GetSocketLocation(TEXT("head")).ToString(),Floor,Anim->AnimationIndex));
    TestTrue(TEXT("Actual rendered head stays above the floor in prone"),HeadHeight>5.f && HeadHeight<60.f);
    TestEqual(TEXT("Prone idle selected"),Anim->AnimationIndex,6);
    Movement->Velocity=FVector(60,0,0); Step(.25f); Step(.25f);
    TestEqual(TEXT("Moving prone uses crawl clip"),Anim->AnimationIndex,7);
    const FVector Hand=Mesh->GetSocketLocation(TEXT("hand_l")); Step(.3f);
    TestTrue(TEXT("Authored crawl moves limbs while body is prone"),FVector::Distance(Hand,Mesh->GetSocketLocation(TEXT("hand_l")))>1.f);
    Movement->Velocity=FVector::ZeroVector; Rider->SetProne(false); Rider->SetCrouching(true);
    Movement->TickComponent(.016f,LEVELTICK_All,nullptr); Step(.25f); Step(.25f);
    const float CrouchFloor=Rider->GetActorLocation().Z-Rider->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float CrouchHead=Mesh->GetSocketLocation(TEXT("head")).Z-CrouchFloor;
    TestTrue(TEXT("Crouch visual also stays above floor"),CrouchHead>65.f && CrouchHead<120.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProneInputMappingTest,"CityCourier.CharacterActions.Posture.ShiftMapping",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProneInputMappingTest::RunTest(const FString&)
{
    auto* Context=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/CityCourier/Input/IMC_Courier.IMC_Courier"));
    if(!TestNotNull(TEXT("Production input context"),Context)) return false;
    bool Prone=false,Sprint=false,ConflictingSprint=false;
    for(const auto& Mapping:Context->GetMappings())
    {
        if(!Mapping.Action) continue;
        Prone|=Mapping.Key==EKeys::LeftShift && Mapping.Action->GetName()==TEXT("IA_Prone");
        Sprint|=Mapping.Key==EKeys::RightShift && Mapping.Action->GetName()==TEXT("IA_Sprint");
        ConflictingSprint|=Mapping.Key==EKeys::LeftShift && Mapping.Action->GetName()==TEXT("IA_Sprint");
    }
    TestTrue(TEXT("Left Shift is prone"),Prone);
    TestTrue(TEXT("Right Shift is sprint"),Sprint);
    TestFalse(TEXT("Left Shift cannot trigger sprint simultaneously"),ConflictingSprint);
    return true;
}
#endif
