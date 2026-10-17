#include "World/DeliveryLocation.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ADeliveryLocation::ADeliveryLocation()
{
    InteractionPoint = CreateDefaultSubobject<USceneComponent>(TEXT("InteractionPoint")); RootComponent = InteractionPoint;
    Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker")); Marker->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    Marker->SetStaticMesh(Mesh.Object); Marker->SetRelativeScale3D(FVector(1.5,1.5,0.03));
    Marker->SetRelativeLocation(FVector(0,0,5)); Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
