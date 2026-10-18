#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Tests/ProfileTestStorage.h"
#include "Delivery/DeliverySubsystem.h"
#include "Delivery/DeliveryNavigationSubsystem.h"
#include "Profile/ProfileSubsystem.h"
#include "World/CityDefinition.h"
#include "World/DeliveryLocation.h"
#include "Vehicles/ScooterPawn.h"
#include "Player/CourierCharacter.h"
#include "Engine/GameInstance.h"
#include "Ambient/AmbientVehicle.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierNavigationTest,"CityCourier.Navigation.TargetAndOffRoute",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourierNavigationTest::RunTest(const FString& Parameters)
{
    FTestCourierWorld F;
    auto* City = DuplicateObject<UCityDefinition>(LoadObject<UCityDefinition>(nullptr,TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition")),F.World);
    auto* Profile = NewObject<UProfileSubsystem>(NewObject<UGameInstance>()); Profile->SetStorageForTests(MakeShared<FTestProfileStorage>()); Profile->LoadProfile();
    auto* Bike = F.World->SpawnActor<AScooterPawn>();
    auto* Rider = F.World->SpawnActor<ACourierCharacter>();
    auto* D = NewObject<UDeliverySubsystem>(F.World); D->Configure(City,Profile,Bike);
    auto* Nav = NewObject<UDeliveryNavigationSubsystem>(F.World); Nav->Configure(D);
    const auto O = D->GetOffers()[0]; D->TryAccept(O.InstanceId);
    FVector Start = City->Station.Position;
    TestTrue(TEXT("Route to shop"),Nav->RefreshRoute(Start));
    TestTrue(TEXT("Shop endpoint"),Nav->GetActiveRoute().Points.Last().Equals(City->FindLocation(O.MerchantId)->Position));
    const auto* Shop = City->FindLocation(O.MerchantId);
    auto* Location = F.World->SpawnActor<ADeliveryLocation>(Shop->Position,FRotator::ZeroRotator); Location->LocationId=Shop->LocationId;
    Rider->SetActorLocation(Shop->Position+FVector(0,0,90)); D->TryPickup(O.MerchantId,Rider);
    Nav->UpdateNavigation(Rider->GetActorLocation(),0.01);
    TestTrue(TEXT("Pickup immediately changes route"),Nav->GetActiveRoute().bReachable && Nav->GetActiveRoute().Points.Last().Equals(City->FindLocation(O.DestinationId)->Position));
    FVector OffRoute=FVector(24000,-24000,90);
    if (Nav->DistanceFromRoute(OffRoute)<1500) OffRoute=FVector(-24000,24000,90);
    const FVector Before=Nav->GetActiveRoute().Points[0];
    Nav->UpdateNavigation(OffRoute,.48); TestTrue(TEXT("Off-route check throttled"),Nav->GetActiveRoute().Points[0].Equals(Before));
    Nav->UpdateNavigation(OffRoute,.02); TestTrue(TEXT("Off-route refresh at half second"),Nav->GetActiveRoute().Points[0].Equals(OffRoute));
    City->Roads.Edges.Empty(); TestFalse(TEXT("Disconnected graph never invents straight route"),Nav->RefreshRoute(OffRoute));
    TestTrue(TEXT("Unreachable route empty"),Nav->GetActiveRoute().Points.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourierRoadProjectionTest,"CityCourier.Navigation.RoadProjectionAndOcclusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourierRoadProjectionTest::RunTest(const FString& Parameters)
{
    auto* City=NewObject<UCityDefinition>();
    City->Roads.Nodes={{0,FVector(0,0,0)},{1,FVector(10000,0,0)},{2,FVector(10000,10000,0)}};
    City->Roads.Edges={{0,1,100,true},{1,2,100,true}};
    FCourierLocationDefinition L; L.LocationId=TEXT("Goal"); L.EntranceNode=2; L.Position=FVector(10000,10000,0); City->Destinations.Add(L);
    const auto Route=City->FindRouteFrom(FVector(5000,200,0),TEXT("Goal"));
    TestTrue(TEXT("Projected route reachable"),Route.bReachable);
    TestTrue(TEXT("Joins road at nearest segment, not diagonal to junction"),Route.Points.Num()>2 && Route.Points[1].Equals(FVector(5000,0,0)));
    TestTrue(TEXT("Correct partial edge distance"),FMath::IsNearlyEqual(Route.LengthMeters,152.0));
    FTestCourierWorld F; F.Box(FVector(500,0,100),FVector(40,200,100));
    auto* Nav=NewObject<UDeliveryNavigationSubsystem>(F.World);
    TestFalse(TEXT("Never draws through a solid building"),Nav->IsRouteClear({FVector(0,0,0),FVector(1000,0,0)}));
    TestTrue(TEXT("Clear street segment accepted"),Nav->IsRouteClear({FVector(0,500,0),FVector(1000,500,0)}));
    F.World->SpawnActor<AAmbientVehicle>(FVector(500,500,100),FRotator::ZeroRotator);
    TestTrue(TEXT("Passing traffic does not invalidate road estimates"),Nav->IsRouteClear({FVector(0,500,0),FVector(1000,500,0)}));
    TestFalse(TEXT("Static building remains blocked with traffic present"),Nav->IsRouteClear({FVector(0,0,0),FVector(1000,0,0)}));
    return true;
}
#endif
