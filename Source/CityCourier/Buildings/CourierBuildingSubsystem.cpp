#include "Buildings/CourierBuildingSubsystem.h"
#include "Buildings/CourierBuildingActor.h"
#include "World/DeliveryLocation.h"
#include "World/RiderStation.h"
#include "Buildings/CourierBuildingResidency.h"
#include "Delivery/DeliverySubsystem.h"
#include "Player/CourierInteractable.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/PackageName.h"

bool UCourierBuildingSubsystem::Configure(UCityBuildingCatalog* Input)
{
    if(bEnabled || !Input) return false;
    TArray<FString> Errors; if(!Input->Validate(Errors)) return false;
    TMap<int32,TObjectPtr<ACourierBuildingActor>> Found;
    for(TActorIterator<ACourierBuildingActor> It(GetWorld());It;++It)
    {
        const auto* D=It->GetDefinition(); const int32 I=D?Input->FindBuilding(D->Id):INDEX_NONE;
        if(I==INDEX_NONE || Found.Contains(I)) return false; Found.Add(I,*It);
    }
    if(Found.Num()!=Input->Buildings.Num()) return false;
    Catalog=Input; Actors=MoveTemp(Found); DetailStates.Init(ECourierBuildingDetailState::Unloaded,Catalog->Buildings.Num());
    for(int32 I=0;I<Catalog->Buildings.Num();++I)
    {
        Actors[I]->EnsureEntranceBarriers(); Actors[I]->SetEntryLoading(true);
        const auto& B=Catalog->Buildings[I];
        for(int32 J=0;J<B.Doors.Num();++J)
        {
            DoorLookup.Add(B.Doors[J].Id,FIntPoint(I,J));
            DoorCells.FindOrAdd(Cell(B.ToWorld(B.Doors[J].Hinge))).Add(FIntPoint(I,J));
        }
    }
    for(TActorIterator<AActor> It(GetWorld());It;++It) CacheActor(*It);
    SpawnHandle=GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this,&UCourierBuildingSubsystem::CacheActor));
    bEnabled=true; BindLocations(); return true;
}
void UCourierBuildingSubsystem::Tick(float Delta)
{
    if(!bEnabled || GetWorld()->IsPaused()) return;
    const auto* PC=GetWorld()->GetFirstPlayerController(); const APawn* Pawn=PC?PC->GetPawn():nullptr; if(!Pawn) return;
    const FVector P=Pawn->GetActorLocation(); UpdateTime+=Delta;
    if(UpdateTime>=.2f)
    {
        UpdateTime=0; FName Target;
        if(const auto* Delivery=GetWorld()->GetSubsystem<UDeliverySubsystem>())
            if(const auto* Binding=Catalog->FindBinding(Delivery->GetTargetId())) Target=Binding->BuildingId;
        UpdateResidency(P,Target); RefreshDoors(P);
    }
    PrepareNext();
}
void UCourierBuildingSubsystem::Deinitialize()
{
    bEnabled=false;
    if(GetWorld()) GetWorld()->RemoveOnActorSpawnedHandler(SpawnHandle);
    TArray<FName> Ids; ActiveDoors.GetKeys(Ids); for(FName Id:Ids) DeactivateDoor(Id);
    for(auto& P:Actors) if(IsValid(P.Value)) P.Value->ReleaseDetails();
    Actors.Reset(); Locations.Reset(); Catalog=nullptr; Super::Deinitialize();
}
FCourierIndoorAddress UCourierBuildingSubsystem::Locate(FVector P) const
{
    if(!bEnabled || P.ContainsNaN()) return {};
    for(int32 I=0;I<Catalog->Buildings.Num();++I)
    {
        const auto& B=Catalog->Buildings[I]; const int32 R=B.LocateRoom(P);
        if(R!=INDEX_NONE) return {I,R,B.Rooms[R].Floor};
    }
    return {};
}
void UCourierBuildingSubsystem::UpdateResidency(FVector P,FName Target)
{
    if(!bEnabled || P.ContainsNaN()) return; LastPosition=P;
    const auto Address=Locate(P); const int32 TargetIndex=Catalog->FindBuilding(Target);
    TArray<FCourierBuildingResidencyEntry> Entries;
    for(int32 I=0;I<Catalog->Buildings.Num();++I)
    {
        const auto& B=Catalog->Buildings[I];
        const auto* D=B.Doors.FindByPredicate([](const auto& Door){return Door.bExternal;});
        const FVector Entry=B.ToWorld(D?D->Hinge+FRotator(0,D->ClosedYaw,0).RotateVector(FVector(D->Width/2,0,0)):FVector::ZeroVector);
        FCourierBuildingResidencyEntry E; E.Index=I; E.DistanceCm=FVector::Dist2D(P,Entry);
        E.bReady=DetailStates[I]==ECourierBuildingDetailState::Ready;
        E.bPinned=Address.Building==I || TargetIndex==I || EntryRequest==I; Entries.Add(E);
    }
    Desired.Reset(); for(int32 I:FCourierBuildingResidency::Select(Entries)) Desired.Add(I);
    for(int32 I=0;I<DetailStates.Num();++I)
    {
        if(!Desired.Contains(I) && DetailStates[I]!=ECourierBuildingDetailState::Unloaded)
        {
            Actors[I]->ReleaseDetails(); DetailStates[I]=ECourierBuildingDetailState::Unloaded; PendingLoads.Remove(I);
        }
        if(Desired.Contains(I) && DetailStates[I]==ECourierBuildingDetailState::Unloaded)
        { DetailStates[I]=ECourierBuildingDetailState::Loading; PendingLoads.AddUnique(I); }
        Actors[I]->SetEntryLoading(DetailStates[I]!=ECourierBuildingDetailState::Ready && Address.Building!=I);
    }
    for(auto& Pair:ActiveDoors)
    {
        const int32 I=DoorLookup[Pair.Key].X;
        Pair.Value->SetReady(DetailStates[I]==ECourierBuildingDetailState::Ready);
    }
}
bool UCourierBuildingSubsystem::PrepareNext()
{
    if(PendingLoads.IsEmpty()) return false;
    const int32 I=PendingLoads[0]; PendingLoads.RemoveAt(0);
    if(!Desired.Contains(I)) return false;
    const bool Ready=Actors[I]->PrepareDetails();
    DetailStates[I]=Ready?ECourierBuildingDetailState::Ready:ECourierBuildingDetailState::Failed;
    Actors[I]->SetEntryLoading(!Ready && Locate(LastPosition).Building!=I);
    if(EntryRequest==I && Ready) EntryRequest=INDEX_NONE;
    for(auto& Pair:ActiveDoors) if(DoorLookup[Pair.Key].X==I) Pair.Value->SetReady(Ready);
    if(!Ready) UE_LOG(LogTemp,Error,TEXT("Interior detail preparation failed: %s"),*Catalog->Buildings[I].Id.ToString());
    return Ready;
}
ECourierBuildingDetailState UCourierBuildingSubsystem::GetDetailState(int32 I) const
{ return DetailStates.IsValidIndex(I)?DetailStates[I]:ECourierBuildingDetailState::Unloaded; }
void UCourierBuildingSubsystem::RequestBuilding(int32 I)
{ if(Actors.Contains(I)) { EntryRequest=I; UpdateResidency(LastPosition); } }
ACourierDoor* UCourierBuildingSubsystem::ActivateDoor(FName Id)
{
    if(auto* Existing=ActiveDoors.Find(Id)) return Existing->Get();
    const auto* Lookup=DoorLookup.Find(Id); if(!Lookup) return nullptr;
    const auto& B=Catalog->Buildings[Lookup->X];
    FActorSpawnParameters Params; Params.Owner=Actors[Lookup->X]; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Door=GetWorld()->SpawnActor<ACourierDoor>(ACourierDoor::StaticClass(),FTransform::Identity,Params); if(!Door) return nullptr;
    Door->Configure(B.Doors[Lookup->Y],B.Transform(),DoorStates.FindRef(Id));
    Door->SetReady(GetDetailState(Lookup->X)==ECourierBuildingDetailState::Ready);
    if(!Actors[Lookup->X]->SetDoorProxy(Lookup->Y,0,false)) { Door->Destroy(); return nullptr; }
    Door->OnDoorState.AddWeakLambda(this,[this](FName Key,const FCourierDoorState& State){DoorStates.Add(Key,State);});
    ActiveDoors.Add(Id,Door); return Door;
}
void UCourierBuildingSubsystem::RefreshDoors(FVector P)
{
    if(!bEnabled) return;
    const FIntVector Center=Cell(P);
    for(int32 X=-1;X<=1;++X) for(int32 Y=-1;Y<=1;++Y) for(int32 Z=-1;Z<=1;++Z)
        if(const auto* List=DoorCells.Find(Center+FIntVector(X,Y,Z))) for(const auto& Index:*List)
        {
            const auto& B=Catalog->Buildings[Index.X]; const auto& D=B.Doors[Index.Y];
            const double Distance=FVector::Dist(P,B.ToWorld(D.Hinge));
            if(Distance>950) continue;
            if(D.bExternal && Distance<300 && GetDetailState(Index.X)==ECourierBuildingDetailState::Unloaded) RequestBuilding(Index.X);
            ActivateDoor(D.Id);
        }
    DeactivateFarDoors(P);
}
void UCourierBuildingSubsystem::DeactivateFarDoors(FVector P)
{
    TArray<FName> Remove;
    for(const auto& Pair:ActiveDoors) if(!IsValid(Pair.Value) || FVector::DistSquared(P,Pair.Value->GetActorLocation())>1200*1200) Remove.Add(Pair.Key);
    for(FName Id:Remove) DeactivateDoor(Id);
}
void UCourierBuildingSubsystem::DeactivateDoor(FName Id)
{
    auto* Door=ActiveDoors.FindRef(Id).Get(); if(!Door) { ActiveDoors.Remove(Id); return; }
    const auto* I=DoorLookup.Find(Id); DoorStates.Add(Id,Door->GetDoorState());
    Door->SetActorEnableCollision(false);
    if(I && Actors.Contains(I->X)) Actors[I->X]->SetDoorProxy(I->Y,Door->GetDoorState().Angle,true);
    Door->Destroy(); ActiveDoors.Remove(Id);
}
TArray<ACourierDoor*> UCourierBuildingSubsystem::GetNearbyDoors(FVector P,double Radius) const
{
    TArray<ACourierDoor*> Result;
    for(const auto& Pair:ActiveDoors) if(IsValid(Pair.Value) && FVector::DistSquared(P,Pair.Value->GetInteractionLocation())<=Radius*Radius) Result.Add(Pair.Value);
    return Result;
}
TArray<AActor*> UCourierBuildingSubsystem::GetInteractionCandidates(FVector P,double Radius) const
{
    TArray<AActor*> Result; for(auto* D:GetNearbyDoors(P,Radius)) Result.Add(D);
    for(const auto& Weak:OtherInteractables) if(auto* A=Weak.Get())
        if(const auto* I=Cast<ICourierInteractable>(A)) if(FVector::DistSquared(P,I->GetInteractionLocation())<=Radius*Radius) Result.Add(A);
    return Result;
}
ADeliveryLocation* UCourierBuildingSubsystem::GetManagedLocation(FName Id) const
{ auto* L=Locations.FindRef(Id).Get(); return IsValid(L)?L:nullptr; }
int32 UCourierBuildingSubsystem::GetReadyCount() const
{ int32 N=0; for(auto S:DetailStates) N+=S==ECourierBuildingDetailState::Ready; return N; }
void UCourierBuildingSubsystem::CacheActor(AActor* A)
{ if(IsValid(A) && !A->IsA<ACourierDoor>() && Cast<ICourierInteractable>(A)) OtherInteractables.AddUnique(A); }
void UCourierBuildingSubsystem::BindLocations()
{
    for(const auto& Binding:Catalog->Bindings)
    {
        ADeliveryLocation* Location=nullptr;
        for(TActorIterator<ADeliveryLocation> It(GetWorld());It;++It) if(It->LocationId==Binding.LocationId) { Location=*It; break; }
        const int32 I=Catalog->FindBuilding(Binding.BuildingId); if(I==INDEX_NONE) continue;
        const auto& B=Catalog->Buildings[I]; const int32 R=B.FindRoom(Binding.RoomId); if(R==INDEX_NONE) continue;
        if(!Location)
        {
            Location=Binding.Role==TEXT("station")?GetWorld()->SpawnActor<ARiderStation>():GetWorld()->SpawnActor<ADeliveryLocation>(); Location->LocationId=Binding.LocationId;
            Location->LocationKind=Binding.Role==TEXT("station")?EDeliveryLocationKind::Station:Binding.Role==TEXT("merchant")?EDeliveryLocationKind::Merchant:EDeliveryLocationKind::Destination;
        }
        RegisterIndoorLocation(Location);
        if(auto* Delivery=GetWorld()->GetSubsystem<UDeliverySubsystem>())
        { if(Delivery->GetCity()) Location->CityDefinition=Delivery->GetCity(); Delivery->RegisterLocation(Location); }
    }
}
FIntVector UCourierBuildingSubsystem::Cell(FVector P)
{ return FIntVector(FMath::FloorToInt(P.X/1000),FMath::FloorToInt(P.Y/1000),FMath::FloorToInt(P.Z/1000)); }
bool UCourierBuildingSubsystem::RegisterIndoorLocation(ADeliveryLocation* L)
{
    if(!bEnabled || !IsValid(L) || L->GetWorld()!=GetWorld()) return false;
    const auto* Binding=Catalog->FindBinding(L->LocationId); if(!Binding) return false;
    const int32 I=Catalog->FindBuilding(Binding->BuildingId); if(I==INDEX_NONE) return false;
    const auto& B=Catalog->Buildings[I]; const int32 R=B.FindRoom(Binding->RoomId); if(R==INDEX_NONE) return false;
    auto* Previous=Locations.FindRef(L->LocationId).Get();
    if(IsValid(Previous) && Previous!=L) return false;
    L->BuildingId=B.Id; L->RoomId=Binding->RoomId; L->StreetAnchor=B.ToWorld(B.Nodes[B.EntranceNode].Position);
    L->SetActorLocation(B.ToWorld(B.Rooms[R].StandingPoint)); Locations.Add(L->LocationId,L); CacheActor(L); return true;
}
void UCourierBuildingSubsystem::UnregisterIndoorLocation(ADeliveryLocation* L)
{ if(L && Locations.FindRef(L->LocationId)==L) Locations.Remove(L->LocationId); }
bool UCourierBuildingSubsystem::CanPrepareLocation(FName Id) const
{
    if(!bEnabled || !IsValid(GetManagedLocation(Id))) return false;
    const auto* Binding=Catalog->FindBinding(Id); if(!Binding) return false;
    const int32 I=Catalog->FindBuilding(Binding->BuildingId);
    if(I==INDEX_NONE || GetDetailState(I)==ECourierBuildingDetailState::Failed) return false;
    for(const FString Asset:{TEXT("SM_CourierDoor"),TEXT("SM_InteriorBed"),TEXT("SM_InteriorDesk"),TEXT("SM_InteriorKitchen"),TEXT("SM_InteriorToilet"),TEXT("SM_InteriorShelf"),TEXT("SM_InteriorSofa"),TEXT("SM_InteriorCounter")})
        if(!FPackageName::DoesPackageExist(TEXT("/Game/CityCourier/Art/Buildings/")+Asset)) return false;
    return true;
}
bool UCourierBuildingSubsystem::CanUseLocation(FName Id,FVector P,FText& Reason) const
{
    if(!CanPrepareLocation(Id)) { Reason=FText::FromString(TEXT("室内资源暂不可用，请返回驿站")); return false; }
    const auto* Binding=Catalog->FindBinding(Id); const int32 I=Catalog->FindBuilding(Binding->BuildingId);
    if(GetDetailState(I)!=ECourierBuildingDetailState::Ready) { Reason=FText::FromString(TEXT("室内正在准备，请稍候")); return false; }
    const auto A=Locate(P);
    if(!A.IsValid() || A.Building!=I || Catalog->Buildings[I].Rooms[A.Room].Id!=Binding->RoomId)
    { Reason=FText::FromString(TEXT("请进入目标楼层和房间后再操作")); return false; }
    return true;
}
