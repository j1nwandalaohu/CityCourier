#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Buildings/CourierBuildingActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingTraversalFaultTest,"CityCourier.Buildings.Traversal.RealMovementAndFaultDetection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingTraversalFaultTest::RunTest(const FString&)
{
    FTestCourierWorld F; F.Box(FVector(0,0,-50),FVector(40000,40000,50));
    auto* C=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
    const int32 I=C->FindBuilding(TEXT("B_Merchant_0")); const auto& B=C->Buildings[I];
    auto* A=F.World->SpawnActor<ACourierBuildingActor>(B.Origin,FRotator(0,B.Yaw,0)); A->ConfigureBuilding(C,I);
    auto R=A->ValidatePhysicalTraversal();
    for(const auto& E:R.Errors) AddError(E);
    TestEqual(TEXT("Actual movement visits every expected room"),R.VisitedRooms.Num(),B.ExpectedRoomIds.Num());
    TestTrue(TEXT("Returns through real entry to street"),R.bReturnedToStreet);
    TestTrue(TEXT("Exercises actual openable leaves"),R.OpenedDoors>0 && R.WalkedMeters>0);
    const auto* Side=B.Doors.FindByPredicate([](const auto& D){return D.Id.ToString().EndsWith(TEXT("SideDoor_-1"));});
    auto* Sidewalk=F.Box(B.ToWorld(FVector(-B.Size.X/2-140,Side->Hinge.Y+60,20)),FVector(220,180,20));
    const auto Raised=A->ValidatePhysicalTraversal();
    TestTrue(TEXT("Actual 40cm sidewalk height does not prevent room return"),Raised.Errors.IsEmpty() && Raised.VisitedRooms.Num()==B.ExpectedRoomIds.Num() && Raised.bReturnedToStreet);
    Sidewalk->Destroy();
    A->AddActorWorldOffset(FVector(1000,0,0));
    TestTrue(TEXT("Wrong actual instance transform explicitly fails"),!A->ValidatePhysicalTraversal().Errors.IsEmpty());
    A->SetActorTransform(B.Transform());
    const auto* Step=B.Boxes.FindByPredicate([](const auto& Box){return Box.Kind==TEXT("step") && Box.Id.ToString().Contains(TEXT("F01_up_0"));});
    if(!TestNotNull(TEXT("Physical stair fixture exists"),Step)) return false;
    auto* Blocker=F.Box(B.ToWorld(Step->Center)+FVector(0,0,150),FVector(70,70,150));
    TestTrue(TEXT("Blocked staircase explicitly fails instead of reducing coverage"),!A->ValidatePhysicalTraversal().Errors.IsEmpty()); Blocker->Destroy();
    auto* Bad=DuplicateObject<UCityBuildingCatalog>(C,GetTransientPackage()); Bad->Buildings[I].Rooms.RemoveAt(2);
    A->Catalog=Bad; TestTrue(TEXT("Missing room cannot reduce expected coverage"),!A->ValidatePhysicalTraversal().Errors.IsEmpty());
    return true;
}
#endif
