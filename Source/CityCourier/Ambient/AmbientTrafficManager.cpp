#include "Ambient/AmbientTrafficManager.h"
#include "Ambient/AmbientVehicle.h"
#include "Ambient/AmbientPedestrian.h"
#include "World/CityDefinition.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
AAmbientTrafficManager::AAmbientTrafficManager(){PrimaryActorTick.bCanEverTick=true;}
void AAmbientTrafficManager::Configure(UCityDefinition* Definition)
{
    if(City || !Definition)return;City=Definition;
    for(int32 I=0;I<6;++I){auto* Car=GetWorld()->SpawnActor<AAmbientVehicle>();Cars.Add(Car);Car->SetPopulationActive(false);}
    for(int32 I=0;I<12;++I){auto* Walker=GetWorld()->SpawnActor<AAmbientPedestrian>();Walkers.Add(Walker);Walker->SetPopulationActive(false);}
}
void AAmbientTrafficManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);RefreshTime-=DeltaSeconds;if(RefreshTime>0)return;RefreshTime=.5f;
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
        auto Ground=[&](FVector& Point)
        {
            FHitResult Hit;
            if(GetWorld()->LineTraceSingleByChannel(Hit,Point+FVector(0,0,500),Point-FVector(0,0,500),ECC_WorldStatic,Query))Point.Z=Hit.ImpactPoint.Z+Height;
        };
        Ground(A);Ground(B);
        if(FVector::Dist2D(A,RiderLocation)<1800)return;
        if(GetWorld()->OverlapBlockingTestByChannel(A,Forward.Rotation().Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(Entity->Body->GetScaledBoxExtent()),Query))return;
        if(bPed)Entity->SetPatrol({A,B});
        else Entity->SetPatrol({A,B,B-Right*Offset*Side*2,A-Right*Offset*Side*2});
        Entity->SetPopulationActive(true);
    };
    for(int32 I=0;I<Cars.Num();++I)Update(Cars[I],I,false);
    for(int32 I=0;I<Walkers.Num();++I)Update(Walkers[I],I,true);
}
int32 AAmbientTrafficManager::GetActiveVehicleCount()const{int32 N=0;for(AAmbientVehicle* Car:Cars)if(Car->IsPopulationActive())++N;return N;}
int32 AAmbientTrafficManager::GetActivePedestrianCount()const{int32 N=0;for(AAmbientPedestrian* Walker:Walkers)if(Walker->IsPopulationActive())++N;return N;}
