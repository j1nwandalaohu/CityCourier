#include "World/RoadGraph.h"
#include <limits>

const FRoadNode* FRoadGraph::FindNode(int32 Id) const
{
    return Nodes.FindByPredicate([Id](const FRoadNode& N) { return N.Id == Id; });
}
bool FRoadGraph::Validate(TArray<FString>& Errors) const
{
    Errors.Empty(); TSet<int32> IDs;
    if (Nodes.IsEmpty()) Errors.Add(TEXT("Road graph has no nodes"));
    for (const auto& N : Nodes)
    {
        if (N.Id < 0 || IDs.Contains(N.Id) || N.Position.ContainsNaN())
            Errors.Add(FString::Printf(TEXT("Invalid or duplicate road node %d"), N.Id));
        IDs.Add(N.Id);
    }
    for (const auto& E : Edges)
        if (!IDs.Contains(E.From) || !IDs.Contains(E.To) || !FMath::IsFinite(E.LengthMeters) || E.LengthMeters < 0)
            Errors.Add(FString::Printf(TEXT("Invalid road edge %d -> %d"), E.From, E.To));
    return Errors.IsEmpty();
}
int32 FRoadGraph::FindNearestNode(FVector Position) const
{
    if (Position.ContainsNaN()) return -1;
    int32 Best = -1; double BestDistance = std::numeric_limits<double>::infinity();
    for (const auto& N : Nodes)
    {
        const double Distance = FVector::DistSquared(Position, N.Position);
        if (Distance < BestDistance) { BestDistance = Distance; Best = N.Id; }
    }
    return Best;
}
FRouteResult FRoadGraph::FindRoute(int32 From, int32 To) const
{
    FRouteResult Result; TArray<FString> Errors;
    if (!Validate(Errors) || !FindNode(From) || !FindNode(To)) return Result;
    TMap<int32, double> Distances; TMap<int32, int32> Previous; TSet<int32> Visited;
    for (const auto& N : Nodes) Distances.Add(N.Id, std::numeric_limits<double>::infinity());
    Distances[From] = 0;
    while (Visited.Num() < Nodes.Num())
    {
        int32 Current = -1; double Best = std::numeric_limits<double>::infinity();
        for (const auto& N : Nodes)
            if (!Visited.Contains(N.Id) && Distances[N.Id] < Best) { Best = Distances[N.Id]; Current = N.Id; }
        if (Current < 0) break;
        if (Current == To)
        {
            Result.bReachable = true; Result.LengthMeters = Best;
            for (int32 Node = To;; Node = Previous[Node])
            {
                Result.Points.Insert(FindNode(Node)->Position, 0);
                if (Node == From) break;
            }
            return Result;
        }
        Visited.Add(Current);
        for (const auto& E : Edges)
        {
            const int32 Next = E.From == Current ? E.To : (E.bBidirectional && E.To == Current ? E.From : -1);
            if (Next < 0 || Visited.Contains(Next)) continue;
            const double Candidate = Best + E.LengthMeters;
            if (FMath::IsFinite(Candidate) && Candidate < Distances[Next]) { Distances[Next] = Candidate; Previous.Add(Next, Current); }
        }
    }
    return Result;
}
