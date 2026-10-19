#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "Buildings/CourierBuildingActor.h"
#include "Buildings/CourierInteriorFinish.h"
#include "Buildings/CourierDoor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteriorFinishTest,"CityCourier.Buildings.Art.CookedCoverageAndDoorContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FInteriorFinishTest::RunTest(const FString&)
{
    auto* Catalog=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
    auto* Finish=UCourierInteriorFinish::Load();
    if(!TestNotNull(TEXT("Cooked art catalog"),Finish) || !TestTrue(TEXT("Art matches canonical catalog"),Finish->Matches(Catalog))) return false;
    TestEqual(TEXT("Every building has an art overlay"),Finish->Buildings.Num(),Catalog->Buildings.Num());
    auto* Missing=DuplicateObject<UCourierInteriorFinish>(Finish,GetTransientPackage());
    Missing->Buildings.FindChecked(Catalog->Buildings[0].Id).Groups[0].Mesh=nullptr;
    TestFalse(TEXT("A missing required mesh rejects delivery admission"),Missing->IsAvailable(Catalog->Buildings[0].Id));
    for(const auto& B:Catalog->Buildings)
    {
        const auto* Art=Finish->Buildings.Find(B.Id); if(!TestNotNull(B.Id.ToString(),Art)) return false;
        bool HasSolid=false,HasLamp=false;
        for(const auto& Group:Art->Groups)
        {
            if(!TestNotNull(TEXT("Every group retains a cooked mesh reference"),Group.Mesh.Get())) return false;
            TestTrue(TEXT("Private art meshes only"),Group.Mesh->GetPathName().StartsWith(TEXT("/Game/CityCourier/Art/Interiors/")));
            HasSolid|=Group.bSolid; HasLamp|=Group.Mesh->GetFName()==TEXT("SM_IF_LightPanel");
        }
        TestTrue(B.Id.ToString()+TEXT(" has blocking furniture and real light fixtures"),HasSolid && HasLamp);
    }
    FTestCourierWorld World;
    for(const TCHAR* Id:{TEXT("B_Merchant_0"),TEXT("B_1_0_0_0_0"),TEXT("B_0_1_0_0_0"),TEXT("B_Station")})
    {
        const int32 Index=Catalog->FindBuilding(Id); const auto& B=Catalog->Buildings[Index];
        auto* Actor=World.World->SpawnActor<ACourierBuildingActor>();
        if(!TestTrue(TEXT("Configure and prepare new instance groups"),Actor->ConfigureBuilding(Catalog,Index) && Actor->PrepareDetails())) return false;
        int32 Expected=0,Actual=0;
        for(const auto& Group:Finish->Buildings.FindChecked(B.Id).Groups) Expected+=Group.Transforms.Num();
        for(const auto& Component:Actor->Details)
            if(const auto* Instances=Cast<UInstancedStaticMeshComponent>(Component))
                if(!Actor->FinishDoorVisuals.Contains(const_cast<UInstancedStaticMeshComponent*>(Instances))) Actual+=Instances->GetInstanceCount();
        TestEqual(TEXT("Every approved placement instantiated exactly once"),Actual,Expected);
        const int32 DI=0; const auto& D=B.Doors[DI]; const int32 Style=UCourierInteriorFinish::DoorStyle(B,D);
        TestTrue(TEXT("Move proxy to a saved partial angle"),Actor->SetDoorProxy(DI,D.OpenAngle*.35f,true));
        FTransform Visual; Actor->FinishDoorVisuals[Style]->GetInstanceTransform(0,Visual,false);
        TestTrue(TEXT("Visual leaf remains hinged in building local space"),Visual.GetLocation().Equals(D.Hinge,.001));
        TestTrue(TEXT("Visual leaf shares original proxy angle"),FMath::IsNearlyEqual(Visual.Rotator().Yaw,D.ClosedYaw+D.OpenAngle*.35f,.001f));
        Actor->SetDoorProxy(DI,0,false);
        int32 Leaves=0; for(const auto& C:Actor->FinishDoorVisuals) Leaves+=C->GetInstanceCount();
        TestEqual(TEXT("Activating a door removes exactly one decorative leaf"),Leaves,B.Doors.Num()-1);
        FActorSpawnParameters Params; Params.Owner=Actor;
        auto* Door=World.World->SpawnActor<ACourierDoor>(ACourierDoor::StaticClass(),FTransform::Identity,Params);
        Door->Configure(D,B.Transform());
        TestTrue(TEXT("Active leaf uses same style as cached leaf"),Door->Visual->GetStaticMesh()==Finish->DoorMeshes[Style]);
        Door->Destroy(); Actor->ReleaseDetails();
        TestTrue(TEXT("Eviction restores persistent collision proxy render"),Actor->DoorProxies->IsVisible());
        TestTrue(TEXT("Reentry recreates art without duplicates"),Actor->PrepareDetails());
        Leaves=0; for(const auto& C:Actor->FinishDoorVisuals) Leaves+=C->GetInstanceCount();
        TestEqual(TEXT("Reentry preserves activated leaf absence"),Leaves,B.Doors.Num()-1);
        Actor->Destroy();
    }
    return true;
}
#endif
