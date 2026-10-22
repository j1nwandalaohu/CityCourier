#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Delivery/NavigationGuidance.h"

namespace
{
struct FDeliveryNavigationFixture : FStationFixture
{
    UDeliverySubsystem* Orders;
    UDeliveryNavigationSubsystem* Navigation;
    FDeliveryNavigationFixture()
    {
        World->AddController(Controller);
        City=DuplicateObject<UCityDefinition>(City,World);
        City->Roads.Nodes={{0,FVector::ZeroVector},{1,FVector(10000,0,0)},{2,FVector(10000,10000,0)}};
        City->Roads.Edges={{0,1,100,true},{1,2,100,true}};
        City->Station.Position=FVector::ZeroVector; City->Station.EntranceNode=0;
        for(auto& L:City->Merchants) { L.Position=FVector(10000,0,0); L.EntranceNode=1; }
        for(auto& L:City->Destinations) { L.Position=FVector(10000,10000,0); L.EntranceNode=2; }
        Rider->SetActorLocation(FVector(1000,0,90)); Bike->SetActorLocation(FVector(0,-200,52));
        Orders=NewObject<UDeliverySubsystem>(World); Orders->Configure(City,Profile,Bike);
        Navigation=NewObject<UDeliveryNavigationSubsystem>(World); Navigation->Configure(Orders);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryNavigationLiveDistanceTest,"CityCourier.Navigation.LiveDistanceAndArrival",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeliveryNavigationLiveDistanceTest::RunTest(const FString&)
{
    FDeliveryNavigationFixture F;
    if(!TestFalse(TEXT("Reachable fixture offers"),F.Orders->GetOffers().IsEmpty())) return false;
    if(!TestTrue(TEXT("Accept order"),F.Orders->TryAccept(F.Orders->GetOffers()[0].InstanceId))) return false;
    F.Navigation->RefreshRoute(F.Rider->GetActorLocation());
    const double Before=F.Navigation->GetRemainingMeters();
    F.Navigation->UpdateNavigation(FVector(3000,0,90),.01);
    TestTrue(TEXT("Distance follows rider without waiting for replan timer"),F.Navigation->GetRemainingMeters()<Before-19);
    F.Navigation->UpdateNavigation(FVector(9900,0,90),.01);
    TestTrue(TEXT("Arrival follows rider in the same update"),F.Navigation->HasArrived());
    F.Navigation->UpdateNavigation(FVector(9500,0,90),.01);
    TestFalse(TEXT("Walking away clears arrival immediately"),F.Navigation->HasArrived());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryNavigationNearbyDeviationTest,"CityCourier.Navigation.NearbyDeviationReplans",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeliveryNavigationNearbyDeviationTest::RunTest(const FString&)
{
    FDeliveryNavigationFixture F;
    if(!TestFalse(TEXT("Reachable fixture offers"),F.Orders->GetOffers().IsEmpty())) return false;
    F.Orders->TryAccept(F.Orders->GetOffers()[0].InstanceId);
    const FVector Start=F.Rider->GetActorLocation(),OffRoute(2000,500,90);
    F.Navigation->RefreshRoute(Start);
    F.Navigation->UpdateNavigation(OffRoute,.49);
    TestTrue(TEXT("Expensive replans remain throttled"),F.Navigation->GetActiveRoute().Points[0].Equals(Start));
    F.Navigation->UpdateNavigation(OffRoute,.01);
    TestTrue(TEXT("Five-metre deviation replans from current position"),F.Navigation->GetActiveRoute().bReachable && F.Navigation->GetActiveRoute().Points[0].Equals(OffRoute));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryAcceptanceOriginTest,"CityCourier.DeliveryWorld.AcceptanceUsesCurrentPosition",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeliveryAcceptanceOriginTest::RunTest(const FString&)
{
    FDeliveryNavigationFixture F;
    if(!TestFalse(TEXT("Reachable fixture offers"),F.Orders->GetOffers().IsEmpty())) return false;
    const auto Offered=F.Orders->GetOffers()[0];
    const FVector Current(5000,0,90); F.Rider->SetActorLocation(Current);
    if(!TestTrue(TEXT("Accept after moving"),F.Orders->TryAccept(Offered.InstanceId))) return false;
    const auto& Accepted=F.Orders->GetState().Offer;
    TestTrue(TEXT("Accepted estimate starts at actual rider"),Accepted.EstimateOrigin.Equals(Current));
    TestTrue(TEXT("Pickup route starts at actual rider"),Accepted.PickupRoute.Points[0].Equals(Current));
    TestTrue(TEXT("Moving closer shortens pickup estimate"),Accepted.PickupRoute.LengthMeters<Offered.PickupRoute.LengthMeters-39);
    TestEqual(TEXT("Updated 150.9m journey rounds deadline up to 121 seconds"),Accepted.DeadlineSeconds,121.);
    TestEqual(TEXT("Refreshing estimate preserves accepted identity"),Accepted.InstanceId,Offered.InstanceId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavigationConnectorReturnTest,"CityCourier.Navigation.OverlappingConnectorReturn",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNavigationConnectorReturnTest::RunTest(const FString&)
{
    FDeliveryNavigationFixture F;
    F.Rider->SetActorLocation(FVector(2000,800,90));
    F.Bike->SetActorLocation(FVector(2000,500,52));
    if(!TestTrue(TEXT("Find off-road parked bike"),F.Navigation->SetNavigationMode(ECourierNavigationMode::ParkedBike).bSucceeded)) return false;
    F.Navigation->UpdateNavigation(FVector(2000,0,90),.01);
    F.Navigation->UpdateNavigation(FVector(2000,300,90),.01);
    TestTrue(TEXT("Return connector has only two metres remaining"),F.Navigation->GetRemainingMeters()<2.1);
    const auto Cue=FNavigationGuidance::FromRoute(F.Navigation->GetActiveRoute(),FVector(2000,300,0),0,F.Navigation->GetRouteSegment());
    TestTrue(TEXT("Arrow follows return connector toward bike"),Cue.NextPoint.Equals(F.Bike->GetActorLocation()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryAcceptanceBlockedOriginTest,"CityCourier.DeliveryWorld.AcceptanceChecksNewConnector",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeliveryAcceptanceBlockedOriginTest::RunTest(const FString&)
{
    FDeliveryNavigationFixture F;
    if(!TestFalse(TEXT("Reachable fixture offers"),F.Orders->GetOffers().IsEmpty())) return false;
    const auto Offered=F.Orders->GetOffers()[0];
    F.Rider->SetActorLocation(FVector(5000,-1000,90));
    auto* Wall=F.Box(FVector(5000,-500,100),FVector(50,50,100));
    TestFalse(TEXT("New origin connector blocked despite clear displayed route"),F.Orders->TryAccept(Offered.InstanceId));
    TestFalse(TEXT("Rejected new connector never starts order"),F.Orders->GetState().IsActive());
    FDeliveryOffer Preview;
    TestTrue(TEXT("Rejected acceptance retains displayed identity"),F.Orders->GetOfferPreview(Offered.InstanceId,Preview));
    TestTrue(TEXT("Rejected acceptance leaves displayed estimate unchanged"),Preview.EstimateOrigin.Equals(Offered.EstimateOrigin));
    Wall->Destroy();
    TestTrue(TEXT("Same offer accepts after connector clears"),F.Orders->TryAccept(Offered.InstanceId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeliveryPickupNavigationTest,"CityCourier.Navigation.PickupImmediatelyChangesTarget",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDeliveryPickupNavigationTest::RunTest(const FString&)
{
    FDeliveryNavigationFixture F;
    if(!TestFalse(TEXT("Reachable fixture offers"),F.Orders->GetOffers().IsEmpty())) return false;
    const auto Offered=F.Orders->GetOffers()[0];
    if(!TestTrue(TEXT("Accept order"),F.Orders->TryAccept(Offered.InstanceId))) return false;
    auto* Shop=F.World->SpawnActor<ADeliveryLocation>(FVector(10000,0,0),FRotator::ZeroRotator);
    Shop->LocationId=Offered.MerchantId; Shop->LocationKind=EDeliveryLocationKind::Merchant;
    F.Orders->RegisterLocation(Shop);
    F.Rider->SetActorLocation(FVector(10000,0,90));
    if(!TestTrue(TEXT("Pickup at current target"),Shop->Interact(F.Rider))) return false;
    TestTrue(TEXT("Pickup event replans to recipient before next navigation tick"),F.Navigation->GetActiveRoute().bReachable && F.Navigation->GetActiveRoute().Points.Last().Equals(FVector(10000,10000,0)));
    FText TestReason;
    TestFalse(TEXT("Completed pickup point leaves interaction candidates"),Shop->CanInteract(F.Rider,TestReason));
    return true;
}
#endif
