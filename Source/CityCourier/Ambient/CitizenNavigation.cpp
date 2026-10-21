#include "Ambient/CitizenNavigation.h"
#include "Ambient/AmbientPedestrian.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

void FCitizenNavigation::Reset(){*this=FCitizenNavigation();}
void FCitizenNavigation::SetPath(const TArray<FVector>& Path)
{Points=Path;Next=0;bWaiting=false;bFailed=false;StuckSeconds=0;}
bool FCitizenNavigation::CanMoveDirect(AAmbientPedestrian& P,FVector Goal,float MaxLength)
{
    const FVector Start=P.GetActorLocation();
    if(Goal.ContainsNaN() || FVector::Dist2D(Start,Goal)>MaxLength || FMath::Abs(Goal.Z-Start.Z)>60)return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(CitizenDirectPath),false,&P);
    FHitResult Hit;
    if(P.GetWorld()->SweepSingleByChannel(Hit,Start,Goal,(Goal-Start).GetSafeNormal2D().Rotation().Quaternion(),ECC_Pawn,
        FCollisionShape::MakeBox(P.Body->GetScaledBoxExtent()),Q))return false;
    const int32 Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Start,Goal)/50.f));
    double Ground=P.StreetGroundZ;
    for(int32 I=1;I<=Steps;++I)
        if(!P.FindStreetGround(FMath::Lerp(Start,Goal,double(I)/Steps),Ground,Ground))return false;
    return FMath::Abs(Ground+90-Goal.Z)<45;
}
float FCitizenNavigation::Advance(AAmbientPedestrian& P,FVector Goal,float DesiredSpeed,float Delta)
{
    if(Goal.ContainsNaN() || !FMath::IsFinite(Delta) || Delta<=0)return 0;
    const FVector Before=P.GetActorLocation();
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(Delta*60),1,32);
    const float Dt=FMath::Min(Delta,.5f)/Steps;
    for(int32 I=0;I<Steps;++I)
    {
        while(Points.IsValidIndex(Next) && FVector::Dist2D(P.GetActorLocation(),Points[Next])<40)++Next;
        const FVector Destination=Points.IsValidIndex(Next)?Points[Next]:Goal;
        FVector Direction=Destination-P.GetActorLocation();Direction.Z=0;
        const float Distance=Direction.Size2D();
        if(Distance<2){P.Speed=0;break;}
        Direction.Normalize();
        FCollisionQueryParams Q(SCENE_QUERY_STAT(CitizenMovement),false,&P);
        const FRotator Facing=FMath::RInterpConstantTo(P.GetActorRotation(),Direction.Rotation(),Dt,300);
        if(!P.GetWorld()->OverlapBlockingTestByChannel(P.GetActorLocation(),Facing.Quaternion(),ECC_Pawn,
            FCollisionShape::MakeBox(P.Body->GetScaledBoxExtent()),Q))P.SetActorRotation(Facing);
        P.Speed=FMath::FInterpConstantTo(P.Speed,DesiredSpeed,Dt,DesiredSpeed<P.Speed?700.f:500.f);
        const FVector Wanted=P.GetActorLocation()+Direction*FMath::Min(Distance,P.Speed*Dt);
        double Ground;
        if(!P.FindStreetGround(Wanted,P.StreetGroundZ,Ground)){P.Speed=0;break;}
        FHitResult Hit;
        // Sweep vertically as well; curbs cannot teleport a head through an overhang.
        const FVector Raised(P.GetActorLocation().X,P.GetActorLocation().Y,Ground+90);
        if(Ground>P.StreetGroundZ)P.SetActorLocation(Raised,true,&Hit);
        P.SetActorLocation(FVector(Wanted.X,Wanted.Y,P.GetActorLocation().Z),true,&Hit);
        double Actual;
        if(!P.FindStreetGround(P.GetActorLocation(),P.StreetGroundZ,Actual)){P.Speed=0;break;}
        P.StreetGroundZ=Actual;
        P.SetActorLocation(FVector(P.GetActorLocation().X,P.GetActorLocation().Y,Actual+90),true);
        if(Hit.bBlockingHit){P.ObserveBump(Hit.GetActor());P.Speed=0;break;}
    }
    const float Moved=FVector::Dist2D(Before,P.GetActorLocation());
    if(DesiredSpeed>0 && FVector::Dist2D(Before,Goal)>60)StuckSeconds=Moved<Delta*10?StuckSeconds+Delta:0;
    else StuckSeconds=0;
    P.Body->ComponentVelocity=(P.GetActorLocation()-Before)/FMath::Max(.001f,Delta);
    return Moved;
}
