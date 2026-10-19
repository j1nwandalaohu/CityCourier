#include "Delivery/NavigationGuidance.h"

FNavigationGuidance FNavigationGuidance::FromRoute(const FRouteResult& Route,FVector GroundPosition,double ViewYaw)
{
    FNavigationGuidance Cue;
    if(!Route.bReachable || Route.Points.Num()<2 || GroundPosition.ContainsNaN() || !FMath::IsFinite(ViewYaw))return Cue;
    for(const FVector& Point:Route.Points)if(Point.ContainsNaN())return Cue;
    auto ForDistance=[&](FVector P){if(!Route.bHasInteriors)P.Z=0;return P;};
    const FVector Position=ForDistance(GroundPosition);
    int32 Segment=INDEX_NONE;double Closest=MAX_dbl;FVector Projection=Position;
    for(int32 I=0;I<Route.Points.Num()-1;++I)
    {
        const FVector A=ForDistance(Route.Points[I]),B=ForDistance(Route.Points[I+1]);
        if(FVector::DistSquared(A,B)<UE_SMALL_NUMBER)continue;
        const FVector P=FMath::ClosestPointOnSegment(Position,A,B);
        const double Distance=FVector::DistSquared(Position,P);
        if(Distance<Closest){Closest=Distance;Segment=I;Projection=P;}
    }
    if(Segment==INDEX_NONE)return Cue;
    // Return to the route before advancing, so an off-route rider isn't sent
    // diagonally across a block. At a corner, advance only within one meter.
    if(Closest>FMath::Square(250.))Cue.NextPoint=Projection;
    else
    {
        while(Segment<Route.Points.Num()-2 && FVector::DistSquared(Position,ForDistance(Route.Points[Segment+1]))<=FMath::Square(100.))++Segment;
        Cue.NextPoint=Route.Points[Segment+1];
    }
    const FVector Delta=Cue.NextPoint-GroundPosition;
    const bool Stairs=Route.SegmentKinds.IsValidIndex(Segment) && Route.SegmentKinds[Segment]==TEXT("stairs");
    Cue.bUpstairs=Route.bHasInteriors && Delta.Z>(Stairs?2.:80.);
    Cue.bDownstairs=Route.bHasInteriors && Delta.Z<-(Stairs?2.:80.);
    Cue.bHasDirection=Delta.SizeSquared2D()>1 || Cue.bUpstairs || Cue.bDownstairs;
    Cue.HeadingDegrees=Delta.SizeSquared2D()>1?FMath::UnwindDegrees(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X))-ViewYaw):0;
    Cue.WaypointMeters=(Route.bHasInteriors?Delta.Size():Delta.Size2D())/100.;
    return Cue;
}
