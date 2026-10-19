#include "Ambient/AmbientVehicle.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
AAmbientVehicle::AAmbientVehicle()
{
    PrimaryActorTick.bCanEverTick=true;
    Body=CreateDefaultSubobject<UBoxComponent>(TEXT("TrafficCollision")); SetRootComponent(Body);
    Body->SetBoxExtent(FVector(205,88,65)); Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarBody")); Visual->SetupAttachment(Body);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetRelativeLocation(FVector(0,0,-70));
}
void AAmbientVehicle::BeginPlay()
{
    Super::BeginPlay();
    if(!bPedestrian) Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/CityCourier/Art/Ambient/SM_CityCar.SM_CityCar")));
}
void AAmbientVehicle::SetPatrol(const TArray<FVector>& Points)
{
    Patrol=Points;NextPoint=1;Speed=0;
    if(Patrol.Num()>1)
    {
        StreetGroundZ=Patrol[0].Z-GroundClearance();
        SetActorLocationAndRotation(Patrol[0],(Patrol[1]-Patrol[0]).GetSafeNormal2D().Rotation(),false,nullptr,ETeleportType::TeleportPhysics);
        double Ground;
        if(FindStreetGround(GetActorLocation(),StreetGroundZ,Ground))
        {
            StreetGroundZ=Ground;
            SetActorLocation(FVector(Patrol[0].X,Patrol[0].Y,Ground+GroundClearance()),false);
        }
    }
}
bool AAmbientVehicle::FindStreetGround(FVector Position,double ReferenceZ,double& OutGroundZ) const
{
    if(!GetWorld() || Position.ContainsNaN() || !FMath::IsFinite(ReferenceZ))return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(AmbientStreetGround),false,this);
    // Object filtering excludes people and cars even when their collision
    // profiles block WorldStatic traces. The short height band excludes roofs
    // and awnings; road graph height seeds it before the first activation.
    const FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);
    const FVector Forward=GetActorForwardVector().GetSafeNormal2D(),Right=GetActorRightVector().GetSafeNormal2D();
    const FVector Extent=Body->GetScaledBoxExtent();
    const FVector Along=Forward*(Extent.X+1),Across=Right*(Extent.Y+1);
    const FVector Probes[]={FVector::ZeroVector,Along+Across,Along-Across,-Along+Across,-Along-Across};
    OutGroundZ=-MAX_dbl;
    for(const FVector& Offset:Probes)
    {
        const FVector Probe=Position+Offset;
        FHitResult Hit;
        if(!GetWorld()->LineTraceSingleByObjectType(Hit,FVector(Probe.X,Probe.Y,ReferenceZ+35),FVector(Probe.X,Probe.Y,ReferenceZ-80),StaticOnly,Query)
            || Hit.bStartPenetrating || Hit.ImpactNormal.Z<.65)return false;
        OutGroundZ=FMath::Max(OutGroundZ,Hit.ImpactPoint.Z);
    }
    return true;
}
void AAmbientVehicle::SetPopulationActive(bool bActive)
{
    bPopulationActive=bActive && Patrol.Num()>1;
    if(bPopulationActive)
    {
        double Ground;
        bPopulationActive=FindStreetGround(GetActorLocation(),StreetGroundZ,Ground);
        if(bPopulationActive)
        {
            StreetGroundZ=Ground;
            const FVector P=GetActorLocation();
            SetActorLocation(FVector(P.X,P.Y,Ground+GroundClearance()),false);
        }
    }
    SetActorHiddenInGame(!bPopulationActive); SetActorEnableCollision(bPopulationActive); SetActorTickEnabled(bPopulationActive);
    if(!bPopulationActive) Speed=0;
}
void AAmbientVehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bPopulationActive || DeltaSeconds<=0 || !FMath::IsFinite(DeltaSeconds))return;
    double Ground;
    if(!FindStreetGround(GetActorLocation(),StreetGroundZ,Ground)){SetPopulationActive(false);return;}
    StreetGroundZ=Ground;
    const FVector Current=GetActorLocation();
    SetActorLocation(FVector(Current.X,Current.Y,Ground+GroundClearance()),false);
    const int32 Steps=FMath::Clamp(FMath::CeilToInt(DeltaSeconds*60),1,32);
    const float Dt=FMath::Min(DeltaSeconds,.5f)/Steps;
    for(int32 Step=0;Step<Steps;++Step)
    {
        FVector Delta=Patrol[NextPoint]-GetActorLocation(); Delta.Z=0;
        if(Delta.Size2D()<40) { NextPoint=(NextPoint+1)%Patrol.Num(); Delta=Patrol[NextPoint]-GetActorLocation(); Delta.Z=0; }
        const FVector Direction=Delta.GetSafeNormal();
        const FRotator Desired=Direction.Rotation();
        const FRotator Rotation=FMath::RInterpConstantTo(GetActorRotation(),Desired,Dt,bPedestrian?180.f:75.f);
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AmbientObstacle),false,this);
        FHitResult Hit;
        const FVector Extent=Body->GetScaledBoxExtent();
        const float StopDistance=70+Speed*.3f+Speed*Speed/(2*Deceleration);
        const bool bObstacle=GetWorld()->SweepSingleByChannel(Hit,GetActorLocation(),GetActorLocation()+Direction*StopDistance,
            Rotation.Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(Extent),Query);
        float DesiredSpeed=bObstacle?0:MaxSpeed;
        const float Angle=FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,Desired.Yaw));
        if(Angle>15)DesiredSpeed=FMath::Min(DesiredSpeed,bPedestrian?100.f:120.f);
        if(Delta.Size2D()<550) DesiredSpeed=FMath::Min(DesiredSpeed,FMath::Max(100.f,(float)Delta.Size2D()*.7f));
        Speed=FMath::FInterpConstantTo(Speed,DesiredSpeed,Dt,DesiredSpeed<Speed?Deceleration:Acceleration);
        // Do not rotate a long car into a neighbouring pawn or wall.
        if(!GetWorld()->OverlapBlockingTestByChannel(GetActorLocation(),Rotation.Quaternion(),ECC_Pawn,FCollisionShape::MakeBox(Extent),Query))
            SetActorRotation(Rotation);
        const FVector Move=(bPedestrian?Direction:GetActorForwardVector())*FMath::Min(Speed*Dt,(float)Delta.Size2D());
        const FVector Destination=GetActorLocation()+Move;
        double NextGround;
        if(!FindStreetGround(Destination,StreetGroundZ,NextGround)){Speed=0;return;}
        // Raise onto small curbs before the horizontal sweep. Keeping the
        // footprint's highest support prevents clipping on curb edges.
        if(NextGround>StreetGroundZ)SetActorLocation(FVector(GetActorLocation().X,GetActorLocation().Y,NextGround+GroundClearance()),false);
        SetActorLocation(FVector(Destination.X,Destination.Y,GetActorLocation().Z),true,&Hit);
        double ActualGround=NextGround;
        if(Hit.bBlockingHit && !FindStreetGround(GetActorLocation(),StreetGroundZ,ActualGround)){SetPopulationActive(false);return;}
        StreetGroundZ=ActualGround;
        const FVector Actual=GetActorLocation();
        SetActorLocation(FVector(Actual.X,Actual.Y,ActualGround+GroundClearance()),false);
        if(Hit.bBlockingHit)Speed=0;
    }
}
