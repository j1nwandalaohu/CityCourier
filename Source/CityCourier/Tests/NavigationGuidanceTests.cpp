#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Delivery/NavigationGuidance.h"
#include <limits>

namespace
{
FRouteResult RightTurn()
{
    FRouteResult R;R.bReachable=true;R.Points={FVector(0,0,0),FVector(1000,0,0),FVector(1000,1000,0)};return R;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavigationArrowTurnsTest,"CityCourier.Navigation.Arrow.CameraAndRouteTurns",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNavigationArrowTurnsTest::RunTest(const FString&)
{
    const auto R=RightTurn();
    const auto Before=FNavigationGuidance::FromRoute(R,FVector(300,0,0),0);
    TestTrue(TEXT("A reachable route supplies a direction"),Before.bHasDirection);
    TestTrue(TEXT("Follow the next road corner rather than cutting diagonally to the destination"),Before.NextPoint.Equals(FVector(1000,0,0)));
    TestTrue(TEXT("Forward is ahead on screen"),FMath::IsNearlyZero(Before.HeadingDegrees));
    const auto Corner=FNavigationGuidance::FromRoute(R,FVector(1000,0,0),0);
    TestTrue(TEXT("At a right corner the pointer turns right"),FMath::IsNearlyEqual(Corner.HeadingDegrees,90.));
    auto Left=R;Left.Points.Last().Y=-1000;
    TestTrue(TEXT("At a left corner the pointer turns left"),FMath::IsNearlyEqual(FNavigationGuidance::FromRoute(Left,FVector(1000,0,0),0).HeadingDegrees,-90.));
    for(double Camera:{-180.,-90.,0.,45.,90.,180.,270.})
    {
        const auto Cue=FNavigationGuidance::FromRoute(R,FVector(300,0,0),Camera);
        TestTrue(TEXT("Camera yaw determines the on-screen bearing"),FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Cue.HeadingDegrees,-Camera)));
    }
    const auto OffRoad=FNavigationGuidance::FromRoute(R,FVector(500,-500,0),0);
    TestTrue(TEXT("Off-route guidance returns to the nearest road before following it"),OffRoad.NextPoint.Equals(FVector(500,0,0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavigationArrowFloorsTest,"CityCourier.Navigation.Arrow.FloorsAndInvalidRoutes",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNavigationArrowFloorsTest::RunTest(const FString&)
{
    auto R=RightTurn();R.bHasInteriors=true;
    R.Points.Append({FVector(1000,1000,400),FVector(1000,0,400),FVector(0,0,400)});
    R.SegmentKinds={TEXT("hall"),TEXT("hall"),TEXT("stairs"),TEXT("hall"),TEXT("hall")};
    const auto Upper=FNavigationGuidance::FromRoute(R,FVector(500,0,400),0);
    TestTrue(TEXT("Overlapping floors use the current floor"),Upper.bHasDirection && Upper.NextPoint.Equals(FVector(0,0,400)));
    const auto Up=FNavigationGuidance::FromRoute(R,FVector(1000,1000,0),0);
    TestTrue(TEXT("Vertical stair section indicates upstairs without a bogus horizontal bearing"),Up.bUpstairs && FMath::IsFinite(Up.HeadingDegrees));
    Algo::Reverse(R.Points);Algo::Reverse(R.SegmentKinds);
    const auto Down=FNavigationGuidance::FromRoute(R,FVector(1000,1000,400),0);
    TestTrue(TEXT("Descending stair section indicates downstairs"),Down.bDownstairs);
    R.bReachable=false;
    TestFalse(TEXT("No misleading arrow for an unreachable route"),FNavigationGuidance::FromRoute(R,FVector::ZeroVector,0).bHasDirection);
    R={};R.bReachable=true;R.Points={FVector::ZeroVector,FVector::ZeroVector};
    TestFalse(TEXT("Repeated zero-length points do not create a direction"),FNavigationGuidance::FromRoute(R,FVector::ZeroVector,0).bHasDirection);
    R=RightTurn();
    TestFalse(TEXT("Nonfinite position is rejected"),FNavigationGuidance::FromRoute(R,FVector(std::numeric_limits<double>::quiet_NaN(),0,0),0).bHasDirection);
    TestFalse(TEXT("Nonfinite camera heading is rejected"),FNavigationGuidance::FromRoute(R,FVector::ZeroVector,std::numeric_limits<double>::quiet_NaN()).bHasDirection);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavigationArrowProgressTest,"CityCourier.Navigation.Arrow.OverlappingRouteProgress",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FNavigationArrowProgressTest::RunTest(const FString&)
{
    FRouteResult R; R.bReachable=true;
    R.Points={FVector(0,0,0),FVector(1000,0,0),FVector(1000,1000,0),FVector(1000,0,0),FVector(0,0,0)};
    const auto Outbound=FNavigationGuidance::FromRoute(R,FVector(500,0,0),0);
    TestTrue(TEXT("First pass follows outbound road"),Outbound.NextPoint.Equals(FVector(1000,0,0)));
    const auto Returning=FNavigationGuidance::FromRoute(R,FVector(500,0,0),0,3);
    TestTrue(TEXT("Return pass never selects the already completed overlapping road"),Returning.NextPoint.Equals(FVector::ZeroVector));
    TestTrue(TEXT("Return bearing points toward the destination"),FMath::IsNearlyEqual(FMath::Abs(Returning.HeadingDegrees),180.));
    const auto Projection=FNavigationRouteProjection::FromRoute(R,FVector(500,0,0),3);
    TestEqual(TEXT("Distance progress agrees with arrow on return leg"),Projection.Segment,3);
    TestTrue(TEXT("Only five metres remain on the return leg"),FMath::IsNearlyEqual(Projection.LengthCentimeters-Projection.ProgressCentimeters,500.));
    const auto Backtracking=FNavigationGuidance::FromRoute(RightTurn(),FVector(800,0,0),0,1);
    TestTrue(TEXT("Backing away from an advanced corner recovers previous road without cutting diagonally"),Backtracking.NextPoint.Equals(FVector(1000,0,0)));
    return true;
}
#endif
