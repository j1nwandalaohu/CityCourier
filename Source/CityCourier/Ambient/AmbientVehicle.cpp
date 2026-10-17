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
    if(Patrol.Num()>1) SetActorLocationAndRotation(Patrol[0],(Patrol[1]-Patrol[0]).Rotation(),false,nullptr,ETeleportType::TeleportPhysics);
}
void AAmbientVehicle::SetPopulationActive(bool bActive)
{
    bPopulationActive=bActive && Patrol.Num()>1;
    SetActorHiddenInGame(!bPopulationActive); SetActorEnableCollision(bPopulationActive); SetActorTickEnabled(bPopulationActive);
    if(!bPopulationActive) Speed=0;
}
void AAmbientVehicle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(!bPopulationActive || DeltaSeconds<=0 || !FMath::IsFinite(DeltaSeconds))return;
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
        SetActorLocation(GetActorLocation()+Move,true,&Hit);
        if(Hit.bBlockingHit)Speed=0;
    }
}
