#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPositionRoutingTest,"CityCourier.FunctionalUpgrade.Navigation.ArbitraryEndpoints",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPositionRoutingTest::RunTest(const FString&)
{
    auto* C=NewObject<UCityDefinition>();
    C->Roads.Nodes={{0,FVector(0,0,0)},{1,FVector(10000,0,0)},{2,FVector(10000,10000,0)}};
    C->Roads.Edges={{0,1,100,true},{1,2,100,true}};
    auto R=C->FindRouteToPosition(FVector(2000,100,0),FVector(8000,-200,0));
    TestTrue(TEXT("Same edge adds connectors: 1+60+2"),R.bReachable && FMath::IsNearlyEqual(R.LengthMeters,63.0));
    R=C->FindRouteToPosition(FVector(2000,100,0),FVector(10200,7000,0));
    TestTrue(TEXT("Two edges: 1+80+70+2"),R.bReachable && FMath::IsNearlyEqual(R.LengthMeters,153.0));
    C->Roads.Edges[0].bBidirectional=false;
    TestFalse(TEXT("Cannot reverse on one-way edge"),C->FindRouteToPosition(FVector(8000,0,0),FVector(2000,0,0)).bReachable);
    TestTrue(TEXT("Forward on one-way edge"),C->FindRouteToPosition(FVector(2000,0,0),FVector(8000,0,0)).bReachable);
    TestTrue(TEXT("Start at one-way junction"),C->FindRouteToPosition(FVector(0,0,0),FVector(10000,10000,0)).bReachable);
    C->Roads.Nodes.Add({3,FVector(-10000,0,0)}); C->Roads.Nodes.Add({4,FVector(-20000,0,0)}); C->Roads.Edges.Add({3,4,100,true});
    TestFalse(TEXT("Disconnected graph is unreachable"),C->FindRouteToPosition(FVector(-15000,0,0),FVector(5000,0,0)).bReachable);
    TestFalse(TEXT("Nonfinite rejected"),C->FindRouteToPosition(FVector(std::numeric_limits<double>::quiet_NaN(),0,0),FVector::ZeroVector).bReachable);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavigationModesTest,"CityCourier.FunctionalUpgrade.Navigation.TargetModes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNavigationModesTest::RunTest(const FString&)
{
    FStationFixture F; auto* N=NewObject<UDeliveryNavigationSubsystem>(F.World); N->Configure(F.Delivery);
    TestFalse(TEXT("No order cannot restore order guidance"),N->SetNavigationMode(ECourierNavigationMode::Order).bSucceeded);
    TestTrue(TEXT("Station without order"),N->SetNavigationMode(ECourierNavigationMode::Station).bSucceeded);
    TestTrue(TEXT("Station label"),!N->GetTargetLabel().IsEmpty());
    const auto O=F.Delivery->GetOffers()[0]; F.Delivery->TryAccept(O.InstanceId); N->UpdateNavigation(F.Rider->GetActorLocation(),.01);
    TestTrue(TEXT("New acceptance switches to order"),N->GetNavigationMode()==ECourierNavigationMode::Order);
    TestTrue(TEXT("Manual station override"),N->SetNavigationMode(ECourierNavigationMode::Station).bSucceeded);
    const auto* Shop=F.City->FindLocation(O.MerchantId); auto* L=F.World->SpawnActor<ADeliveryLocation>(Shop->Position,FRotator::ZeroRotator); L->LocationId=O.MerchantId;
    F.Rider->SetActorLocation(Shop->Position+FVector(0,0,90)); TestTrue(TEXT("Pickup"),F.Delivery->TryPickup(O.MerchantId,F.Rider)); N->UpdateNavigation(F.Rider->GetActorLocation(),.5);
    TestTrue(TEXT("Pickup keeps station override"),N->GetNavigationMode()==ECourierNavigationMode::Station);
    const FVector Start=FVector(0,-12000,90); F.Rider->SetActorLocation(Start);
    F.Bike->SetActorLocation(FVector(10000,-12000,52));
    TestTrue(TEXT("Find actual parked bike"),N->SetNavigationMode(ECourierNavigationMode::ParkedBike).bSucceeded);
    N->RefreshRoute(Start); const double Before=N->GetRemainingMeters();
    N->UpdateNavigation(FVector(5000,-12000,90),.5);
    TestTrue(TEXT("Remaining distance decreases along route"),Before>0 && N->GetRemainingMeters()<Before-40);
    F.Bike->SetActorLocation(FVector(11000,-12000,52)); N->UpdateNavigation(FVector(5000,-12000,90),.5);
    TestTrue(TEXT("Recovery/movement updates target"),N->GetTargetPosition().Equals(F.Bike->GetActorLocation()));
    F.Rider->SetActorLocation(F.Bike->GetActorLocation()+FVector(0,-150,38)); TestTrue(TEXT("Mount parked bike"),F.Bike->TryMount(F.Rider));
    N->UpdateNavigation(F.Bike->GetActorLocation(),.5);
    TestTrue(TEXT("Mount restores active order"),N->GetNavigationMode()==ECourierNavigationMode::Order);
    TestFalse(TEXT("Already riding cannot find own bike"),N->SetNavigationMode(ECourierNavigationMode::ParkedBike).bSucceeded);
    N->SetNavigationMode(ECourierNavigationMode::Station); F.Delivery->TryCancel(EDeliveryFailure::Cancelled); N->UpdateNavigation(F.Bike->GetActorLocation(),.5);
    TestTrue(TEXT("Terminal order keeps station override"),N->GetNavigationMode()==ECourierNavigationMode::Station);
    return true;
}
#endif
