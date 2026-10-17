#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStationChargeTest,"CityCourier.Station.ChargeAndCancel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStationChargeTest::RunTest(const FString&)
{
    FStationFixture F; F.Bike->SetBatteryFraction(.2f);
    F.Controller->OpenStation(F.Station);
    TestTrue(TEXT("Start at station on foot"),F.Station->BeginCharge(F.Rider));
    F.Station->Tick(4.99f); TestEqual(TEXT("Not full before five seconds"),F.Bike->GetBatteryFraction(),.2f);
    F.Station->Tick(.011f); TestEqual(TEXT("Full at five seconds"),F.Bike->GetBatteryFraction(),1.f);
    F.Bike->SetBatteryFraction(.3f); F.Station->BeginCharge(F.Rider); F.Station->Tick(3); F.Station->CancelCharge(); F.Station->Tick(3);
    TestEqual(TEXT("Cancel preserves battery"),F.Bike->GetBatteryFraction(),.3f);
    F.Station->BeginCharge(F.Rider); F.Station->Tick(4); F.Rider->SetActorLocation(F.Station->GetActorLocation()+FVector(250,0,90)); F.Station->Tick(2);
    TestFalse(TEXT("Leaving range cancels"),F.Station->IsCharging()); TestEqual(TEXT("No remote charge"),F.Bike->GetBatteryFraction(),.3f);
    F.Rider->SetActorLocation(F.Station->GetActorLocation()+FVector(0,0,90)); F.Bike->GetScooterMovement()->Velocity=FVector(100,0,0);
    TestFalse(TEXT("Moving vehicle rejected"),F.Station->BeginCharge(F.Rider)); F.Bike->GetScooterMovement()->StopMovementImmediately();
    F.Rider->SetMounted(true); TestFalse(TEXT("Mounted rejected"),F.Station->BeginCharge(F.Rider));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStationUpgradeTest,"CityCourier.Station.UpgradeAndReload",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FStationUpgradeTest::RunTest(const FString&)
{
    FStationFixture F; F.Credit(); F.Bike->SetBatteryFraction(.37f);
    TestTrue(TEXT("Battery purchase"),F.Station->TryPurchase(EUpgradeKind::Battery,F.Rider));
    TestEqual(TEXT("Battery percent unchanged"),F.Bike->GetBatteryFraction(),.37f);
    TestTrue(TEXT("Power purchase"),F.Station->TryPurchase(EUpgradeKind::Power,F.Rider));
    TestTrue(TEXT("Handling purchase"),F.Station->TryPurchase(EUpgradeKind::Handling,F.Rider));
    TestTrue(TEXT("Speed applied"),F.Bike->GetScooterMovement()->MaxForwardSpeedCm>1000.f);
    TestTrue(TEXT("Handling applied"),F.Bike->GetScooterMovement()->HandlingMultiplier>1.f);
    F.Bike->ConsumeTravel(400); TestTrue(TEXT("New 4km capacity applied"),FMath::IsNearlyEqual(F.Bike->GetBatteryFraction(),.27f));
    auto* Reload=NewObject<UProfileSubsystem>(NewObject<UGameInstance>()); Reload->SetStorageForTests(F.Storage); Reload->LoadProfile();
    TestEqual(TEXT("Persisted all upgrades"),Reload->GetProfile().PowerLevel+Reload->GetProfile().HandlingLevel+Reload->GetProfile().BatteryLevel,3);
    F.Rider->SetActorLocation(F.Station->GetActorLocation()+FVector(300,0,90));
    TestFalse(TEXT("Remote purchase rejected"),F.Station->TryPurchase(EUpgradeKind::Battery,F.Rider));
    return true;
}
#endif
