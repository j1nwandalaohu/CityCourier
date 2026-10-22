#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Player/CourierCharacter.h"
#include "World/DeliveryLocation.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/WorldSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierInteractionTest, "CityCourier.Interaction.DistanceOcclusionPriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourierInteractionTest::RunTest(const FString& Parameters)
{
    FTestCourierWorld Fixture;
    auto* Courier = Fixture.World->SpawnActor<ACourierCharacter>(FVector(0,0,100), FRotator::ZeroRotator);
    auto* Target = Fixture.World->SpawnActor<ADeliveryLocation>(FVector(199,0,100), FRotator::ZeroRotator);
    int32 Interactions = 0;
    Target->OnInteraction.BindLambda([&](ACourierCharacter*) { ++Interactions; return true; });
    TestFalse(TEXT("Unrelated delivery point cannot consume interaction"),Courier->TryInteract());
    TestEqual(TEXT("Unrelated point never calls handler"),Interactions,0);
    Target->bActiveOrderTarget = true;
    TestTrue(TEXT("199cm can interact"), Courier->TryInteract());
    TestEqual(TEXT("Handler called once"), Interactions, 1);
    Target->SetActorLocation(FVector(201,0,100));
    TestFalse(TEXT("201cm denied"), Courier->TryInteract());
    TestEqual(TEXT("Out of reach never invokes handler"), Interactions, 1);
    TestTrue(TEXT("No target clears hint"), Courier->GetInteractionPrompt().IsEmpty());
    Target->SetActorLocation(FVector(199,0,100));
    AActor* Wall = Fixture.Box(FVector(100,0,150), FVector(10,100,150));
    TestFalse(TEXT("Solid wall blocks interaction"), Courier->TryInteract());
    Wall->Destroy();
    auto* Near = Fixture.World->SpawnActor<ADeliveryLocation>(FVector(100,20,100), FRotator::ZeroRotator);
    Near->bActiveOrderTarget = true;
    Near->OnInteraction.BindLambda([](ACourierCharacter*) { return true; });
    Courier->RefreshInteraction();
    TestTrue(TEXT("Equal priority chooses nearest"), Courier->GetInteractionTarget() == Near);
    Target->bActiveOrderTarget = true;
    Near->LocationKind = EDeliveryLocationKind::Station;
    Near->bActiveOrderTarget = false;
    Courier->RefreshInteraction();
    TestTrue(TEXT("Order beats nearer station"), Courier->GetInteractionTarget() == Target);
    Target->bActiveOrderTarget = false;
    Courier->RefreshInteraction();
    TestTrue(TEXT("Station beats other location"), Courier->GetInteractionTarget() == Near);
    Courier->SetMounted(true);
    TestFalse(TEXT("Mounted rider cannot pickup on foot"), Courier->TryInteract());
    Courier->SetMounted(false);
    Near->Destroy(); Target->Destroy(); Courier->RefreshInteraction();
    TestTrue(TEXT("Destroyed targets clear cached prompt"), Courier->GetInteractionPrompt().IsEmpty());
    TestNull(TEXT("No stale weak target"), Courier->GetInteractionTarget());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierMovementTest, "CityCourier.Interaction.WalkingRunningCamera",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourierMovementTest::RunTest(const FString& Parameters)
{
    FTestCourierWorld Fixture;
    Fixture.Box(FVector(0,0,-50), FVector(5000,5000,50));
    auto* Rider = Fixture.World->SpawnActor<ACourierCharacter>(FVector(0,0,90), FRotator::ZeroRotator);
    Rider->GetCharacterMovement()->bRunPhysicsWithNoController = true;
    Fixture.World->InitializeActorsForPlay(FURL()); Fixture.World->BeginPlay();
    // No GameMode or GameInstance is created in this isolated fixture; dispatch the actor begin-play phase explicitly.
    Fixture.World->GetWorldSettings()->NotifyBeginPlay();
    Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    auto Advance = [&]()
    {
        for (int32 Frame = 0; Frame < 60; ++Frame)
        {
            Rider->AddMovementInput(FVector::ForwardVector);
            // Synchronous automation runs within one engine frame. Tick the real movement component
            // directly so UWorld's once-per-frame actor scheduling does not discard steps.
            Rider->GetCharacterMovement()->TickComponent(1.f / 60.f, LEVELTICK_All, nullptr);
        }
    };
    Advance(); const double WalkDistance = Rider->GetActorLocation().X;
    TestTrue(TEXT("Actual character movement walks"), WalkDistance > 300);
    Rider->SetActorLocation(FVector(0,0,90), false, nullptr, ETeleportType::TeleportPhysics);
    Rider->ClearMovementInput(); Rider->SetSprinting(true); Advance();
    const double RunDistance = Rider->GetActorLocation().X;
    TestTrue(TEXT("Sprinting travels materially farther in same time"), RunDistance > WalkDistance * 1.4);
    AddInfo(FString::Printf(TEXT("One-second physics route: walk %.1fcm, run %.1fcm"), WalkDistance, RunDistance));
    Fixture.Box(FVector(1100,0,200), FVector(20,500,200)); Advance(); Advance();
    TestTrue(TEXT("Character cannot pass solid wall"), Rider->GetActorLocation().X < 1080);
    Rider->SetActorLocation(FVector(0,0,90), false, nullptr, ETeleportType::TeleportPhysics); Rider->ClearMovementInput();
    Rider->CameraBoom->bEnableCameraLag = false;
    Fixture.Box(FVector(-120,0,180), FVector(10,300,180));
    Rider->CameraBoom->TickComponent(1.f/60.f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Camera retracts in front of wall behind rider"), Rider->FollowCamera->GetComponentLocation().X > -110);
    return true;
}
#endif
