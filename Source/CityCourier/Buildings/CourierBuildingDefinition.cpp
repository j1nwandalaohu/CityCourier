#include "Buildings/CourierBuildingDefinition.h"
#if WITH_EDITOR
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#endif

int32 FCourierBuildingDefinition::FindRoom(FName RoomId) const
{ return Rooms.IndexOfByPredicate([&](const auto& R){return R.Id==RoomId;}); }
int32 FCourierBuildingDefinition::LocateRoom(FVector World) const
{
    if(World.ContainsNaN()) return INDEX_NONE;
    const FVector Local=ToLocal(World); int32 Best=INDEX_NONE; double BestVolume=MAX_dbl;
    for(int32 I=0;I<Rooms.Num();++I)
    {
        const auto& R=Rooms[I]; if(!R.Contains(Local)) continue;
        const FVector S=R.Maximum-R.Minimum; double Volume=S.X*S.Y*S.Z;
        // A real room takes priority over its public/circulation envelope.
        if(R.Kind==TEXT("roof")) Volume*=100;
        if(Volume<BestVolume) { Best=I; BestVolume=Volume; }
    }
    return Best;
}
bool FCourierBuildingDefinition::Validate(TArray<FString>& Errors) const
{
    const int32 Before=Errors.Num();
    auto Error=[&](const TCHAR* What){Errors.Add(Id.ToString()+TEXT(": ")+What);};
    if(Id.IsNone() || Origin.ContainsNaN() || !FMath::IsFinite(Yaw) || Size.ContainsNaN() || Size.GetMin()<=0 || !Nodes.IsValidIndex(EntranceNode))
    { Error(TEXT("invalid identity, bounds or entrance")); return false; }
    TSet<int32> FloorNumbers; TSet<FName> FloorIds,RoomIds,NodeIds,DoorIds;
    float Previous=-MAX_flt;
    for(const auto& F:Floors)
    {
        if(F.Id.IsNone() || F.Number<=0 || FloorIds.Contains(F.Id) || FloorNumbers.Contains(F.Number) || !FMath::IsFinite(F.Elevation) || F.Elevation<=Previous) Error(TEXT("invalid floor"));
        FloorNumbers.Add(F.Number); FloorIds.Add(F.Id); Previous=F.Elevation;
    }
    for(const auto& R:Rooms)
    {
        if(R.Id.IsNone() || RoomIds.Contains(R.Id) || !FloorNumbers.Contains(R.Floor) || !Nodes.IsValidIndex(R.RouteNode) || R.Minimum.ContainsNaN() || R.Maximum.ContainsNaN() || R.StandingPoint.ContainsNaN() || (R.Maximum-R.Minimum).GetMin()<=0)
            Error(TEXT("invalid room"));
        RoomIds.Add(R.Id);
    }
    TSet<FName> Expected; for(FName IdValue:ExpectedRoomIds) { if(Expected.Contains(IdValue)) Error(TEXT("duplicate expected room")); Expected.Add(IdValue); }
    if(Expected.Num()==0 || Expected.Num()!=RoomIds.Num()) Error(TEXT("incomplete expected room inventory"));
    for(FName IdValue:Expected) if(!RoomIds.Contains(IdValue)) Error(TEXT("missing expected room"));
    for(const auto& N:Nodes)
    {
        if(N.Id.IsNone() || NodeIds.Contains(N.Id) || N.Position.ContainsNaN() || (N.RoomId!=TEXT("Outside") && !RoomIds.Contains(N.RoomId))) Error(TEXT("invalid route node"));
        NodeIds.Add(N.Id);
    }
    for(const auto& D:Doors)
    {
        if(D.Id.IsNone() || DoorIds.Contains(D.Id) || !Nodes.IsValidIndex(D.NodeA) || !Nodes.IsValidIndex(D.NodeB) || D.NodeA==D.NodeB ||
           D.Hinge.ContainsNaN() || !FMath::IsFinite(D.ClosedYaw) || !FMath::IsFinite(D.OpenAngle) || FMath::Abs(D.OpenAngle)<1 || FMath::Abs(D.OpenAngle)>180 ||
           !FMath::IsFinite(D.Width) || !FMath::IsFinite(D.Height) || D.Width<(D.bExternal?120:100) || D.Height<220 ||
           (D.FromRoom!=TEXT("Outside") && !RoomIds.Contains(D.FromRoom)) || (D.ToRoom!=TEXT("Outside") && !RoomIds.Contains(D.ToRoom))) Error(TEXT("invalid door"));
        DoorIds.Add(D.Id);
    }
    TArray<TArray<int32>> Graph; Graph.SetNum(Nodes.Num()); TSet<uint64> EdgePairs;
    for(const auto& E:Edges)
    {
        if(!Nodes.IsValidIndex(E.From) || !Nodes.IsValidIndex(E.To) || E.From==E.To || !FMath::IsFinite(E.LengthMeters) || E.LengthMeters<=0 ||
           (E.Kind!=TEXT("flat") && E.Kind!=TEXT("stairs") && E.Kind!=TEXT("door")) || (E.Kind==TEXT("door") && !DoorIds.Contains(E.DoorId)))
        { Error(TEXT("invalid route edge")); continue; }
        const uint64 Pair=(uint64(FMath::Min(E.From,E.To))<<32)|uint32(FMath::Max(E.From,E.To));
        if(EdgePairs.Contains(Pair)) Error(TEXT("duplicate route edge")); EdgePairs.Add(Pair);
        if(!FMath::IsNearlyEqual(E.LengthMeters,FVector::Distance(Nodes[E.From].Position,Nodes[E.To].Position)/100,.01)) Error(TEXT("route length mismatch"));
        Graph[E.From].Add(E.To); Graph[E.To].Add(E.From);
    }
    TSet<int32> Reached; TArray<int32> Pending{EntranceNode};
    while(!Pending.IsEmpty())
    {
        const int32 N=Pending.Pop(EAllowShrinking::No); if(Reached.Contains(N)) continue;
        Reached.Add(N); for(int32 Next:Graph[N]) if(!Reached.Contains(Next)) Pending.Add(Next);
    }
    if(Reached.Num()!=Nodes.Num()) Error(TEXT("disconnected building space"));
    for(const auto& B:Boxes) if(B.Center.ContainsNaN() || B.Size.ContainsNaN() || B.Size.GetMin()<=0 || !FMath::IsFinite(B.Yaw)) Error(TEXT("invalid structural box"));
    return Errors.Num()==Before;
}
int32 UCityBuildingCatalog::FindBuilding(FName Id) const
{ return Buildings.IndexOfByPredicate([&](const auto& B){return B.Id==Id;}); }
const FCourierIndoorBinding* UCityBuildingCatalog::FindBinding(FName Id) const
{ return Bindings.FindByPredicate([&](const auto& B){return B.LocationId==Id;}); }
bool UCityBuildingCatalog::Validate(TArray<FString>& Errors) const
{
    const int32 Before=Errors.Num(); TSet<FName> Actual,Expected,AllRooms,AllDoors,LocationIds;
    if(SchemaVersion!=1 || Buildings.IsEmpty()) Errors.Add(TEXT("Unsupported or empty building catalog"));
    for(FName Id:ExpectedBuildingIds) { if(Id.IsNone() || Expected.Contains(Id)) Errors.Add(TEXT("Invalid expected building ID")); Expected.Add(Id); }
    for(const auto& B:Buildings)
    {
        if(Actual.Contains(B.Id)) Errors.Add(TEXT("Duplicate building ID")); Actual.Add(B.Id); B.Validate(Errors);
        for(const auto& R:B.Rooms) { if(AllRooms.Contains(R.Id)) Errors.Add(TEXT("Duplicate global room ID")); AllRooms.Add(R.Id); }
        for(const auto& D:B.Doors) { if(AllDoors.Contains(D.Id)) Errors.Add(TEXT("Duplicate global door ID")); AllDoors.Add(D.Id); }
    }
    if(Actual.Num()!=Expected.Num()) Errors.Add(TEXT("Building coverage does not match expected inventory"));
    for(FName Id:Expected) if(!Actual.Contains(Id)) Errors.Add(TEXT("Missing expected building: ")+Id.ToString());
    for(const auto& L:Bindings)
    {
        const int32 BI=FindBuilding(L.BuildingId);
        if(L.LocationId.IsNone() || LocationIds.Contains(L.LocationId) || L.StreetPosition.ContainsNaN() || BI==INDEX_NONE || Buildings[BI].FindRoom(L.RoomId)==INDEX_NONE) Errors.Add(TEXT("Invalid indoor location binding"));
        LocationIds.Add(L.LocationId);
    }
    return Errors.Num()==Before;
}
bool UCityBuildingCatalog::ValidateCatalog() const
{
    TArray<FString> Errors; const bool Valid=Validate(Errors);
    for(const auto& Error:Errors) UE_LOG(LogTemp,Error,TEXT("Building catalog: %s"),*Error);
    return Valid;
}
bool UCityBuildingCatalog::ImportJsonFile(const FString& Filename)
{
#if WITH_EDITOR
    FString Text; if(!FFileHelper::LoadFileToString(Text,*Filename)) return false;
    TSharedPtr<FJsonObject> Json;
    if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json) || !Json.IsValid()) return false;
    auto* Candidate=NewObject<UCityBuildingCatalog>(); FText Reason;
    if(!FJsonObjectConverter::JsonObjectToUStruct(Json.ToSharedRef(),Candidate,0,CPF_Transient,false,&Reason))
    { UE_LOG(LogTemp,Error,TEXT("Building import rejected: %s"),*Reason.ToString()); return false; }
    if(!Candidate->ValidateCatalog()) return false;
    Modify(); SchemaVersion=Candidate->SchemaVersion; SourceDigest=MoveTemp(Candidate->SourceDigest);
    Buildings=MoveTemp(Candidate->Buildings); Bindings=MoveTemp(Candidate->Bindings); ExpectedBuildingIds=MoveTemp(Candidate->ExpectedBuildingIds);
    MarkPackageDirty(); return true;
#else
    return false;
#endif
}
