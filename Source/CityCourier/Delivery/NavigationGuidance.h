#pragma once
#include "World/RoadGraph.h"

/** Shared projection for distance, deviation and arrow guidance. */
struct CITYCOURIER_API FNavigationRouteProjection
{
    int32 Segment=INDEX_NONE;
    FVector Point=FVector::ZeroVector;
    double DistanceCentimeters=0;
    double ProgressCentimeters=0;
    double LengthCentimeters=0;
    static FNavigationRouteProjection FromRoute(const FRouteResult& Route,FVector GroundPosition,int32 FirstSegment=0);
};

/** The next safe route leg, expressed relative to the viewing camera. */
struct CITYCOURIER_API FNavigationGuidance
{
    bool bHasDirection=false;
    bool bUpstairs=false;
    bool bDownstairs=false;
    FVector NextPoint=FVector::ZeroVector;
    double HeadingDegrees=0;
    double WaypointMeters=0;
    static FNavigationGuidance FromRoute(const FRouteResult& Route,FVector GroundPosition,double ViewYaw,int32 FirstSegment=0);
};
