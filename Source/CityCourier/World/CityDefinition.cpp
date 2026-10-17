#include "World/CityDefinition.h"

const FCourierLocationDefinition* UCityDefinition::FindLocation(FName Id) const
{
    if (Station.LocationId == Id && !Id.IsNone()) return &Station;
    for (const auto& L : Merchants) if (L.LocationId == Id) return &L;
    for (const auto& L : Destinations) if (L.LocationId == Id) return &L;
    return nullptr;
}
int64 UCityDefinition::GetFee(FName Merchant, FName Destination) const
{
    for (const auto& F : Fees) if (F.MerchantId == Merchant && F.DestinationId == Destination) return F.FeeCents;
    return -1;
}
FRouteResult UCityDefinition::FindRouteBetween(const FCourierLocationDefinition& From, const FCourierLocationDefinition& To) const
{
    auto Route = Roads.FindRoute(From.EntranceNode, To.EntranceNode);
    if (Route.bReachable && !From.Position.ContainsNaN() && !To.Position.ContainsNaN())
    {
        Route.LengthMeters += FRoadGraph::DistanceMeters(From.Position, Route.Points[0]) + FRoadGraph::DistanceMeters(To.Position, Route.Points.Last());
        Route.Points.Insert(From.Position, 0); Route.Points.Add(To.Position);
    }
    else Route = {};
    return Route;
}
FRouteResult UCityDefinition::FindRouteFrom(FVector Position, FName Destination) const
{
    const auto* To = FindLocation(Destination);
    if (!To || Position.ContainsNaN()) return {};
    FRouteResult Best; double Closest=MAX_dbl;
    for (const auto& Edge : Roads.Edges)
    {
        const auto* A=Roads.FindNode(Edge.From); const auto* B=Roads.FindNode(Edge.To);
        if (!A || !B || Edge.LengthMeters<=0) continue;
        const FVector Projection=FMath::ClosestPointOnSegment(Position,A->Position,B->Position);
        const double Offset=FVector::DistSquared2D(Position,Projection);
        if (Offset>Closest+1) continue;
        if (Offset<Closest-1) { Best={}; Closest=Offset; }
        const double Fraction=FVector::Distance(A->Position,Projection)/FMath::Max(1.0,FVector::Distance(A->Position,B->Position));
        for(int32 Side=0;Side<2;++Side)
        {
            if (Side==0 && !Edge.bBidirectional) continue;
            auto Route=Roads.FindRoute(Side==0?Edge.From:Edge.To,To->EntranceNode);
            if (!Route.bReachable) continue;
            Route.LengthMeters+=FRoadGraph::DistanceMeters(Position,Projection)+Edge.LengthMeters*(Side==0?Fraction:1-Fraction)
                +FRoadGraph::DistanceMeters(Route.Points.Last(),To->Position);
            Route.Points.Insert(Projection,0); Route.Points.Insert(Position,0); Route.Points.Add(To->Position);
            if (!Best.bReachable || Route.LengthMeters<Best.LengthMeters) Best=MoveTemp(Route);
        }
    }
    return Best;
}
bool UCityDefinition::Validate(TArray<FString>& Errors) const
{
    Roads.Validate(Errors);
    if (Merchants.Num() != 4 || Destinations.Num() != 8 || Fees.Num() != 32)
        Errors.Add(TEXT("City requires 4 merchants, 8 recipients and 32 fees"));
    TSet<FName> IDs;
    auto CheckLocation = [&](const FCourierLocationDefinition& L, EDeliveryLocationKind Kind)
    {
        if (L.LocationId.IsNone() || IDs.Contains(L.LocationId) || L.Kind != Kind || L.Position.ContainsNaN() ||
            !Roads.FindNode(L.EntranceNode) || L.DisplayName.IsEmpty())
            Errors.Add(TEXT("Invalid location: ") + L.LocationId.ToString());
        IDs.Add(L.LocationId);
        if (!Roads.FindRoute(Station.EntranceNode, L.EntranceNode).bReachable)
            Errors.Add(TEXT("Unreachable from station: ") + L.LocationId.ToString());
    };
    CheckLocation(Station, EDeliveryLocationKind::Station);
    for (const auto& L : Merchants) CheckLocation(L, EDeliveryLocationKind::Merchant);
    for (const auto& L : Destinations) CheckLocation(L, EDeliveryLocationKind::Destination);
    TSet<FString> Pairs;
    for (const auto& F : Fees)
    {
        const auto* M = FindLocation(F.MerchantId); const auto* D = FindLocation(F.DestinationId);
        const FString Key = F.MerchantId.ToString() + TEXT("/") + F.DestinationId.ToString();
        if (Pairs.Contains(Key) || !M || !D || M->Kind != EDeliveryLocationKind::Merchant || D->Kind != EDeliveryLocationKind::Destination ||
            F.FeeCents < 1600 || F.FeeCents > 3200 || !FindRouteBetween(*M, *D).bReachable)
            Errors.Add(TEXT("Invalid order pair: ") + Key);
        Pairs.Add(Key);
    }
    for (const auto& M : Merchants) for (const auto& D : Destinations)
        if (GetFee(M.LocationId, D.LocationId) < 0) Errors.Add(TEXT("Missing order pair: ") + M.LocationId.ToString() + TEXT("/") + D.LocationId.ToString());
    return Errors.IsEmpty();
}
