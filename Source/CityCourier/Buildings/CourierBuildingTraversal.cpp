#include "Buildings/CourierBuildingActor.h"
#include "Buildings/CourierDoor.h"
#include "Player/CourierCharacter.h"
#include "Player/CourierController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Misc/ScopeExit.h"

FCourierPhysicalTraversalReport ACourierBuildingActor::ValidatePhysicalTraversal(bool WithDetails)
{
    FCourierPhysicalTraversalReport Out;
#if !UE_BUILD_SHIPPING
    const auto* B=GetDefinition();
    if(!B || !B->Validate(Out.Errors)) { Out.Errors.Add(TEXT("Invalid canonical building")); return Out; }
    if(!GetActorTransform().Equals(B->Transform(),.01)) { Out.Errors.Add(B->Id.ToString()+TEXT(": wrong actual instance transform")); return Out; }
    const bool HadDetails=GetDetailComponentCount()>0;
    if(WithDetails && !PrepareDetails()) { Out.Errors.Add(B->Id.ToString()+TEXT(": required details unavailable")); return Out; }
    const FVector Start=B->ToWorld(B->Nodes[B->EntranceNode].Position)+FVector(0,0,90);
    FActorSpawnParameters Params; Params.ObjectFlags|=RF_Transient; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Rider=GetWorld()->SpawnActor<ACourierCharacter>(ACourierCharacter::StaticClass(),Start,FRotator::ZeroRotator,Params);
    if(!Rider) { Out.Errors.Add(TEXT("Traversal rider could not spawn")); return Out; }
    Rider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Rider->RecalculateBaseEyeHeight();
    auto* Movement=Rider->GetCharacterMovement(); Movement->MaxWalkSpeed=240;
    Movement->SetUpdatedComponent(Rider->GetCapsuleComponent());
    if(!Movement->HasBeenInitialized()) Movement->InitializeComponent();
    Movement->bUpdateOnlyIfRendered=false;
    Movement->bRunPhysicsWithNoController=true;
    const bool WasLoading=!EntryBarriers.IsEmpty() && EntryBarriers[0]->GetCollisionResponseToChannel(ECC_Pawn)==ECR_Block;
    SetEntryLoading(false);
    TMap<FName,ACourierDoor*> Doors;
    TMap<int32,float> SavedProxyAngles;
    for(int32 I=0;I<B->Doors.Num();++I)
    {
        // Validation owns temporary leaves; preserve the production proxy state.
        FTransform T; const int32 Proxy=ProxyDoors.IndexOfByKey(I);
        if(Proxy!=INDEX_NONE && DoorProxies->GetInstanceTransform(Proxy,T,false))
            SavedProxyAngles.Add(I,FMath::FindDeltaAngleDegrees(B->Doors[I].ClosedYaw,T.Rotator().Yaw));
        else { Out.Errors.Add(TEXT("Traversal requires inactive production door actors")); break; }
        Params.Owner=this; // Same building/style context as production ActivateDoor.
        auto* Door=GetWorld()->SpawnActor<ACourierDoor>(ACourierDoor::StaticClass(),FVector::ZeroVector,FRotator::ZeroRotator,Params);
        Door->Configure(B->Doors[I],B->Transform()); Door->SetReady(true);
        SetDoorProxy(I,0,false); Doors.Add(B->Doors[I].Id,Door);
    }
    ON_SCOPE_EXIT
    {
        Rider->Destroy();
        for(auto& P:Doors) { P.Value->SetActorEnableCollision(false); P.Value->Destroy(); }
        for(auto& P:SavedProxyAngles) SetDoorProxy(P.Key,P.Value,true);
        if(!HadDetails) ReleaseDetails();
        SetEntryLoading(WasLoading);
    };
    if(!Out.Errors.IsEmpty()) return Out;
    auto Walk=[&](int32 From,int32 To,const FCourierBuildingEdge& Edge)
    {
        FVector Destination=B->ToWorld(B->Nodes[To].Position)+FVector(0,0,90);
        if(B->Nodes[To].RoomId==TEXT("Outside"))
        {
            FHitResult Ground; FCollisionQueryParams Query(SCENE_QUERY_STAT(TraversalOutdoorGround),false,Rider);
            if(GetWorld()->LineTraceSingleByChannel(Ground,Destination+FVector(0,0,60),Destination-FVector(0,0,190),ECC_Visibility,Query) && Ground.Normal.Z>=.7)
                Destination.Z=Ground.ImpactPoint.Z+90;
        }
        const double MaxSeconds=FVector::Distance(Rider->GetActorLocation(),Destination)/100+5;
        FVector Previous=Rider->GetActorLocation(); double Stuck=0;
        ACourierDoor* Door=Doors.FindRef(Edge.DoorId);
        for(double Elapsed=0;Elapsed<MaxSeconds;Elapsed+=.05)
        {
            const FVector Position=Rider->GetActorLocation(); FVector Delta=Destination-Position;
            if(Delta.Size2D()<6 && FMath::Abs(Delta.Z)<25 && !Movement->IsFalling()) { Movement->StopMovementImmediately(); return true; }
            if(Door && !Door->GetDoorState().bWantOpen && FVector::Dist(Position,Door->GetInteractionLocation())<195)
            {
                const auto Result=Door->TryToggle(Rider);
                if(!Result.bSucceeded) { Out.Errors.Add(Door->GetDefinition().Id.ToString()+TEXT(": opening rejected: ")+Result.Reason.ToString()); return false; }
                Door->AdvanceDoor(.6f);
                if(Door->GetDoorState().bBlocked) { Out.Errors.Add(Door->GetDefinition().Id.ToString()+TEXT(": opening sweep blocked")); return false; }
                ++Out.OpenedDoors;
            }
            const float Dt=FMath::Clamp(Delta.Size2D()/240.f,.01f,.05f);
            Rider->AddMovementInput(Delta.GetSafeNormal2D(),1,true);
            // Same walking/step-up/gravity/capsule sweeps used by real play;
            // only the wall-clock scheduling of these fixed steps is accelerated.
            Movement->TickComponent(Dt,LEVELTICK_All,nullptr);
            const FVector Now=Rider->GetActorLocation(); const double Travel=FVector::Distance(Previous,Now);
            Out.WalkedMeters+=Travel/100; Stuck=Travel<.05?Stuck+Dt:0; Previous=Now;
            if(Stuck>.6 || Now.Z<Start.Z-150) break;
        }
        Out.Errors.Add(FString::Printf(TEXT("%s: physical traversal blocked %s -> %s at %s, target %s, movement %d, role %d, data %d, skip %d, input %s, velocity %s"),*B->Id.ToString(),*B->Nodes[From].Id.ToString(),*B->Nodes[To].Id.ToString(),*Rider->GetActorLocation().ToString(),*Destination.ToString(),int32(Movement->MovementMode),int32(Rider->GetLocalRole()),Movement->HasValidData(),Movement->ShouldSkipUpdate(.05f),*Rider->GetPendingMovementInputVector().ToString(),*Movement->Velocity.ToString()));
        return false;
    };
    TArray<TArray<int32>> Links; Links.SetNum(B->Nodes.Num());
    for(int32 I=0;I<B->Edges.Num();++I) { Links[B->Edges[I].From].Add(I); Links[B->Edges[I].To].Add(I); }
    TSet<int32> Visited,UsedEdges;
    auto Mark=[&](int32 Node)
    {
        if(!Visited.Contains(Node)) { Visited.Add(Node); ++Out.VisitedNodes; }
        for(const auto& R:B->Rooms) if(R.RouteNode==Node) Out.VisitedRooms.AddUnique(R.Id);
    };
    TFunction<bool(int32)> Traverse=[&](int32 Node)
    {
        Mark(Node);
        for(int32 EI:Links[Node])
        {
            if(UsedEdges.Contains(EI)) continue; UsedEdges.Add(EI);
            const auto& E=B->Edges[EI]; const int32 Next=E.From==Node?E.To:E.From;
            if(!Walk(Node,Next,E)) return false;
            const bool Seen=Visited.Contains(Next); Mark(Next);
            if(!Seen && !Traverse(Next)) return false;
            if(!Walk(Next,Node,E)) return false;
        }
        return true;
    };
    if(Traverse(B->EntranceNode))
    {
        Out.bReturnedToStreet=true;
        for(FName Id:B->ExpectedRoomIds) if(!Out.VisitedRooms.Contains(Id)) Out.Errors.Add(TEXT("Room physically unreached: ")+Id.ToString());
        if(Out.OpenedDoors!=B->Doors.Num()) Out.Errors.Add(TEXT("Not every door was physically traversed"));
    }
#else
    Out.Errors.Add(TEXT("Physical validation is only available in Development/Editor"));
#endif
    return Out;
}
