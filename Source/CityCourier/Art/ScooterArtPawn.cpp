#include "Art/ScooterArtPawn.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/PawnMovementComponent.h"

AScooterArtPawn::AScooterArtPawn()
{
    BodyArt=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyArt"));BodyArt->SetupAttachment(VisualRoot);
    BodyArt->SetRelativeLocation(FVector(0,0,-52));BodyArt->SetRelativeRotation(FRotator(0,-90,0));
    FrontWheelArt=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrontWheelArt"));FrontWheelArt->SetupAttachment(VisualRoot);
    RearWheelArt=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RearWheelArt"));RearWheelArt->SetupAttachment(VisualRoot);
    FrontWheelArt->SetRelativeLocation(FVector(63,0,-25.5));RearWheelArt->SetRelativeLocation(FVector(-62,0,-25.5));
    FrontWheelArt->SetRelativeRotation(FRotator(0,-90,0));RearWheelArt->SetRelativeRotation(FRotator(0,-90,0));
    for(auto* Part:{BodyArt.Get(),FrontWheelArt.Get(),RearWheelArt.Get()}) Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    USceneComponent* Anchors[]={RiderSeat,LeftGrip,RightGrip,LeftPedal,RightPedal};
    const FVector Positions[]={{6,0,27},{49,-34,56},{49,34,56},{17,-20,-21},{17,20,-21}};
    for(int32 I=0;I<5;++I)
    {
        auto* Node=Anchors[I];Node->SetRelativeLocation(Positions[I]);
        RiderAnchors.Add(Node);
    }
}
void AScooterArtPawn::BeginPlay()
{
    Super::BeginPlay();
    BodyMesh->SetVisibility(false);
    if(BodyArt->GetStaticMesh()) for(const auto& Part:PrototypeParts) Part->SetVisibility(false,true);
}
void AScooterArtPawn::Tick(float Delta)
{
    Super::Tick(Delta);
    const float Distance=GetMovementComponent()?FVector::DotProduct(GetMovementComponent()->Velocity,GetActorForwardVector())*Delta:0;
    const float Degrees=FMath::RadiansToDegrees(Distance/26.5f);
    for(auto* Wheel:{FrontWheelArt.Get(),RearWheelArt.Get()}) Wheel->AddLocalRotation(FRotator(0,0,Degrees));
}
