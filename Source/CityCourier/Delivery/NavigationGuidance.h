#pragma once
#include "World/RoadGraph.h"

/** The next safe route leg, expressed relative to the viewing camera. */
struct CITYCOURIER_API FNavigationGuidance
{
    bool bHasDirection=false;
    bool bUpstairs=false;
    bool bDownstairs=false;
    FVector NextPoint=FVector::ZeroVector;
    double HeadingDegrees=0;
    double WaypointMeters=0;
    static FNavigationGuidance FromRoute(const FRouteResult& Route,FVector GroundPosition,double ViewYaw);
};
