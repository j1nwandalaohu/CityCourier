#include "World/CityDefinition.h"
#include "Buildings/CourierBuildingDefinition.h"
#include "Buildings/CourierInteriorRouting.h"

namespace
{
struct FAddress { int32 Building=INDEX_NONE,Room=INDEX_NONE,Node=INDEX_NONE; FVector Foot; };
FAddress Locate(const UCityBuildingCatalog& C,FVector P)
{
    FAddress A; A.Foot=P;
    for(int32 I=0;I<C.Buildings.Num();++I)
    {
        const auto& B=C.Buildings[I]; const int32 R=B.LocateRoom(P); if(R==INDEX_NONE) continue;
        A.Building=I; A.Room=R; const auto& Room=B.Rooms[R]; A.Node=Room.RouteNode;
        if(Room.Kind==TEXT("hall") || Room.Kind==TEXT("stairs"))
        {
            FVector Foot=P; Foot.Z=Room.Kind==TEXT("stairs")?P.Z-88:B.Origin.Z+Room.Minimum.Z;
            double Best=MAX_dbl;
            for(int32 N=0;N<B.Nodes.Num();++N) if(B.Nodes[N].RoomId==Room.Id)
            {
                const double D=FVector::DistSquared(Foot,B.ToWorld(B.Nodes[N].Position));
                if(D<Best) { Best=D; A.Node=N; }
            }
            A.Foot=Foot;
        }
        else A.Foot.Z=B.Origin.Z+Room.Minimum.Z;
        return A;
    }
    // Outdoor callers may supply a capsule center; roads are ground-level.
    A.Foot.Z=0; return A;
}
void AddPoint(FRouteResult& R,FVector P,FName Kind)
{
    if(!R.Points.IsEmpty() && FVector::DistSquared(R.Points.Last(),P)<.01) return;
    if(!R.Points.IsEmpty()) R.SegmentKinds.Add(Kind);
    R.Points.Add(P);
}
bool AddIndoor(FRouteResult& Out,const FCourierBuildingDefinition& B,int32 From,int32 To)
{
    const auto R=FCourierInteriorRouting::FindRoute(B,From,To); if(!R.bReachable) return false;
    Out.bHasInteriors=true;
    for(int32 I=0;I<R.Nodes.Num();++I)
    {
        FName Kind=TEXT("flat");
        if(I>0)
        {
            const auto* E=B.Edges.FindByPredicate([&](const auto& Edge){return (Edge.From==R.Nodes[I-1] && Edge.To==R.Nodes[I]) || (Edge.To==R.Nodes[I-1] && Edge.From==R.Nodes[I]);});
            if(!E) return false; Kind=E->Kind;
        }
        AddPoint(Out,R.Points[I],Kind);
    }
    Out.IndoorFlatMeters+=R.FlatMeters; Out.StairMeters+=R.StairMeters;
    for(FName D:R.Doors) Out.RequiredDoors.AddUnique(D);
    return true;
}
// Visibility corners keep courtyard access off solid footprints. Road graph
// costs remain separate from indoor distances and logical openable doors.
TArray<FVector> OutdoorDetour(FVector From,FVector To,const UCityBuildingCatalog& C)
{
    From.Z=To.Z=0; TArray<FBox> Obstacles;
    for(const auto& B:C.Buildings)
    {
        FBox Box(EForceInit::ForceInit);
        for(int32 X:{-1,1}) for(int32 Y:{-1,1}) Box+=B.ToWorld(FVector(X*B.Size.X/2,Y*B.Size.Y/2,0));
        Box.Min.Z=-1000; Box.Max.Z=10000; Box=Box.ExpandBy(FVector(40,40,0)); Obstacles.Add(Box);
    }
    auto Clear=[&](FVector A,FVector Z)
    {
        for(const auto& Box:Obstacles) if(FMath::LineBoxIntersection(Box,A,Z,Z-A)) return false;
        return true;
    };
    if(Clear(From,To)) return {From,To};
    for(double Padding:{5000.,20000.})
    {
        const FBox Region(FVector(FMath::Min(From.X,To.X)-Padding,FMath::Min(From.Y,To.Y)-Padding,-1),FVector(FMath::Max(From.X,To.X)+Padding,FMath::Max(From.Y,To.Y)+Padding,1));
        TArray<FVector> Nodes{From,To};
        for(const auto& Box:Obstacles) for(int32 X:{0,1}) for(int32 Y:{0,1})
        {
            FVector P(X?Box.Max.X+2:Box.Min.X-2,Y?Box.Max.Y+2:Box.Min.Y-2,0);
            if(!Region.IsInsideOrOn(P)) continue;
            bool Inside=false; for(const auto& Other:Obstacles) if(Other.IsInsideOrOn(P)) { Inside=true; break; }
            if(!Inside) Nodes.Add(P);
        }
        TArray<double> Cost; Cost.Init(MAX_dbl,Nodes.Num()); Cost[0]=0;
        TArray<int32> Previous; Previous.Init(INDEX_NONE,Nodes.Num()); TArray<bool> Done; Done.Init(false,Nodes.Num());
        for(int32 Step=0;Step<Nodes.Num();++Step)
        {
            int32 Current=INDEX_NONE; double Best=MAX_dbl;
            for(int32 I=0;I<Nodes.Num();++I) if(!Done[I] && Cost[I]!=MAX_dbl)
            { const double F=Cost[I]+FVector::Distance(Nodes[I],To); if(F<Best) { Best=F; Current=I; } }
            if(Current==INDEX_NONE) break;
            if(Current==1)
            {
                TArray<FVector> Path; for(int32 I=1;I!=INDEX_NONE;I=Previous[I]) Path.Insert(Nodes[I],0); return Path;
            }
            Done[Current]=true;
            for(int32 I=0;I<Nodes.Num();++I) if(!Done[I])
            {
                const double Next=Cost[Current]+FVector::Distance(Nodes[Current],Nodes[I]);
                if(Next<Cost[I] && Clear(Nodes[Current],Nodes[I])) { Cost[I]=Next; Previous[I]=Current; }
            }
        }
    }
    return {};
}
FVector BindingPosition(const UCityBuildingCatalog& C,FName Id,bool& Valid)
{
    Valid=false; const auto* Binding=C.FindBinding(Id); if(!Binding) return {};
    const int32 I=C.FindBuilding(Binding->BuildingId); if(I==INDEX_NONE) return {};
    const auto& B=C.Buildings[I]; const int32 R=B.FindRoom(Binding->RoomId); if(R==INDEX_NONE) return {};
    Valid=true; return B.ToWorld(B.Rooms[R].StandingPoint);
}
}
FRouteResult UCityDefinition::FindJourneyToPosition(FVector From,FVector To,const UCityBuildingCatalog& C) const
{
    if(From.ContainsNaN() || To.ContainsNaN()) return {};
    const FAddress A=Locate(C,From),Z=Locate(C,To); FRouteResult Out;
    AddPoint(Out,A.Foot,TEXT("flat"));
    auto Finish=[&]()
    {
        if(Z.Building!=INDEX_NONE)
        {
            const auto& B=C.Buildings[Z.Building]; Out.TargetBuilding=B.Id; Out.TargetRoom=B.Rooms[Z.Room].Id; Out.TargetFloor=B.Rooms[Z.Room].Floor;
        }
        Out.LengthMeters=Out.RoadMeters+Out.IndoorFlatMeters+Out.StairMeters; Out.bReachable=true; return Out;
    };
    if(A.Building!=INDEX_NONE && A.Building==Z.Building)
    {
        const auto& B=C.Buildings[A.Building];
        Out.IndoorFlatMeters+=FVector::Distance(A.Foot,B.ToWorld(B.Nodes[A.Node].Position))/100;
        if(!AddIndoor(Out,B,A.Node,Z.Node)) return {};
        Out.IndoorFlatMeters+=FVector::Distance(Z.Foot,Out.Points.Last())/100; AddPoint(Out,Z.Foot,TEXT("flat")); return Finish();
    }
    FVector Start=A.Foot,End=Z.Foot;
    if(A.Building!=INDEX_NONE)
    {
        const auto& B=C.Buildings[A.Building];
        Out.IndoorFlatMeters+=FVector::Distance(A.Foot,B.ToWorld(B.Nodes[A.Node].Position))/100;
        if(!AddIndoor(Out,B,A.Node,B.EntranceNode)) return {}; Start=Out.Points.Last();
    }
    if(Z.Building!=INDEX_NONE) { const auto& B=C.Buildings[Z.Building]; End=B.ToWorld(B.Nodes[B.EntranceNode].Position); }
    const auto Road=FindRouteToPosition(Start,End); if(!Road.bReachable) return {};
    for(int32 I=1;I<Road.Points.Num();++I)
    {
        const auto Path=OutdoorDetour(Road.Points[I-1],Road.Points[I],C); if(Path.IsEmpty()) return {};
        for(int32 J=1;J<Path.Num();++J) { Out.RoadMeters+=FVector::Distance(Path[J-1],Path[J])/100; AddPoint(Out,Path[J],TEXT("road")); }
    }
    if(Z.Building!=INDEX_NONE)
    {
        const auto& B=C.Buildings[Z.Building]; if(!AddIndoor(Out,B,B.EntranceNode,Z.Node)) return {};
        Out.IndoorFlatMeters+=FVector::Distance(Out.Points.Last(),Z.Foot)/100; AddPoint(Out,Z.Foot,TEXT("flat"));
    }
    return Finish();
}
FRouteResult UCityDefinition::FindJourneyFrom(FVector From,FName Destination,const UCityBuildingCatalog& C) const
{
    if(!FindLocation(Destination)) return {};
    bool Valid; const FVector To=BindingPosition(C,Destination,Valid); return Valid?FindJourneyToPosition(From,To,C):FRouteResult{};
}
FRouteResult UCityDefinition::FindJourneyBetween(FName From,FName To,const UCityBuildingCatalog& C) const
{
    bool Valid; const FVector P=BindingPosition(C,From,Valid); return Valid?FindJourneyFrom(P,To,C):FRouteResult{};
}
