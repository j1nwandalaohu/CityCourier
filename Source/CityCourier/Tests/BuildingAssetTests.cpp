#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/DataAsset.h"
#include "Buildings/CourierBuildingDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingCatalogAssetTest,"CityCourier.Buildings.Assets.CanonicalCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBuildingCatalogAssetTest::RunTest(const FString&)
{
    auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
    if(!TestNotNull(TEXT("The complete cooked city building catalog exists"),Catalog)) return false;
    TArray<FString> Errors; TestTrue(TEXT("Persisted canonical catalog validates"),Catalog->Validate(Errors));
    TestEqual(TEXT("Every canonical building persisted"),Catalog->Buildings.Num(),293);
    TestEqual(TEXT("Four merchants eight destinations and station retain bindings"),Catalog->Bindings.Num(),13);
    TestEqual(TEXT("Source fingerprint retained"),Catalog->SourceDigest.Len(),64);
    int32 Rooms=0,Doors=0; for(const auto& B:Catalog->Buildings) { Rooms+=B.Rooms.Num(); Doors+=B.Doors.Num(); }
    TestEqual(TEXT("All canonical rooms/public spaces persisted"),Rooms,18321);
    TestEqual(TEXT("All doors persisted"),Doors,15985);
    return true;
}
#endif
