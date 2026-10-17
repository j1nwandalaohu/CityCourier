#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Vehicles/ScooterPawn.h"
#include "Vehicles/ScooterMovementComponent.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "World/DeliveryLocation.h"

struct FScooterFixture : FTestCourierWorld
{
    AScooterPawn* Scooter;
    ACourierCharacter* Rider;
    ACourierController* Controller;
    FScooterFixture()
    {
        Box(FVector(0,0,-50), FVector(100000,100000,50));
        Scooter = World->SpawnActor<AScooterPawn>(FVector(0,0,52), FRotator::ZeroRotator);
        Rider = World->SpawnActor<ACourierCharacter>(FVector(0,-150,90), FRotator::ZeroRotator);
        Controller = World->SpawnActor<ACourierController>(); Controller->Possess(Rider);
    }
    void Step(float Delta, int32 Frames)
    {
        for (int32 I = 0; I < Frames; ++I) Scooter->GetScooterMovement()->TickComponent(Delta, LEVELTICK_All, nullptr);
    }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScooterMountTest, "CityCourier.Scooter.MountAndSafeDismount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FScooterMountTest::RunTest(const FString& Parameters)
{
    FScooterFixture F;
    auto* Order = F.World->SpawnActor<ADeliveryLocation>(FVector(120,-150,90), FRotator::ZeroRotator);
    Order->bActiveOrderTarget = true; F.Rider->RefreshInteraction();
    TestTrue(TEXT("Active order beats nearby scooter"), F.Rider->GetInteractionTarget() == Order);
    Order->bActiveOrderTarget = false; F.Rider->RefreshInteraction();
    TestTrue(TEXT("Scooter beats unrelated location"), F.Rider->GetInteractionTarget() == F.Scooter);
    Order->Destroy();
    TestTrue(TEXT("Mount transfers possession"), F.Scooter->TryMount(F.Rider));
    TestTrue(TEXT("Scooter controlled"), F.Controller->GetPawn() == F.Scooter && F.Rider->IsMounted());
    TestFalse(TEXT("Already mounted refused"), F.Scooter->TryMount(F.Rider));
    F.Scooter->GetScooterMovement()->Velocity = FVector(3.01 / .036,0,0);
    TestFalse(TEXT("3.01kmh cannot dismount"), F.Scooter->TryDismount());
    F.Scooter->GetScooterMovement()->Velocity = FVector(3.0 / .036,0,0);
    TestTrue(TEXT("3kmh can dismount"), F.Scooter->TryDismount());
    TestTrue(TEXT("Rider repossessed"), F.Controller->GetPawn() == F.Rider && !F.Rider->IsMounted());
    F.Scooter->GetScooterMovement()->Velocity = FVector::ZeroVector;
    TestTrue(TEXT("Remount"), F.Scooter->TryMount(F.Rider));
    AActor* Left = F.Box(FVector(0,-110,100), FVector(100,40,100));
    AActor* Right = F.Box(FVector(0,110,100), FVector(100,40,100));
    TestFalse(TEXT("Both sides blocked"), F.Scooter->TryDismount());
    TestTrue(TEXT("Failure preserves possession"), F.Controller->GetPawn() == F.Scooter && F.Rider->IsMounted());
    Right->Destroy();
    TestTrue(TEXT("Uses safe right side"), F.Scooter->TryDismount());
    TestTrue(TEXT("Right candidate selected"), F.Rider->GetActorLocation().Y > 90);
    Left->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScooterDrivingTest, "CityCourier.Scooter.DrivingBatteryImpact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FScooterDrivingTest::RunTest(const FString& Parameters)
{
    double Distances[2]; float Battery[2];
    for (int32 Case = 0; Case < 2; ++Case)
    {
        FScooterFixture F; F.Scooter->TryMount(F.Rider);
        const int32 FPS = Case == 0 ? 30 : 60;
        F.Scooter->SetDriveInput(1,0,false); F.Step(1.f/FPS, FPS * 10);
        Distances[Case] = F.Scooter->GetActorLocation().X; Battery[Case] = F.Scooter->GetBatteryFraction();
        TestTrue(TEXT("Accelerates to base speed cap"), FMath::IsNearlyEqual(F.Scooter->GetSpeedKmh(), 36.f, .1f));
        TestTrue(TEXT("Physical travel uses battery"), Battery[Case] < 1 && Battery[Case] > .9);
        F.Scooter->SetDriveInput(0,0,true); F.Step(1.f/FPS, FPS * 2);
        TestTrue(TEXT("Brake stops vehicle"), F.Scooter->GetSpeedKmh() < .01f);
        F.Scooter->SetDriveInput(-1,0,false); const float BeforeReverse = F.Scooter->GetBatteryFraction(); F.Step(1.f/FPS, FPS * 2);
        TestTrue(TEXT("Reverse speed bounded"), F.Scooter->GetSpeedKmh() <= 6.01f);
        TestTrue(TEXT("Reversing consumes battery"), F.Scooter->GetBatteryFraction() < BeforeReverse);
        F.Scooter->GetScooterMovement()->Velocity = FVector::ZeroVector; F.Scooter->SetBatteryFraction(0);
        const FVector BeforeEmpty = F.Scooter->GetActorLocation(); F.Scooter->SetDriveInput(1,0,false); F.Step(1.f/FPS,FPS);
        TestTrue(TEXT("Empty battery cannot drive"), FVector::Dist2D(BeforeEmpty,F.Scooter->GetActorLocation()) < .01);
        F.Scooter->SetBatteryFraction(.8f); F.Scooter->SetDriveInput(0,0,true);
        F.Scooter->SetActorLocation(FVector(20000,0,52), false, nullptr, ETeleportType::TeleportPhysics); F.Step(1.f/FPS,1);
        TestTrue(TEXT("Teleport is not mileage"), FMath::IsNearlyEqual(F.Scooter->GetBatteryFraction(), .8f));
    }
    TestTrue(TEXT("30/60FPS distance stable"), FMath::Abs(Distances[0]-Distances[1]) < 5);
    TestTrue(TEXT("30/60FPS consumption stable"), FMath::Abs(Battery[0]-Battery[1]) < .0001f);
    AddInfo(FString::Printf(TEXT("10s route: 30FPS %.2fm; 60FPS %.2fm"), Distances[0]/100,Distances[1]/100));
    FScooterFixture F; F.Scooter->TryMount(F.Rider);
    float Impact = 0; F.Scooter->OnEffectiveImpact.AddLambda([&](float Speed) { Impact = FMath::Max(Impact,Speed); });
    F.Box(FVector(2500,0,150), FVector(30,500,150));
    F.Scooter->SetDriveInput(1,0,false); F.Step(1.f/60,600);
    TestTrue(TEXT("Swept vehicle cannot tunnel through wall"), F.Scooter->GetActorLocation().X < 2470);
    TestTrue(TEXT("Front impact reports closing speed"), Impact > 8);
    int32 LowEvents = 0; F.Scooter->OnLowBattery.AddLambda([&]() { ++LowEvents; });
    F.Scooter->SetActorLocation(FVector(0,2000,52), false, nullptr, ETeleportType::TeleportPhysics);
    F.Scooter->GetScooterMovement()->Velocity = FVector::ZeroVector; F.Scooter->SetBatteryFraction(.20001f);
    F.Scooter->SetDriveInput(1,0,false); F.Step(1.f/60,180);
    TestEqual(TEXT("Low battery notification once per threshold crossing"), LowEvents, 1);
    FScooterFixture Glancing; Glancing.Scooter->SetActorRotation(FRotator(0,5,0));
    Glancing.Scooter->TryMount(Glancing.Rider);
    float SideImpact = 0;
    Glancing.Scooter->OnEffectiveImpact.AddLambda([&](float Speed) { SideImpact = FMath::Max(SideImpact,Speed); });
    Glancing.Box(FVector(5000,100,150),FVector(10000,20,150));
    Glancing.Scooter->SetDriveInput(1,0,false); Glancing.Step(1.f/60,480);
    TestTrue(TEXT("Shallow wall scrape uses normal closing speed"),SideImpact > 0 && SideImpact < 8);
    TestTrue(TEXT("Glancing contact slides along wall"),Glancing.Scooter->GetActorLocation().X > 500);
    return true;
}
#endif
