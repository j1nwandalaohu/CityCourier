#include "World/CityDefinition.h"

namespace
{
struct FProjection
{
    const FRoadEdge* Edge;
    FVector Point;
    double Fraction;
    double OffsetMeters;
};
TArray<FProjection> ProjectNearest(const FRoadGraph& Roads,FVector Position)
{
    TArray<FProjection> Result; double Nearest=MAX_dbl;
    for(const auto& Edge:Roads.Edges)
    {
        const auto* A=Roads.FindNode(Edge.From); const auto* B=Roads.FindNode(Edge.To);
        if(!A || !B || Edge.LengthMeters<=0) continue;
        const double Length=FVector::Distance(A->Position,B->Position); if(Length<.01) continue;
        const FVector Point=FMath::ClosestPointOnSegment(Position,A->Position,B->Position);
        const double Distance=FVector::DistSquared2D(Position,Point);
        if(Distance>Nearest+1) continue;
        if(Distance<Nearest-1) { Result.Reset(); Nearest=Distance; }
        Result.Add({&Edge,Point,FVector::Distance(A->Position,Point)/Length,FRoadGraph::DistanceMeters(Position,Point)});
    }
    return Result;
}
}

FRouteResult UCityDefinition::FindRouteToPosition(FVector From,FVector To) const
{
    FRouteResult Best; TArray<FString> Errors;
    if(From.ContainsNaN() || To.ContainsNaN() || !Roads.Validate(Errors)) return Best;
    const auto Starts=ProjectNearest(Roads,From), Ends=ProjectNearest(Roads,To);
    auto Consider=[&](FRouteResult Route,const FProjection& Start,const FProjection& End)
    {
        if(!Route.bReachable) return;
        Route.LengthMeters+=Start.OffsetMeters+End.OffsetMeters;
        Route.Points.Insert(From,0); Route.Points.Add(To);
        if(!Best.bReachable || Route.LengthMeters<Best.LengthMeters) Best=MoveTemp(Route);
    };
    for(const auto& S:Starts) for(const auto& E:Ends)
    {
        if(S.Edge==E.Edge && (S.Edge->bBidirectional || E.Fraction>=S.Fraction))
        {
            FRouteResult Direct; Direct.bReachable=true; Direct.LengthMeters=FMath::Abs(E.Fraction-S.Fraction)*S.Edge->LengthMeters;
            Direct.Points={S.Point,E.Point}; Consider(MoveTemp(Direct),S,E);
        }
        for(int32 Exit=0;Exit<2;++Exit) for(int32 Entry=0;Entry<2;++Entry)
        {
            if(Exit==0 && !S.Edge->bBidirectional && S.Fraction>UE_SMALL_NUMBER) continue;
            if(Entry==1 && !E.Edge->bBidirectional && E.Fraction<1-UE_SMALL_NUMBER) continue;
            auto Route=Roads.FindRoute(Exit==0?S.Edge->From:S.Edge->To,Entry==0?E.Edge->From:E.Edge->To);
            if(!Route.bReachable) continue;
            Route.LengthMeters+=S.Edge->LengthMeters*(Exit==0?S.Fraction:1-S.Fraction)+E.Edge->LengthMeters*(Entry==0?E.Fraction:1-E.Fraction);
            Route.Points.Insert(S.Point,0); Route.Points.Add(E.Point); Consider(MoveTemp(Route),S,E);
        }
    }
    return Best;
}
