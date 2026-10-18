#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Buildings/CourierBuildingActor.h"
#include "Buildings/CourierBuildingSubsystem.h"
#include "Buildings/CourierBuildingResidency.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"

namespace
{
struct FInteriorRuntimeFixture:FStationFixture
{
    UCityBuildingCatalog* Catalog;
    ACourierBuildingActor* Building;
    UCourierBuildingSubsystem* Interiors;
    FInteriorRuntimeFixture()
    {
        auto* Source=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
        Catalog=NewObject<UCityBuildingCatalog>(); Catalog->Buildings={Source->Buildings[Source->FindBuilding(TEXT("B_Merchant_0"))]};
        Catalog->ExpectedBuildingIds={Catalog->Buildings[0].Id}; Catalog->Bindings={*Source->FindBinding(TEXT("merchant_noodle"))};
        const auto& B=Catalog->Buildings[0]; Building=World->SpawnActor<ACourierBuildingActor>(B.Origin,FRotator(0,B.Yaw,0)); Building->ConfigureBuilding(Catalog,0);
        const auto& Door=B.Doors[B.Doors.IndexOfByPredicate([&](const auto& D){return D.Id==FName(*(B.Id.ToString()+TEXT("_Entry_Door")));})];
        Rider->SetActorLocation(B.ToWorld(Door.Hinge+FVector(Door.Width/2,-140,0))+FVector(0,0,90)); Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Interiors=World->GetSubsystem<UCourierBuildingSubsystem>();
    }
    FName DoorId() const { return FName(*(Catalog->Buildings[0].Id.ToString()+TEXT("_Entry_Door"))); }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingResidencyTest,"CityCourier.Buildings.Runtime.ResidencyHysteresisAndPins",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingResidencyTest::RunTest(const FString&)
{
    TArray<FCourierBuildingResidencyEntry> Entries;
    for(int32 I=0;I<12;++I) { FCourierBuildingResidencyEntry E; E.Index=I; E.DistanceCm=1000+I*100; Entries.Add(E); }
    auto Selected=FCourierBuildingResidency::Select(Entries); TestEqual(TEXT("At most eight unoccupied detail caches"),Selected.Num(),8);
    for(int32 I=0;I<8;++I) TestTrue(TEXT("Nearest candidate selected"),Selected.Contains(I));
    for(auto& E:Entries) E.DistanceCm=5000;
    Entries[0].DistanceCm=4500; Entries[0].bReady=true; Entries[11].bPinned=true;
    Selected=FCourierBuildingResidency::Select(Entries);
    TestTrue(TEXT("Loaded details retain at 45m boundary"),Selected.Contains(0));
    TestTrue(TEXT("Occupied/target/entry building cannot be evicted"),Selected.Contains(11));
    Entries[0].DistanceCm=4501; Selected=FCourierBuildingResidency::Select(Entries); TestFalse(TEXT("Unoccupied beyond 45m releases"),Selected.Contains(0));
    Entries[0].bReady=false; Entries[0].DistanceCm=3000; Selected=FCourierBuildingResidency::Select(Entries); TestTrue(TEXT("Unloaded starts preparing at 30m"),Selected.Contains(0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingLifecycleTest,"CityCourier.Buildings.Runtime.DetailLifecycleAndPersistentStructure",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingLifecycleTest::RunTest(const FString&)
{
    FInteriorRuntimeFixture F;
    if(!TestNotNull(TEXT("World subsystem exists"),F.Interiors) || !TestTrue(TEXT("Runtime accepts complete structural world"),F.Interiors->Configure(F.Catalog))) return false;
    const int32 Boxes=F.Building->GetStructuralBoxCount(); const FVector Near=F.Rider->GetActorLocation();
    F.Interiors->UpdateResidency(Near);
    TestTrue(TEXT("Preparation is queued before readiness"),F.Interiors->GetDetailState(0)==ECourierBuildingDetailState::Loading);
    TestTrue(TEXT("One pending building can prepare"),F.Interiors->PrepareNext());
    TestTrue(TEXT("Complete formal details ready"),F.Interiors->GetDetailState(0)==ECourierBuildingDetailState::Ready);
    const auto& R=F.Catalog->Buildings[0].Rooms[0]; const FVector Inside=F.Catalog->Buildings[0].ToWorld(R.StandingPoint)+FVector(0,0,90);
    TestTrue(TEXT("Correct room/floor membership"),F.Interiors->Locate(Inside).IsValid());
    F.Interiors->UpdateResidency(Inside); TestEqual(TEXT("Occupied room retained"),F.Interiors->GetReadyCount(),1);
    F.Interiors->UpdateResidency(Near+FVector(10000,10000,0));
    TestTrue(TEXT("Far unoccupied details unload"),F.Interiors->GetDetailState(0)==ECourierBuildingDetailState::Unloaded);
    TestEqual(TEXT("Unloading never removes floor/stair/wall structure"),F.Building->GetStructuralBoxCount(),Boxes);
    F.Interiors->UpdateResidency(Near); TestTrue(TEXT("Reentry can prepare again"),F.Interiors->PrepareNext());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingProxyLifecycleTest,"CityCourier.Buildings.Runtime.DoorProxyPoseContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingProxyLifecycleTest::RunTest(const FString&)
{
    FInteriorRuntimeFixture F;
    if(!TestTrue(TEXT("Runtime configure"),F.Interiors->Configure(F.Catalog))) return false;
    F.Interiors->UpdateResidency(F.Rider->GetActorLocation()); F.Interiors->PrepareNext();
    const int32 Total=F.Catalog->Buildings[0].Doors.Num(); auto* Door=F.Interiors->ActivateDoor(F.DoorId());
    if(!TestNotNull(TEXT("Nearby door activates"),Door)) return false;
    TestEqual(TEXT("Active leaf replaces a single collision proxy"),F.Building->GetDoorProxyCount(),Total-1);
    TestTrue(TEXT("Ready entrance can open"),Door->TryToggle(F.Rider).bSucceeded); Door->AdvanceDoor(.2f);
    const float Angle=Door->GetDoorState().Angle; TestTrue(TEXT("Motion reaches a partial pose"),Angle>0 && Angle<90);
    F.Interiors->DeactivateFarDoors(F.Rider->GetActorLocation()+FVector(10000,10000,0));
    TestEqual(TEXT("Far door regains its proxy"),F.Building->GetDoorProxyCount(),Total);
    Door=F.Interiors->ActivateDoor(F.DoorId());
    if(TestNotNull(TEXT("Reactivation succeeds"),Door)) TestEqual(TEXT("Reactivation preserves partial angle"),Door->GetDoorState().Angle,Angle);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingDelayedPointTest,"CityCourier.Buildings.Runtime.DelayedDeliveryPointRegistration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingDelayedPointTest::RunTest(const FString&)
{
    FInteriorRuntimeFixture F;
    if(!TestTrue(TEXT("Late configure after delivery subsystem"),F.Interiors->Configure(F.Catalog))) return false;
    auto* Point=F.Interiors->GetManagedLocation(TEXT("merchant_noodle"));
    if(TestNotNull(TEXT("Indoor merchant binds after original delivery configure"),Point))
    {
        TestTrue(TEXT("Late point has authoritative order interaction"),Point->OnInteraction.IsBound());
        const auto Address=F.Interiors->Locate(Point->GetActorLocation()+FVector(0,0,90));
        TestTrue(TEXT("Point is in actual room rather than old sidewalk coordinate"),Address.IsValid());
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingDetailsCoverageTest,"CityCourier.Buildings.Runtime.FurnitureAndVehicleAdmission",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingDetailsCoverageTest::RunTest(const FString&)
{
    FTestCourierWorld F;
    auto* Source=LoadObject<UCityBuildingCatalog>(nullptr,TEXT("/Game/CityCourier/Data/Buildings/DA_Buildings.DA_Buildings"));
    TSet<FString> Tested;
    for(int32 I=0;I<Source->Buildings.Num();++I)
    {
        const auto& B=Source->Buildings[I];
        const FString Key=FString::Printf(TEXT("%d_%.2f_%.2f"),B.Variant,B.Size.X,B.Size.Y);
        if(Tested.Contains(Key)) continue; Tested.Add(Key);
        auto* A=F.World->SpawnActor<ACourierBuildingActor>(B.Origin,FRotator(0,B.Yaw,0));
        if(!TestTrue(TEXT("Structure configures"),A->ConfigureBuilding(Source,I))) return false;
        A->EnsureEntranceBarriers(); A->SetEntryLoading(false);
        TestTrue(B.Id.ToString()+TEXT(" fits all formal furniture"),A->PrepareDetails());
        TestTrue(TEXT("Prepared real details exist"),A->GetDetailComponentCount()>0);
        const auto Errors=A->GetPhysicalRoomErrors(); for(const auto& E:Errors) AddError(E);
        const auto* Entry=B.Doors.FindByPredicate([](const auto& D){return D.bExternal;});
        const int32 DI=B.Doors.IndexOfByPredicate([&](const auto& D){return D.Id==Entry->Id;}); A->SetDoorProxy(DI,Entry->OpenAngle,true);
        FVector Center=B.ToWorld(Entry->Hinge+FRotator(0,Entry->ClosedYaw,0).RotateVector(FVector(Entry->Width/2,0,100)));
        const FVector Normal=B.Transform().TransformVectorNoScale(FRotator(0,Entry->ClosedYaw,0).RotateVector(FVector(0,80,0)));
        FHitResult Hit;
        TestTrue(TEXT("Vehicle cannot pass an open entrance"),F.World->LineTraceSingleByChannel(Hit,Center-Normal,Center+Normal,ECC_GameTraceChannel1));
        TestFalse(TEXT("Walking player can pass ready open entrance"),F.World->LineTraceSingleByChannel(Hit,Center-Normal,Center+Normal,ECC_Pawn));
        A->SetEntryLoading(true);
        TestTrue(TEXT("Incomplete interior holds entry collision"),F.World->LineTraceSingleByChannel(Hit,Center-Normal,Center+Normal,ECC_Pawn));
        A->Destroy();
    }
    TestTrue(TEXT("Both production widths, depths and station exercised"),Tested.Num()>=7);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingMissingFurnitureTest,"CityCourier.Buildings.Runtime.MissingAssetAdmission",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingMissingFurnitureTest::RunTest(const FString&)
{
    if(FPackageName::DoesPackageExist(TEXT("/Game/CityCourier/Art/Buildings/SM_InteriorBed")))
    { AddInfo(TEXT("Fault injection is exercised in a separate run with this worktree's bed asset temporarily moved and restored.")); return true; }
    FInteriorRuntimeFixture F;
    TestTrue(TEXT("Structure remains available without furnishings"),F.Interiors->Configure(F.Catalog));
    F.Interiors->UpdateResidency(F.Rider->GetActorLocation());
    AddExpectedError(TEXT("Interior detail preparation failed"),EAutomationExpectedErrorFlags::Contains,1);
    TestFalse(TEXT("Missing required asset cannot report ready"),F.Interiors->PrepareNext());
    TestTrue(TEXT("Explicit failure state"),F.Interiors->GetDetailState(0)==ECourierBuildingDetailState::Failed);
    const auto* Door=F.Interiors->ActivateDoor(F.DoorId()); FText Reason;
    TestFalse(TEXT("Failed entry refuses opening"),Door->CanInteract(F.Rider,Reason));
    TestTrue(TEXT("Failure preserves every structural box"),F.Building->GetStructuralBoxCount()==F.Catalog->Buildings[0].Boxes.Num());
    return true;
}
#endif
