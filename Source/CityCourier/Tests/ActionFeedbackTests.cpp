#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStationParkingDistanceTest,"CityCourier.FunctionalUpgrade.Feedback.ParkingDistance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStationParkingDistanceTest::RunTest(const FString&)
{
    FStationFixture F; F.Credit(); F.Bike->SetBatteryFraction(.2f);
    const FVector Entrance=F.Station->GetInteractionLocation();
    F.Bike->SetActorLocation(Entrance+FVector(1201,0,0));
    TestFalse(TEXT("Cannot charge a distant bike"),F.Station->BeginCharge(F.Rider));
    F.Station->CancelCharge();
    const int64 Balance=F.Profile->GetProfile().BalanceCents;
    TestFalse(TEXT("Cannot upgrade distant bike"),F.Station->TryPurchase(EUpgradeKind::Power,F.Rider));
    TestEqual(TEXT("Rejected purchase preserves balance"),F.Profile->GetProfile().BalanceCents,Balance);
    F.Bike->SetActorLocation(Entrance+FVector(1200,0,0));
    TestTrue(TEXT("12m boundary can charge"),F.Station->BeginCharge(F.Rider));
    F.Station->Tick(4);
    F.Bike->SetActorLocation(Entrance+FVector(1201,0,0)); F.Station->Tick(2);
    TestFalse(TEXT("Moving beyond boundary cancels charge"),F.Station->IsCharging());
    TestEqual(TEXT("Interrupted charge cannot fill battery"),F.Bike->GetBatteryFraction(),.2f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionReasonsTest,"CityCourier.FunctionalUpgrade.Feedback.RejectionReasons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FActionReasonsTest::RunTest(const FString&)
{
    FStationFixture F;
    const auto Poor=F.Station->TryPurchaseWithFeedback(EUpgradeKind::Power,F.Rider);
    TestFalse(TEXT("Insufficient balance rejects"),Poor.bSucceeded);
    TestTrue(TEXT("Insufficient balance explains needed and available"),Poor.Reason.ToString().Contains(TEXT("余额")) && Poor.Reason.ToString().Contains(TEXT("60")));
    F.Credit(); F.Station->TryPurchase(EUpgradeKind::Power,F.Rider); F.Station->TryPurchase(EUpgradeKind::Power,F.Rider);
    const auto Max=F.Station->TryPurchaseWithFeedback(EUpgradeKind::Power,F.Rider);
    TestFalse(TEXT("Maximum level rejects"),Max.bSucceeded);
    TestTrue(TEXT("Max level explained distinctly"),!Max.Reason.IsEmpty() && !Max.Reason.EqualTo(Poor.Reason));
    F.Bike->GetScooterMovement()->Velocity=FVector(100,0,0);
    const auto Moving=F.Station->BeginChargeWithFeedback(F.Rider);
    TestFalse(TEXT("Moving bike rejects"),Moving.bSucceeded); TestFalse(TEXT("Moving reason present"),Moving.Reason.IsEmpty());
    F.Bike->GetScooterMovement()->StopMovementImmediately();
    const FVector Entrance=F.Station->GetInteractionLocation();
    F.Bike->SetActorLocation(Entrance+FVector(3000,0,52));
    F.Rider->SetActorLocation(Entrance+FVector(300,0,90));
    auto Far=F.Rider->TryInteractWithFeedback();
    TestFalse(TEXT("Far interaction denied"),Far.bSucceeded); TestTrue(TEXT("Far prompt explains reach"),Far.Reason.ToString().Contains(TEXT("2米")));
    F.Rider->SetActorLocation(Entrance+FVector(700,0,90));
    auto None=F.Rider->TryInteractWithFeedback(); TestFalse(TEXT("No nearby target denied"),None.bSucceeded);
    TestTrue(TEXT("No target distinguished from distance"),!None.Reason.IsEmpty() && !None.Reason.EqualTo(Far.Reason));
    F.Rider->SetActorLocation(Entrance+FVector(0,0,200)); TestTrue(TEXT("Player 2m allowed"),F.Rider->CanReachInteraction(F.Station,Entrance,Far.Reason));
    F.Rider->SetActorLocation(Entrance+FVector(0,0,201)); TestFalse(TEXT("Player above 2m denied"),F.Rider->CanReachInteraction(F.Station,Entrance,Far.Reason));
    F.Rider->SetActorLocation(FVector(0,-150,90)); F.Bike->SetActorLocation(FVector(0,0,52));
    TestTrue(TEXT("Mount fixture"),F.Bike->TryMount(F.Rider));
    F.Bike->GetScooterMovement()->Velocity=FVector(100,0,0);
    const auto Fast=F.Bike->TryDismountWithFeedback(); TestFalse(TEXT("Fast dismount denied"),Fast.bSucceeded); TestFalse(TEXT("Speed explained"),Fast.Reason.IsEmpty());
    F.Bike->GetScooterMovement()->StopMovementImmediately(); F.Box(FVector(0,-110,100),FVector(100,40,100)); F.Box(FVector(0,110,100),FVector(100,40,100));
    const auto Blocked=F.Bike->TryDismountWithFeedback(); TestFalse(TEXT("Blocked dismount denied"),Blocked.bSucceeded);
    TestTrue(TEXT("Blocked reason differs from speed"),!Blocked.Reason.IsEmpty() && !Blocked.Reason.EqualTo(Fast.Reason));
    TestTrue(TEXT("Denied actions keep mounted rider"),F.Controller->GetPawn()==F.Bike);
    return true;
}
#endif
