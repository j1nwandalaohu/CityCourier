#include "Ambient/AmbientTrafficManager.h"
#include "Ambient/AmbientVehicle.h"
#include "Ambient/AmbientPedestrian.h"
#include "World/CityDefinition.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"
AAmbientTrafficManager::AAmbientTrafficManager(){PrimaryActorTick.bCanEverTick=true;}
void AAmbientTrafficManager::Configure(UCityDefinition* Definition)
{
    if(City || !Definition)return;City=Definition;
    for(int32 I=0;I<6;++I){auto* Car=GetWorld()->SpawnActor<AAmbientVehicle>();Cars.Add(Car);Car->SetPopulationActive(false);}
    for(int32 I=0;I<12;++I){auto* Walker=GetWorld()->SpawnActor<AAmbientPedestrian>();Walkers.Add(Walker);Walker->SetPopulationActive(false);}
}
void AAmbientTrafficManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(GetWorld()->IsPaused())return;
    ProcessOneCitizenPath();Coordination.Prune(GetWorld()->GetTimeSeconds());
    RefreshTime-=DeltaSeconds;if(RefreshTime>0)return;RefreshTime=.5f;
    if(auto* PC=GetWorld()->GetFirstPlayerController())if(APawn* Pawn=PC->GetPawn())UpdatePopulation(Pawn->GetActorLocation());
}
void AAmbientTrafficManager::UpdatePopulation(FVector RiderLocation)
{
    if(!City || RiderLocation.ContainsNaN())return;
    struct FLane { FVector A,B;float Distance; };
    TArray<FLane> Lanes;
    for(const auto& Edge:City->Roads.Edges)
    {
        const FVector A=City->Roads.FindNode(Edge.From)->Position,B=City->Roads.FindNode(Edge.To)->Position;
        // Only full-width streets; the two shortcut alleys remain available for the rider.
        if(FMath::IsNearlyEqual(FMath::Abs(A.X),6000.) && FMath::IsNearlyEqual(A.X,B.X))continue;
        const float Distance=FVector::Dist2D(RiderLocation,(A+B)*.5);
        if(Distance<16000 && Distance>1800)Lanes.Add({A,B,Distance});
    }
    Lanes.Sort([](const FLane& A,const FLane& B){return A.Distance<B.Distance;});
    auto Update=[&](AAmbientVehicle* Entity,int32 Slot,bool bPed)
    {
        if(Entity->IsPopulationActive() && FVector::Dist2D(Entity->GetActorLocation(),RiderLocation)<18000)return;
        Entity->SetPopulationActive(false);
        if(Lanes.IsEmpty())return;
        const FLane& Lane=Lanes[Slot%Lanes.Num()];
        const FVector Forward=(Lane.B-Lane.A).GetSafeNormal2D();const FVector Right(-Forward.Y,Forward.X,0);
        const float Side=(Slot%2?1.f:-1.f),Offset=bPed?620.f:220.f,Height=bPed?90.f:70.f;
        FVector A=Lane.A+Forward*900+Right*Offset*Side+FVector(0,0,Height);
        FVector B=Lane.B-Forward*900+Right*Offset*Side+FVector(0,0,Height);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AmbientSpawn),false,Entity);
        if(FVector::Dist2D(A,RiderLocation)<1800)return;
        if(bPed)Entity->SetPatrol({A,B});
        else Entity->SetPatrol({A,B,B-Right*Offset*Side*2,A-Right*Offset*Side*2});
        if(GetWorld()->OverlapBlockingTestByChannel(Entity->GetActorLocation(),Forward.Rotation().Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(Entity->Body->GetScaledBoxExtent()),Query))return;
        Entity->SetPopulationActive(true);
        if(bPed && Entity->IsPopulationActive())CastChecked<AAmbientPedestrian>(Entity)->InitializeCitizen(this,Slot);
    };
    for(int32 I=0;I<Cars.Num();++I)Update(Cars[I],I,false);
    for(int32 I=0;I<Walkers.Num();++I)Update(Walkers[I],I,true);
}
int32 AAmbientTrafficManager::GetActiveVehicleCount()const{int32 N=0;for(AAmbientVehicle* Car:Cars)if(Car->IsPopulationActive())++N;return N;}
int32 AAmbientTrafficManager::GetActivePedestrianCount()const{int32 N=0;for(AAmbientPedestrian* Walker:Walkers)if(Walker->IsPopulationActive())++N;return N;}
void AAmbientTrafficManager::QueueCitizenPath(const FCitizenPathRequest& R)
{
    if(!R.Citizen.IsValid() || R.Goal.ContainsNaN())return;
    PathRequests.RemoveAll([&](const auto& Pending){return !Pending.Citizen.IsValid() || Pending.Citizen==R.Citizen;});
    PathRequests.Add(R);
}
void AAmbientTrafficManager::ProcessOneCitizenPath()
{
    if(PathRequests.IsEmpty())return;
    const auto R=PathRequests[0];PathRequests.RemoveAt(0);
    auto* P=R.Citizen.Get();
    if(!P || !P->IsPopulationActive() || P->GetActivationGeneration()!=R.ActivationGeneration || P->PathSerial!=R.RequestSerial)return;
    FCitizenPathResult Result;Result.ActivationGeneration=R.ActivationGeneration;Result.RequestSerial=R.RequestSerial;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavAgentProperties Agent(40,180);
    const auto* Data=Nav?Nav->GetNavDataForProps(Agent,P->GetActorLocation()):nullptr;
    if(!Data)Result.Status=ECitizenPathStatus::NoNavigation;
    else
    {
        FNavLocation From,To;
        if(Nav->ProjectPointToNavigation(P->GetActorLocation()-FVector(0,0,90),From,FVector(100,100,150),Data)
            && Nav->ProjectPointToNavigation(R.Goal-FVector(0,0,90),To,FVector(100,100,150),Data))
        {
            FPathFindingQuery Query(P,*Data,From.Location,To.Location);
            Query.SetAllowPartialPaths(false);
            const FPathFindingResult Found=Nav->FindPathSync(Agent,Query);
            if(Found.IsSuccessful() && Found.Path.IsValid())
            {
                Result.Status=Found.Path->IsPartial()?ECitizenPathStatus::Partial:ECitizenPathStatus::Complete;
                for(const auto& Point:Found.Path->GetPathPoints())Result.Points.Add(Point.Location+FVector(0,0,90));
            }
        }
    }
    P->ApplyCitizenPath(Result);
}
void AAmbientTrafficManager::NotifyCitizenAssault(AAmbientPedestrian& Victim,const FCourierStrikeContext& C)
{
    auto* Attacker=C.Instigator.Get();
    if(!IsValid(Attacker) || !Coordination.RememberEvent(C.InstanceId,GetWorld()->GetTimeSeconds()))return;
    TArray<AAmbientPedestrian*> Nearby;
    for(AAmbientPedestrian* P:Walkers)
        if(IsValid(P) && P!=&Victim && P!=Attacker && P->IsPopulationActive()
            && FVector::DistSquared2D(P->GetActorLocation(),Victim.GetActorLocation())<=FMath::Square(1200.))Nearby.Add(P);
    Nearby.Sort([&](const auto& A,const auto& B){return FVector::DistSquared2D(A.GetActorLocation(),Victim.GetActorLocation())<
        FVector::DistSquared2D(B.GetActorLocation(),Victim.GetActorLocation());});
    int32 Count=0;
    for(auto* P:Nearby)
    {
        if(!P->CanSeeTarget(&Victim) || !P->CanSeeTarget(Attacker))continue;
        P->ReceiveWitnessEvent({C.InstanceId,Attacker,&Victim,C.Origin,1});
        if(++Count>=4)break;
    }
}
bool AAmbientTrafficManager::TryAcquireCitizenAttack(AAmbientPedestrian& P,AActor& Target)
{return P.IsReadyForAttackToken() && P.GetThreat()==&Target && Coordination.TryAcquire(P,Target,GetWorld()->GetTimeSeconds());}
bool AAmbientTrafficManager::TryJoinCitizenFight(AAmbientPedestrian& P,AActor& Target)
{return P.CanParticipateInFight() && P.GetThreat()==&Target && Coordination.TryJoin(P,Target,GetWorld()->GetTimeSeconds());}
void AAmbientTrafficManager::ReleaseCitizenAttack(AAmbientPedestrian& P,bool bLeaveGroup)
{Coordination.Release(P,bLeaveGroup);}
bool AAmbientTrafficManager::OwnsCitizenAttack(const AAmbientPedestrian& P,const AActor& Target) const
{return Coordination.Owns(P,Target,GetWorld()->GetTimeSeconds());}
