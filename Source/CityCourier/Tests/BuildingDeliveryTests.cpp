#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/BuildingDeliverySupport.h"
#include "Core/DeliveryRules.h"
#include "Misc/PackageName.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndoorDeadlineTest,"CityCourier.Buildings.Delivery.StructuredDeadline",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIndoorDeadlineTest::RunTest(const FString&)
{
    FRouteResult A,B; A.bReachable=B.bReachable=true;
    A.RoadMeters=B.RoadMeters=100; A.IndoorFlatMeters=B.IndoorFlatMeters=50; A.StairMeters=20; B.StairMeters=10;
    A.RequiredDoors={TEXT("A"),TEXT("B")}; B.RequiredDoors={TEXT("B"),TEXT("C"),TEXT("D")};
    A.LengthMeters=170; B.LengthMeters=160;
    TestEqual(TEXT("Road + indoor flat + stairs + every traversed door"),FDeliveryRules::CalculateDeadline(A,B),200.0);
    A.StairMeters=10000; TestEqual(TEXT("Honest estimate still respects profile maximum"),FDeliveryRules::CalculateDeadline(A,B),300.0);
    A.StairMeters=-1; TestEqual(TEXT("Invalid estimate fails"),FDeliveryRules::CalculateDeadline(A,B),-1.0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndoorDeliveryAdmissionTest,"CityCourier.Buildings.Delivery.RoomGatingLateRegistrationAndHistory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIndoorDeliveryAdmissionTest::RunTest(const FString&)
{
    FBuildingDeliveryFixture F;
    if(!TestTrue(TEXT("Fixture has indoor offer"),F.Delivery->GetOffers().Num()>0)) return false;
    const auto Offer=F.Delivery->GetOffers()[0]; TestTrue(TEXT("Accept complete indoor offer"),F.Delivery->TryAccept(Offer.InstanceId));
    auto* Merchant=F.Merchant(); const auto& B=F.Catalog->Buildings[F.Catalog->FindBuilding(TEXT("B_Merchant_0"))];
    F.At(Merchant);
    const int32 Door=B.Doors.IndexOfByPredicate([](const auto& D){return D.ToRoom==FName(TEXT("B_Merchant_0_F01_A01"));});
    FVector Hall=B.ToLocal(Merchant->GetActorLocation()); Hall.X=-65;
    F.Rider->SetActorLocation(B.ToWorld(Hall)+FVector(0,0,90));
    FText Reason;
    F.Buildings[B.Id]->SetDoorProxy(Door,0,true);
    TestFalse(TEXT("Closed real door blocks interaction trace"),F.Rider->CanReachInteraction(Merchant,Merchant->GetInteractionLocation(),Reason));
    F.Buildings[B.Id]->SetDoorProxy(Door,90,true);
    TestTrue(TEXT("Door is physically open and point is within two meters"),F.Rider->CanReachInteraction(Merchant,Merchant->GetInteractionLocation(),Reason));
    TestFalse(TEXT("A reachable open-door point in another room is not pickup"),F.Delivery->TryPickup(Merchant->LocationId,F.Rider));
    F.Rider->SetActorLocation(Merchant->GetActorLocation()+FVector(0,0,90+385));
    TestFalse(TEXT("Same XY one floor above cannot pickup"),F.Delivery->TryPickup(Merchant->LocationId,F.Rider));
    F.At(Merchant); TestTrue(TEXT("Actual ready room allows pickup"),F.Delivery->TryPickup(Merchant->LocationId,F.Rider));
    auto* Customer=F.Customer(); const FName Id=Customer->LocationId;
    F.Delivery->UnregisterLocation(Customer); Customer->Destroy();
    Customer=F.World->SpawnActor<ADeliveryLocation>(); Customer->LocationId=Id; Customer->LocationKind=EDeliveryLocationKind::Destination;
    F.Delivery->RegisterLocation(Customer); F.Delivery->RegisterLocation(Customer);
    TestTrue(TEXT("Re-registered target stays targeted"),Customer->bActiveOrderTarget && Customer->OnInteraction.IsBound());
    auto* Duplicate=F.World->SpawnActor<ADeliveryLocation>(); Duplicate->LocationId=Id;
    F.Delivery->RegisterLocation(Duplicate);
    TestFalse(TEXT("Duplicate identity cannot steal authoritative binding"),Duplicate->OnInteraction.IsBound()); Duplicate->Destroy();
    F.At(Customer); F.Now+=40;
    TestTrue(TEXT("Correct room settles through registered actor"),Customer->Interact(F.Rider));
    TestFalse(TEXT("Repeated interaction cannot settle twice"),Customer->Interact(F.Rider));
    TestEqual(TEXT("One settlement"),F.Profile->GetProfile().CompletedCount,1);
    const auto& History=F.Profile->GetProfile().OrderHistory;
    if(TestEqual(TEXT("History contains one record"),History.Num(),1))
    {
        TestTrue(TEXT("Friendly floor address retained"),History[0].DestinationName.Contains(TEXT("楼")));
        TestTrue(TEXT("Existing bounded profile strings"),History[0].DestinationName.Len()<=64 && History[0].MerchantName.Len()<=64);
    }
    auto* Reloaded=NewObject<UProfileSubsystem>(NewObject<UGameInstance>()); Reloaded->SetStorageForTests(F.Storage);
    TestTrue(TEXT("Existing v2 profile reloads"),Reloaded->LoadProfile()==EProfileLoadResult::Loaded);
    TestTrue(TEXT("Indoor address and income survive restart"),Reloaded->GetProfile()==F.Profile->GetProfile());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndoorExpiryRecoveryTest,"CityCourier.Buildings.Delivery.ExactExpiryAndSafeOutdoorRecovery",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIndoorExpiryRecoveryTest::RunTest(const FString&)
{
    FBuildingDeliveryFixture F; const auto Offer=F.Delivery->GetOffers()[0]; F.Delivery->TryAccept(Offer.InstanceId);
    F.At(F.Merchant()); TestTrue(TEXT("Pick up before deadline"),F.Delivery->TryPickup(Offer.MerchantId,F.Rider));
    F.At(F.Customer()); F.Now+=Offer.DeadlineSeconds+60;
    TestFalse(TEXT("Exact end of grace cannot deliver"),F.Delivery->TryDeliver(Offer.DestinationId,F.Rider));
    TestEqual(TEXT("Failure settles once"),F.Profile->GetProfile().FailedCount,1);
    F.Delivery->TryDeliver(Offer.DestinationId,F.Rider); TestEqual(TEXT("Repeated expiry cannot settle twice"),F.Profile->GetProfile().FailedCount,1);
    F.Recovery->Configure(F.City,F.Bike,F.Rider,F.Delivery,F.Station); F.Hold(2.1f);
    TestFalse(TEXT("Recovery clears indoor membership"),F.Interiors->Locate(F.Rider->GetActorLocation()).IsValid());
    TestFalse(TEXT("Recovered bike stays outside buildings"),F.Interiors->Locate(F.Bike->GetActorLocation()).IsValid());
    F.At(F.Station); TestTrue(TEXT("Station works inside its ready service room with bike at external parking"),F.Station->CheckService(F.Rider).bSucceeded);
    F.Bike->SetActorLocation(F.Station->StreetAnchor+FVector(1400,0,52));
    TestFalse(TEXT("Station still enforces 12m parking distance"),F.Station->CheckService(F.Rider).bSucceeded);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIndoorMissingResourceTest,"CityCourier.Buildings.Delivery.MissingTargetResourceFailsBeforeTimer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FIndoorMissingResourceTest::RunTest(const FString&)
{
    if(FPackageName::DoesPackageExist(TEXT("/Game/CityCourier/Art/Buildings/SM_InteriorBed")))
    { AddInfo(TEXT("Separate actual missing-asset run required; normal packaged assets are present.")); return true; }
    FBuildingDeliveryFixture F(false);
    TestEqual(TEXT("No offer points into a building with missing required resources"),F.Delivery->GetOffers().Num(),0);
    TestFalse(TEXT("Missing resource cannot start order timer"),F.Delivery->TryAccept(FGuid::NewGuid()));
    TestTrue(TEXT("Order remains idle"),F.Delivery->GetState().Status==EDeliveryStatus::Idle && F.Delivery->GetState().ElapsedSeconds==0);
    return true;
}
#endif
