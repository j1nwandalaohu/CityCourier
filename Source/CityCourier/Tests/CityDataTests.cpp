#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "World/CityDefinition.h"
#include "World/DeliveryLocation.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCityDataTest, "CityCourier.CityData.AuthoredDistrict",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCityDataTest::RunTest(const FString& Parameters)
{
    auto* City = LoadObject<UCityDefinition>(nullptr, TEXT("/Game/CityCourier/Data/DA_CityDefinition.DA_CityDefinition"));
    if (!TestNotNull(TEXT("City data authored"), City)) return false;
    TArray<FString> Errors;
    TestTrue(TEXT("All city constraints valid"), City->Validate(Errors));
    for (const auto& Error : Errors) AddError(Error);
    TestEqual(TEXT("Four merchants"), City->Merchants.Num(), 4);
    TestEqual(TEXT("Eight recipients"), City->Destinations.Num(), 8);
    TestEqual(TEXT("Every order pair priced"), City->Fees.Num(), 32);
    for (const auto& Merchant : City->Merchants)
        for (const auto& Destination : City->Destinations)
        {
            const auto Route = City->FindRouteBetween(Merchant, Destination);
            TestTrue(TEXT("Every delivery route connected"), Route.bReachable);
            const auto Road = City->Roads.FindRoute(Merchant.EntranceNode, Destination.EntranceNode);
            TestTrue(TEXT("Entrance walks included"), Route.LengthMeters > Road.LengthMeters);
            const int64 Fee = City->GetFee(Merchant.LocationId, Destination.LocationId);
            TestTrue(TEXT("Fee within range"), Fee >= 1600 && Fee <= 3200);
        }
    auto* Bad = DuplicateObject<UCityDefinition>(City, GetTransientPackage());
    Bad->Destinations[0].LocationId = Bad->Merchants[0].LocationId;
    TestFalse(TEXT("Duplicate location invalid"), Bad->Validate(Errors));
    TestTrue(TEXT("Error names offending ID"), Errors.ContainsByPredicate([Bad](const FString& E) { return E.Contains(Bad->Merchants[0].LocationId.ToString()); }));
    auto* World = LoadObject<UWorld>(nullptr, TEXT("/Game/CityCourier/Maps/L_CourierCity.L_CourierCity"));
    if (!TestNotNull(TEXT("District map exists"), World)) return false;
    TSet<FName> IDs; int32 ShortcutSegments = 0;
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (Actor && Actor->Tags.ContainsByPredicate([](FName Tag) { return Tag.ToString().StartsWith(TEXT("CC_City_Road_")); }))
            TestTrue(TEXT("Authored road surface is horizontal"), Actor->GetActorUpVector().Z > 0.99);
        if (auto* Location = Cast<ADeliveryLocation>(Actor))
        {
            TestFalse(TEXT("No duplicate physical location"), IDs.Contains(Location->LocationId)); IDs.Add(Location->LocationId);
            const auto* Definition = City->FindLocation(Location->LocationId);
            TestNotNull(TEXT("Physical location has stable data ID"), Definition);
            if (Definition) TestTrue(TEXT("Actor matches data position"), Location->GetActorLocation().Equals(Definition->Position, 1));
            TestTrue(TEXT("Locations on ground"), FMath::Abs(Location->GetActorLocation().Z) < 100);
        }
        if (Actor && Actor->ActorHasTag(TEXT("CC_Shortcut"))) ++ShortcutSegments;
        if (Actor && Actor->ActorHasTag(TEXT("CC_BootstrapGround")))
            TestTrue(TEXT("District ground spans 600m"), Actor->GetActorScale3D().X >= 600);
    }
    TestEqual(TEXT("12 order locations plus station"), IDs.Num(), 13);
    TestTrue(TEXT("Two shortcut corridors exist"), ShortcutSegments >= 2);
    return true;
}
#endif
