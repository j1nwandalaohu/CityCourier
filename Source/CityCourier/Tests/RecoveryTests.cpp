#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryHoldTest,"CityCourier.Recovery.HoldPenaltyAndFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryHoldTest::RunTest(const FString&)
{
    FStationFixture F;
    const auto Offer=F.Delivery->GetOffers()[0]; F.Delivery->TryAccept(Offer.InstanceId);
    const auto* Merchant=F.City->FindLocation(Offer.MerchantId);
    auto* Door=F.World->SpawnActor<ADeliveryLocation>(Merchant->Position,FRotator::ZeroRotator); Door->LocationId=Offer.MerchantId;
    F.Rider->SetActorLocation(Merchant->Position+FVector(0,0,90));
    TestTrue(TEXT("Pick up before damage test"),F.Delivery->TryPickup(Offer.MerchantId,F.Rider));
    F.Bike->SetActorLocation(F.Rider->GetActorLocation()+FVector(120,0,-38));
    TestTrue(TEXT("Mount"),F.Bike->TryMount(F.Rider)); F.Bike->SetBatteryFraction(.4f);
    const FTransform Safe(FRotator::ZeroRotator,FVector(0,0,52)); F.Recovery->RecordSafeLocation(Safe);
    F.Bike->OnEffectiveImpact.Broadcast(18);
    const auto Integrity=F.Delivery->GetState().IntegrityPercent;
    TestTrue(TEXT("Actual damaged food before recovery"),Integrity<100);
    F.Bike->SetActorLocation(FVector(1000,0,52)); F.Hold(1.99f);
    TestTrue(TEXT("Short hold no teleport"),F.Bike->GetActorLocation().X>900);
    F.Recovery->SetRecoverHeld(false); F.Hold(1); TestTrue(TEXT("Release resets hold"),F.Bike->GetActorLocation().X>900);
    F.Recovery->TickComponent(1.01f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Recovered to sampled point"),F.Bike->GetActorLocation().Equals(Safe.GetLocation(),1));
    TestEqual(TEXT("Five second penalty"),F.Delivery->GetState().ElapsedSeconds,5.0);
    TestEqual(TEXT("No food repair"),F.Delivery->GetState().IntegrityPercent,Integrity); TestEqual(TEXT("Battery preserved"),F.Bike->GetBatteryFraction(),.4f);
    TestTrue(TEXT("Still mounted"),F.Rider->IsMounted()); TestTrue(TEXT("Stopped"),F.Bike->GetScooterMovement()->Velocity.IsNearlyZero());
    F.Box(FVector(0,0,100),FVector(200,200,100)); F.Bike->SetActorLocation(FVector(5000,0,-2000));
    F.Recovery->SetRecoverHeld(false); F.Hold(2.01f);
    TestTrue(TEXT("Obstructed old point falls back to station"),FVector::Dist2D(F.Bike->GetActorLocation(),F.Station->GetActorLocation())<1500);
    TestTrue(TEXT("Above floor"),F.Bike->GetActorLocation().Z>0);
    F.Now=100+F.Delivery->GetState().Offer.DeadlineSeconds+56;
    F.Recovery->SetRecoverHeld(false); F.Hold(2.01f);
    TestFalse(TEXT("Penalty can expire order immediately"),F.Delivery->GetState().IsActive()); TestEqual(TEXT("One failure"),F.Profile->GetProfile().FailedCount,1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRecoveryRescueTest,"CityCourier.Recovery.RescueAndBelowMap",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FRecoveryRescueTest::RunTest(const FString&)
{
    FStationFixture F; F.Bike->SetBatteryFraction(0);
    TestFalse(TEXT("Near station no rescue"),F.Recovery->TryRescue());
    F.Bike->SetActorLocation(FVector(0,0,52)); F.Bike->SetBatteryFraction(.01f);
    TestFalse(TEXT("Battery remaining no rescue"),F.Recovery->TryRescue()); F.Bike->SetBatteryFraction(0);
    F.Delivery->TryAccept(F.Delivery->GetOffers()[0].InstanceId);
    TestTrue(TEXT("Empty far vehicle rescued"),F.Recovery->TryRescue()); TestEqual(TEXT("Rescue fails once"),F.Profile->GetProfile().FailedCount,1);
    TestEqual(TEXT("Rescue charges fully"),F.Bike->GetBatteryFraction(),1.f); TestFalse(TEXT("Repeated rescue rejected"),F.Recovery->TryRescue());
    F.Bike->SetActorLocation(FVector(0,0,52)); F.Bike->SetBatteryFraction(0);
    TestTrue(TEXT("Rescue without active order"),F.Recovery->TryRescue()); TestEqual(TEXT("No spurious failure"),F.Profile->GetProfile().FailedCount,1);
    F.Rider->SetActorLocation(FVector(0,0,-3000)); F.Hold(2.01f);
    TestTrue(TEXT("On foot recovered above map"),F.Rider->GetActorLocation().Z>80); TestFalse(TEXT("On foot remains on foot"),F.Rider->IsMounted());
    F.Recovery->RecordSafeLocation(FTransform(FVector(20000,20000,52))); // inside a block, far from roads
    F.Bike->SetActorLocation(FVector(0,0,-3000)); F.Recovery->SetRecoverHeld(false); F.Hold(2.01f);
    TestTrue(TEXT("Off-road sample rejected"),FVector::Dist2D(F.Bike->GetActorLocation(),F.Station->GetActorLocation())<1500);
    return true;
}
#endif
