#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "World/RoadGraph.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoadGraphTest, "CityCourier.Roads.Routing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoadGraphTest::RunTest(const FString& Parameters)
{
    FRoadGraph G;
    G.Nodes = {{0, FVector(0,0,0)}, {1, FVector(10000,0,0)}, {2, FVector(10000,10000,0)}, {3, FVector(30000,0,0)}};
    G.Edges = {{0,1,100,false}, {1,2,100,true}, {0,2,300,false}};
    TArray<FString> Errors;
    TestTrue(TEXT("Valid roads"), G.Validate(Errors));
    auto Route = G.FindRoute(0,2);
    TestTrue(TEXT("Route connected"), Route.bReachable);
    TestEqual(TEXT("Choose weighted shortest path"), Route.LengthMeters, 200.0);
    TestEqual(TEXT("Route includes junction"), Route.Points.Num(), 3);
    TestFalse(TEXT("One-way cannot reverse"), G.FindRoute(1,0).bReachable);
    TestTrue(TEXT("Bidirectional can reverse"), G.FindRoute(2,1).bReachable);
    TestFalse(TEXT("Disconnected node rejected"), G.FindRoute(0,3).bReachable);
    TestTrue(TEXT("Same valid node reachable"), G.FindRoute(1,1).bReachable);
    TestEqual(TEXT("Same node length zero"), G.FindRoute(1,1).LengthMeters, 0.0);
    TestFalse(TEXT("Unknown same node invalid"), G.FindRoute(99,99).bReachable);
    TestEqual(TEXT("Nearest junction"), G.FindNearestNode(FVector(9900,100,0)), 1);
    TestEqual(TEXT("Centimeters converted to meters"), FRoadGraph::DistanceMeters(G.Nodes[0].Position, G.Nodes[1].Position), 100.0);
    for (int32 Bad = 0; Bad < 5; ++Bad)
    {
        FRoadGraph Invalid = G;
        if (Bad == 0) Invalid.Nodes.Add(G.Nodes[0]);
        if (Bad == 1) Invalid.Edges[0].To = 99;
        if (Bad == 2) Invalid.Edges[0].LengthMeters = -1;
        if (Bad == 3) Invalid.Edges[0].LengthMeters = std::numeric_limits<double>::infinity();
        if (Bad == 4) Invalid.Nodes[0].Position.X = std::numeric_limits<double>::quiet_NaN();
        TestFalse(TEXT("Malformed graph rejected"), Invalid.Validate(Errors));
        TestTrue(TEXT("Error supplied"), Errors.Num() > 0);
        TestFalse(TEXT("Malformed graph cannot create offer route"), Invalid.FindRoute(0,1).bReachable);
    }
    return true;
}
#endif
