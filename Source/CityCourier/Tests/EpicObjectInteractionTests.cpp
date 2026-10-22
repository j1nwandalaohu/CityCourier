#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierActionComponent.h"
#include "Player/Epic/CourierObjectInteractionComponent.h"
#include "World/CourierGrabbable.h"
#include "World/CourierInteractionButton.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

struct FEpicInteractionFixture : FTestCourierWorld
{
    ACourierCharacter* Rider;
    FEpicInteractionFixture()
    {
        Box(FVector(0,0,-50),FVector(4000,4000,50));
        Rider=World->SpawnActor<ACourierCharacter>(FVector(0,0,90),FRotator::ZeroRotator);
        World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
        Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Rider->GetActions()->TickComponent(.3f,LEVELTICK_All,nullptr);
        Rider->GetMesh()->TickAnimation(.01f,false); Rider->GetMesh()->RefreshBoneTransforms();
    }
    ACourierGrabbable* Crate()
    { return World->SpawnActor<ACourierGrabbable>(FVector(100,0,70),FRotator::ZeroRotator); }
    ACourierInteractionButton* Button()
    {
        const FVector Hand=Rider->GetMesh()->GetSocketLocation(TEXT("hand_r"));
        auto* Result=World->SpawnActor<ACourierInteractionButton>(Hand+FVector(8,0,0),FRotator::ZeroRotator);
        Result->HandContact->SetWorldLocation(Hand); return Result;
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicGrabReleaseTest,"CityCourier.EpicInteraction.GrabRelease",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicGrabReleaseTest::RunTest(const FString&)
{
    FEpicInteractionFixture F; auto* Crate=F.Crate(); auto* Interaction=F.Rider->GetObjectInteraction();
    Crate->Body->SetMassOverrideInKg(NAME_None,20,true);
    const auto Response=Crate->Body->GetCollisionResponseToChannel(ECC_Pawn);
    TestTrue(TEXT("Twenty kg boundary accepted through E"),F.Rider->TryInteract());
    TestEqual(TEXT("Physics handle holds the actual component"),F.Rider->FindComponentByClass<UPhysicsHandleComponent>()->GetGrabbedComponent(),static_cast<UPrimitiveComponent*>(Crate->Body.Get()));
    TestTrue(TEXT("Holding state"),Interaction->IsHolding());
    TestTrue(TEXT("E releases the object"),F.Rider->TryInteract());
    TestFalse(TEXT("Released state"),Interaction->IsBusy());
    TestEqual(TEXT("Original pawn response restored"),Crate->Body->GetCollisionResponseToChannel(ECC_Pawn),Response);
    TestTrue(TEXT("Object remains physically simulated"),Crate->Body->IsSimulatingPhysics());
    Crate->Body->SetMassOverrideInKg(NAME_None,20.01f,true);
    TestFalse(TEXT("Twenty point zero one kg rejected"),Interaction->TryGrab(Crate->Body).bSucceeded);
    auto* Ordinary=F.Box(FVector(80,150,70),FVector(10));
    TestFalse(TEXT("Arbitrary scene primitives are not grabbable"),Interaction->TryGrab(Cast<UPrimitiveComponent>(Ordinary->GetRootComponent())).bSucceeded);
    TestTrue(TEXT("Optimized building query sees registered object"),F.World->GetSubsystem<UCourierBuildingSubsystem>()->GetInteractionCandidates(F.Rider->GetActorLocation()).Contains(Crate));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicButtonContactTest,"CityCourier.EpicInteraction.ContactRevalidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicButtonContactTest::RunTest(const FString&)
{
    FEpicInteractionFixture F; auto* Button=F.Button(); auto* Interaction=F.Rider->GetObjectInteraction();
    TestTrue(TEXT("Reachable contact accepted"),Interaction->TryPress(Button).bSucceeded);
    TestEqual(TEXT("No activation at input time"),Button->GetPressCount(),0);
    Interaction->TickComponent(.2f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("No activation before contact"),Button->GetPressCount(),0);
    Interaction->TickComponent(.06f,LEVELTICK_All,nullptr);
    Interaction->TickComponent(.06f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Contact submits exactly once"),Button->GetPressCount(),1);
    Interaction->Cancel();
    auto* Obscured=F.Button();
    TestTrue(TEXT("Second contact initially accepted"),Interaction->TryPress(Obscured).bSucceeded);
    const FVector Mid=(F.Rider->GetPawnViewLocation()+Obscured->GetInteractionLocation())*.5;
    F.Box(Mid,FVector(8));
    Interaction->TickComponent(.3f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Occlusion before contact prevents submission"),Obscured->GetPressCount(),0);
    Interaction->Cancel();
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEpicInteractionCancelTest,"CityCourier.EpicInteraction.CancellationRestoresObject",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEpicInteractionCancelTest::RunTest(const FString&)
{
    FEpicInteractionFixture F; auto* Crate=F.Crate(); auto* Interaction=F.Rider->GetObjectInteraction();
    TestTrue(TEXT("Grab before interruption"),Interaction->TryGrab(Crate->Body).bSucceeded);
    F.Rider->SetMounted(true);
    TestFalse(TEXT("Mount releases object immediately"),Interaction->IsBusy());
    TestNull(TEXT("No lingering constraint"),F.Rider->FindComponentByClass<UPhysicsHandleComponent>()->GetGrabbedComponent());
    TestTrue(TEXT("Physics survives interruption"),Crate->Body->IsSimulatingPhysics());
    F.Rider->SetMounted(false);
    auto* Button=F.Button();
    TestTrue(TEXT("Press before target destruction"),Interaction->TryPress(Button).bSucceeded);
    Button->Destroy(); Interaction->TickComponent(.3f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Destroyed target receives zero submissions"),Button->GetPressCount(),0);
    TestFalse(TEXT("Destroyed target releases state"),Interaction->IsBusy());
    return true;
}
#endif
