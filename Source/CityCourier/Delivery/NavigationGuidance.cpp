#include "Delivery/NavigationGuidance.h"

FNavigationRouteProjection FNavigationRouteProjection::FromRoute(const FRouteResult& Route,FVector GroundPosition,int32 FirstSegment)
{
    FNavigationRouteProjection Result;
    if(!Route.bReachable || Route.Points.Num()<2 || GroundPosition.ContainsNaN())return Result;
    for(const FVector& Point:Route.Points)if(Point.ContainsNaN())return Result;
    auto ForDistance=[&](FVector P){if(!Route.bHasInteriors)P.Z=0;return P;};
    const FVector Position=ForDistance(GroundPosition);
    double Closest=MAX_dbl,PreviousClosest=MAX_dbl,Traversed=0;
    FNavigationRouteProjection Previous;
    FirstSegment=FMath::Clamp(FirstSegment,0,Route.Points.Num()-2);
    for(int32 I=0;I<Route.Points.Num()-1;++I)
    {
        const FVector A=ForDistance(Route.Points[I]),B=ForDistance(Route.Points[I+1]);
        const double Length=FVector::Distance(A,B);
        if(Length>UE_SMALL_NUMBER)
        {
            const FVector P=FMath::ClosestPointOnSegment(Position,A,B);
            const double Distance=FVector::DistSquared(Position,P);
            auto& Candidate=I<FirstSegment?Previous:Result;
            double& Best=I<FirstSegment?PreviousClosest:Closest;
            if(Distance<Best)
            {
                Best=Distance; Candidate.Segment=I; Candidate.Point=P;
                Candidate.ProgressCentimeters=Traversed+FVector::Distance(A,P);
            }
        }
        Traversed+=Length;
    }
    // Backing away from a corner can legitimately return to an earlier leg.
    // A one-metre advantage avoids flipping to a completed overlapping connector.
    if(Previous.Segment!=INDEX_NONE && (Result.Segment==INDEX_NONE || FMath::Sqrt(PreviousClosest)+100.<FMath::Sqrt(Closest)))
    { Result=Previous; Closest=PreviousClosest; }
    Result.LengthCentimeters=Traversed;
    if(Result.Segment!=INDEX_NONE)
    {
        // Reaching the end commits this leg, including out-and-back connectors
        // whose next leg occupies exactly the same ground as the previous one.
        const int32 Selected=Result.Segment;
        while(Result.Segment<Route.Points.Num()-2 && FVector::DistSquared(Position,ForDistance(Route.Points[Result.Segment+1]))<=FMath::Square(100.))++Result.Segment;
        if(Result.Segment!=Selected)
        {
            const FVector A=ForDistance(Route.Points[Result.Segment]),B=ForDistance(Route.Points[Result.Segment+1]);
            Result.Point=FMath::ClosestPointOnSegment(Position,A,B);
            Closest=FVector::DistSquared(Position,Result.Point);
            Result.ProgressCentimeters=FVector::Distance(A,Result.Point);
            for(int32 I=0;I<Result.Segment;++I)Result.ProgressCentimeters+=FVector::Distance(ForDistance(Route.Points[I]),ForDistance(Route.Points[I+1]));
        }
        Result.DistanceCentimeters=FMath::Sqrt(Closest);
    }
    return Result;
}

FNavigationGuidance FNavigationGuidance::FromRoute(const FRouteResult& Route,FVector GroundPosition,double ViewYaw,int32 FirstSegment)
{
    FNavigationGuidance Cue;
    if(!FMath::IsFinite(ViewYaw))return Cue;
    const auto Projection=FNavigationRouteProjection::FromRoute(Route,GroundPosition,FirstSegment);
    if(Projection.Segment==INDEX_NONE)return Cue;
    const int32 Segment=Projection.Segment;
    // Return to the route before advancing, so an off-route rider isn't sent
    // diagonally across a block. At a corner, advance only within one meter.
    if(Projection.DistanceCentimeters>250.)Cue.NextPoint=Projection.Point;
    else Cue.NextPoint=Route.Points[Segment+1];
    const FVector Delta=Cue.NextPoint-GroundPosition;
    const bool Stairs=Route.SegmentKinds.IsValidIndex(Segment) && Route.SegmentKinds[Segment]==TEXT("stairs");
    Cue.bUpstairs=Route.bHasInteriors && Delta.Z>(Stairs?2.:80.);
    Cue.bDownstairs=Route.bHasInteriors && Delta.Z<-(Stairs?2.:80.);
    Cue.bHasDirection=Delta.SizeSquared2D()>1 || Cue.bUpstairs || Cue.bDownstairs;
    Cue.HeadingDegrees=Delta.SizeSquared2D()>1?FMath::UnwindDegrees(FMath::RadiansToDegrees(FMath::Atan2(Delta.Y,Delta.X))-ViewYaw):0;
    Cue.WaypointMeters=(Route.bHasInteriors?Delta.Size():Delta.Size2D())/100.;
    return Cue;
}
