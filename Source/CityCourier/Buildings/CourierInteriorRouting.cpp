#include "Buildings/CourierInteriorRouting.h"

FCourierIndoorRoute FCourierInteriorRouting::FindRoute(const FCourierBuildingDefinition& B,int32 From,int32 To)
{
    FCourierIndoorRoute Out;
    if(!B.Nodes.IsValidIndex(From) || !B.Nodes.IsValidIndex(To)) return Out;
    TArray<double> Cost; Cost.Init(MAX_dbl,B.Nodes.Num()); Cost[From]=0;
    TArray<int32> Previous,PreviousEdge; Previous.Init(INDEX_NONE,B.Nodes.Num()); PreviousEdge.Init(INDEX_NONE,B.Nodes.Num());
    TArray<bool> Visited; Visited.Init(false,B.Nodes.Num());
    TArray<TArray<int32>> Links; Links.SetNum(B.Nodes.Num());
    for(int32 I=0;I<B.Edges.Num();++I)
    {
        const auto& E=B.Edges[I];
        if(!B.Nodes.IsValidIndex(E.From) || !B.Nodes.IsValidIndex(E.To) || !FMath::IsFinite(E.LengthMeters) || E.LengthMeters<=0) continue;
        Links[E.From].Add(I); Links[E.To].Add(I);
    }
    for(int32 Iteration=0;Iteration<B.Nodes.Num();++Iteration)
    {
        int32 Current=INDEX_NONE; double Best=MAX_dbl;
        for(int32 I=0;I<Cost.Num();++I) if(!Visited[I] && Cost[I]<Best) { Current=I; Best=Cost[I]; }
        if(Current==INDEX_NONE) break;
        if(Current==To) break;
        Visited[Current]=true;
        for(int32 EI:Links[Current])
        {
            const auto& E=B.Edges[EI]; const int32 Next=E.From==Current?E.To:E.From;
            if(Best+E.LengthMeters<Cost[Next]) { Cost[Next]=Best+E.LengthMeters; Previous[Next]=Current; PreviousEdge[Next]=EI; }
        }
    }
    if(Cost[To]==MAX_dbl) return Out;
    TArray<int32> Edges;
    for(int32 Current=To;Current!=INDEX_NONE;Current=Previous[Current])
    {
        Out.Nodes.Insert(Current,0); if(PreviousEdge[Current]!=INDEX_NONE) Edges.Insert(PreviousEdge[Current],0);
        if(Current==From) break;
    }
    for(int32 N:Out.Nodes) Out.Points.Add(B.ToWorld(B.Nodes[N].Position));
    for(int32 EI:Edges)
    {
        const auto& E=B.Edges[EI];
        if(E.Kind==TEXT("stairs")) Out.StairMeters+=E.LengthMeters; else Out.FlatMeters+=E.LengthMeters;
        if(E.Kind==TEXT("door")) Out.Doors.AddUnique(E.DoorId);
    }
    Out.bReachable=true; return Out;
}
