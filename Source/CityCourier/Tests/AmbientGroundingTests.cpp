#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/WorldTestSupport.h"
#include "World/CityDefinition.h"
#include "Ambient/AmbientTrafficManager.h"
#include "Ambient/AmbientVehicle.h"
#include "Ambient/AmbientPedestrian.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbientGroundSpawnTest,"CityCourier.Ambient.Grounding.IgnoresCanopiesAndTraffic",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAmbientGroundSpawnTest::RunTest(const FString&)
{
    FTestCourierWorld F;
    F.Box(FVector(0,0,-50),FVector(20000,20000,50));
    auto* City=NewObject<UCityDefinition>();
    City->Roads.Nodes={{0,FVector(0,0,8)},{1,FVector(6000,0,8)}};
    City->Roads.Edges={{0,1,60,true}};
    auto* Manager=F.World->SpawnActor<AAmbientTrafficManager>();Manager->Configure(City);
    // A parked dynamic car and a 3.1m-high awning must never be used as street ground.
    auto* Parked=F.World->SpawnActor<AAmbientVehicle>(FVector(900,-220,70),FRotator::ZeroRotator);
    F.Box(FVector(900,620,300),FVector(650,110,10));
    F.Box(FVector(900,-620,300),FVector(650,110,10));
    Manager->UpdatePopulation(FVector(-4000,0,90));
    int32 Cars=0,Walkers=0;
    for(TActorIterator<AAmbientVehicle> It(F.World);It;++It)
    {
        if(*It==Parked || !It->IsPopulationActive())continue;
        const bool bWalker=It->IsA<AAmbientPedestrian>();
        if(bWalker)++Walkers;else ++Cars;
        TestTrue(*FString::Printf(TEXT("%s spawns on the actual street, not a car/awning; z=%.1f"),*It->GetName(),It->GetActorLocation().Z),
            FMath::IsNearlyEqual(It->GetActorLocation().Z,bWalker?90.:70.,.5));
    }
    TestTrue(TEXT("Cars can still populate a clear lane"),Cars>0);
    TestTrue(TEXT("Walkers can still populate underneath the awnings"),Walkers>0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbientGroundTravelTest,"CityCourier.Ambient.Grounding.FollowsStreetHeight",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAmbientGroundTravelTest::RunTest(const FString&)
{
    FTestCourierWorld F;
    F.Box(FVector(0,0,-50),FVector(10000,10000,50));
    // Leave a raised sidewalk and continue onto the lower street.
    F.Box(FVector(0,1000,7),FVector(500,200,7));
    auto* Walker=F.World->SpawnActor<AAmbientPedestrian>();
    Walker->SetPatrol({FVector(0,1000,104),FVector(4000,1000,90)});Walker->SetPopulationActive(true);
    for(int32 I=0;I<600;++I)Walker->Tick(1.f/60);
    TestTrue(TEXT("Walker actually crosses the sidewalk end"),Walker->GetActorLocation().X>800);
    TestTrue(TEXT("Walker feet follow the lower road instead of retaining sidewalk height"),FMath::IsNearlyEqual(Walker->GetActorLocation().Z,90.,.5));

    auto* Car=F.World->SpawnActor<AAmbientVehicle>();
    Car->SetPatrol({FVector(0,-1000,70),FVector(6000,-1000,70)});Car->SetPopulationActive(true);
    Car->SetActorLocation(FVector(0,-1000,370));
    for(int32 I=0;I<120;++I)Car->Tick(1.f/60);
    TestTrue(TEXT("Reused vehicle recovers from an elevated transform"),FMath::IsNearlyEqual(Car->GetActorLocation().Z,70.,.5));
    TestTrue(TEXT("Recovered vehicle keeps driving"),Car->GetActorLocation().X>100);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAmbientMissingGroundTest,"CityCourier.Ambient.Grounding.NoUnsupportedMovement",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAmbientMissingGroundTest::RunTest(const FString&)
{
    FTestCourierWorld F;
    auto* Car=F.World->SpawnActor<AAmbientVehicle>();
    Car->SetPatrol({FVector(0,0,70),FVector(6000,0,70)});Car->SetPopulationActive(true);
    for(int32 I=0;I<120;++I)Car->Tick(1.f/60);
    TestFalse(TEXT("No visible floating vehicle when the street collision is absent"),Car->IsPopulationActive());
    TestTrue(TEXT("No movement without supporting street collision"),FMath::IsNearlyZero(Car->GetActorLocation().X));
    return true;
}
#endif
