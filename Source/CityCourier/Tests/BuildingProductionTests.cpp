#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "Buildings/CourierBuildingActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "FileHelpers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingHollowAssetTest,"CityCourier.Buildings.Production.HollowExteriorAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingHollowAssetTest::RunTest(const FString&)
{
    FTestCourierWorld F;
    for(const TCHAR* Name:{TEXT("SM_EnterableShopHouseA"),TEXT("SM_EnterableShopHouseB"),TEXT("SM_EnterableShopHouseC"),TEXT("SM_EnterableAnnex")})
    {
        auto* Mesh=LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Art/Buildings/%s.%s"),Name,Name));
        if(!TestNotNull(Name,Mesh)) continue;
        auto* Actor=F.World->SpawnActor<AActor>(); auto* Component=NewObject<UStaticMeshComponent>(Actor); Actor->SetRootComponent(Component);
        Component->SetStaticMesh(Mesh); Component->SetCollisionProfileName(TEXT("BlockAll")); Component->RegisterComponent();
        FHitResult Hit;
        TestFalse(TEXT("Former solid core is actually empty"),F.World->LineTraceSingleByChannel(Hit,FVector(0,0,100),FVector(0,0,1000),ECC_Visibility));
        const float Front=FString(Name).Contains(TEXT("Annex"))?250:576;
        TestFalse(TEXT("Main door aperture is free of baked panels"),F.World->LineTraceSingleByChannel(Hit,FVector(0,Front+150,150),FVector(0,0,150),ECC_Visibility));
        Actor->Destroy();
    }
    for(const TCHAR* Name:{TEXT("SM_CourierDoor"),TEXT("SM_InteriorBed"),TEXT("SM_InteriorDesk"),TEXT("SM_InteriorKitchen"),TEXT("SM_InteriorToilet"),TEXT("SM_InteriorShelf"),TEXT("SM_InteriorSofa"),TEXT("SM_InteriorCounter")})
        TestNotNull(Name,LoadObject<UStaticMesh>(nullptr,*FString::Printf(TEXT("/Game/CityCourier/Art/Buildings/%s.%s"),Name,Name)));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingMapCoverageTest,"CityCourier.Buildings.Production.CanonicalMapCoverage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingMapCoverageTest::RunTest(const FString&)
{
    const auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
    if(!Catalog) return false;
    for(const TCHAR* Map:{TEXT("/Game/CityCourier/Maps/L_CourierCity"),TEXT("/Game/CityCourier/Maps/L_CourierArtPreview")})
    {
        UWorld* World=UEditorLoadingAndSavingUtils::LoadMap(Map); if(!TestNotNull(Map,World)) continue;
        TSet<FName> Actual; int32 Count=0;
        for(TActorIterator<AActor> It(World);It;++It)
        {
            if(!It->ActorHasTag(TEXT("CC_Enterable_Building"))) continue;
            ++Count;
            for(FName T:It->Tags) if(T.ToString().StartsWith(TEXT("CC_Enterable_Id_"))) Actual.Add(FName(*T.ToString().RightChop(16)));
        }
        TestEqual(FString(Map)+TEXT(" has one actor per canonical building"),Count,Catalog->Buildings.Num());
        TestEqual(TEXT("No duplicate mapped identities"),Actual.Num(),Count);
        for(FName Id:Catalog->ExpectedBuildingIds) TestTrue(TEXT("Expected building present: ")+Id.ToString(),Actual.Contains(Id));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingPhysicalStructureTest,"CityCourier.Buildings.Production.RoomStandingClearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingPhysicalStructureTest::RunTest(const FString&)
{
    auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
    if(!Catalog) return false;
    for(FName Id:{FName(TEXT("B_0_0_0_0_0")),FName(TEXT("B_Merchant_0")),FName(TEXT("B_Station"))})
    {
        FTestCourierWorld F; const int32 Index=Catalog->FindBuilding(Id); if(!TestTrue(TEXT("Fixture belongs to inventory"),Index!=INDEX_NONE)) continue;
        const auto& B=Catalog->Buildings[Index];
        auto* Actor=F.World->SpawnActor<ACourierBuildingActor>(B.Origin,FRotator(0,B.Yaw,0));
        if(!TestTrue(TEXT("Actual instanced structure builds"),Actor->ConfigureBuilding(Catalog,Index))) continue;
        TestEqual(TEXT("All structural boxes present"),Actor->GetStructuralBoxCount(),B.Boxes.Num());
        TestEqual(TEXT("All closed doors represented"),Actor->GetDoorProxyCount(),B.Doors.Num());
        FCollisionQueryParams Params(SCENE_QUERY_STAT(BuildingRoomClearance),false);
        for(const auto& R:B.Rooms)
        {
            const FVector Position=B.ToWorld(R.StandingPoint)+FVector(0,0,90);
            const bool Blocked=F.World->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(34,88),Params);
            TestFalse(TEXT("Character can stand in actual room: ")+R.Id.ToString(),Blocked);
        }
    }
    return true;
}
#endif
