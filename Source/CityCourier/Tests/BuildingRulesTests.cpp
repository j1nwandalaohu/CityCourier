#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/StationTestSupport.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "Buildings/CourierInteriorRouting.h"
#include "Buildings/CourierDoor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/BoxComponent.h"

namespace
{
FCourierBuildingDefinition BuildingFixture()
{
    FCourierBuildingDefinition B; B.Id=TEXT("B_Test"); B.Size=FVector(800,2300,700); B.EntranceNode=0;
    B.Origin=FVector(100,200,0); B.Yaw=90;
    FCourierBuildingFloor F; F.Id=TEXT("F01"); F.Number=1; B.Floors.Add(F);
    F.Id=TEXT("F02"); F.Number=2; F.Elevation=330; B.Floors.Add(F);
    FCourierBuildingNode N; N.Id=TEXT("Street"); N.RoomId=TEXT("Outside"); B.Nodes.Add(N);
    N.Id=TEXT("A"); N.RoomId=TEXT("RoomA"); N.Position=FVector(100,0,0); N.Floor=1; B.Nodes.Add(N);
    N.Id=TEXT("B"); N.RoomId=TEXT("RoomB"); N.Position=FVector(100,0,330); N.Floor=2; B.Nodes.Add(N);
    FCourierRoomDefinition R; R.Id=TEXT("RoomA"); R.Floor=1; R.Kind=TEXT("office"); R.Minimum=FVector(50,-100,0); R.Maximum=FVector(200,100,280); R.RouteNode=1; R.StandingPoint=FVector(100,0,0); B.Rooms.Add(R);
    R.Id=TEXT("RoomB"); R.Floor=2; R.Minimum.Z=330; R.Maximum.Z=610; R.RouteNode=2; R.StandingPoint.Z=330; B.Rooms.Add(R);
    B.ExpectedRoomIds={TEXT("RoomA"),TEXT("RoomB")};
    FCourierDoorDefinition D; D.Id=TEXT("DoorA"); D.FromRoom=TEXT("Outside"); D.ToRoom=TEXT("RoomA"); D.NodeA=0; D.NodeB=1; B.Doors.Add(D);
    FCourierBuildingEdge E; E.From=0; E.To=1; E.Kind=TEXT("door"); E.DoorId=D.Id; E.LengthMeters=1; B.Edges.Add(E);
    E.From=1; E.To=2; E.Kind=TEXT("stairs"); E.DoorId=NAME_None; E.LengthMeters=3.3; B.Edges.Add(E);
    return B;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingIdentityTest,"CityCourier.Buildings.Rules.IdentityAndCoverage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingIdentityTest::RunTest(const FString&)
{
    auto B=BuildingFixture(); TArray<FString> Errors;
    TestTrue(TEXT("Complete fixture accepted"),B.Validate(Errors));
    auto Broken=B; Broken.Rooms.Pop(); Errors.Reset(); TestFalse(TEXT("Missing actual room cannot reduce expected inventory"),Broken.Validate(Errors));
    Broken=B; Broken.Rooms[1].Id=Broken.Rooms[0].Id; Errors.Reset(); TestFalse(TEXT("Duplicate room ID rejected"),Broken.Validate(Errors));
    Broken=B; Broken.Edges.Pop(); Errors.Reset(); TestFalse(TEXT("Disconnected upper room rejected"),Broken.Validate(Errors));
    Broken=B; Broken.Doors[0].NodeB=77; Errors.Reset(); TestFalse(TEXT("Invalid door endpoint rejected"),Broken.Validate(Errors));
    auto* Catalog=NewObject<UCityBuildingCatalog>(); Catalog->Buildings={B}; Catalog->ExpectedBuildingIds={B.Id}; Errors.Reset();
    TestTrue(TEXT("Complete catalog accepted"),Catalog->Validate(Errors)); Catalog->ExpectedBuildingIds.Add(TEXT("MissingHouse")); Errors.Reset();
    TestFalse(TEXT("Missing building remains a coverage error"),Catalog->Validate(Errors));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingRouteRulesTest,"CityCourier.Buildings.Rules.CrossFloorRoutes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingRouteRulesTest::RunTest(const FString&)
{
    const auto B=BuildingFixture(); const auto Route=FCourierInteriorRouting::FindRoute(B,0,2);
    TestTrue(TEXT("Closed but openable door permits planning"),Route.bReachable);
    TestEqual(TEXT("One mandatory door"),Route.Doors.Num(),1);
    TestEqual(TEXT("Flat portion not counted as stairs"),Route.FlatMeters,1.);
    TestEqual(TEXT("Real stair length retained"),Route.StairMeters,3.3);
    if(Route.Points.Num()==3) TestTrue(TEXT("World transform is applied once"),Route.Points[2].Equals(FVector(100,300,330),.01));
    TestTrue(TEXT("Room B is on floor 2"),B.LocateRoom(FVector(100,300,420))==1);
    TestTrue(TEXT("Same XY below target stays on floor 1"),B.LocateRoom(FVector(100,300,90))==0);
    TestTrue(TEXT("All indoor paths return to the street"),FCourierInteriorRouting::FindRoute(B,2,0).bReachable);
    TestFalse(TEXT("Unknown node cannot claim reachability"),FCourierInteriorRouting::FindRoute(B,0,99).bReachable);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingDoorTest,"CityCourier.Buildings.Rules.DoorMovementAndAdmission",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingDoorTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->SetActorLocation(FVector(50,-120,90)); F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    auto* Door=F.World->SpawnActor<ACourierDoor>(); FCourierDoorDefinition D; D.Id=TEXT("TestDoor");
    Door->Configure(D,FTransform::Identity);
    TestTrue(TEXT("Closed door accepts nearby E"),Door->TryToggle(F.Rider).bSucceeded);
    TestFalse(TEXT("Repeated input cannot stack a second movement"),Door->TryToggle(F.Rider).bSucceeded);
    Door->AdvanceDoor(.6f); TestTrue(TEXT("Door reaches ninety degrees"),FMath::IsNearlyEqual(Door->GetDoorState().Angle,90.f,.01f));
    F.Rider->SetActorLocation(FVector(120,80,90));
    TestTrue(TEXT("Other side can close door"),Door->TryToggle(F.Rider).bSucceeded);
    F.Rider->SetActorLocation(FVector(50,-120,90)); Door->AdvanceDoor(.6f);
    TestTrue(TEXT("Door closes precisely"),FMath::IsNearlyZero(Door->GetDoorState().Angle,.01f));
    F.Rider->SetActorLocation(FVector(1000,0,90)); TestFalse(TEXT("Distance guard"),Door->TryToggle(F.Rider).bSucceeded);
    F.Rider->SetActorLocation(FVector(50,-120,90)); F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    TestFalse(TEXT("Airborne guard"),Door->TryToggle(F.Rider).bSucceeded);
    F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking); F.Controller->SetInterfaceMode(ECourierInterfaceMode::Phone);
    TestFalse(TEXT("UI guard"),Door->TryToggle(F.Rider).bSucceeded);
    F.Controller->SetInterfaceMode(ECourierInterfaceMode::Gameplay); Door->SetReady(false);
    TestFalse(TEXT("Unprepared interior cannot open"),Door->TryToggle(F.Rider).bSucceeded);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingDoorBlockTest,"CityCourier.Buildings.Rules.SweptDoorAndRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingDoorBlockTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->SetActorLocation(FVector(50,-120,90)); F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    auto* Door=F.World->SpawnActor<ACourierDoor>(); FCourierDoorDefinition D; D.Id=TEXT("SweepDoor"); Door->Configure(D,FTransform::Identity);
    // The endpoint at 90 degrees is clear; this blocker only crosses the arc.
    auto* Block=F.Box(FVector(55,55,110),FVector(5,5,80));
    Door->TryToggle(F.Rider); Door->AdvanceDoor(.6f);
    const auto State=Door->GetDoorState(); TestTrue(TEXT("Obstacle stops swept rotational motion"),State.bBlocked && State.Angle>0 && State.Angle<90);
    auto* Replacement=F.World->SpawnActor<ACourierDoor>(); Replacement->Configure(D,FTransform(FVector(500,0,0)),State);
    TestEqual(TEXT("Proxy recreation preserves exact partial angle"),Replacement->GetDoorState().Angle,State.Angle);
    Block->Destroy(); TestTrue(TEXT("Retry reverses after obstruction"),Door->TryToggle(F.Rider).bSucceeded);
    Door->AdvanceDoor(.6f); TestTrue(TEXT("Reverse returns to closed"),FMath::IsNearlyZero(Door->GetDoorState().Angle,.01f));
    FHitResult Hit; FCollisionQueryParams Params(SCENE_QUERY_STAT(DoorRules),false,F.Rider);
    TestTrue(TEXT("Closed leaf really blocks visibility/strikes"),F.World->LineTraceSingleByChannel(Hit,FVector(50,-100,110),FVector(50,100,110),ECC_Visibility,Params) && Hit.GetActor()==Door);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingDoorSafetyTest,"CityCourier.Buildings.Rules.DoorConfigurationSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FBuildingDoorSafetyTest::RunTest(const FString&)
{
    FStationFixture F; F.Rider->SetActorLocation(FVector(50,-120,90)); F.Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    auto* Door=F.World->SpawnActor<ACourierDoor>(); FCourierDoorDefinition D; D.Id=TEXT("UnsafeDoor"); D.bExternal=true; D.Width=100;
    Door->Configure(D,FTransform::Identity);
    TestFalse(TEXT("External aperture below 120cm cannot become ready"),Door->TryToggle(F.Rider).bSucceeded);
    D.Width=120; FCourierDoorState State; State.Angle=120;
    Door->Configure(D,FTransform::Identity,State);
    TestFalse(TEXT("Out-of-range restored angle cannot start a motion"),Door->IsMoving());
    return true;
}
#endif
