#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/CityDefinition.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "Buildings/CourierBuildingActor.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "Tests/StationTestSupport.h"
#include "Delivery/DeliveryNavigationSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingJourneyTest,"CityCourier.Buildings.Journey.ComposedFloorAndRoomRoutes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingJourneyTest::RunTest(const FString&)
{
    auto* City=LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
    auto* C=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
    const auto& B=C->Buildings[C->FindBuilding(TEXT("B_Merchant_0"))];
    const auto& Low=B.Rooms[B.FindRoom(TEXT("B_Merchant_0_F01_A01"))];
    const auto& High=B.Rooms[B.FindRoom(TEXT("B_Merchant_0_F04_A01"))];
    const FVector A=B.ToWorld(Low.StandingPoint),Z=B.ToWorld(High.StandingPoint);
    auto Route=City->FindJourneyToPosition(A+FVector(0,0,90),Z,*C);
    if(!TestTrue(TEXT("Same XY different floors requires a real reachable staircase"),Route.bReachable)) return false;
    TestTrue(TEXT("Stairs contribute length"),Route.StairMeters>0);
    TestEqual(TEXT("Same-building trip avoids exterior road detour"),Route.RoadMeters,0.0);
    TestTrue(TEXT("Closed but openable doors remain in logical route"),Route.RequiredDoors.Num()>=2);
    TestEqual(TEXT("Target floor preserved"),Route.TargetFloor,High.Floor);
    TestEqual(TEXT("Segment metadata complete"),Route.SegmentKinds.Num(),Route.Points.Num()-1);
    TestEqual(TEXT("Mutually exclusive lengths"),Route.LengthMeters,Route.RoadMeters+Route.IndoorFlatMeters+Route.StairMeters);
    Route=City->FindJourneyFrom(Z+FVector(0,0,90),City->Destinations[0].LocationId,*C);
    TestTrue(TEXT("Different building uses source exit, roads, target rooms"),Route.bReachable && Route.RoadMeters>0 && Route.StairMeters>0 && Route.RequiredDoors.Num()>=4);
    Route=City->FindJourneyToPosition(Z+FVector(0,0,90),City->Station.Position+FVector(500,0,0),*C);
    TestTrue(TEXT("FindBike upstairs first exits via stairs"),Route.bReachable && Route.StairMeters>0 && Route.RequiredDoors.Num()>=2);
    auto* Broken=DuplicateObject<UCityBuildingCatalog>(C,GetTransientPackage());
    Broken->Buildings[Broken->FindBuilding(B.Id)].Edges.Reset();
    TestFalse(TEXT("Missing actual connection rejects journey"),City->FindJourneyToPosition(Z,A,*Broken).bReachable);
    TestFalse(TEXT("Missing required address cannot silently fall back to sidewalk"),City->FindJourneyFrom(A,TEXT("missing"),*C).bReachable);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingNavigationTest,"CityCourier.Buildings.Journey.FloorAwareNavigationAndPreviewSnapshot",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingNavigationTest::RunTest(const FString&)
{
    FStationFixture F;
    F.World->AddController(F.Controller);
    TestEqual(TEXT("Navigation fixture registers the real possessed controller"),F.World->GetFirstPlayerController(),static_cast<APlayerController*>(F.Controller));
    auto* Source=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
    auto* C=NewObject<UCityBuildingCatalog>();
    for(FName Id:{F.City->Station.LocationId,F.City->Merchants[0].LocationId,F.City->Destinations[0].LocationId})
    {
        const auto* Binding=Source->FindBinding(Id); C->Bindings.Add(*Binding);
        if(C->FindBuilding(Binding->BuildingId)==INDEX_NONE)
        { C->Buildings.Add(Source->Buildings[Source->FindBuilding(Binding->BuildingId)]); C->ExpectedBuildingIds.Add(Binding->BuildingId); }
    }
    for(int32 I=0;I<C->Buildings.Num();++I)
    { const auto& B=C->Buildings[I]; auto* A=F.World->SpawnActor<ACourierBuildingActor>(B.Origin,FRotator(0,B.Yaw,0)); TestTrue(TEXT("Fixture structure"),A->ConfigureBuilding(C,I)); }
    auto* Interiors=F.World->GetSubsystem<UCourierBuildingSubsystem>();
    if(!TestTrue(TEXT("Actual buildings registered"),Interiors->Configure(C))) return false;
    const auto& B=C->Buildings[C->FindBuilding(TEXT("B_Merchant_0"))];
    const auto& High=B.Rooms[B.FindRoom(TEXT("B_Merchant_0_F04_A01"))];
    const FVector Upstairs=B.ToWorld(High.StandingPoint)+FVector(0,0,90); F.Rider->SetActorLocation(Upstairs);
    const auto& Station=C->Buildings[C->FindBuilding(TEXT("B_Station"))];
    F.Bike->SetActorLocation(Station.ToWorld(Station.Nodes[Station.EntranceNode].Position)+FVector(300,0,52));
    auto* Delivery=F.World->GetSubsystem<UDeliverySubsystem>(); Delivery->SetClockForTests([&F](){return F.Now;});
    Delivery->Configure(F.City,F.Profile,F.Bike);
    if(!TestTrue(TEXT("At least one complete fixture offer"),Delivery->GetOffers().Num()>0)) return false;
    const auto Offer=Delivery->GetOffers()[0]; FDeliveryOffer Preview;
    TestTrue(TEXT("Complete snapshot exposed before accepting"),Delivery->GetOfferPreview(Offer.InstanceId,Preview));
    TestTrue(TEXT("Snapshot includes current stairs and required doors"),Preview.PickupRoute.StairMeters>0 && Preview.PickupRoute.RequiredDoors.Num()>0);
    auto* Nav=F.World->GetSubsystem<UDeliveryNavigationSubsystem>(); Nav->Configure(Delivery);
    TestTrue(TEXT("Upstairs FindBike succeeds"),Nav->SetNavigationMode(ECourierNavigationMode::ParkedBike).bSucceeded);
    TestTrue(TEXT("Bike trip includes source stairs"),Nav->GetActiveRoute().bReachable && Nav->GetActiveRoute().StairMeters>0);
    TestTrue(TEXT("Station mode remains independent"),Nav->SetNavigationMode(ECourierNavigationMode::Station).bSucceeded);
    FDeliveryOffer Again; TestTrue(TEXT("Preview remains available"),Delivery->GetOfferPreview(Offer.InstanceId,Again));
    TestTrue(TEXT("Unaccepted snapshot immutable after navigation"),Again.PickupRoute.Points==Preview.PickupRoute.Points && Again.DeliveryRoute.Points==Preview.DeliveryRoute.Points && Delivery->GetState().Status==EDeliveryStatus::Idle);
    TestTrue(TEXT("Logically openable closed doors do not reject acceptance"),Delivery->TryAccept(Offer.InstanceId));
    Nav->SetNavigationMode(ECourierNavigationMode::Order); Nav->RefreshRoute(Upstairs);
    TestFalse(TEXT("Same XY on fourth floor has not arrived at first-floor merchant"),Nav->HasArrived());
    const auto* Point=Interiors->GetManagedLocation(Offer.MerchantId);
    F.Rider->SetActorLocation(Point->GetActorLocation()+FVector(0,0,90)); Nav->UpdateNavigation(F.Rider->GetActorLocation(),.6);
    TestTrue(TEXT("Actual correct room reports arrival"),Nav->HasArrived());
    return true;
}
#endif
