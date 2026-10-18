#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Delivery/DeliveryNavigationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOfferPreviewTest,"CityCourier.FunctionalUpgrade.Offers.EstimatesAndIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FOfferPreviewTest::RunTest(const FString&)
{
    FStationFixture F;
    auto* City=DuplicateObject<UCityDefinition>(F.City,F.World);
    City->Roads.Nodes={{0,FVector(0,0,0)},{1,FVector(10000,0,0)},{2,FVector(30000,0,0)}};
    City->Roads.Edges={{0,1,100,true},{1,2,200,true}};
    City->Station.Position=FVector::ZeroVector; City->Station.EntranceNode=0;
    for(auto& L:City->Merchants) { L.Position=FVector(10000,100,0); L.EntranceNode=1; }
    for(auto& L:City->Destinations) { L.Position=FVector(30000,100,0); L.EntranceNode=2; }
    auto* D=NewObject<UDeliverySubsystem>(F.World); TestTrue(TEXT("Fixture configures"),D->Configure(City,F.Profile,F.Bike));
    D->RefreshOffers(FVector::ZeroVector);
    const auto O=D->GetOffers()[0];
    TestTrue(TEXT("Estimate stores origin"),O.EstimateOrigin.IsNearlyZero());
    TestTrue(TEXT("Pickup route includes 100m road and 1m entrance"),O.PickupRoute.bReachable && FMath::IsNearlyEqual(O.PickupRoute.LengthMeters,101.0));
    TestTrue(TEXT("Delivery route includes both entrance connectors"),O.DeliveryRoute.bReachable && FMath::IsNearlyEqual(O.DeliveryRoute.LengthMeters,202.0));
    TestEqual(TEXT("303m route deadline ceil 90+303/5"),O.DeadlineSeconds,151.0);
    auto* Nav=NewObject<UDeliveryNavigationSubsystem>(F.World); Nav->Configure(D);
    FDeliveryOffer Preview; TestTrue(TEXT("Preview current candidate"),D->GetOfferPreview(O.InstanceId,Preview));
    TestEqual(TEXT("Preview same ID"),Preview.InstanceId,O.InstanceId);
    TestFalse(TEXT("Preview does not accept"),D->GetState().IsActive()); TestFalse(TEXT("Preview does not set navigation"),Nav->GetActiveRoute().bReachable);
    D->RefreshOffers(FVector(5000,0,0));
    TestFalse(TEXT("Stale preview rejected"),D->GetOfferPreview(O.InstanceId,Preview));
    TestFalse(TEXT("Stale accept rejected"),D->TryAccept(O.InstanceId));
    const auto Current=D->GetOffers()[0]; TestTrue(TEXT("New ID accepts"),D->TryAccept(Current.InstanceId));
    TestEqual(TEXT("Fee remains displayed fee"),D->GetState().Offer.BaseFeeCents,Current.BaseFeeCents);
    TestEqual(TEXT("Deadline remains displayed deadline"),D->GetDeadlineSeconds(),Current.DeadlineSeconds);
    TestFalse(TEXT("Active order rejects refresh"),D->RefreshOffers(FVector::ZeroVector));
    TestFalse(TEXT("Active order rejects candidate preview"),D->GetOfferPreview(Current.InstanceId,Preview));
    return true;
}
#endif
